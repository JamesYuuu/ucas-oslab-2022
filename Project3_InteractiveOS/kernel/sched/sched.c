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

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

extern void ret_from_exception();

extern mutex_lock_t mlocks[LOCK_NUM];

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

pcb_t* list_to_pcb(list_node_t *list)
{
    return (pcb_t*)((int)list-2*sizeof(reg_t)-2*sizeof(ptr_t));
}

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    // TODO: [p2-task1] Modify the current_running pointer.
    // If there is no task in ready_queue but current_running is running 
    // then their is no need to switch
    if (list_empty(&ready_queue) && current_running->status==TASK_RUNNING) return;
    // If there is no task in the ready queue, wait for sleep processes
    // then what to do if their is no sleeping processes and the shell is blocked?
    while (list_empty(&ready_queue))
        check_sleeping();     
    // If current_running is running and it's not kernel
    // re-add it to the ready_queue
    // else which means it's already in ready_queue or sleep_queue or other block_queue
    pcb_t *prev_running=current_running;
    if (prev_running->pid!=0 && prev_running->status==TASK_RUNNING)  //make sure the kernel stays outside the ready_queue
    {
        prev_running->status=TASK_READY;
        list_add(&ready_queue,&prev_running->list); //add the prev_running to the end of the queue
    }
    // Find the next task to run
    // Make sure set current_running's status to TASK_RUNNING
    // AFTER check if prev_running's status is TASK_RUNNING
    // because prev_running might be the same as current_running and
    // it might be re-add to ready_queue by other syscalls
    list_node_t *current_list=ready_queue.prev;        //ready_queue.prev is the first node of the queue
    list_del(current_list);
    current_running=list_to_pcb(current_list);
    current_running->status = TASK_RUNNING;
    // TODO: [p2-task1] switch_to current_running
    switch_to(prev_running,current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running->status=TASK_BLOCKED;
    current_running->wakeup_time=get_timer()+sleep_time;
    list_add(&sleep_queue,&current_running->list);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_add(queue,pcb_node);
    pcb_t *pcb=list_to_pcb(pcb_node);
    pcb->status=TASK_BLOCKED;
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    list_add(&ready_queue,pcb_node);
    pcb_t *pcb=list_to_pcb(pcb_node);
    pcb->status=TASK_READY;
}

// syscalls for processes
pid_t do_exec(char *name, int argc, char *argv[])
{
    // init pcb
    uint64_t entry_point = 0;
    for (int i=0;i<task_num;i++)
        if (strcmp(tasks[i].task_name,name)==0)
        {
            entry_point = tasks[i].entry_point;
            break;
        }
    if (entry_point == 0) return 0;
    int freepcb=0;
    for (int i=1;i<NUM_MAX_TASK;i++)
        if (pcb[i].is_used==0)
        {
            freepcb=i;
            break;
        }
    if (freepcb==0) return 0;
    pcb[freepcb].pid=++process_id;
    pcb[freepcb].kernel_sp = allocKernelPage(1);
    pcb[freepcb].user_sp = allocUserPage(1);
    pcb[freepcb].kernel_stack_base = pcb[freepcb].kernel_sp;
    pcb[freepcb].user_stack_base = pcb[freepcb].user_sp;
    pcb[freepcb].status = TASK_READY;
    pcb[freepcb].cursor_x = pcb[freepcb].cursor_y = 0;
    pcb[freepcb].thread_num = -1;
    list_init(&pcb[freepcb].wait_list);
    list_add(&ready_queue,&pcb[freepcb].list);
    // copy argv to user_stack
    char *p[argc];
    for (int i=0;i<argc;i++)
    {
        int len=strlen(argv[i])+1;
        pcb[freepcb].user_sp-=len;
        strcpy((char*)pcb[freepcb].user_sp,argv[i]);
        p[i]=(char*)pcb[freepcb].user_sp;
    }
    pcb[freepcb].user_sp-=sizeof(p);
    memcpy((uint8_t*)pcb[freepcb].user_sp,(uint8_t*)p,sizeof(p));
    reg_t addr_argv=(reg_t)pcb[freepcb].user_sp;
    // align sp
    pcb[freepcb].user_sp-=pcb[freepcb].user_sp%128;
    // init pcb_stack
    ptr_t kernel_stack = pcb[freepcb].kernel_sp;
    ptr_t user_stack = pcb[freepcb].user_sp;
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[2] = (reg_t)user_stack;             // sp
    pt_regs->regs[4] = (reg_t)&pcb[freepcb];       // tp
    pt_regs->regs[10] = (reg_t)argc;                  // a0
    pt_regs->regs[11] = addr_argv;                    // a1
    // special registers
    pt_regs->sstatus = SR_SPIE & ~SR_SPP;             // make spie(1) and spp(0)
    pt_regs->sepc = (reg_t)entry_point;
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

    return process_id;
}

int do_kill(pid_t pid)
{
    // Note that we can't kill kernel, shell or process that not exist
    if (pid<=1 || pcb[pid].is_used==0) return 0;
    // Note that we don't need to kill the process that is EXITED
    if (pcb[pid-1].status==TASK_EXITED) return pid;
    pcb[pid-1].status = TASK_EXITED;
    // Delete the process from it's queue (ready_queue or sleep_queue or some block_queue)
    list_del(&pcb[pid-1].list);
    // Release all the waiting process
    while (!list_empty(&pcb[pid-1].wait_list))
        do_unblock(pcb[pid-1].wait_list.prev);
    // Release acquired locks
    for (int i=0;i<LOCK_NUM;i++)
        if (mlocks[i].pid==pid) do_mutex_lock_release(i);
    // Release pcb;
    pcb[pid-1].is_used=0;
    return pid;
}

void do_exit(void)
{
    current_running->status=TASK_EXITED;
    // Unblock all the waiting process
    while (!list_empty(&current_running->wait_list))
        do_unblock(current_running->wait_list.prev);
    // Release all the acquired locks
    for (int i=0;i<LOCK_NUM;i++)
        if (mlocks[i].pid==current_running->pid) do_mutex_lock_release(i);
    // Release pcb;
    pcb[current_running->pid-1].is_used=0;
    // Find the next process to run
    do_scheduler();
}

int do_waitpid(pid_t pid)
{
    // Note that we can't wait for kernel, shell or process that not exist
    if (pid<=1 || pcb[pid].is_used==0) return 0;
    // Note that we don't need to wait for a EXITED process
    if (pcb[pid-1].status!=TASK_EXITED)
        do_block(&current_running->list,&pcb[pid-1].wait_list);
    return pid;
}

void do_process_show()
{
    printk("[Process Table]:\n");
    for (int i=0;i<NUM_MAX_TASK;i++)
    {
        if (pcb[i].is_used==0) continue;
        printk("[%d] PID : %d , STATUS : ",i,pcb[i].pid);
        switch (pcb[i].status)
        {
            case TASK_RUNNING:
                printk("RUNNING\n");
                break;
            case TASK_READY:
                printk("READY\n");
                break;
            case TASK_BLOCKED:
                printk("BLOCKED\n");
                break;
            default:
                break;
        }
    }
}

pid_t do_getpid()
{
    return current_running->pid;
}