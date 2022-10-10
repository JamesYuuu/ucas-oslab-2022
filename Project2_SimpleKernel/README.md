# Project2 Simple Kernel

## 任务1: 任务启动与非抢占式调度

```c
/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;

    /* previous, next pointer */
    list_node_t list;

    /* process id */
    pid_t pid;

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

} pcb_t;
```

`pcb`结构体的定义如上所示，对于`kernel_sp`和`user_sp`的初始化，需要注意栈的大小，避免用户程序的栈溢出，导致破坏了其他`pcb`的栈空间。对于pid的初始化，只需要满足不同的`pcb`各不相同即可。对于`task_status_t`的初始化，则统一初始化为`TASK_READY`。

初始完成`pcb`后，我们将其加入`ready_queue`中，等待调度器的调度。并将`current_running`指向`pid0_pcb`，也就是`kernel`，为第一次上下文切换做好准备。

```c
static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (short i=0;i<task_num;i++)
    {
        pcb[i].pid=i+1;
        pcb[i].kernel_sp=allocKernelPage(4);
        pcb[i].user_sp=allocUserPage(4);
        pcb[i].status=TASK_READY;
        pcb[i].cursor_x=pcb[i].cursor_y=0;
        pcb[i].thread_num=-1;
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, tasks[i].entry_point, &pcb[i]);
        list_add(&ready_queue,&(pcb[i].list));
    }
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running=&pid0_pcb;
}
```

对于pcb的内核栈，需要做`switchto_context_t`的初始化，为所有用户程序的第一次上下文切换做好准备，为了能够让我们第一次切换后，能够顺利运行程序，我们需要将`ra`寄存器设置为程序的入口地址，并将`sp`寄存器设置为用户栈的地址，这样我们完成第一次上下文切换后，能正常跳转到用户程序的入口地址，从而开始执行用户程序。

```c
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i=2;i<14;i++)
        pt_switchto->regs[i]=0;
    pt_switchto->regs[0]=(reg_t)entry_point;  //ra
    pt_switchto->regs[1]=(reg_t)user_stack;   //sp 
    pcb->kernel_sp=(ptr_t)pt_switchto;
}
```

完成所有的任务后，我们调用`do_scheduler`函数，进行第一次的上下文切换，从`kernel`切换到第一个用户程序，在调度器中，我们采用的是非抢占式调度，即当一个用户程序运行完毕后主动放弃CPU资源，才会进行下一个用户程序的调度。而具体的调度策略采用的先进先出的策略，因此每次调用`do_scheduler`函数，都意味这当前用户程序主动放弃了CPU资源，我们将其加入到`ready_queue`的队尾，并从`ready_queue`中取出队头，作为下一个运行的程序。

```c
void do_scheduler(void)
{
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
```

上下文切换的`switch_to`函数将当前`s0-s11`、`ra`和`sp`共14个寄存器的值保存到`prev_running`的内核栈中，然后将`current_running`的内核栈中的值加载到`s0-s11`、`ra`和`sp`这14个寄存器中，并跳转到`ra`寄存器指向的地址，由于在之前我们为第一次切换的`ra`寄存器设置了用户程序的入口地址，因此我们能够顺利跳转到用户程序的入口地址，从而开始执行用户程序。

```assembly
  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  ld t0,PCB_KERNEL_SP(a0)
  sd ra,SWITCH_TO_RA(t0)
    ...
  sd s11,SWITCH_TO_S11(t0)
  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  ld t0,PCB_KERNEL_SP(a1)
  ld ra,SWITCH_TO_RA(t0)
    ...
  ld s11,SWITCH_TO_S11(t0)
  mv tp,a1
  jr ra
```

至此我们已经实现了调度器和上下文切换，接下来补充说明关于ready_queue的一些细节。

在本次实验中，`ready_queue`采用的是双向的循环链表，这意味着`ready_queue`的队头和队尾都是`ready_queue`本身。

在初始化`ready_queue`时，需要将`ready_queue`的`prev`和`next`指针都指向`ready_queue`本身。

将一个进程加入到`ready_queue`时，我们将其加入到`ready_queue`的队尾，即将其加入到`ready_queue`的`next`指针所指向的节点的前面。

将一个进程从`ready_queue`中取出时，我们将其从`ready_queue`的队头取出，即将其从`ready_queue`的`prev`指针所指向的节点的后面取出。

判断`ready_queue`是否为空，只需判断`ready_queue`的`prev`和`next`指针是否指向`ready_queue`本身即可。

