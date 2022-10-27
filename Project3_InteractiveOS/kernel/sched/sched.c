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
    if (list_empty(&ready_queue)) return;     // If there is no task in the ready queue, return

    pcb_t *prev_running=current_running;
    list_node_t *current_list=ready_queue.prev;        //ready_queue.prev is the first node of the queue
    list_del(current_list);
    current_running=list_to_pcb(current_list);
    current_running->status = TASK_RUNNING;
    if (prev_running->pid!=0 && prev_running->status==TASK_RUNNING)  //make sure the kernel stays outside the ready_queue
    {
        prev_running->status=TASK_READY;
        list_add(&ready_queue,&prev_running->list); //add the prev_running to the end of the queue
    }
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
    uint64_t entry_point = load_task_img(name,task_num);
    if (entry_point == 0) return -1;
    pcb[process_id].pid=process_id+1;
    pcb[process_id].kernel_sp = allocKernelPage(1);
    pcb[process_id].user_sp = allocUserPage(1);
    pcb[process_id].kernel_stack_base = pcb[process_id].kernel_sp;
    pcb[process_id].user_stack_base = pcb[process_id].user_sp;
    pcb[process_id].status = TASK_READY;
    pcb[process_id].cursor_x = pcb[process_id].cursor_y = 0;
    pcb[process_id].thread_num = -1;
    list_init(&pcb[process_id].wait_list);
    list_add(&ready_queue,&pcb[process_id].list);
    // copy argv to user_stack
    char *p[argc];
    for (int i=0;i<argc;i++)
    {
        int len=strlen(argv[i])+1;
        pcb[process_id].user_sp-=len;
        strcpy((char*)pcb[process_id].user_sp,argv[i]);
        p[i]=(char*)pcb[process_id].user_sp;
    }
    pcb[process_id].user_sp-=sizeof(p);
    memcpy((uint8_t*)pcb[process_id].user_sp,(uint8_t*)p,sizeof(p));
    reg_t addr_argv=(reg_t)pcb[process_id].user_sp;
    // align sp
    pcb[process_id].user_sp-=pcb[process_id].user_sp%128;
    // init pcb_stack
    ptr_t kernel_stack = pcb[process_id].kernel_sp;
    ptr_t user_stack = pcb[process_id].user_sp;
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[2] = (reg_t)user_stack;             // sp
    pt_regs->regs[4] = (reg_t)&pcb[process_id];       // tp
    pt_regs->regs[10] = (reg_t)argc;                  // a0
    pt_regs->regs[11] = addr_argv;                    // a1
    // special registers
    pt_regs->sstatus = SR_SPIE & ~SR_SPP;             // make spie(1) and spp(0)
    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;

    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 2; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pt_switchto->regs[0] = (reg_t)ret_from_exception; // ra
    pt_switchto->regs[1] = (reg_t)user_stack;         // sp
    pcb[process_id].kernel_sp = (ptr_t)pt_switchto;

    return ++process_id;
}

int do_kill(pid_t pid)
{
    if (pid<=1 || pid>process_id) return 0;
    if (pcb[pid-1].status==TASK_EXITED) return pid;
    pcb[pid-1].status = TASK_EXITED;
    list_del(&pcb[pid-1].list);
    // release all the wait_pid
    while (!list_empty(&pcb[pid-1].wait_list))
        do_unblock(pcb[pid-1].wait_list.prev);
    return pid;
}

void do_exit(void)
{
    current_running->status=TASK_EXITED;
    while (!list_empty(&current_running->wait_list))
        do_unblock(current_running->wait_list.prev);
    do_scheduler();
}

int do_waitpid(pid_t pid)
{
    if (pid<=1 || pid>process_id) return 0;
    if (pcb[pid-1].status!=TASK_EXITED)
        do_block(&current_running->list,&pcb[pid-1].wait_list);
    return pid;
}

void do_process_show()
{
    printk("[Process Table]:\n");
    for (int i=0;i<process_id;i++)
    {
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
            case TASK_EXITED:
                printk("EXITED\n");
                break;
        }
    }
}

pid_t do_getpid()
{
    return current_running->pid;
}