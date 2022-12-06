#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/loader.h>
#include <csr.h>
#include <os/string.h>
#include <os/task.h>
#include <os/smp.h>
#include <os/irq.h>
#include <os/net.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack_ori = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t pid1_stack_ori = INIT_KERNEL_STACK + PAGE_SIZE * 2;
const ptr_t pid0_stack = pid0_stack_ori - sizeof(switchto_context_t) - sizeof(regs_context_t);
const ptr_t pid1_stack = pid1_stack_ori - sizeof(switchto_context_t) - sizeof(regs_context_t);
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack_ori,
    .kernel_stack_base = (ptr_t)pid0_stack_ori,
    .user_stack_base = (ptr_t)pid0_stack_ori,
    .pgdir = (uintptr_t)PGDIR_KVA,
};

pcb_t pid1_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid1_stack,
    .user_sp = (ptr_t)pid1_stack_ori,
    .kernel_stack_base = (ptr_t)pid1_stack_ori,
    .user_stack_base = (ptr_t)pid1_stack_ori,
    .pgdir = (uintptr_t)PGDIR_KVA,
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

extern void ret_from_exception();

extern mutex_lock_t mlocks[LOCK_NUM];

/* current running task PCB */
pcb_t *volatile current_running[2];

/* global process id */
pid_t process_id = 1;

pcb_t *list_to_pcb(list_node_t *list)
{
    return (pcb_t *)((uint64_t)list - 2 * sizeof(reg_t) - 2 * sizeof(ptr_t));
}

void do_scheduler(void)
{
    int cpu_id = get_current_cpu_id();
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    // unblock_net_queue();
    // TODO: [p2-task1] Modify the current_running[cpu_id] pointer.
    // Find the next task to run for this core.
    int flag = 0;
    mask_status_t mask = (cpu_id == 0) ? CORE_0 : CORE_1;
    list_node_t *next_running;
    list_head *temp_queue = &ready_queue;
    while (temp_queue->prev != &ready_queue)
    {
        pcb_t *pcb = list_to_pcb(temp_queue->prev);
        if (pcb->mask == mask || pcb->mask == CORE_BOTH)
        {
            flag = 1;
            next_running = temp_queue->prev;
            break;
        }
        temp_queue = temp_queue->prev;
    }
    // If there is no task to run
    if (flag == 0)
    {
        // If the prev_running is still running or it's kernel, just return
        if (current_running[cpu_id]->status == TASK_RUNNING || current_running[cpu_id]->pid == 0)
            return;
        else /* switch to kernel and wait for interrupt */
        {
            pcb_t *prev_running = current_running[cpu_id];
            current_running[cpu_id] = (cpu_id == 0) ? &pid0_pcb : &pid1_pcb;
            set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
            local_flush_tlb_all();
            switch_to(prev_running, current_running[cpu_id]);
            return;
        }
    }
    // If current_running[cpu_id] is running and it's not kernel
    // re-add it to the ready_queue
    // else which means it's already in ready_queue or sleep_queue or other block_queue
    pcb_t *prev_running = current_running[cpu_id];
    if (prev_running->pid != 0 && prev_running->status == TASK_RUNNING) // make sure the kernel stays outside the ready_queue
    {
        prev_running->status = TASK_READY;
        list_add(&ready_queue, &prev_running->list); // add the prev_running to the end of the queue
    }
    // Find the next task to run
    // Make sure set current_running[cpu_id]'s status to TASK_RUNNING
    // AFTER check if prev_running's status is TASK_RUNNING
    // because prev_running might be the same as current_running[cpu_id] and
    // it might be re-add to ready_queue by other syscalls
    list_node_t *current_list = next_running;
    list_del(current_list);
    current_running[cpu_id] = list_to_pcb(current_list);
    current_running[cpu_id]->status = TASK_RUNNING;
    unsigned long ppn = kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT;
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, ppn);
    local_flush_tlb_all();
    // TODO: [p2-task1] switch_to current_running[cpu_id]
    switch_to(prev_running, current_running[cpu_id]);
}

void do_sleep(uint32_t sleep_time)
{
    int cpu_id = get_current_cpu_id();
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running[cpu_id]
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running[cpu_id] is blocked.
    current_running[cpu_id]->status = TASK_BLOCKED;
    current_running[cpu_id]->wakeup_time = get_timer() + sleep_time;
    list_add(&sleep_queue, &current_running[cpu_id]->list);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_add(queue, pcb_node);
    pcb_t *pcb = list_to_pcb(pcb_node);
    pcb->status = TASK_BLOCKED;
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    list_add(&ready_queue, pcb_node);
    pcb_t *pcb = list_to_pcb(pcb_node);
    pcb->status = TASK_READY;
}