```c
/* TODO: [p2-task1] implement your own list API */
static inline void list_init(list_head *head)
{
    head->next = head;
    head->prev = head;
}
static inline void list_add(list_node_t *pre, list_node_t *node)
{
    pre->next->prev = node;
    node->next = pre->next;
    node->prev = pre;
    pre->next = node;
}
static inline void list_del(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}
static inline int list_empty(list_head *head)
{
    return (head->prev==head);
}
```

## 任务2: 互斥锁的实现

```c
typedef struct spin_lock
{
    volatile lock_status_t status;
} spin_lock_t;

typedef struct mutex_lock
{
    spin_lock_t lock;
    list_head block_queue;
    int key;
} mutex_lock_t;
```

互斥锁的结构如上所示，包含了一个自旋锁，一个阻塞队列和一个key，在做整体初始化的时候，我们需要初始化阻塞队列，将自旋锁的状态设置为UNLOCKED，并将key设置为-1，代表其未被分配。

```c
void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i=0;i<LOCK_NUM;i++)
    {
        mlocks[i].lock.status=UNLOCKED;
        list_init(&mlocks[i].block_queue);
        mlocks[i].key=-1;
    }
}
```

当用户程序请求分配锁的时候，我们优先分配拥有相同key的锁，如果没有的话，则分配一个全新的锁，通过这样的机制，我们允许了不同的程序申请同一把锁，也允许一个程序申请多把锁，只要这些锁的key不同即可。

```c
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
```

当用户程序申请锁的时候，我们首先判断锁的状态，如果锁的状态为`UNLOCKED`，那么我们将锁的状态设置为`LOCKED`。如果此时锁的状态已经为`LOCKED`，则代表锁已经被其他进程占用，我们将当前进程加入到锁的阻塞队列中，并将当前进程的状态设置为`TASK_BLOCKED`，然后调用`do_scheduler`函数进行进程切换，等到锁被释放后，再次尝试申请锁，直到成功申请为止。

```c
void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    while (mlocks[mlock_idx].lock.status==LOCKED) 
        do_block(&current_running->list,&mlocks[mlock_idx].block_queue);
    mlocks[mlock_idx].lock.status=LOCKED;
}
void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_add(queue,pcb_node);
    pcb_t *pcb=list_to_pcb(pcb_node);
    pcb->status=TASK_BLOCKED;
    do_scheduler();
}
```

当用户程序释放锁的时候，如果锁的阻塞队列为空，那么我们将锁的状态设置为`UNLOCKED`，否则我们将该锁阻塞队列中所有的进程唤醒，并加入到`ready_queue`中，然后调用`do_scheduler`函数进行进程切换。

```c
void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    while (!list_empty(&mlocks[mlock_idx].block_queue))
        do_unblock(mlocks[mlock_idx].block_queue.prev);
    mlocks[mlock_idx].lock.status=UNLOCKED;
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    list_add(&ready_queue,pcb_node);
    pcb_t *pcb=list_to_pcb(pcb_node);
    pcb->status=TASK_READY;
}
```

这样我们就实现了互斥锁的相关操作。

## 任务3: 系统调用

首先在这个任务中，我们需要增加一个`do_sleep`方法，能够让当前进程睡眠一段时间，先单独介绍一下`do_sleep`方法的实现。

在`do_sleep`方法中，我们首先将当前进程的状态设置为`TASK_BLOCKED`，然后将当前进程加入到`sleep_queue`中，并设置当前进程的`wakeup_time`，以便睡眠结束后能够被唤醒。然后调用`do_scheduler`函数进行进程切换。

```c
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
```

实现了进程的睡眠后，我们需要在每次调用`do_scheduler`的时候，都先调用`check_sleeping`函数。该函数的作用是检查当前的`sleep_queue`中是否有进程需要被唤醒，如果有，我们将其从`sleep_queue`中移除，并加入到`ready_queue`中，以便之后的调度器重新调用这些进程。

```c
void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_head *temp_queue=&sleep_queue;
    while (temp_queue->prev!=&sleep_queue)
    {
        pcb_t *pcb=list_to_pcb(temp_queue->prev);
        if (pcb->wakeup_time<=get_timer())
        {
            list_node_t *temp_node=temp_queue->prev;
            list_del(temp_queue->prev);
            list_add(&ready_queue,temp_node);
            pcb->status=TASK_READY;
        }
        temp_queue=temp_queue->prev;
    }
}
```

