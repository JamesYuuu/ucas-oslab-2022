#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARRIER_NUM];
condition_t conditions[CONDITION_NUM];
mailbox_t mailboxes[MBOX_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    // Note that key=-1 means the lock is not used
    // Note that pid=-1 means the lock is not acquired
    for (int i=0;i<LOCK_NUM;i++)
    {
        mlocks[i].lock.status=UNLOCKED;
        list_init(&mlocks[i].block_queue);
        mlocks[i].key=-1;
        mlocks[i].pid=-1;
    }
}

// Note: spinlock is used to lock kernel
void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
    return;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return atomic_swap(LOCKED, &lock->status);
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (spin_lock_try_acquire(lock) == LOCKED);
    return;
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
    return;
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    // First, find the lock with the same key
    for (int i=0;i<LOCK_NUM;i++)
       if (mlocks[i].key==key) return i;
    // Second, find the unused lock
    for (int i=0;i<LOCK_NUM;i++)
       if (mlocks[i].key==-1)
       {
           mlocks[i].key=key;
           return i;
       }
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    int cpu_id = get_current_cpu_id();
    /* TODO: [p2-task2] acquire mutex lock */
    // Continuously block the process if the lock is acquired
    while (mlocks[mlock_idx].lock.status==LOCKED) 
        do_block(&current_running[cpu_id]->list,&mlocks[mlock_idx].block_queue);
    // Acquire the lock
    mlocks[mlock_idx].lock.status=LOCKED;
    mlocks[mlock_idx].pid=current_running[cpu_id]->pid;
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    // Release all the blocked process
    while (!list_empty(&mlocks[mlock_idx].block_queue))
        do_unblock(mlocks[mlock_idx].block_queue.prev);
    // Release the lock
    mlocks[mlock_idx].lock.status=UNLOCKED;
    mlocks[mlock_idx].pid=-1;
}

// syscalls for barrier
void init_barriers(void)
{
    /* Initialize barriers */
    for (int i=0;i<BARRIER_NUM;i++)
    {
        barriers[i].is_use=0;
        list_init(&barriers[i].block_queue);
    }
}

int do_barrier_init(int key, int goal)
{
    // First, find the barrier with the same key
    for (int i=0;i<BARRIER_NUM;i++)
        if (barriers[i].is_use==1 && barriers[i].key==key) 
            return i;
    // Second, find the unused barrier
    for (int i=0;i<BARRIER_NUM;i++)
        if (barriers[i].is_use==0)
        {
            barriers[i].is_use=1;
            barriers[i].key=key;
            barriers[i].goal=goal;
            barriers[i].wait_num=0;
            return i;
        }
    return -1;
}
void do_barrier_wait(int bar_idx)
{
    int cpu_id = get_current_cpu_id();
    barriers[bar_idx].wait_num++;
    if (barriers[bar_idx].wait_num!=barriers[bar_idx].goal)
        do_block(&current_running[cpu_id]->list,&barriers[bar_idx].block_queue);
    else 
    {
        while (!list_empty(&barriers[bar_idx].block_queue))
            do_unblock(barriers[bar_idx].block_queue.prev);
        barriers[bar_idx].wait_num=0;
    }
    return;
}
void do_barrier_destroy(int bar_idx)
{
    // Release all the blocked process
    while (!list_empty(&barriers[bar_idx].block_queue))
        do_unblock(barriers[bar_idx].block_queue.prev);
    barriers[bar_idx].is_use=0;
    return;
}

// syscalls for condition
void init_conditions(void)
{
    /* Initialize conditions */
    for (int i=0;i<CONDITION_NUM;i++)
    {
        conditions[i].is_use=0;
        list_init(&conditions[i].block_queue);
    }
}

