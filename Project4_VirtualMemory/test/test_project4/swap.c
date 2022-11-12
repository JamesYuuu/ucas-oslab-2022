#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define LEN 5
int main(int argc, char* argv[])
{
    sys_clear(0);
    sys_move_cursor(0, 0);
    printf("[TASK] Starting test swap system");
    for (int i=1;i<=10;i++)
    {
        char print_location[LEN];
        itoa(i, print_location, LEN, 10);
        char *argv[2] = {"lock",print_location};
        sys_exec(argv[0],2,argv);
    }
    return;
}