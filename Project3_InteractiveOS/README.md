# Project3 Interactive OS and Process Management

## 任务1：终端和终端命令的实现

### 1.1 终端的实现

终端本质上是一个用户程序，最重要的功能是实现对用户输入输出的解析，并根据用户输入的命令调用相应的输入输出，下面函数实现了对用户输入的分割。实现了分割之后，就可以简单的根据命令和相应的参数调用对应的系统调用了。

```c
void seperate_command()
{
    int i,start;
    i=start=0;
    while (i<len)
    {
        if (command_buffer[i] == ' ')
        {
            strncpy(argv[argc],command_buffer+start,i-start);
            start = i+1;
            argc++;
        }
        i++;
    }
    strncpy(argv[argc++],command_buffer+start,i-start);
    return;
}
```

为了实现用户输入时的退格键，需要在系统调用的原语中对退格键'\b'进行特殊处理，在`screen_write_ch`函数中，如果是退格键，就将光标向前移动一格，并重置屏幕输出，修改后的函数如下。

```c
static void screen_write_ch(char ch)
{
    int cpu_id = get_current_cpu_id();
    if (ch == '\n')
    {
        current_running[cpu_id]->cursor_x = 0;
        current_running[cpu_id]->cursor_y++;
    }
    else if (ch == '\b')
    {
        current_running[cpu_id]->cursor_x--;
        new_screen[SCREEN_LOC(current_running[cpu_id]->cursor_x, current_running[cpu_id]->cursor_y)] = ' ';
    }
    else
    {
        new_screen[SCREEN_LOC(current_running[cpu_id]->cursor_x, current_running[cpu_id]->cursor_y)] = ch;
        current_running[cpu_id]->cursor_x++;
    }
}
```

### 1.2 系统调用`sys_clear`的实现

用户命令`clear [line]`，对应系统调用`sys_clear`，其中`line`参数对应清除的起始行数，该参数缺省的时候，则仅清除用户与`shell`交互的部分，对应的系统原语实现如下。

```c
void screen_clear(int height)
{
    int cpu_id = get_current_cpu_id();
    int i, j;
    for (i = height; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            new_screen[SCREEN_LOC(j, i)] = ' ';
        }
    }
    current_running[cpu_id]->cursor_x = 0;
    current_running[cpu_id]->cursor_y = 0;
    screen_reflush();
}
```

### 1.3 系统调用`sys_ps`的实现

用户命令`ps`，对应系统调用`sys_ps`，其实现的功能为展示当前的任务列表和相关信息，对应的系统原语实现如下。实现方式即为遍历所有的`pcb`，输出其相应信息。

```c
void do_process_show()
{
    printk("[Process Table]:\n");
    for (int i=0;i<NUM_MAX_TASK;i++)
    {
        if (pcb[i].is_used==0) continue;
        printk("[%d] PID : %d  ",i,pcb[i].pid);
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
            else printk("RUNNING ON CORE 1");
        }
        printk("\n");
    }
}
```

### 1.4 系统调用`sys_exec`的实现

用户命令`exec [task_name] [argv]`，对应系统调用`sys_exec`，其中`task_name`为用户输入的任务名，`argv`为用户输入的参数，实现的方法基本复用之前`init_pcb`和`init_pcb_stack`中的代码。唯一需要注意的是，用户程序标准的`main`函数为`int main(int argc, char *argv[])`，因此`kernel`需要承担向用户程序传递参数的任务，因此`kernel`需要将相关参数拷贝到用户程序的用户栈中，供用户使用，还需要注意的是，`RISC-V`的调用规范要求栈指针是`128bit`对齐的，因此我们需要额外进行对齐。具体的实现代码如下。

```c
pid_t do_exec(char *name, int argc, char *argv[])
{
    // init pcb
    uint64_t entry_point = 0;
    for (int i=0;i<TASK_MAXNUM;i++)
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
    pcb[freepcb].kernel_sp = pcb[freepcb].kernel_stack_base;
    pcb[freepcb].user_sp = pcb[freepcb].user_stack_base;
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
    pt_regs->regs[4] = (reg_t)&pcb[freepcb];          // tp
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

    // inherit mask from shell or it's parent
    int cpu_id = get_current_cpu_id();
    pcb[freepcb].mask = current_running[cpu_id]->mask;

    return process_id;
}
```

