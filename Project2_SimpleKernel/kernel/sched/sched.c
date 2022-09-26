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

pcb_t* list_to_pcb(list_node_t *list)
{
    return (pcb_t*)((int)list-2*sizeof(reg_t));
}

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs


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
    do_scheduler();
}
