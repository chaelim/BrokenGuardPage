Broken Stack Guard Page
================================
Demo programs show how stack expansion works in Windows and how it's surprisingly fragile so that an application can even break other program's stack guard page that can cause clueless crash sometime later.

## Note ##
* How to compile and Run demo program
    1. Open Visual C++ command prompt.
    1. To compile, go to Src folder and enter `cl.exe /Ox /EHsc demoN.cpp` (N is number and currently 1, 2 or 3)
    1. Enter `demoX.exe` to run the demoX.

* As a first step, all demo programs get stack top address of main thread from [TIB](https://en.wikipedia.org/wiki/Win32_Thread_Information_Block) (Thread Information Block).

## Demo1 ##
* In this demo, `AccessStackGuardMemory` function is simply trying to read memory address at the beginning of the one page (4KiB) above the current stack top address. (Remember stack grows to lower address from higher address).
* Use this command line to compile `cl.exe /Ox /EHsc demo1.cpp /link /stack:262144` to set max stack size as 256KiB.
* It continues to move the memory pointer up by 1 page (in other words, subtract 0x1000 from the current address) and try to read `int` (4 bytes) value. You have to hit any key Other than ESC to continue to move to the next page.
* This essentially simulating nested functions calls and each function allocate or use 4KiB stack memory probably for local variables.
* Stack guard page is set up above the current stack top. Whenever this program is trying to access (read) a memory in the guard page area, `STATUS_GUARD_PAGE_VIOLATION` exception occurs. This exception is caught by the Windows Kernel exception handler and it'll expand or commit current stack by 1 page. 
* When you keep hold a non-ESC key for a while, this program eventually hit stackoverflow exception (0xC00000FD) because it'll hit the maximum stack size which is specified at link time. ([Default is 1MiB](https://msdn.microsoft.com/en-us/library/windows/desktop/ms686774(v=vs.85).aspx))
    * ![Demo1 output](img/Demo1_StackOverflow.PNG)
* There is linker option `/STACK:reserve[,commit]` sets the size of the stack. Default reserve size is 1 MiB. On my Windows 10, the minimum reserved stack size seems 256 KiB because the value is ignored if I specify smaller than 256KiB.
* While running, launch [`Vmmap.exe`](https://technet.microsoft.com/en-us/sysinternals/vmmap.aspx) and select `demo1.exe` 
    * ![VMMAP](img/vmmap.PNG)
* Hit <Space> key and on the command window where `demo1.exe` is running to expand the stack then go back to the vmmap and refresh. You can see the commit memory is growing.

## Demo2 ##
* This program tries to just **read** main thread's stack guard pages from a **different** thread in same process.
* What this program does:
    1. Sets up unhandled exception filter to catch and write to console when Windows structured exception is thrown.
    1. Create a new thread and passes main thread's stack limit address (stack top)
    1. In a new thread, try to read above the stack top address where stack guard page exists.
* You can see `STATUS_GUARD_PAGE_VIOLATION` (OurUnhandledExceptionFilter receives the exception) is thrown whenever try to access stack guard page. Normally when thread stack is growing and touching guard page this exception is handled by Kernel code you won't see the exception. However, in this demo, the guard page is touched by **other** thread then no stack expansion happens. If you continue to hit <Space> key, it eventually tries to access above  the stack guard pages and you will see `ACCESS_VIOLATION` exception.
* As far as I remember, in previous versions of Windows (Maybe it was Windows 7), Windows didn't even throw the `STATUS_GUARD_PAGE_VIOLATION` exception so it just blew up the guard pages.

## Demo3 ##
* Modified Demo2 a little bit. `ThreadProc` accesses (3 pages) above the current stack top to break all 3 stack guard pages. Then calling Crash() function which allocate large local variable.
* The Crash() function throws `ACCESS_VIOLATION` and never returns.
* This program shows that random memory read on other thread stack's guard pages can cause access violation later. It should be very rare and probably almost never happen if your program is well-written.

## Demo 4 ##
This is last demo showing the most interesting scenario. A malicious process just doing **READ** access on other process thread's stack guard page causes access violation crash of the victim process.

* Malicious process needs following privileges to target victim process
    * `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_CREATE_THREAD`
* Executing `IsBadCodePtr` from the threads in the victim process.
    * There may be another good reason that Microsoft trying to discontinue the `IsBadCodePtr` API (https://msdn.microsoft.com/en-us/library/windows/desktop/aa366712(v=vs.85).aspx)
* Let's crash Chrome browser:
    1. Compile command line is slightly different for this demo
        * Use `cl.exe /Ox /EHsc demo4.cpp /lib Shlwapi.lib`
    1. Launch Chrome and load couple of tabs.
    1. Run demo4.exe like below on command line. 
        * `demo4.exe chrome.exe`
        * I opened 3 YouTube tabs.
        * ![Chrome Loaded 3 YouTude Tabs](img/Chrome1.PNG)
    1. Go back to the Chrome browser click individual tab.
        * You are likely see some of tabs are crashed. If it's not crashing, you can try again or load other more expensive web site.
        * ![Crashed Tabs](img/Chrome2.PNG)
    1. Also sometimes Windows Error Reporting dialog pops up.
        * ![Chrome WER](img/Chrome_WER.PNG) 
    1. When you attach debugger, you will see something like below. It's access violation in random function. Unfortunately it won't tell what really happened.
        * ![Postmortem debugger VS](img/VS_Debugger.PNG)
        * ![Postmortem debugger ntsd](img/ntsd_postmortem_debug.PNG)

## What this means?
- It shows how stack growing mechanism is fragile in especially multi-threaded environment. A subtle bug in one thread that read access to other thread's can crash the application. See this [Mark's blog](http://blogs.technet.com/b/markrussinovich/archive/2009/07/08/3261309.aspx).
- Allowing `PROCESS_VM_READ` from you program can lead serious security issue like allowing crash your application from any other apps. See this [Blog](http://blogs.msdn.com/b/oldnewthing/archive/2006/01/17/513779.aspx)
- IsBadxxxPtr APIs are dangerous [3](http://blogs.msdn.com/b/larryosterman/archive/2004/05/18/134471.aspx) 
- .NET commits whole stack memory (no run-time growing). They chose stability by sacrificing more memory.
- For servers, I always **commits whole stack memory** especially for the worker threads. I think eliminating run-time stack growing will give tiny bit of perf benefits as well.