int do_condition_init(int key)
{
    // First, find the condition with the same key
    for (int i=0;i<CONDITION_NUM;i++)
        if (conditions[i].is_use==1 && conditions[i].key==key) 
            return i;
    // Second, find the unused condition
    for (int i=0;i<CONDITION_NUM;i++)
        if (conditions[i].is_use==0)
        {
            conditions[i].is_use=1;
            conditions[i].key=key;
            return i;
        }
    return -1;
}
void do_condition_wait(int cond_idx, int mutex_idx)
{
    int cpu_id = get_current_cpu_id();
    // Release the mutex lock
    do_mutex_lock_release(mutex_idx);
    // Block the process
    do_block(&current_running[cpu_id]->list,&conditions[cond_idx].block_queue);
    // Acquire the mutex lock
    do_mutex_lock_acquire(mutex_idx);
    return;
}
void do_condition_signal(int cond_idx)
{
    // Release the first blocked process
    if (!list_empty(&conditions[cond_idx].block_queue))
        do_unblock(conditions[cond_idx].block_queue.prev);
    return;
}
void do_condition_broadcast(int cond_idx)
{
    // Release all the blocked process
    while (!list_empty(&conditions[cond_idx].block_queue))
        do_unblock(conditions[cond_idx].block_queue.prev);
    return;
}
void do_condition_destroy(int cond_idx)
{
    // Release all the blocked process
    while (!list_empty(&conditions[cond_idx].block_queue))
        do_unblock(conditions[cond_idx].block_queue.prev);
    conditions[cond_idx].is_use=0;
    return;
}

// syscalls for mailbox
void init_mbox()
{
    /* Initialize mailboxes */
    for (int i=0;i<MBOX_NUM;i++)
    {
        mailboxes[i].is_use=0;
        mailboxes[i].max_length=MAX_MBOX_LENGTH;
        mailboxes[i].length=0;
        list_init(&mailboxes[i].recv_queue);
        list_init(&mailboxes[i].send_queue);
    }
}
int do_mbox_open(char *name)
{
    // First, find the mailbox with the same name
    for (int i=0;i<MBOX_NUM;i++)
        if (mailboxes[i].is_use==1 && strcmp(mailboxes[i].name,name)==0) 
            return i;
    // Second, find the unused mailbox
    for (int i=0;i<MBOX_NUM;i++)
        if (mailboxes[i].is_use==0)
        {
            mailboxes[i].is_use=1;
            strcpy(mailboxes[i].name,name);
            return i;
        }
    return -1;
}
void do_mbox_close(int mbox_idx)
{
    mailboxes[mbox_idx].is_use=0;
    mailboxes[mbox_idx].length=0;
    // Release all the blocked reveive process
    while (!list_empty(&mailboxes[mbox_idx].recv_queue))
        do_unblock(mailboxes[mbox_idx].recv_queue.prev);
    // Release all the blocked send process
    while (!list_empty(&mailboxes[mbox_idx].send_queue))
        do_unblock(mailboxes[mbox_idx].send_queue.prev);
    return;
}
int do_mbox_send(int mbox_idx, void * msg, int msg_length)
{
    int cpu_id = get_current_cpu_id();
    int block_num=0;
    // If the mailbox has no available space, block the send process.
    // Note that when the process leaves the while the space should be enough.
    while (msg_length>mailboxes[mbox_idx].max_length-mailboxes[mbox_idx].length)
    {
        block_num++;
        do_block(&current_running[cpu_id]->list,&mailboxes[mbox_idx].send_queue);
    }
    // Copy the message to the mailbox
    memcpy(mailboxes[mbox_idx].buffer+mailboxes[mbox_idx].length,msg,msg_length);
    mailboxes[mbox_idx].length+=msg_length;
    // Release all the blocked receive processes
    // Let all the blocked receive processes compete for the new message
    while (!list_empty(&mailboxes[mbox_idx].recv_queue))
        do_unblock(mailboxes[mbox_idx].recv_queue.prev);
    return block_num;
}
int do_mbox_recv(int mbox_idx, void * msg, int msg_length)
{
    int cpu_id = get_current_cpu_id();
    int block_num=0;
    // If the mailbox has no available message , block the receive process.
    // Note that when the process leaves the while there should be available message.
    while (msg_length>mailboxes[mbox_idx].length)
    {
        block_num++;
        do_block(&current_running[cpu_id]->list,&mailboxes[mbox_idx].recv_queue);
    }
    // Copy the message from the mailbox
    mailboxes[mbox_idx].length-=msg_length;
    memcpy(msg,mailboxes[mbox_idx].buffer,msg_length);
    // Copy the remaining message to the head of the buffer
    memcpy(mailboxes[mbox_idx].buffer,mailboxes[mbox_idx].buffer+msg_length,mailboxes[mbox_idx].length);
    // Release all the blocked send processes
    // Let all the blocked send processes compete for the spare buffer space    
    while (!list_empty(&mailboxes[mbox_idx].send_queue))
        do_unblock(mailboxes[mbox_idx].send_queue.prev);
    return block_num;
}