下面的实现重点在于处理中断和实现系统调用，接下来将从用户程序和内核两个部分讲解系统调用是如何实现的。

### 用户程序

用户程序触发中断或实现系统调用的方式，是通过`ecall`指令，调用前需要将系统调用号保存于`a7`寄存器中，然后将参数保存于`a0,a1...`等寄存器中进行传递，并将返回值保存在`a0`寄存器中。故我们利用内联汇编，实现一个简单的系统调用函数，该函数的功能是将系统调用号和参数传递给内核，并返回内核返回的值。

```c
static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,long arg3, long arg4)
{
    long result;
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    asm volatile(
        "mv a7,%1;\
        mv a0,%2;\
        mv a1,%3;\
        mv a2,%4;\
        mv a3,%5;\
        mv a4,%6;\
        ecall;\
        mv %0,a0"
        :"=r"(result)
        :"r"(sysno),"r"(arg0),"r"(arg1),"r"(arg2),"r"(arg3),"r"(arg4)
        :"a0","a1","a2","a3","a4","a7"
    );
    return result;
}
```

而我们其余的系统调用API，都是通过调用`invoke_syscall`来实现的，比如`sys_yield`。

```c
void sys_yield(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement // sys_yield */
    invoke_syscall(SYSCALL_YIELD,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}
```

到此，我们已经为用户程序提供了系统调用的API,接下来需要在内核中实现中断处理，并根据系统调用号调用相应的系统调用函数。

### 内核

`risc-v`的特权级共有三级，分别是`User`、`Supervisor`和`Machine`，因此我们的操作系统是运行在`Supervisor`特权级下的，而用户程序是运行在`User`特权级下的。当用户程序触发中断或系统调用时，`risc-v`硬件会将当前的特权级切换到`Supervisor`，并将当前的`pc`保存在`sepc`寄存器中，然后跳转到`stvec`寄存器指向的地址，即中断处理函数的入口地址。

除了上述两个寄存器外，还有`sie`、`scause`、`stval`和`sstatus`等寄存器帮助我们辅助处理中断。`sie`寄存器用于控制中断的使能，`scause`寄存器用于保存中断的原因，`stval`寄存器用于保存中断的附加信息，`sstatus`寄存器中包含三个重要的字段，分别是`SPP`、`SPIE`和`SIE`，`SPP`用于保存当前特权级，`SPIE`用于保存中断使能状态，`SIE`用于控制全局中断。

当发生中断时，`SIE`被硬件自动设置为0，用来屏蔽中断，同时`SPIE`被硬件自动设置为`SIE`的值，如果特权级发生改变，则将之前的特权级记录在`SPP`上。当从中断中返回时，`SIE`被自动设置为`SPIE`的值，同时`SPIE`被自动设置为1，特权级别恢复为`SPP`记录的级别，然后`SPP`被设置为`User`模式。

通过这些寄存器，我们可以进行中断处理和系统调用的操作了，首先我们需要将中断函数的地址放到`stvec`寄存器中。

```assembly
ENTRY(setup_exception)
  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la s0, exception_handler_entry
  csrw CSR_STVEC, s0
ENDPROC(setup_exception)
```

在中断处理函数`exception_handler_entry`中，我们需要先保存上下文，接着我们将当前线程内核栈的入口，`stval`和`scause`寄存器的值传递给函数`interrupt_helper`，让这个函数根据相关的中断类型分别处理，调用对应的函数进行处理。等到`interrupt_helper`处理完毕后，我们再恢复上下文，然后返回到用户程序中，需要注意返回的时候需要使用`sret`指令，以保证用户程序的特权级为`User`模式。

```assembly
ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT
  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  ld a0,PCB_KERNEL_SP(tp)
  addi a0,a0,SWITCH_TO_SIZE
  csrr a1,CSR_STVAL
  csrr a2,CSR_SCAUSE
  call interrupt_helper
  j ret_from_exception

ENDPROC(exception_handler_entry)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT
  sret
ENDPROC(ret_from_exception)
```

在恢复和保存上下文的时候，需要时刻注意tp寄存器的值，因为tp寄存器需要时刻指向当前线程的PCB，以保证我们能顺利定位到当前线程的内核栈，方便我们进行上下文的保存和恢复。

