/* killtest.c
 *	Project 2 test: Kill.
 *	Fork a child that would yield forever, then Kill it.
 *	Killing it again, or killing a pid that doesn't exist, fails.
 *	Expected prints: "killed process 2", then two
 *	"cannot kill process ...: doesn't exist" lines, and the parent
 *	exits with 0. Exits with 1 if any return value is wrong.
 */
#include "syscall.h"

void
forever()
{
    while (1)
        Yield();
}

int
main()
{
    int id, r1, r2, r3;

    id = Fork(forever);
    Yield();
    Yield();
    r1 = Kill(id);              /* should work: 0 */
    r2 = Kill(id);              /* already dead: -1 */
    r3 = Kill(20);              /* never existed: -1 */

    if (r1 == 0 && r2 == -1 && r3 == -1)
        Exit(0);
    Exit(1);
}