// syscalls for processes
pid_t do_exec(char *name, int argc, char *argv[])
{
    // init pcb
    int task_id = 0;
    for (int i = 0; i < TASK_MAXNUM; i++)
        if (strcmp(tasks[i].task_name, name) == 0)
        {
            task_id = i;
            break;
        }
    if (task_id == 0)
        return 0;
    int freepcb = 0;
    for (int i = 1; i < NUM_MAX_TASK; i++)
        if (pcb[i].is_used == 0)
        {
            freepcb = i;
            break;
        }
    if (freepcb == 0)
        return 0;
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
    pcb[freepcb].pid = ++process_id;
    pcb[freepcb].kernel_sp = pcb[freepcb].kernel_stack_base;
    pcb[freepcb].user_sp = pcb[freepcb].user_stack_base;
    pcb[freepcb].status = TASK_READY;
    pcb[freepcb].cursor_x = pcb[freepcb].cursor_y = 0;
    pcb[freepcb].thread_num = 0;
    pcb[freepcb].is_shot = 0;
    list_add(&ready_queue, &pcb[freepcb].list);
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
    // align sp
    pcb[freepcb].user_sp -= pcb[freepcb].user_sp % 128;
    // init pcb_stack
    ptr_t kernel_stack = pcb[freepcb].kernel_sp;
    ptr_t user_stack = pcb[freepcb].user_sp;
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[2] = (reg_t)user_stack;    // sp
    pt_regs->regs[4] = (reg_t)&pcb[freepcb]; // tp
    pt_regs->regs[10] = (reg_t)argc;         // a0
    pt_regs->regs[11] = addr_argv;           // a1
    // special registers
    pt_regs->sstatus = SR_SPIE & ~SR_SPP | SR_SUM; // make spie(1) and spp(0)
    pt_regs->sepc = (reg_t)tasks[task_id].entry_point;
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;
    // init switch_to context
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 2; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pt_switchto->regs[0] = (reg_t)ret_from_exception; // ra
    pt_switchto->regs[1] = (reg_t)user_stack;         // sp
    pcb[freepcb].kernel_sp = (ptr_t)pt_switchto;
    pcb[freepcb].is_used = 1;
    // inherit mask from shell or it's parent
    int cpu_id = get_current_cpu_id();
    pcb[freepcb].mask = current_running[cpu_id]->mask;
    // change to current_running pgdir
    ppn = kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT;
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, ppn);
    local_flush_tlb_all();
    return process_id;
}

int do_kill(pid_t pid)
{
    // Find the corresponding pcb
    int current_pcb = 0;
    for (int i = 0; i < TASK_MAXNUM; i++)
        if (pcb[i].pid == pid && pcb[i].is_used == 1)
        {
            current_pcb = i;
            break;
        }
    // Note that we can't kill kernel, shell or process that not exist
    if (current_pcb == 0)
        return 0;
    // Note that we don't need to kill the process that is EXITED
    if (pcb[current_pcb].status == TASK_EXITED)
        return pid;
    // Delete the process from it's queue (ready_queue or sleep_queue or some block_queue)
    // If it's not running on the other core;
    if (pcb[current_pcb].status != TASK_RUNNING)
        list_del(&pcb[current_pcb].list);
    pcb[current_pcb].status = TASK_EXITED;
    // Release pcb;
    pcb[current_pcb].is_used = 0;
    // Release all the waiting process
    while (!list_empty(&pcb[current_pcb].wait_list))
        do_unblock(pcb[current_pcb].wait_list.prev);
    // Release acquired locks
    for (int i = 0; i < LOCK_NUM; i++)
        if (mlocks[i].pid == pid)
            do_mutex_lock_release(i);
    // free page
    freePage(&pcb[current_pcb].mm_list);
    return pid;
}

void do_exit(void)
{
    int cpu_id = get_current_cpu_id();
    // Find the corresponding pcb
    int current_pcb = 0;
    for (int i = 0; i < TASK_MAXNUM; i++)
        if (pcb[i].pid == current_running[cpu_id]->pid && pcb[i].is_used == 1)
        {
            current_pcb = i;
            break;
        }
    current_running[cpu_id]->status = TASK_EXITED;
    // Release pcb;
    pcb[current_pcb].is_used = 0;
    // Unblock all the waiting process
    while (!list_empty(&current_running[cpu_id]->wait_list))
        do_unblock(current_running[cpu_id]->wait_list.prev);
    // Release all the acquired locks
    for (int i = 0; i < LOCK_NUM; i++)
        if (mlocks[i].pid == current_running[cpu_id]->pid)
            do_mutex_lock_release(i);
    // free page
    freePage(&pcb[current_pcb].mm_list);
    // Find the next process to run
    do_scheduler();
}