### 1.5 系统调用`sys_kill`的实现

用户调用`kill [pid]`，对应系统调用`sys_kill`，其中`pid`为用户输入的进程号。当一个进程被`kill`掉之后，我们需要回收其资源，例如回收其持有的`pcb`块、未释放的锁等，此外我们还需要将其等待队列中将等待其结束的进程唤醒。

需要特别处理如下几种例外：`sys_kill`不可以杀死`shell`和`kernel`进程；当`sys_kill`需要杀死的进程正在运行的时候，它此时不存在任何的队列的当中，因此我们只需要标记其状态，确保下一次调度的时候不将其重新加入调度队列即可。具体实现的系统原语如下。

```c
int do_kill(pid_t pid)
{
    // Find the corresponding pcb
    int current_pcb=0;
    for (int i=0;i<TASK_MAXNUM;i++)
        if (pcb[i].pid==pid && pcb[i].is_used==1)
        {
            current_pcb=i;
            break;
        }
    // Note that we can't kill kernel, shell or process that not exist
    if (current_pcb==0) return 0;
    // Note that we don't need to kill the process that is EXITED
    if (pcb[current_pcb].status==TASK_EXITED) return pid;
    // Delete the process from it's queue (ready_queue or sleep_queue or some block_queue)
    // If it's not running on the other core;
    if (pcb[current_pcb].status!=TASK_RUNNING)
        list_del(&pcb[current_pcb].list);
    pcb[current_pcb].status = TASK_EXITED;
    // Release pcb;
    pcb[current_pcb].is_used=0;
    // Release all the waiting process
    while (!list_empty(&pcb[current_pcb].wait_list))
        do_unblock(pcb[current_pcb].wait_list.prev);
    // Release acquired locks
    for (int i=0;i<LOCK_NUM;i++)
        if (mlocks[i].pid==pid) do_mutex_lock_release(i);
    return pid;
}
```

### 1.6 系统调用`sys_exit`的实现

系统调用`sys_exit`的实现方式和`sys_kill`基本相同，区别就在于是“自杀”还是“他杀”。相比于`sys_kill`，不需要考虑上述的例外，只需要回收相应的资源即可。

特别的，在回收内存空间上，我采取了`faq`上取巧的方法，即与PCB一起回收复用，在第一次分配完PCB后，内核栈/用户栈就固定地归属于相应的PCB。等到下次`sys_exec`开启新进程时，直接复用之前的PCB和内核栈/用户栈，而不
是重新分配新的内核栈/用户栈，具体可以参考`do_exec`函数的实现。

```c
void do_exit(void)
{
    int cpu_id = get_current_cpu_id();
    // Find the corresponding pcb
    int current_pcb=0;
    for (int i=0;i<TASK_MAXNUM;i++)
        if (pcb[i].pid==current_running[cpu_id]->pid && pcb[i].is_used==1)
        {
            current_pcb=i;
            break;
        }
    current_running[cpu_id]->status=TASK_EXITED;
    // Release pcb;
    pcb[current_pcb].is_used=0;
    // Unblock all the waiting process
    while (!list_empty(&current_running[cpu_id]->wait_list))
        do_unblock(current_running[cpu_id]->wait_list.prev);
    // Release all the acquired locks
    for (int i=0;i<LOCK_NUM;i++)
        if (mlocks[i].pid==current_running[cpu_id]->pid) do_mutex_lock_release(i);
    // Find the next process to run
    do_scheduler();
}
```

### 1.7 系统调用`sys_waitpid`的实现

系统调用`sys_waitpid`的实现并不复杂，只需要将其加入到相应进程的等待队列，等待这些进程结束或被杀死后将其唤醒即可。需要注意的是，我们不能等待`kernel`和`shell`进程结束。

