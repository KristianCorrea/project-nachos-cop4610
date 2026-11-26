/* jointest.c
 *	Project 2 test: Join.
 *	1) Fork a child that yields then exits with 7. Join waits for it
 *	   and gets 7.
 *	2) Fork a child that exits with 8 right away. The parent yields
 *	   first so the child is already gone, then Joins: still gets 8.
 *	3) Join on a pid that isn't our child returns -1.
 *	Expected: parent exits with 15 (7 + 8). Anything else = a bug.
 */
#include "syscall.h"

void
slowChild()
{
    Yield();
    Yield();
    Exit(7);
}

void
fastChild()
{
    Exit(8);
}

int
main()
{
    int id1, id2, s1, s2, bad;

    id1 = Fork(slowChild);
    s1 = Join(id1);             /* waits for slowChild */

    id2 = Fork(fastChild);
    Yield();
    Yield();
    Yield();
    s2 = Join(id2);             /* child already exited */

    bad = Join(25);             /* not our child */

    if (s1 == 7 && s2 == 8 && bad == -1)
        Exit(s1 + s2);
    Exit(-5);
}
