#include <stdio.h>
#include <unistd.h>

#define TOTAL 1000
#define THREAD_NUM 2
#define NUM TOTAL/THREAD_NUM

static char blank[] = {"                                                "};
static int print_location = 6;
int data[TOTAL];
int sum[THREAD_NUM];
int thread_finish[THREAD_NUM];

void* add(void *arg)
{
    int offset=(int)arg;
    for (int i=0;i<NUM;i++)
    {
        sys_move_cursor(0, print_location+offset);
        printf("> [TASK] Thread%d adds %d to sum \n", offset+1,i+offset*NUM);
        sum[offset]+=data[i+offset*NUM];
    }
    thread_finish[offset]=1;
    while (1);
}
int main(void)
{
    int total_sum,finish;
    total_sum=0;
    for (int i=0;i<THREAD_NUM;i++)
        thread_finish[i]=sum[i]=0;
    for (int i=0;i<1000;i++)
        data[i]=i;
    sys_move_cursor(0,print_location+THREAD_NUM);
    printf("> [TASK] Thread Test Start");
    for (int i=0;i<THREAD_NUM;i++)
        sys_create_thread(add,(void*)i);
    while (1)
    {
        finish=0;
        for (int i=0;i<THREAD_NUM;i++)
            finish+=thread_finish[i];
        if (finish==THREAD_NUM) break;
    }
    for (int i=0;i<THREAD_NUM;i++)
        total_sum+=sum[i];
    sys_move_cursor(0,print_location+THREAD_NUM);
    printf("> [TASK] Thread finished and total sum is %d.\n",total_sum);
    while (1);
}