int do_waitpid(pid_t pid)
{
    int cpu_id = get_current_cpu_id();
    // Find the corresponding pcb
    int current_pcb = 0;
    for (int i = 0; i < TASK_MAXNUM; i++)
        if (pcb[i].pid == pid && pcb[i].is_used == 1)
        {
            current_pcb = i;
            break;
        }
    // Note that we can't wait for kernel, shell or process that not exist
    if (current_pcb == 0)
        return 0;
    // Note that we don't need to wait for a EXITED process
    if (pcb[current_pcb].status != TASK_EXITED)
        do_block(&current_running[cpu_id]->list, &pcb[current_pcb].wait_list);
    return pid;
}

void do_process_show()
{
    printk("[Process Table]:\n");
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].is_used == 0)
            continue;
        printk("[%d] PID : %d  ", i, pcb[i].pid);
        switch (pcb[i].status)
        {
        case TASK_RUNNING:
            printk("STATUS : RUNNING  ");
            break;
        case TASK_READY:
            printk("STATUS : READY    ");
            break;
        case TASK_BLOCKED:
            printk("STATUS : BLOCKED  ");
            break;
        default:
            break;
        }
        switch (pcb[i].mask)
        {
        case CORE_0:
            printk("MASK : 0x1  ");
            break;
        case CORE_1:
            printk("MASK : 0x2  ");
            break;
        case CORE_BOTH:
            printk("MASK : 0x3  ");
            break;
        default:
            break;
        }
        if (pcb[i].status == TASK_RUNNING)
        {
            if (pcb[i].pid == current_running[0]->pid)
                printk("RUNNING ON CORE 0");
            else
                printk("RUNNING ON CORE 1");
        }
        printk("\n");
    }
    printk("[Thread Table]:\n");
    for (int i = 0; i < NUM_MAX_THREAD; i++)
    {
        if (tcb[i].is_used == 0)
            continue;
        printk("[%d] TID : %d  ", i, tcb[i].pid);
        switch (tcb[i].status)
        {
        case TASK_RUNNING:
            printk("STATUS : RUNNING  ");
            break;
        case TASK_READY:
            printk("STATUS : READY    ");
            break;
        case TASK_BLOCKED:
            printk("STATUS : BLOCKED  ");
            break;
        default:
            break;
        }
        switch (tcb[i].mask)
        {
        case CORE_0:
            printk("MASK : 0x1  ");
            break;
        case CORE_1:
            printk("MASK : 0x2  ");
            break;
        case CORE_BOTH:
            printk("MASK : 0x3  ");
            break;
        default:
            break;
        }
        if (tcb[i].status == TASK_RUNNING)
        {
            if (tcb[i].pid == current_running[0]->pid)
                printk("RUNNING ON CORE 0");
            else
                printk("RUNNING ON CORE 1");
        }
        printk("\n");
    }
}

pid_t do_getpid()
{
    int cpu_id = get_current_cpu_id();
    return current_running[cpu_id]->pid;
}

// add taskset and taskset -p
pid_t do_taskset_p(pid_t pid, int mask)
{
    // Find the corresponding pcb
    int current_pcb = 0;
    for (int i = 0; i < TASK_MAXNUM; i++)
        if (pcb[i].pid == pid && pcb[i].is_used == 1)
        {
            current_pcb = i;
            break;
        }
    // Note that we can't change kernel, shell or process that not exist
    if (current_pcb == 0)
        return 0;
    // change the mask
    switch (mask)
    {
    case 1:
        pcb[current_pcb].mask = CORE_0;
        break;
    case 2:
        pcb[current_pcb].mask = CORE_1;
        break;
    case 3:
        pcb[current_pcb].mask = CORE_BOTH;
        break;
    default:
        break;
    }
    return pid;
}

pid_t do_taskset(char *name, int argc, char *argv[], int mask)
{
    int pid = do_exec(name, argc, argv);
    if (pid == 0)
        return 0;
    // Find the corresponding pcb
    int current_pcb = 0;
    for (int i = 0; i < TASK_MAXNUM; i++)
        if (pcb[i].pid == pid && pcb[i].is_used == 1)
        {
            current_pcb = i;
            break;
        }
    // change the mask
    switch (mask)
    {
    case 1:
        pcb[current_pcb].mask = CORE_0;
        break;
    case 2:
        pcb[current_pcb].mask = CORE_1;
        break;
    case 3:
        pcb[current_pcb].mask = CORE_BOTH;
        break;
    default:
        break;
    }
    return pid;
}