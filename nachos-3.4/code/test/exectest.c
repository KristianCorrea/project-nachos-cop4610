/* exectest.c
 *	Project 2 test: Exec.
 *	First tries to Exec a file that doesn't exist (should return -1
 *	and keep running), then Execs yieldexit, which replaces this
 *	program. Expected: yieldexit's 3 Yields, then
 *	"Process 1 exits with 5". If we ever see "exits with 99",
 *	Exec did not replace the program.
 */
#include "syscall.h"

int
main()
{
    int result;

    result = Exec("../test/doesnotexist");
    if (result == -1)
        Exec("../test/yieldexit");
    Exit(99);
}
