/* forkexec.c
 *	Project 2 test: Fork + Exec together (the way to start a new
 *	program without replacing yourself).
 *	The parent forks a child, the child Execs yieldexit. The parent
 *	keeps yielding and then exits with 0. Expected: the child
 *	(process 2) exits with 5, the parent (process 1) exits with 0.
 */
#include "syscall.h"

void
child()
{
    Exec("../test/yieldexit");
    Exit(2);                    /* only reached if Exec failed */
}

int
main()
{
    Fork(child);
    Yield();
    Yield();
    Yield();
    Yield();
    Yield();
    Exit(0);
}
