#include <time.h>

clock_t clock()
{
    return (clock_t)sys_get_tick();
}

clock_t sec()
{
    return clock() / CLOCKS_PER_SEC;
}