```c
int do_waitpid(pid_t pid)
{
    int cpu_id = get_current_cpu_id();
    // Find the corresponding pcb
    int current_pcb=0;
    for (int i=0;i<TASK_MAXNUM;i++)
        if (pcb[i].pid==pid && pcb[i].is_used==1)
        {
            current_pcb=i;
            break;
        }
    // Note that we can't wait for kernel, shell or process that not exist
    if (current_pcb==0) return 0;
    // Note that we don't need to wait for a EXITED process
    if (pcb[current_pcb].status!=TASK_EXITED)
        do_block(&current_running[cpu_id]->list,&pcb[current_pcb].wait_list);
    return pid;
}
```

### 1.8 系统调用`sys_getpid`的实现

系统调用`sys_getpid`的实现非常简单，只需要返回当前进程的`pid`即可。

```c
pid_t do_getpid()
{
    int cpu_id = get_current_cpu_id();
    return current_running[cpu_id]->pid;
}
```

## 任务 2：实现同步原语：barriers、condition variables

### 2.1 同步原语`barrier`的实现

同步原语`barrier`的作用是同步所有的进程，因此我们需要维护一个等待队列，当有进程没到达时阻塞其余所有进程；当所有进程都到达时，唤醒所有进程即可，具体实现如下。

#### 2.1.1 `barrier`的数据结构

`barrier`的结构体如下，各个成员的含义可以参考注释。

```c
typedef struct barrier
{
    // TODO [P3-TASK2 barrier]
    int key;                // key
    int wait_num;           // current waiting number
    int is_use;             // is being used?
    int goal;               // numbers need to wait
    list_head block_queue;  // block queue
} barrier_t;
```

#### 2.1.2 `barrier`的初始化

`barrier`的初始化中，我们需要初始化等待队列，并标记所有的`barrier`为未使用。

```c
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
```

#### 2.1.3 `barrier`的创建

`barrier`的创建中，我们需要找到一个未使用的`barrier`，并设置相关的值。

```c
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
```

#### 2.1.4 `barrier`的摧毁

`barrier`的摧毁函数中，我们释放所有的等待进程，并将其标记为未使用。

```c
void do_barrier_destroy(int bar_idx)
{
    // Release all the blocked process
    while (!list_empty(&barriers[bar_idx].block_queue))
        do_unblock(barriers[bar_idx].block_queue.prev);
    barriers[bar_idx].is_use=0;
    return;
}
```

#### 2.1.5 `barrier`的同步

`barrier`的同步函数中，当进程到达时，我们将其加入到等待队列中，如果等待队列中的进程数目达到了目标数目，则唤醒所有进程。

```c
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
```

### 2.2 同步原语`condition variable`的实现

`condition variable`的实现也并不复杂，我们同样需要维护一个等待队列，当其等待条件变量的时候，我们将其加入等待队列；当条件满足的时候，我们根据需求唤醒一个等待进程或所有的等待进程。

#### 2.2.1 `condition variable`的数据结构

`condition variable`的结构体如下，各个成员的含义可以参考注释。

```c
typedef struct condition
{
    // TODO [P3-TASK2 condition]
    int key;                // key
    int is_use;             // is being used?
    list_head block_queue;  // block queue
} condition_t;
```

#### 2.2.2 `condition variable`的初始化

`condition variable`的初始化中，我们需要初始化等待队列，并标记所有的`condition variable`为未使用。

```c
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
```

#### 2.2.3 `condition variable`的创建

`condition variable`的创建中，我们需要找到一个未使用的`condition variable`，并设置相关值。

```c
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
```

#### 2.2.4 `condition variable`的摧毁

`condition variable`的摧毁函数中，我们释放所有的等待进程，并将其标记为未使用。

```c
void do_condition_destroy(int cond_idx)
{
    // Release all the blocked process
    while (!list_empty(&conditions[cond_idx].block_queue))
        do_unblock(conditions[cond_idx].block_queue.prev);
    conditions[cond_idx].is_use=0;
    return;
}
```

#### 2.2.5 `condition variable`的等待

`condition variable`的等待函数中，我们将其加入等待队列。

