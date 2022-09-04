#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000

#define NAME_MAXNUM      16
/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char task_name[NAME_MAXNUM];
    uint64_t entry_point;
    uint32_t block_num;
    uint32_t block_id;
    uint32_t offset;
    uint32_t task_size;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif