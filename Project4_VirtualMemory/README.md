# Project4 Virtual Memory

## 任务一：启用虚存机制进入内核

构建内核页表的映射部分已经由助教提供，对我后续建立用户页表提供了非常大的帮助和借鉴。我需要实现的是完成一些函数API，方便后续建立页表时调用，详见如下。

```c
/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    /* TODO: [P4-task1] */
    return kva - KVA_BASE_ADDR;
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    /* TODO: [P4-task1] */
    return pa + KVA_BASE_ADDR;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    return (entry & PFN_MASK) >> _PAGE_PFN_SHIFT;
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    *entry = (pfn << _PAGE_PFN_SHIFT) & PFN_MASK;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    /* TODO: [P4-task1] */
    return get_pfn(entry) << NORMAL_PAGE_SHIFT;
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    /* TODO: [P4-task1] */
    return entry & mask;
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    *entry |= bits;
}
static inline void del_attribute(PTE *entry, uint64_t bits)
{
    *entry &= ~bits;
}

static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    /* TODO: [P4-task1] */
    memset((void *)pgdir_addr, 0, NORMAL_PAGE_SIZE);
}
```

此外还有一个函数，通过输入的`va`和`pgdir_va`，返回对应的`kva`，对于我后续debug有很大的帮助。

```c
static inline uintptr_t get_kva_of(uintptr_t va, uintptr_t pgdir_va)
{
    // TODO: [P4-task1] (todo if you need)
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
    PTE *pme = (PTE*) pa2kva(get_pa(((PTE *)pgdir_va)[vpn2]));
    PTE *pte = (PTE*) pa2kva(get_pa(pme[vpn1]));
    return pa2kva(get_pa(pte[vpn0]));
}
```

完成了这些API之后，就能够顺利的进入kernel的main函数之中，接下来要做的就是取消之前建立的临时映射，采取的办法则是简单的将页表的valid位清零，有一些可能的坑可参考我的实验过程记录文档。

```c
void cancel_mapping()
{
    // Cancel temporary mapping by setting the pmd to 0
    PTE *pg_dir = (PTE *)PGDIR_KVA;
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu)
    {
        uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
        del_attribute(&pg_dir[vpn2], _PAGE_PRESENT);
    }
    local_flush_tlb_all();
}
```

## 任务一续：执行用户程序

### 内存管理机制的设计

在本次实验中，我们需要管理所有可用的内存，为了方便起见，我们使用结构体`mm_page_t`管理一个物理页框，其成员的含义如下：

```c
typedef enum page_type{
    PAGE_MEM,
    PAGE_DISK,
}page_type_t;

typedef struct mm_page{
    uintptr_t va;           // virtual address
    list_head list;         // list head
    page_type_t page_type;  // page type (memory or disk)
    int fixed;              // whether the page is fixed
    union       
    {                       
        uintptr_t kva;      // kernel virtual address
        uint32_t block_id;  // block id in disk
    };
} mm_page_t;
```

我们用一个大的数组`free_page`来存储所有的`mm_page_t`，并用一个链表`free_mm_list`来管理所有空闲的`mm_page_t`，这样就可以方便的分配和回收`mm_page_t`，一些API示例如下

```c
// Initialize the memory management
void init_memory()
{
    for (uint32_t i = 0; i < FREE_PAGE_NUM; i++)
    {
        uint64_t addr = FREEMEM_KERNEL + i * NORMAL_PAGE_SIZE;
        free_page[i].page_type = PAGE_MEM;
        free_page[i].kva = addr;
        list_add(&free_mm_list, &free_page[i].list);
    }
}
// list to mm_page_t
mm_page_t *list_to_mm(list_node_t *list)
{
    return (mm_page_t *)((uint64_t)list - sizeof(uintptr_t));
}
// Allocate a new mm_page_t
mm_page_t *allocPage()
{
    // align PAGE_SIZE
    list_node_t *free_mem_list;
    // if there is no free page , swap from disk
    if (list_empty(&free_mm_list))
        return swap_out();
    // alloc a new page
    free_mem_list = free_mm_list.prev;
    list_del(free_mem_list);
    return list_to_mm(free_mem_list);
}
// Free all mm_page_t allocated by a process
void freePage(list_node_t *mm_list)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    list_node_t *pgdir_list = mm_list->prev;
    list_del(pgdir_list);
    list_add(&free_mm_list, pgdir_list);
    PTE *pgdir = (PTE *)list_to_mm(pgdir_list)->kva;
    while (!list_empty(mm_list))
    {
        // recycle memory
        list_node_t *temp_list = mm_list->prev;
        list_del(temp_list);
        mm_page_t *temp_mm = list_to_mm(temp_list);
        if (temp_mm->page_type == PAGE_MEM)
            list_add(&free_mm_list, temp_list);
        else
            list_add(&free_disk_list, temp_list);
    }
}
```

### 页表管理机制的设计

有了对于物理内存管理的API，接下来的重点在实现页表管理机制，主要函数为`set_mapping`，它使用pcb的页目录，将将一个给定的va和kva建立映射，并将其中新建立的页表项加入到`mm_list`中，这样在进程结束时可以方便的回收这些页表项。

```c
void set_mapping(uintptr_t va, uintptr_t kva, pcb_t *pcb)
{
    PTE *pg_base = (PTE *)pcb->pgdir;

    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;

    if (pg_base[vpn2] == 0)
    {
        mm_page_t *free_page_table = allocPage();
        free_page_table->fixed = 1;
        list_add(&pcb->mm_list, &free_page_table->list);
        set_pfn(&pg_base[vpn2], kva2pa(free_page_table->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pg_base[vpn2])));
    }
    set_attribute(&pg_base[vpn2], _PAGE_PRESENT | _PAGE_USER);
    PTE *pmd = (PTE *)pa2kva(get_pa(pg_base[vpn2]));

    if (pmd[vpn1] == 0)
    {
        mm_page_t *free_page_table = allocPage();
        free_page_table->fixed = 1;
        list_add(&pcb->mm_list, &free_page_table->list);
        set_pfn(&pmd[vpn1], kva2pa(free_page_table->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_USER);
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    set_pfn(&pte[vpn0], kva2pa(kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_USER | _PAGE_EXEC | _PAGE_WRITE | _PAGE_READ | _PAGE_ACCESSED | _PAGE_DIRTY);

    return;
}
```
在`alloc_page_helper`中，我们为一个全新的虚地址分配一个物理页，调用上述函数建立映射，并返回该物理页的虚地址。

```c
uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb)
{
    // TODO [P4-task1] alloc_page_helper:

    mm_page_t *free_page = allocPage();
    free_page->va = va;
    list_add(&pcb->mm_list, &free_page->list);
    set_mapping(va, free_page->kva, pcb);
    return free_page->kva;
}
```

此外，我还设计了一些另外的API，用于对某个虚地址对应的页表项，增加或删除不同的属性。

```c
// delete attribute
void del_mapping(uintptr_t va, uintptr_t pgdir, uint64_t bits)
{
    PTE *pg_base = (PTE *)pgdir;
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
    PTE *pmd = (PTE *)pa2kva(get_pa(pg_base[vpn2]));
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    del_attribute(&pte[vpn0], bits);
}
// set attribute
void reset_mapping(uintptr_t va, uintptr_t pgdir, uint64_t bits)
{
    PTE *pg_base = (PTE *)pgdir;
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
    PTE *pmd = (PTE *)pa2kva(get_pa(pg_base[vpn2]));
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    set_attribute(&pte[vpn0], bits);
}
``` 

### `do_exec`、`do_kill`、`do_scheduler`等函数的重构

有了上述的内存管理或页表管理模块，我们就需要对原先的`do_exec`进行一定的重构。由于采取了虚页机制，我们需要增加管理页表和拷贝程序镜像的部分。

```c
    // alloc pgdir
    mm_page_t *page_dir = allocPage();
    list_add(&pcb[freepcb].mm_list,&page_dir->list);
    page_dir->fixed = 1;
    pcb[freepcb].pgdir = page_dir->kva;
    clear_pgdir(page_dir->kva);
    // copy kernel page table to user page table
    share_pgtable(pcb[freepcb].pgdir, PGDIR_KVA);
    // alloc user stack
    alloc_page_helper(USER_STACK_ADDR - NORMAL_PAGE_SIZE, &pcb[freepcb]);
    // alloc user page table and copy task_image to kva    
    int page_num = tasks[task_id].memsz / NORMAL_PAGE_SIZE + 1;
    uintptr_t prev_kva;
    for (int j = 0; j < page_num; j++)
    {
        uintptr_t va = tasks[task_id].entry_point + j * NORMAL_PAGE_SIZE;
        uintptr_t kva = alloc_page_helper(va, &pcb[freepcb]);
        load_task_img(task_id, kva, prev_kva, j);
        prev_kva = kva;
    }
```

有如下需要注意的事项：

1. 我们需要分配新的页目录，并将所有页表项的fixed设置成1，以免后续被swap。
2. 我们需要将内核页表的内容拷贝到用户页表中，以保证内核代码和数据的可访问性。
3. 我们需要分配用户栈，并建立用户栈的映射。
4. 我们建立映射的时候，需要同时为`bss`段建立虚页映射。
5. 拷贝用户程序时，需要注意padding，详见我的实验过程记录文档和`load_task_img`函数。

```c
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
    sd_read(kva2pa(kva), BLOCK_NUM, block_id);
    if (page_num != 0)
        memcpy((void *)prev_kva + NORMAL_PAGE_SIZE - offset, (void *)kva, offset);
    memcpy((void *)kva, (void *)(kva + offset), NORMAL_PAGE_SIZE - offset);
    return;
}
```

此外需要注意的是拷贝命令行参数，由于在`shell`的地址空间中无法直接访问用户程序的地址空间，但是两者在内核中能够同时访问内核的地址空间，因此我选择的办法是先将参数拷贝到内核地址空间中，接着临时切换页表到用户程序的地址空间中，再将参数拷贝到用户栈中，拷贝完成后，再把页表切换回`shell`的页表。

```c
    // temporarily copy argv to kernel 
    char argv_buff[ARG_MAX][ARG_LEN];
    for (int i = 0; i < argc; i++)
        strcpy(argv_buff[i], argv[i]);
    // temporarily change page table to exec process to copy argv
    unsigned long ppn = kva2pa(pcb[freepcb].pgdir) >> NORMAL_PAGE_SHIFT;
    set_satp(SATP_MODE_SV39, pcb[freepcb].pid, ppn);
    local_flush_tlb_all();
    // copy argv to user stack
    char *p[argc];
    for (int i = 0; i < argc; i++)
    {
        int len = strlen(argv_buff[i]) + 1;
        pcb[freepcb].user_sp -= len;
        strcpy((char *)pcb[freepcb].user_sp, argv_buff[i]);
        p[i] = (char *)pcb[freepcb].user_sp;
    }
    pcb[freepcb].user_sp -= sizeof(p);
    memcpy((uint8_t *)pcb[freepcb].user_sp, (uint8_t *)p, sizeof(p));
    reg_t addr_argv = (reg_t)pcb[freepcb].user_sp;
    ...
    // change to current_running pgdir
    ppn = kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT;
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, ppn);
    local_flush_tlb_all();
```

在`do_kill`中，我们最后调用`freePage(&pcb[current_pcb].mm_list);`来释放用户进程占用的内存。

在`do_scheduler`中，我们增加如下代码来切换页表

```c
    unsigned long ppn = kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT;
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, ppn);
    local_flush_tlb_all();
```

## 任务二：动态页表和按需调页

RISC-V 架构中缺页共有三种可能，分别是INST_PAGE_FAULT：指令缺页，LOAD_PAGE_FAULT：读缺页，STORE_PAGE_FAULT：写缺页。我们需要在`irq.c`中的`handle_page_fault`函数中处理这三种缺页。

如果是采取纯粹的`on-demand paging`的话，我们应该在PAGE_FAULT中，按需加载相应的代码段或数据段等，但是由于时间限制和不存在`padding`的多种原因，我们在这里不实现纯粹的`on-demand paging`。

我们在这里直接统一分配空页，并建立映射，不再考虑上述的`on-demand paging`。

```c
void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    int cpu_id = get_current_cpu_id();
    // allocate a new page
    alloc_page_helper(stval, current_running[cpu_id]);
    local_flush_tlb_all();
}
```

## 任务三：换页机制和页替换算法

换页机制要求我们管理磁盘的空间，类似`init_memory`函数，我们需要初始化磁盘可用空间。

```c
void init_disk()
{
    uint32_t final_block_num;
    for (uint32_t i = 0; i < NUM_MAX_TASK; i++)
        if (tasks[i].entry_point == 0)
        {
            final_block_num = (tasks[i - 1].start_addr + tasks[i - 1].filesz) / BLOCK_SIZE + 1;
            break;
        }
    for (uint32_t i = 0; i < FREE_DISK_NUM; i++)
    {
        uint32_t block_id = final_block_num + i * BLOCK_NUM;
        free_disk[i].page_type = PAGE_DISK;
        free_disk[i].block_id = block_id;
        list_add(&free_disk_list, &free_disk[i].list);
    }
}
```

由于时间限制，且对页替换算法不做要求，我的设计较为简单，大致如下：

1. 从上次被`swap`的`PCB`块开始，找到第一个可用的`mm_page_t`，将其`swap`。
2. 我们不`swap`页表项、页目录以及所有被`fix`的物理页框
3. 我们不`swap`用户栈，因为会被多次使用

```c
mm_page_t *swap_out()
{
    static int j = 1;
    int cpu_id = get_current_cpu_id();
    list_node_t *disk_list = free_disk_list.prev;
    list_del(disk_list);
    mm_page_t *free_disk = list_to_mm(disk_list);
    int start_id = j;
    // FIFO and RANDOM Alogrithm
    while (1)
    {
        int i = (j++) % TASK_MAXNUM;
        if (pcb[i].is_used && pcb[i].status != TASK_RUNNING)
        {
            list_node_t *temp_list = pcb[i].mm_list.prev; // we don't swap pg_dir
            temp_list = temp_list->prev;                  // we don't swap stack
            while (temp_list->prev != &pcb[i].mm_list)
            {
                // we make sure we don't swap disk_page
                temp_list = temp_list->prev;
                mm_page_t *temp_mm = list_to_mm(temp_list);
                if (temp_mm->page_type == PAGE_DISK || temp_mm->fixed == 1)
                    continue;
                // we decided to swap temp_list->prev
                list_del(temp_list);
                list_add(&pcb[i].mm_list, disk_list);
                // copy va for swap in
                uintptr_t va = temp_mm->va;
                free_disk->va = va;
                sd_write(kva2pa(temp_mm->kva), BLOCK_NUM, free_disk->block_id);
                // set va pg_dir to invalid
                del_mapping(va, pcb[i].pgdir, _PAGE_PRESENT);
                printl("> [Info] Swap out va 0x%x from pid(%d) into block %d\n", temp_mm->va, pcb[i].pid, free_disk->block_id);
                j = i;
                return temp_mm;
            }
        }
        if (j == start_id)
            break;
    }
    // FIXME: we should think if their is no free_disk_page or no page to swap out
    printl("> [Error] No page to swap out because all pages are page table!\n");
    while (1);
}
```

我们需要修改异常处理函数，当发生缺页异常的时候，我们需要先检查是否被swap，如果是，我们需要将其`swap in`，否则我们需要分配一个新的物理页框。

```c
    list_node_t *temp_list = current_running[cpu_id]->mm_list.prev;
    // check if stval is in disk
    while (temp_list != &current_running[cpu_id]->mm_list)
    {
        mm_page_t *temp_mm = list_to_mm(temp_list);
        if (temp_mm->va <= stval && temp_mm->va + NORMAL_PAGE_SIZE >= stval)
        {
            if (temp_mm->page_type == PAGE_DISK)
            {
                swap_in(temp_mm);
                local_flush_tlb_all();
                return;
            }
        }
        temp_list = temp_list->prev;
    }
```

swap_in 函数中，我们找到一个空闲的物理页框，重现读入磁盘中的数据。

```c
void swap_in(mm_page_t *disk_page)
{
    int cpu_id = get_current_cpu_id();
    // copy disk back to memory
    list_del(&disk_page->list);
    list_add(&free_disk_list, &disk_page->list);
    uintptr_t va = disk_page->va;
    uintptr_t kva = alloc_page_helper(va, current_running[cpu_id]);
    sd_read(kva2pa(kva), BLOCK_NUM, disk_page->block_id);
    printl("> [Info] Swap in block %d into va 0x%x for pid(%d) \n", disk_page->block_id, va, current_running[cpu_id]->pid);
}
```

## 任务四：多线程的 mailbox 收发测试

创建新的线程和创建新的进程几乎一致，需要注意的区别如下：

1. 线程的页目录直接使用父进程的页目录。
2. 线程需要分配新的用户栈，且和父进程的用户栈不同。

具体新增的代码如下：

```c
    // set father process
    current_running[cpu_id]->son[current_running[cpu_id]->thread_num] = &tcb[free_tcb];
    current_running[cpu_id]->thread_num++;
    // set son thread
    tcb[free_tcb].kernel_stack_base = allocPage()->kva + PAGE_SIZE;
    tcb[free_tcb].user_stack_base = current_running[cpu_id]->user_stack_base + PAGE_SIZE * current_running[cpu_id]->thread_num;
    // copying father's pg_dir
    tcb[free_tcb].pgdir = current_running[cpu_id]->pgdir;
    // alloc user_stack;
    alloc_page_helper(tcb[free_tcb].user_stack_base - PAGE_SIZE, current_running[cpu_id]);
```

##  任务五：多进程共享内存与进程通信

我们设计一个`share_page_t`结构体来管理所有的共享页框

```c
typedef struct shared_pair_t{
    uintptr_t va;       // virtual address
    uintptr_t pid;      // pid
} shared_pair_t;
typedef struct shared_page{
    int count;          // count of process share this page 
    shared_pair_t pair[NUM_MAX_TASK];   // pid and va for each process
    uintptr_t kva;      // kernel virtual address
    int key;            // key
    int is_used;        // is used
    list_head mm_list;  // list for mm_page
} shared_page_t;
```
我们通过`shm_page_get`分配一个共享页框，相同的`key`会返回相同的共享页框。

```c
uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    int cpu_id = get_current_cpu_id();
    uintptr_t va = get_available_va(current_running[cpu_id]);
    int index = -1;
    for (int i = 0; i < SHARED_PAGE_NUM; i++)
    {
        if (shared_page[i].key == key)
        {
            index = i;
            break;
        }
    }
    if (index != -1) // there is already allocated shared page table
    {
        shared_page[index].pair[shared_page[index].count].va = va;
        shared_page[index].pair[shared_page[index].count].pid = current_running[cpu_id]->pid;
        shared_page[index].count++;
        set_mapping(va, shared_page[index].kva, current_running[cpu_id]);
        local_flush_tlb_all();
        return va;
    }

    for (int i = 0; i < SHARED_PAGE_NUM; i++)
    {
        if (shared_page[i].is_used == 0)
        {
            index = i;
            break;
        }
    }
    if (index == -1)
        return 0;
    // allocate a new shared page table
    list_init(&shared_page[index].mm_list);
    mm_page_t *free_page = allocPage();
    shared_page[index].key = key;
    shared_page[index].is_used = 1;
    shared_page[index].kva = free_page->kva;
    shared_page[index].count = 1;
    list_add(&shared_page[index].mm_list, &free_page->list);
    shared_page[index].pair[0].va = va;
    shared_page[index].pair[0].pid = current_running[cpu_id]->pid;
    set_mapping(va, shared_page[index].kva, current_running[cpu_id]);
    local_flush_tlb_all();
    return va;
}
``` 

在上面的函数中，我们通过`get_available_va`找到一个可用的`va`，我采用的算法为该进程最大的`va`+`PAGE_SIZE`。

```c
uintptr_t get_available_va(pcb_t *pcb)
{
    list_node_t *temp_list = pcb->mm_list.prev;
    uintptr_t max_va = 0;
    while (temp_list != &pcb->mm_list)
    {
        mm_page_t *temp_mm = list_to_mm(temp_list);
        if (temp_mm->va > max_va && temp_mm->fixed == 0)
            max_va = temp_mm->va;
        temp_list = temp_list->prev;
    }
    return max_va + PAGE_SIZE;
}
```

回收的时候，我们取消映射，然后将`count`减一，如果`count`为0，我们将`is_used`置为0，然后释放所有的页框。

```c
void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    int cpu_id = get_current_cpu_id();
    for (int i = 0; i < SHARED_PAGE_NUM; i++)
    {
        if (shared_page[i].is_used == 0)
            continue;
        for (int j = 0; j < shared_page[i].count; j++)
        {
            if (shared_page[i].pair[j].va == addr && shared_page[i].pair[j].pid == current_running[cpu_id]->pid)
            {
                shared_page[i].pair[j].va = shared_page[i].pair[shared_page[i].count - 1].va;
                shared_page[i].pair[j].pid = shared_page[i].pair[shared_page[i].count - 1].pid;
                shared_page[i].count--;
                // set pg_dir to invalid
                del_mapping(shared_page[i].pair[j].va, current_running[cpu_id]->pgdir, _PAGE_PRESENT);
                // check if shared_page is empty
                if (shared_page[i].count == 0)
                {
                    shared_page[i].is_used = 0;
                    list_node_t *temp_node = shared_page[i].mm_list.prev;
                    list_del(temp_node);
                    list_add(&free_mm_list, temp_node);
                }
                local_flush_tlb_all();
                return;
            }
        }
    }
}
```

## 任务六：copy-on-write 策略的实现与在 Snapshot 技术下的验证

在Snapshot的实现上，我采取的是增量开发的策略，在不改变上面五项任务的基础上，实现Snapshot的功能，因此在完备性上可能有所缺陷。

`snapshot_t`结构体设计如下：

```c
typedef struct snapshot{
    int is_used;         // is used
    uintptr_t pgdir;     // pgdir
    list_head mm_list;   // list for mm_page
    pid_t pid;           // pid
} snapshot_t;
```

此外我设计了两个Snapshot的API，简介如下。

1. `int do_snapshot_shot(uintptr_t start_addr)`

该API的功能为拍下`start_addr`虚地址之后所有虚地址的快照，并返回`index`值，后续可利用该值进行回退。

在该函数中，我们做下列操作：

- 找到一个空的`snapshot_t`结构体。
- 新建一个页目录，并在其中建立父进程`va`到`pa`的映射。
- 将父进程上述`va`的可写权限去除，以便实现`copy-on-write`。

```c
int do_snapshot_shot(uintptr_t start_addr)
{
    int cpu_id = get_current_cpu_id();
    // find a new snapshot
    int index = -1;
    for (int i = 0; i < SNAPSHOT_NUM; i++)
    {
        if (snapshots[i].is_used == 0)
        {
            index = i;
            break;
        }
    }
    if (index == -1) return -1;
    // copy all pages
    list_init(&snapshots[index].mm_list);
    mm_page_t *page_dir = allocPage();
    list_add(&snapshots[index].mm_list, &page_dir->list);
    page_dir->fixed = 1;
    snapshots[index].pgdir = page_dir->kva;
    snapshots[index].pid = current_running[cpu_id]->pid;
    snapshots[index].is_used = 1;
    current_running[cpu_id]->is_shot = 1;
    share_pgtable(snapshots[index].pgdir,PGDIR_KVA);
    // set all pages non-writeable and reset mapping
    list_node_t *temp_node = current_running[cpu_id]->mm_list.prev;
    while (temp_node != &current_running[cpu_id]->mm_list)
    {
        mm_page_t *temp_mm = list_to_mm(temp_node);
        if (temp_mm->fixed == 1)
        {
            temp_node = temp_node->prev;
            continue;
        }
        if (temp_mm->va != USER_STACK_ADDR - PAGE_SIZE && temp_mm->va >= start_addr)
            del_mapping(temp_mm->va, current_running[cpu_id]->pgdir, _PAGE_WRITE);
        set_mapping_snapshot(temp_mm->va, temp_mm->kva, &snapshots[index]);
        temp_node = temp_node->prev;
    }
    return index;
}
```
2. `void do_snapshot_restore(int index)`

该API的功能为恢复`index`值对应的快照，我们找到相应的`snapshot_t`结构体，将其`pgdir`赋值给当前进程的`pgdir`，已到达恢复的目的。

```c
void do_snapshot_restore(int index)
{
    int cpu_id = get_current_cpu_id();
    current_running[cpu_id]->pgdir = snapshots[index].pgdir;
    unsigned long ppn = kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT;
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, ppn);
    local_flush_tlb_all();
}
```

3. 此外在异常处理中，我们需要增加对`copy_on_write`的处理，当发生`page fault`时，我们需要判断当前进程是否有快照，如果有，我们则需要拷贝原先的物理页框。

修改`handle_page_fault`函数如下：

```c
    list_node_t *temp_list = current_running[cpu_id]->mm_list.prev;
    // check if stval is in disk
    while (temp_list != &current_running[cpu_id]->mm_list)
    {
        mm_page_t *temp_mm = list_to_mm(temp_list);
        if (temp_mm->va <= stval && temp_mm->va + NORMAL_PAGE_SIZE >= stval)
        {
            if (temp_mm->page_type == PAGE_DISK)
            {
                swap_in(temp_mm);
                local_flush_tlb_all();
                return;
            }
            if (current_running[cpu_id]->is_shot == 1)
            {
                copy_on_write(stval,current_running[cpu_id]);
                local_flush_tlb_all();
                return;
            }
        }
        temp_list = temp_list->prev;
    }
```

`copy_on_write`函数如下：

```c
void copy_on_write(uint64_t stval,pcb_t *pcb)
{
    reset_mapping(stval,pcb->pgdir,_PAGE_WRITE);
    for (int i = 0; i < SNAPSHOT_NUM; i++)
    {
        // snapshots not used or not belongs to this pcb;
        if (snapshots[i].is_used == 0 || snapshots[i].pid != pcb->pid)
            continue;
        // if it haven't been copied then copy the new page
        uintptr_t original_kva = get_kva_of(stval, pcb->pgdir);
        uintptr_t snapshot_kva = get_kva_of(stval, snapshots[i].pgdir);
        if (snapshot_kva == original_kva)
            set_new_page(stval,original_kva,&snapshots[i]);
    }
}
```

在验证中，我们只需要验证未经修改的快照，是否和原先的进程有相同的`pa`即可，提供系统调用如下：

```c
uintptr_t do_getpa(uintptr_t addr)
{
    int cpu_id = get_current_cpu_id();
    return kva2pa(get_kva_of(addr, current_running[cpu_id]->pgdir));
}
```
