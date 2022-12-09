/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>
#include <os/list.h>
#include <os/sched.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK + PAGE_SIZE * 3)

#define FREEMEM_KERNEL_END 0xffffffc060000000
// #define FREE_PAGE_NUM 100    // used for testing swap system
#define FREE_PAGE_NUM (FREEMEM_KERNEL_END-FREEMEM_KERNEL) / NORMAL_PAGE_SIZE

#define FREE_DISK_SIZE 1024 * 1024 * 128            // 128M swap space
#define FREE_DISK_NUM (FREE_DISK_SIZE / NORMAL_PAGE_SIZE)

#define SHARED_PAGE_NUM 16
#define SNAPSHOT_NUM 16

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

typedef enum page_type{
    PAGE_MEM,
    PAGE_DISK,
}page_type_t;

typedef enum state{
    FREE,
    USED,
}state_t;

typedef struct mm_page{
    uintptr_t va;
    list_head list;
    page_type_t page_type;
    int fixed;
    union
    {
        uintptr_t kva;
        uint32_t block_id;
    };
} mm_page_t;

typedef struct shared_pair_t{
    uintptr_t va;
    uintptr_t pid;
} shared_pair_t;
typedef struct shared_page{
    int count;
    uintptr_t kva;
    shared_pair_t pair[NUM_MAX_TASK];
    int key;
    state_t is_used;
    list_head mm_list;
} shared_page_t;

typedef struct snapshot{
    state_t is_used;
    uintptr_t pgdir;
    list_head mm_list;
    pid_t pid;
} snapshot_t;

extern mm_page_t* allocPage();
// TODO [P4-task1] */
void freePage(list_node_t* mm_list);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

#define THREAD_STACK_ADDR 0xf00020000

// TODO [P4-task1] */
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb);

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);
extern uintptr_t get_available_va(pcb_t *pcb);

extern void init_memory();

extern mm_page_t* list_to_mm(list_node_t *list);

extern mm_page_t *swap_out();
extern void swap_in(mm_page_t *disk_page);

extern void set_mapping(uintptr_t va, uintptr_t kva, pcb_t *pcb);
extern void del_mapping(uintptr_t va, uintptr_t pgdir,uint64_t bits);
extern void reset_mapping(uintptr_t va, uintptr_t pgdir, uint64_t bits);

extern int do_snapshot_shot(uintptr_t start_addr);
extern void do_snapshot_restore(int index);
extern uintptr_t do_getpa(uintptr_t va);

extern snapshot_t snapshots[SNAPSHOT_NUM];

extern void set_mapping_snapshot(uintptr_t va, uintptr_t kva,  snapshot_t *snapshot);
extern void set_new_page(uint64_t va, uintptr_t original_kva , snapshot_t *snapshot);

#endif /* MM_H */
