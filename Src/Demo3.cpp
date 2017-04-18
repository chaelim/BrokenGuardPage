// cl.exe /Ox /EHsc demo1.cpp

#include <SDKDDKVer.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <process.h>

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

void Crash()
{
    // This function simply allocate a big local array (24KiB) and initalizes first element
    volatile char test[0x6000];
    test[0] = 1;
}

static unsigned __stdcall ThreadProc(LPVOID param)
{
    printf ("ThreadProc()\n");

    void *stacktopPtr = (void *) ((SIZE_T *) param - sizeof(DWORD_PTR));

    // Try to break all guard pages (3 pages) above the current stack top.
    for (int i = 0; i < 3; i++) {
        printf ("%3d: Try to access memory at %p\n", i, stacktopPtr);
        // Read the memory
        volatile int test = *(int *)stacktopPtr;
        stacktopPtr = (void *)((UINT_PTR)stacktopPtr - 0x1000);
    }

    return 0;
}

static void AccessStackGuardFromOtherThread()
{
    _NT_TIB * tib = getTib();

    printf("StackBase = %p, StackLimit = %p\n\n", tib->StackBase, tib->StackLimit);

    auto guardMemory = tib->StackLimit;
    auto teb = NtCurrentTeb();

    HANDLE exceptionThread = (HANDLE)_beginthreadex(
        NULL,
        0,          // stack size
        ThreadProc,
        guardMemory, // <== Pass the stack top memory location
        0,          // suspended = false
        NULL
    );

    // Wait for the thread finish
    WaitForSingleObject(exceptionThread, INFINITE);
}


static LONG WINAPI OurUnhandledExceptionFilter (EXCEPTION_POINTERS * ep)
{
    printf(
        "OurUnhandledExceptionFilter: exception code = %X\n",
        ep->ExceptionRecord->ExceptionCode
    );  
    
    if (ep->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION) {
        printf(
            "STATUS_GUARD_PAGE_VIOLATION :%llx\n",
            ep->ExceptionRecord->ExceptionInformation[1]
        );
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

int _tmain(int argc, _TCHAR* argv[])
{
    // Set our unhandled exception filter to see what exception occurs
    // but it's not necessary to demonstrate the problem.
    SetUnhandledExceptionFilter(OurUnhandledExceptionFilter);

    AccessStackGuardFromOtherThread();

    printf("\nCalling crash()\n\n");

    Crash();

    printf("End of Program.\n");   // <=== This will never be called
}