```asssembly
.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */
  sd sp,PCB_USER_SP(tp)
  ld sp,PCB_KERNEL_SP(tp)
  addi sp,sp,SWITCH_TO_SIZE // make sp point to regs_context_t

  // save all regs except sp
  sd zero,OFFSET_REG_ZERO(sp)
    ...
  sd t6,OFFSET_REG_T6(sp)

  // save sp
  ld s0,PCB_USER_SP(tp)
  sd s0,OFFSET_REG_SP(sp)

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */
  csrrc s0,CSR_SSTATUS,t0
  sd s0,OFFSET_REG_SSTATUS(sp)
  csrr s0,CSR_SEPC
  sd s0,OFFSET_REG_SEPC(sp)
  csrr s0,CSR_STVAL
  sd s0,OFFSET_REG_SBADADDR(sp)
  csrr s0,CSR_SCAUSE
  sd s0,OFFSET_REG_SCAUSE(sp)

  addi sp,sp,-(SWITCH_TO_SIZE)
.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
  ld sp,PCB_KERNEL_SP(tp)
  addi sp,sp,SWITCH_TO_SIZE // make sp point to regs_context_t

  // restore special registers
  ld s0,OFFSET_REG_SSTATUS(sp)
  csrw CSR_SSTATUS,s0
  ld s0,OFFSET_REG_SEPC(sp)
  csrw CSR_SEPC,s0

  // restore registers except sp
  ld ra,OFFSET_REG_RA(sp)
    ...
  ld t6,OFFSET_REG_T6(sp)

  // restore sp
  ld sp,OFFSET_REG_SP(sp)
.endm
```

回到刚刚的`interrupt_helper`函数中，我们根据`scause`的第一位，确定是中断还是异常，然后再根据`scause`剩下的值代表的具体类型，调用相应的函数。

```c
void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    uint64_t irq_type=scause & SCAUSE_IRQ_FLAG; 
    uint64_t irq_code=scause & ~SCAUSE_IRQ_FLAG;
    if (irq_type==0) exc_table[irq_code](regs,stval,scause);
        else irq_table[irq_code](regs,stval,scause);
}
```

我们再`init_exception`函数中，初始化`exc_table`和`irq_table`，并且调用先前写好的`setup_exception`函数，将`exception_handler_entry`函数的地址写入`stvec`寄存器中。由于在任务3中，我们只需要考虑系统调用，因此只需要初始化`exc_table`即可。

```c
void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for (int i=0;i<EXCC_COUNT;i++)
        exc_table[i]=(handler_t)handle_other;
    exc_table[EXCC_SYSCALL]=(handler_t)handle_syscall;
    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}
```

到这里，我们的用户程序使用的系统调用函数，都进入了`handle_syscall`函数中，我们需要在这个函数中，根据`a7`寄存器的值，调用相应的系统调用函数，并将结果保存在`a0`寄存器中。要注意的是，目前的`sepc`也就是返回地址指向的是`ecall`指令，我们需要手动为其加上4，使其指向下一条指令。

```c
void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    regs->sepc = regs->sepc + 4;
    regs->regs[10] = syscall[regs->regs[17]](regs->regs[10],regs->regs[11],regs->regs[12],regs->regs[13],regs->regs[14]); //a7,a0,a1,a2,a3,a4
}
```

在这里的使用的`syscall`跳转表，我们在`init_syscall`函数中进行初始化。

```c
static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]=(long (*)())do_sleep;
        ...
    syscall[SYSCALL_CREATE_THREAD]=(long(*)())create_thread;
}
```

到这里，我们的系统调用函数就已经全部实现完毕了，此外，我们需要为第一次上下文切换做好准备，因此我们需要在`init_pcb_stack`中增加对于`regs_context_t`的初始化。我们需要初始化将`sepc`寄存器的值保存为用户程序的入口地址，确保第一次切换到用户程序时，能够正确执行。此外我们需要保证`sp`寄存器指向用户栈，`tp`寄存器指向`pcb`的地址，`ssatus`中的`spie`设置为1,`spp`设置为0，以确保能正常响应中断。

```c
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i=0;i<32;i++)
        pt_regs->regs[i]=0;
    pt_regs->regs[2]=(reg_t)user_stack;  //sp
    pt_regs->regs[4]=(reg_t)pcb;         //tp
    // special registers
    pt_regs->sstatus=SR_SPIE & ~SR_SPP;  // make spie(1) and spp(0)
    pt_regs->sepc=(reg_t)entry_point;
    pt_regs->sbadaddr=0;
    pt_regs->scause=0;

        ...
}
```

至此，我们已经能顺利响应系统调用，并执行对应的函数了。

