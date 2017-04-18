// 
// Compile command: cl.exe /Ox /EHsc /stack:10000 demo1.cpp
//

#include <SDKDDKVer.h>
#include <stdio.h>
#include <conio.h>
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

static void AccessStackGuardMemory()
{
    printf ("TryToAccessStackGuardMemory()\n");

    _NT_TIB * tib = getTib();

    printf("StackBase = %p, StackLimit = %p (Size=%d)\n\n",
        tib->StackBase,
        tib->StackLimit,
        (unsigned)((char *)(tib->StackBase) - (char *)(tib->StackLimit)) + 1);

    void *stacktopPtr = (void *) ((SIZE_T *) tib->StackLimit - sizeof(DWORD_PTR));

    // Accessing memory pages until surpass stack top.
    for (int i = 1; i <= 256; i++) {
        printf ("%3d: Try to access memory at %p\n", i, stacktopPtr);
        volatile int test = *(int *)stacktopPtr;
        stacktopPtr = (void *)((UINT_PTR)stacktopPtr - 0x1000);
        printf("    Press ESC key to exit, otherwise hit any key to continue.\n");
        if (_getch() == 27)
            break;
    }

    printf("\nStackBase = %p, StackLimit = %p\n\n", tib->StackBase, tib->StackLimit);
}

static LONG WINAPI OurUnhandledExceptionFilter (EXCEPTION_POINTERS * ep)
{
    printf(
        "OurUnhandledExceptionFilter: exception code = 0x%X\n",
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
    SetUnhandledExceptionFilter(OurUnhandledExceptionFilter);

    printf("\nCalling AccessStackGuardMemory()\n\n");

    AccessStackGuardMemory();

    printf("End of Program.\n");   // <=== This will never be called
}
