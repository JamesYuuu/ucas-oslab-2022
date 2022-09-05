#include <os/task.h>
#include <os/string.h>
#include <os/bios.h>
#include <type.h>

#define BLOCK_NUM       15

//uint64_t load_task_img(int task_id)
uint64_t load_task_img(char *taskname, int task_num)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    // The code below load task from image via task id for [p1-task3]
    /*
    unsigned int current_address=TASK_MEM_BASE+TASK_SIZE*(task_id-1);
    unsigned int block_id=1+task_id*BLOCK_NUM; 
    bios_sdread(current_address,BLOCK_NUM,block_id);
    return (uint64_t)current_address;
    */

    for (int i=0;i<task_num;i++)
    {
        if (strcmp(taskname,tasks[i].task_name)==0)
        {         
            bios_sdread(tasks[i].entry_point,tasks[i].block_num,tasks[i].block_id);
            
            // The code below copy the app to the original place if needed
            uint8_t *dest=(uint8_t*)tasks[i].entry_point;
            uint8_t *src=(uint8_t*)(tasks[i].entry_point+tasks[i].offset);
            uint32_t len=tasks[i].task_size;
            memcpy(dest,src,len);
            return tasks[i].entry_point;     
        }
    }
    return -1;
}