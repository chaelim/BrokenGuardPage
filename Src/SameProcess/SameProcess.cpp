// SameProcess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// Using Microsoft's intrinsics instead of inline assembly
static _NT_TIB * getTib()
{
    return (_NT_TIB *) __readfsdword(0x18);
}

void Crash() {
    printf("Crash\n");

    // This function simply allocate a big local array and initalizes first element
    volatile char test[0x4000];
    test[0] = 0;
}

static unsigned __stdcall ThreadProc(LPVOID param) {
    printf ("ThreadProc\n\tTry to access Stack Guard Page\n");
    
    IsBadCodePtr((FARPROC)((SIZE_T *)param-1));
    // param points stack limit of main thread
    // try to read access stack guard page of the main thread.
    //volatile SIZE_T mem = *((SIZE_T *)param-1);
    return 0;
}

static void AccessStackGuardFromOtherThread() {
    printf("AccessStackGuardFromOtherThread\n");

    _NT_TIB * tib = getTib();

    HANDLE exceptionThread = (HANDLE)_beginthreadex(
        NULL,
        0,          // stack size
        ThreadProc,
        tib->StackLimit, // <== Pass the stack top memory location
        0,          // suspended = false
        NULL
    );
    
    // Wait for the thread finish
    WaitForSingleObject(exceptionThread, INFINITE);
}

static LONG WINAPI OurUnhandledExceptionFilter (EXCEPTION_POINTERS * ep) {
    printf(
        "OurUnhandledExceptionFilter: exception code = %X\n",
        ep->ExceptionRecord->ExceptionCode
    );  
    
    if (ep->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION) {
        printf(
            "STATUS_GUARD_PAGE_VIOLATION :%0X\n",
            ep->ExceptionRecord->ExceptionInformation[1]
        );
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

int _tmain(int argc, _TCHAR* argv[]) {
    // Set our unhandled exception filter to see what exception occurs
    // but it's not necessary to demonstrate the problem.
    SetUnhandledExceptionFilter(OurUnhandledExceptionFilter);

    AccessStackGuardFromOtherThread();
    Crash();

    printf("End of Program.\n");   // <=== This will never be called
}
