#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

#define TASK_ADDRESS    0x52000000
#define START_ADDRESS   0x50200000
#define BLOCK_NUM       15

uint64_t load_task_img(int task_id)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    // The code below load task from image via task id for [p1-task3]

    unsigned int current_address=TASK_ADDRESS+0x10000*task_id;
    unsigned int block_id=1+task_id*BLOCK_NUM;
    
    bios_sdread(current_address,BLOCK_NUM,block_id);

    return (long)current_address;
}