需要特别注意的是，在加入等待队列前，我们需要先解锁互斥锁，以便让其他的进程进入临界区；当被唤醒后，我们需要重新加锁，以保护临界区。

```c
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
```

#### 2.2.6 `condition variable`的唤醒

`condition variable`的唤醒函数中，我们根据需求唤醒一个等待进程或所有的等待进程。

```c
// wake up one block process
void do_condition_signal(int cond_idx)
{
    // Release the first blocked process
    if (!list_empty(&conditions[cond_idx].block_queue))
        do_unblock(conditions[cond_idx].block_queue.prev);
    return;
}
// wake up all blocked processes
void do_condition_broadcast(int cond_idx)
{
    // Release all the blocked process
    while (!list_empty(&conditions[cond_idx].block_queue))
        do_unblock(conditions[cond_idx].block_queue.prev);
    return;
}
```

### 任务 2 续：进程间的通信——mailbox 实现

`mailbox`为一个信箱，用于进程间的通信。`mailbox`存在多个`client`用于发送信息，当信息的长度超过信箱剩余空间的时候，则阻塞`client`，直到顺利发送为止。同样`mailbox`也存在多个`server`用于接收信息，当信箱中信息长度小于接收长度的时候，则阻塞`server`，直到顺利接收为止，具体实现如下。

#### 2.3.1 `mailbox`的结构体

`mailbox`的结构体如下，各个成员的含义可以参考注释。

```c
typedef struct mailbox
{
    // TODO [P3-TASK2 mailbox]
    char name[MAX_MBOX_LENGTH];     // name
    int is_use;                     // is being used?
    int length;                     // current length
    int max_length;                 // max length
    char buffer[MAX_MBOX_LENGTH];   // buffer
    list_head recv_queue;           // blocked receive queue
    list_head send_queue;           // blocked send queue
} mailbox_t;
```

#### 2.3.2 `mailbox`的初始化

`mailbox`的初始化中，我们需要初始化发送和接收这两个等待队列，并标记所有的`mailbox`为未使用。

```c
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
```

#### 2.3.3 `mailbox`的创建

`mailbox`的创建中，我们需要找到一个未使用的`mailbox`，并设置相关值。

```c
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
```

#### 2.3.4 `mailbox`的关闭

`mailbox`的关闭中，我们需要将`mailbox`标记为未使用，并唤醒所有等待的进程。

```c
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
```

#### 2.3.5 `mailbox`的发送

`mailbox`的发送中，我们检查是否`mailbox`中有足够多的空间，如果没有，则阻塞当前进程。如果有的话，我们将信息写入`mailbox`中，并唤醒所有的等待接收的进程，让他们重新抢占新的信息。

需要注意的是，如果阻塞的进程被唤醒后，不仅需要检查`mailbox`中是否有足够多的空间，如果没有的话，需要再次阻塞；同样需要检查信息是否还存在，如果信箱不存在则直接返回。

```c
int do_mbox_send(int mbox_idx, void * msg, int msg_length)
{
    int cpu_id;
    int block_num=0;
    // If the mailbox has no available space, block the send process.
    // Note that when the process leaves the while the space should be enough.
    while (msg_length>mailboxes[mbox_idx].max_length-mailboxes[mbox_idx].length)
    {
        if (mailboxes[mbox_idx].is_use == 0) return;
        cpu_id = get_current_cpu_id();
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
```

#### 2.3.6 `mailbox`的接收

`mailbox`的接收中，我们的实现方式和发送类似，同样在成功接收后，释放所有等待发送的进程，让他们重新抢占新的空间。

```c
int do_mbox_recv(int mbox_idx, void * msg, int msg_length)
{
    if (mailboxes[mbox_idx].is_use == 0) return 0;
    int cpu_id = get_current_cpu_id();
    int block_num=0;
    // If the mailbox has no available message , block the receive process.
    // Note that when the process leaves the while there should be available message.
    while (msg_length>mailboxes[mbox_idx].length)
    {
        cpu_id = get_current_cpu_id();
        block_num++;
        do_block(&current_running[cpu_id]->list,&mailboxes[mbox_idx].recv_queue);
        if (mailboxes[mbox_idx].is_use == 0) return 0;
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
```

