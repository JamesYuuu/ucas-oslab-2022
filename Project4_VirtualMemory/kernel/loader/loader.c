#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define BLOCK_SIZE 512
#define BLOCK_NUM NORMAL_PAGE_SIZE / BLOCK_SIZE

void load_task_img(int task_id, uintptr_t kva, int page_num)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    int filesz = tasks[task_id].filesz;
    int memsz = tasks[task_id].memsz;

    // If this page is bss sector, set it to zero and return;
    if (page_num * NORMAL_PAGE_SIZE > filesz)
    {
        memset((void *)(kva), 0, NORMAL_PAGE_SIZE);
        return;
    }

    // If page_num = 0, we need to align
    if (page_num == 0)
    {
        int offset = tasks[task_id].start_addr % BLOCK_SIZE;
        int block_id = tasks[task_id].start_addr / BLOCK_SIZE;
        bios_sdread(kva2pa(kva), BLOCK_NUM, block_id);
        memcpy((void *)kva, (void *)(kva + offset), NORMAL_PAGE_SIZE);
    }
    else
    {
        int block_id = tasks[task_id].start_addr / BLOCK_SIZE + page_num * BLOCK_NUM;
        bios_sdread(kva2pa(kva), BLOCK_NUM, block_id);
    }

    if ((page_num + 1) * NORMAL_PAGE_SIZE > filesz)
        memset((void *)(kva + filesz - page_num * NORMAL_PAGE_SIZE), 0, (page_num + 1) * NORMAL_PAGE_SIZE - filesz);

    return;
}