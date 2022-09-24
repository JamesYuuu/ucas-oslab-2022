#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs


    // TODO: [p2-task1] Modify the current_running pointer.
    if (ready_queue.prev==&ready_queue) return;     // If there is no task in the ready queue, return

    pcb_t *prev_running=current_running;
    list_node_t *prev_list=ready_queue.prev;        //ready_queue.prev is the first node of the queue
    list_del(prev_list);
    current_running=(pcb_t*)((int)prev_list-2*sizeof(reg_t));
    current_running->status = TASK_RUNNING;
    if (prev_running->pid!=0 && prev_running->status==TASK_RUNNING)  //make sure the kernel stays outside the ready_queue
    {
        prev_running->status=TASK_READY;
        list_add(&ready_queue,&prev_running->list); //add the prev_running to the end of the queue
    }
    
    //printl("current_running.pid:%d,current_running.status:%d\n",current_running->pid,current_running->status);
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
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_add(queue,pcb_node);
    pcb_t *pcb=(pcb_t*)((int)pcb_node-2*sizeof(reg_t));
    pcb->status=TASK_BLOCKED;

    //printl("pcb.pid:%d,pcb.status:%d BLOCKED\n",current_running->pid,current_running->status);
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    list_add(&ready_queue,pcb_node);
    pcb_t *pcb=(pcb_t*)((int)pcb_node-2*sizeof(reg_t));
    pcb->status=TASK_READY;

    //printl("pcb.pid:%d,pcb.status:%d RELEASED ready_queue.next:0x%x\n",current_running->pid,current_running->status,ready_queue.next);
    do_scheduler();
}