## 任务 3：开启双核并行运行

在`bootbloader`中，从核并不需要重复加载`kernel`镜像，从核需要做的就是设置中断函数的入口地址为`kernel`的`main`函数入口地址，然后开启软件中断，等待主核完成初始化后将其唤醒。

```assembly
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	// mask all interrupts
	csrw CSR_SIE, zero
	// let stvec pointer to kernel_main
	la s0, kernel
  	csrw CSR_STVEC, s0
	// enable software interrupts for ipi
	li s0,SIE_SSIE
	csrs CSR_SIE, s0
	li s0,SR_SIE
	csrw CSR_SSTATUS, s0

wait_for_wakeup:
	wfi
	j wait_for_wakeup
```

在`kernel`中，从核只需要初始化自己的栈指针和线程指针，且不需要清空bss段，然后打开中断并设置第一个时钟中断，等待时钟中断到来后，进入`do_scheduler`调度空闲的进程。

```assembly
#include <asm.h>
#include <csr.h>

.section ".entry_function","ax"
ENTRY(_start)
    /* Mask all interrupts */
    csrw CSR_SIE, zero
    csrw CSR_SIP, zero
    // If cpu is core1 then no need to clear BSS
    csrr a0, CSR_MHARTID
    bnez a0, core1
    /* TODO: [p1-task2] clear BSS for flat non-ELF images */
    // check riscv.lds for __bss_start and __BSS_END__
    la s1,__bss_start
    la s2,__BSS_END__
    bge s1,s2,core0

loop:
    sw zero,(s1)
    addi s1,s1,4
    blt s1,s2,loop

core0:
    /* TODO: [p1-task2] setup C environment */
    lw sp,pid0_stack
    la tp,pid0_pcb
    call main

core1:
    lw sp,pid1_stack
    la tp,pid1_pcb
    call main

END(_start)
```

```c
    if (cpu_id == 1) 
    {
        setup_exception();
        set_timer(get_ticks() + TIMER_INTERVAL);
        while (1)
        {
            enable_preempt();
            asm volatile("wfi");
        }
    }
```

而主核需要完成所有的初始化工作，在完成后，通过核间中断唤醒从核。

```c
void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    // // Wake up the other cores
    disable_interrupt();
    send_ipi(NULL);
    asm volatile("csrw 0x144,zero");  // clear CSR_SIP to avoid ipi interrupt
    enable_interrupt();
}
```

为了保证内核中要将共享的变量或一些不能打断的过程不被两个核心打断，可直接采取内核锁的方式，即在进入内核前申请内核锁，在退出内核后释放内核锁。

内核锁的实现采取了自旋锁的方式，并使用原子指令保证了对临界区操作的原子性

```c
void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
    return;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return atomic_swap_d(LOCKED, &lock->status);
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
```

在进入内核前申请内核锁。

```assembly
ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT
  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  
  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  call lock_kernel
  ld a0,PCB_KERNEL_SP(tp)
  addi a0,a0,SWITCH_TO_SIZE
  csrr a1,CSR_STVAL
  csrr a2,CSR_SCAUSE
  call interrupt_helper
  j ret_from_exception

ENDPROC(exception_handler_entry)
```

在退出内核后释放内核锁。

```assembly
ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  call unlock_kernel
  RESTORE_CONTEXT
  sret
ENDPROC(ret_from_exception)
```

当然在进程调度的时候，我们需要两个`current_running`，标记两个核心正在运行的进程，具体对于`do_scheduler`的修改参考下个任务。

## 任务 4：shell 命令 taskset————将进程绑定在指定的核上

在`pcb`中增加`mask_status_t`字段，用于标记进程的绑定状态。

```c
typedef enum {
    CORE_0,
    CORE_1,
    CORE_BOTH,
} mask_status_t;
```

### 系统调用`sys_taskset`的实现

