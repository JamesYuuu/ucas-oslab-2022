#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define BLOCK_SIZE 512
#define BLOCK_NUM NORMAL_PAGE_SIZE / BLOCK_SIZE

void load_task_img(int task_id, uintptr_t kva, uintptr_t prev_kva, int page_num)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    uint32_t filesz = tasks[task_id].filesz;
    uint32_t memsz = tasks[task_id].memsz;
    uint32_t offset = tasks[task_id].start_addr % BLOCK_SIZE;
    uint32_t block_id = tasks[task_id].start_addr / BLOCK_SIZE + page_num * BLOCK_NUM;

    // bss sector
    if ((int64_t)(page_num * NORMAL_PAGE_SIZE - offset) > (int64_t)filesz)
        return;

    bios_sdread(kva2pa(kva), BLOCK_NUM, block_id);

    if (page_num != 0)
        memcpy((void *)prev_kva + NORMAL_PAGE_SIZE - offset, (void *)kva, offset);

    memcpy((void *)kva, (void *)(kva + offset), NORMAL_PAGE_SIZE - offset);

    return;
}