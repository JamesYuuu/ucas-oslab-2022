#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define SHMP_KEY 42
#define BARRIER_KEY 42

char *send_buffer = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

char *log_name;
int buffer_length;
int send_length[8];
char buffer[256];

void display_usage(void)
{
    printf("Usage: exec test [log_name] [mailbox_name] [buffer_length]\n");
}

void send_thread(void *i)
{
    int thread_id = (int)i + 1;
    int id = sys_barrier_init(BARRIER_KEY, 8);
    char *mailbox_name = (char*)sys_shmpageget(SHMP_KEY);
    int mailbox_id = sys_mbox_open((char *)mailbox_name);
    for (int i=0;i<10;i++)
    {
        sys_barrier_wait(id);
        send_length[thread_id-1] = rand() % 10;
        sys_mbox_send(mailbox_id, send_buffer, send_length[thread_id-1]);
        sys_move_cursor(0,thread_id);
        printf("Thread %d send message length %d",thread_id,send_length[thread_id-1]);
        sys_sleep(1);
    } 
    sys_exit();
}

void father_process(void)
{
    char* mailbox_name = (char*)sys_shmpageget(SHMP_KEY);
    int mailbox_id = sys_mbox_open(mailbox_name);
    int fd = sys_fopen(log_name,O_RDWR);
    int total_length = 0;
    char *second;
    for (int i=0;i<10;i++)
    {
        for (int j=0;j<8;j++)
        {
            sys_mbox_recv(mailbox_id,buffer,send_length[j]);
            total_length += send_length[j];
            sys_move_cursor(0,0);
            printf("Recieve %s length %d",buffer,send_length[j]);
            if (total_length > buffer_length)
            {
                int current_time = sec();
                int len = current_time < 10 ? 1 : 2;
                itoa(current_time,second,len,10);
                sys_fwrite(fd,"Blocking time:",14);
                sys_fwrite(fd,second,len);
                sys_fwrite(fd,"\n",1);
                total_length = 0;
            }
        }
    }
    sys_exit();
}

void child_process(void)
{
    pthread_t thread[8];
    for (int i=0;i<8;i++)
        pthread_create(thread+i,send_thread,(void*)i);
}

int main(int argc, char *argv[])
{
    if (argc<4)
    {
        display_usage();
        return;
    }

    log_name = argv[1];
    buffer_length = atoi(argv[3]);

    char *mailbox_name = (char*)sys_shmpageget(SHMP_KEY);
    memcpy(mailbox_name,argv[2],strlen(argv[2]));

    sys_touch(log_name);
    sys_mbox_open(mailbox_name);

    srand(clock());

    // if (sys_fork()==0)  child_process();
    //     else father_process();

    pthread_t a;
    pthread_t b;

    pthread_create(&a,father_process,NULL);
    child_process();

    return 0;
}