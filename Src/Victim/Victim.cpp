// Victim.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

static bool g_stackGuardPageBroken = false;

// Get Windows TIB for current thread
static _NT_TIB * getTib()
{
    return (_NT_TIB *) __readfsdword(0x18);
}

void DoSomething() {
    Sleep(200);
}

void TryToExpandStack() {
    // This function simply allocate a big local array and initalizes first element
    volatile char test[0x4000];
    test[0] = 0;
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
        g_stackGuardPageBroken = true;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

int _tmain(int argc, _TCHAR* argv[]) {
    // Set our unhandled exception filter to see what exception occurs
    // but it's not necessary to demonstrate the problem.
    SetUnhandledExceptionFilter(OurUnhandledExceptionFilter);
    printf("Hit Ctrl+C to exit.\n");

    _NT_TIB * tib = getTib();
    printf("StackLimit = 0x%08X\n", tib->StackLimit);
    //while (!g_stackGuardPageBroken) {
    //    DoSomething();
    //}
    Sleep(20000);
    TryToExpandStack();
    printf("End of the Program\n");   // <=== This will never be called
}
