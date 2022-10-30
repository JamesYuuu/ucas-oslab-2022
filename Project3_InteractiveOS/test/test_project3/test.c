#include <unistd.h>
#include <stdio.h>

int main()
{
    char *send = 'Hello There';
    char *receive;
    int handle = sys_mbox_open("test");
    sys_mbox_send(handle, send, 11);
    sys_mbox_recv(handle, receive, 11);
    printf("%s", receive);
    return 0;
}