#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARRIER_NUM];
condition_t conditions[CONDITION_NUM];
mailbox_t mailboxes[MBOX_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i=0;i<LOCK_NUM;i++)
    {
        mlocks[i].lock.status=UNLOCKED;
        list_init(&mlocks[i].block_queue);
        mlocks[i].key=-1;
        mlocks[i].pid=-1;
    }
}

// note: no need to perform spinlock as we had perform mutex lock;
void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    for (int i=0;i<LOCK_NUM;i++)
       if (mlocks[i].key==key) return i;

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
    /* TODO: [p2-task2] acquire mutex lock */
    while (mlocks[mlock_idx].lock.status==LOCKED) 
        do_block(&current_running->list,&mlocks[mlock_idx].block_queue);
    mlocks[mlock_idx].lock.status=LOCKED;
    mlocks[mlock_idx].pid=current_running->pid;
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    while (!list_empty(&mlocks[mlock_idx].block_queue))
        do_unblock(mlocks[mlock_idx].block_queue.prev);
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
    barriers[bar_idx].wait_num++;
    if (barriers[bar_idx].wait_num!=barriers[bar_idx].goal)
        do_block(&current_running->list,&barriers[bar_idx].block_queue);
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
    if (barriers[bar_idx].wait_num!=barriers[bar_idx].goal) 
        barriers[bar_idx].wait_num=barriers[bar_idx].goal;
    barriers[bar_idx].is_use=0;
    return;
}

// syscalls for condition
void init_conditions(void)
{

}

int do_condition_init(int key)
{

}
void do_condition_wait(int cond_idx, int mutex_idx)
{

}
void do_condition_signal(int cond_idx)
{

}
void do_condition_broadcast(int cond_idx)
{

}
void do_condition_destroy(int cond_idx)
{

}

// syscalls for mailbox
void init_mbox()
{

}
int do_mbox_open(char *name)
{

}
void do_mbox_close(int mbox_idx)
{

}
int do_mbox_send(int mbox_idx, void * msg, int msg_length)
{

}
int do_mbox_recv(int mbox_idx, void * msg, int msg_length)
{
    
}