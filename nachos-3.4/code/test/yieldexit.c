/* yieldexit.c
 *	Project 2 test: Yield a few times, then Exit with status 5.
 *	Expected: three "invoked Yield" lines, then
 *	"Process 1 exits with 5".
 */
#include "syscall.h"

int
main()
{
    Yield();
    Yield();
    Yield();
    Exit(5);
}
