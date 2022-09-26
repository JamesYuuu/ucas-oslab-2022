#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

uint64_t load_task_img(char *taskname, short task_num)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    
    for (int i=0;i<task_num;i++)
    {
        if (strcmp(taskname,tasks[i].task_name)==0)
        {   
            int offset=tasks[i].start_addr%512;
            int block_num=(tasks[i].end_addr-1)/512+1-tasks[i].start_addr/512;
            int block_id=tasks[i].start_addr/512;
            int task_size=tasks[i].end_addr-tasks[i].start_addr;
            bios_sdread(tasks[i].entry_point,block_num,block_id);
           
            // The code below copy the app to the original place if needed
            uint8_t *dest=(uint8_t*)tasks[i].entry_point;
            uint8_t *src=(uint8_t*)(tasks[i].entry_point+offset);
            memcpy(dest,src,task_size);
            return tasks[i].entry_point;     
        }
    }
    return -1;
}