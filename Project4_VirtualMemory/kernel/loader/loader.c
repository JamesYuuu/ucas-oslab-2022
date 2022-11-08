#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define BLOCK_NUM       15

//uint64_t load_task_img(int task_id)
uint64_t load_task_img(char *taskname)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    for (short i=0;i<TASK_MAXNUM;i++)
    {
        if (strcmp(taskname,tasks[i].task_name)==0)
        {   
            int offset=tasks[i].start_addr%512;
            int block_num=(tasks[i].filesz-1)/512+1;
            int block_id=tasks[i].start_addr/512;
            int task_size=tasks[i].filesz;
            bios_sdread(tasks[i].entry_point,block_num,block_id);
           
            // The code below copy the app to the original place if needed
            uint8_t *dest=(uint8_t*)tasks[i].entry_point;
            uint8_t *src=(uint8_t*)(tasks[i].entry_point+offset);
            memcpy(dest,src,task_size);
            return tasks[i].entry_point;     
        }
    }
    return 0;
}