终端命令`taskset [mask] [task_name]`，根据`mask`的值，启动进程`task_name`，并将其绑定在指定的核上。我们先调用`do_exec`启动进程，并将其`pcb`的`mask`字段修改为指定的`mask`值。

```c
pid_t do_taskset(char* name , int argc, char *argv[] , int mask)
{
    int pid = do_exec(name,argc,argv);
    if (pid==0) return 0;
    // Find the corresponding pcb
    int current_pcb=0;
    for (int i=0;i<TASK_MAXNUM;i++)
        if (pcb[i].pid==pid && pcb[i].is_used==1)
        {
            current_pcb=i;
            break;
        }
    // change the mask
    switch (mask)
    {
        case 1:
            pcb[current_pcb].mask=CORE_0;
            break;
        case 2:
            pcb[current_pcb].mask=CORE_1;
            break;
        case 3:
            pcb[current_pcb].mask=CORE_BOTH;
            break;
        default:
            break;
    }
    return pid;
}
```

### 系统调用`sys_taskset_p`的实现

终端命令`taskset -p [mask] [pid]`，根据`mask`的值，将进程`pid`绑定在指定的核上。我们只需要修改`pcb`的`mask`字段即可。

```c
pid_t do_taskset_p(pid_t pid , int mask)
{
    // Find the corresponding pcb
    int current_pcb=0;
    for (int i=0;i<TASK_MAXNUM;i++)
        if (pcb[i].pid==pid && pcb[i].is_used==1)
        {
            current_pcb=i;
            break;
        }
    // Note that we can't change kernel, shell or process that not exist
    if (current_pcb==0) return 0;
    // change the mask
    switch (mask)
    {
        case 1:
            pcb[current_pcb].mask=CORE_0;
            break;
        case 2:
            pcb[current_pcb].mask=CORE_1;
            break;
        case 3:
            pcb[current_pcb].mask=CORE_BOTH;
            break;
        default:
            break;
    }
    return pid;
}
```

### 调度器的实现

在`do_scheduler`中，我们需要根据`cpu_id`检索符合条件的进程，然后进行调度。

```c
void do_scheduler(void)
{
    int cpu_id = get_current_cpu_id();
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    // TODO: [p2-task1] Modify the current_running[cpu_id] pointer.
    // Find the next task to run for this core.
    int flag=0;
    mask_status_t mask = (cpu_id == 0) ? CORE_0 : CORE_1;
    list_node_t *next_running;
    list_head *temp_queue=&ready_queue;
    while (temp_queue->prev!=&ready_queue)
    {
        pcb_t *pcb=list_to_pcb(temp_queue->prev);
        if (pcb->mask == mask || pcb->mask == CORE_BOTH)
        {
            flag=1;
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
        else  /* switch to kernel and wait for interrupt */
        {
            pcb_t *prev_running=current_running[cpu_id];
            current_running[cpu_id] = (cpu_id == 0) ? &pid0_pcb : &pid1_pcb;
            switch_to(prev_running,current_running[cpu_id]);
            return;
        }
    }
    // If current_running[cpu_id] is running and it's not kernel
    // re-add it to the ready_queue
    // else which means it's already in ready_queue or sleep_queue or other block_queue
    pcb_t *prev_running=current_running[cpu_id];
    if (prev_running->pid!=0 && prev_running->status==TASK_RUNNING)  //make sure the kernel stays outside the ready_queue
    {
        prev_running->status=TASK_READY;
        list_add(&ready_queue,&prev_running->list); //add the prev_running to the end of the queue
    }
    // Find the next task to run
    // Make sure set current_running[cpu_id]'s status to TASK_RUNNING
    // AFTER check if prev_running's status is TASK_RUNNING
    // because prev_running might be the same as current_running[cpu_id] and
    // it might be re-add to ready_queue by other syscalls
    list_node_t *current_list=next_running;        
    list_del(current_list);
    current_running[cpu_id]=list_to_pcb(current_list);
    current_running[cpu_id]->status = TASK_RUNNING;
    // TODO: [p2-task1] switch_to current_running[cpu_id]
    switch_to(prev_running,current_running[cpu_id]);
}
```