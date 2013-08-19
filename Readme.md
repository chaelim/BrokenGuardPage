C++ Template class implementation of Hash Array Mapped Trie
================================

http://www.bluebytesoftware.com/blog/2006/07/15/CheckingForSufficientStackSpace.aspx
For example, I know that, at least as late as Windows XP, a Win32 CRITICAL_SECTION that has been initialized so as to never block can actually end up stack overflowing in the process of trying to acquire the lock. Yet MSDN claims it cannot fail if the spin count is high enough. A stack overflow here can actually lead to orphaned critical sections, deadlocks, and generally unreliable software in low stack conditions.
we pre-commit the entire stack to ensure that overflows won't occur due to failure to commit individual pages in the stack. 

http://blogs.msdn.com/b/larryosterman/archive/2004/05/18/134471.aspx