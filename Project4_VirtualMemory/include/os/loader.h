#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

void load_task_img(int task_id, uintptr_t kva, uintptr_t prev_kva, int page_num);

#endif