/* forktest.c
 *	Project 2 test: Fork.
 *	The parent forks a child that runs child(). Both yield back and
 *	forth a few times, then the child exits with 7 and the parent
 *	exits with 0. A second Fork is tried to show what happens when
 *	memory is full (it should return -1, not crash).
 */
#include "syscall.h"

void
child()
{
    Yield();
    Yield();
    Exit(7);
}

int
main()
{
    Fork(child);
    Fork(child);
    Yield();
    Yield();
    Yield();
    Exit(0);
}