## 任务 4：时钟中断、抢占式调度

为了实现时钟中断，我们首先打开全局中断使能，修改`setup_exception`函数如下。

```assmebly
ENTRY(setup_exception)
    ...
  /* TODO: [p2-task4] enable interrupts globally */
  j enable_interrupt
ENDPROC(setup_exception)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)
```

接着我们完善`init_exception`函数，增加对于时钟中断的处理。

```c
void init_exception()
{
    ...
    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for (int i=0;i<IRQC_COUNT;i++)
        irq_table[i]=(handler_t)handle_other;
    irq_table[IRQC_S_TIMER]=(handler_t)handle_irq_timer;
    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}
```

在`handle_irq_timer`函数中，我们重新设置下一次时钟中断的时间，然后调用`do_scheduler`函数进行抢占式调度。

```c
void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    set_timer(get_ticks()+TIMER_INTERVAL);
    do_scheduler();
}
```

当然别忘了在`main`函数中设置第一次时钟中断，并打开时钟中断使能。

```c
    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    set_timer(get_ticks()+TIMER_INTERVAL);

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }
```

## 任务5: 实现线程的创建 thread_create

线程是操作系统能够进行运算调度的最小单位，通常一个进程中包含多条线程，这些线程共享进程的资源，但又有自己独立的内核栈和用户栈，用于保存自己的运行状态。在这个简易的操作系统中，我们仍然用`pcb_t`的结构体指代线程，但增加了如下成员变量，来标志子线程或者父进程。

```c
    int thread_num;
    union{
        struct pcb *father;
        struct pcb *son[NUM_MAX_THREAD];
    };
```

初始化线程的操作和初始化进程基本一致,不同的是我们的内核栈和用户栈都是从父进程空闲的栈中分配的，而不是从内存中分配，并且初始化的上下文信息也是从父进程的上下文信息中复制的。需要注意的是，我们需要将上下文寄存器中的`a0`赋值为函数的参数，以便线程能够正确调用函数。

```c
#define THREAD_STACK_SIZE 1024
#define MAX_THREAD_NUM 16

pcb_t tcb[MAX_THREAD_NUM];
int thread_id = 0;

void create_thread(long entry, long arg) 
{
    ptr_t kernel_stack,user_stack;
    kernel_stack = allocKernelPage(1);
    user_stack = allocUserPage(1);

    // init regs_context_t by copying from father pcb;
    regs_context_t *pcb_regs =
        (regs_context_t*)(current_running->kernel_sp+sizeof(switchto_context_t));
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    for (int i = 0; i < 31; i++)
        pt_regs->regs[i] = pcb_regs->regs[i];

    // init some special regs
    pt_regs->regs[1]=(reg_t)entry;       //ra
    pt_regs->regs[2]=(reg_t)user_stack;  //sp
    pt_regs->regs[4]=(reg_t)&tcb[thread_id]; //tp
    pt_regs->regs[10]=(reg_t)arg;        //a0
    // special registers
    pt_regs->sstatus=SR_SPIE & ~SR_SPP;  // make spie(1) and spp(0)
    pt_regs->sepc=(reg_t)entry;
    pt_regs->sbadaddr=0;
    pt_regs->scause=0;
    
    // init switchto_context_t by copying from father pcb;
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    switchto_context_t *pcb_switchto =
        (switchto_context_t *)(current_running->kernel_sp);
    for (int i=0;i<14;i++)
        pt_switchto->regs[i]=pcb_switchto->regs[i];

    // init some special regs
    pt_switchto->regs[0]=(reg_t)entry;        //ra
    pt_switchto->regs[1]=(reg_t)user_stack;   //sp 

    // set new thread
    tcb[thread_id].kernel_sp=(ptr_t)pt_switchto;
    tcb[thread_id].user_sp=user_stack;
    tcb[thread_id].father=current_running;
    tcb[thread_id].pid=current_running->thread_num+100;
    tcb[thread_id].cursor_x=current_running->cursor_x;
    tcb[thread_id].cursor_y=current_running->cursor_y;
    tcb[thread_id].status=TASK_READY;

    // manage father thread
    current_running->thread_num++;
    current_running->son[current_running->thread_num]=&tcb[thread_id];

    // add new thread to ready_queue
    list_add(&ready_queue,&tcb[thread_id].list);
    thread_id++;
}
```

至此，我们创造出了一个新的线程，也将其加入了`ready_queue`中，等待调度。