#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define BASE_TEST_NUM 5
#define BASE_TEST_ADDR 0x100000
#define BASE_TEST_NUMBER 1000
#define PAGE_SIZE 4096

int main(int argc, char *argv[])
{
    int index[BASE_TEST_NUM];
    srand(clock());
    printf("> [Info] Start generating random numbers...\n");
    for (int i = 0; i < BASE_TEST_NUM; i++)
    {
        uintptr_t addr = BASE_TEST_ADDR + PAGE_SIZE * i;
        int val = rand() % BASE_TEST_NUMBER;
        *(int *)addr = val;
        printf("0x%x:%d ",sys_getpa(addr,-1),val);
    }
    printf("\n> [Info] Initialization finished and start testing snapshot!\n");
    for (int i = 0; i < BASE_TEST_NUM; i++)
    {
        index[i] = sys_snapshot_shot(BASE_TEST_ADDR);
        int j = rand() % BASE_TEST_NUM;
        int val = rand() % BASE_TEST_NUMBER;
        *(int *)(BASE_TEST_ADDR + PAGE_SIZE * j) = val;
        printf("> [Info] Snapshot[%d]: change %dth page to %d\n",i,j+1,val);
    }
    printf("> [Info] Snapshot finished and start testing snapshot restore!");
    for (int i = 0; i < BASE_TEST_NUM; i++)
    {
        sys_snapshot_restore(index[i]);
        printf("\n> [Info] Snapshot[%d]:\n",i);
        for (int j = 0; j < BASE_TEST_NUM; j++)
        {
            uintptr_t addr = BASE_TEST_ADDR + PAGE_SIZE * j;
            int val = *(int *)addr;
            printf("0x%x:%d ",sys_getpa(addr,index[i]),val);
        }
    }
    sys_snapshot_restore(-1);
    printf("\n> [Info] Final result...\n");
    for (int i = 0; i < BASE_TEST_NUM; i++)
    {
        uintptr_t addr = BASE_TEST_ADDR + PAGE_SIZE * i;
        int val = *(int *)addr;
        printf("0x%x:%d ",sys_getpa(addr,-1),val);
    }
    return 0;
}