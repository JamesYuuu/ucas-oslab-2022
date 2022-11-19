#include <type.h>
#include <printk.h>
#include <asm.h>
#include <csr.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/list.h>

pcb_t tcb[NUM_MAX_THREAD];
extern void ret_from_exception();

void pthread_create(pthread_t *thread, void (*start_routine)(void *), void *arg)
{
    int cpu_id = get_current_cpu_id();
    int free_tcb = NUM_MAX_THREAD;
    for (int i = 0; i < NUM_MAX_THREAD; i++)
    {
        if (tcb[i].is_used == 0)
        {
            free_tcb = i;
            break;
        }
    }
    if (free_tcb == NUM_MAX_THREAD)
    {
        *thread = -1;
        return;
    }
    // set father process
    current_running[cpu_id]->son[current_running[cpu_id]->thread_num] = &tcb[free_tcb];
    current_running[cpu_id]->thread_num++;

    tcb[free_tcb].kernel_stack_base = allocPage()->kva + PAGE_SIZE;
    tcb[free_tcb].user_stack_base = current_running[cpu_id]->user_stack_base + PAGE_SIZE * current_running[cpu_id]->thread_num;
    list_init(&tcb[free_tcb].mm_list);
    list_init(&tcb[free_tcb].wait_list);

    // copying father's pg_dir
    tcb[free_tcb].pgdir = current_running[cpu_id]->pgdir;
    // alloc user_stack;
    alloc_page_helper(tcb[free_tcb].user_stack_base - PAGE_SIZE, current_running[cpu_id]);

    tcb[free_tcb].pid = current_running[cpu_id]->pid;
    tcb[free_tcb].status = TASK_READY;
    tcb[free_tcb].is_used = 1;
    tcb[free_tcb].kernel_sp = tcb[free_tcb].kernel_stack_base;
    tcb[free_tcb].user_sp = tcb[free_tcb].user_stack_base;
    tcb[free_tcb].father = current_running[cpu_id];
    tcb[free_tcb].mask = current_running[cpu_id]->mask;
    tcb[free_tcb].cursor_x = current_running[cpu_id]->cursor_x;
    tcb[free_tcb].cursor_y = current_running[cpu_id]->cursor_y;
    tcb[free_tcb].thread_num = -1;
    

    // init regs_context_t by copying from father pcb;
    regs_context_t *pt_regs =
        (regs_context_t *)(tcb[free_tcb].kernel_stack_base - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;

    // init some special regs
    pt_regs->regs[2] = (reg_t)tcb[free_tcb].user_sp; // sp
    pt_regs->regs[4] = (reg_t)&tcb[free_tcb];        // tp
    pt_regs->regs[10] = (reg_t)arg;                  // a0
    // special registers
    pt_regs->sstatus = SR_SPIE & ~SR_SPP | SR_SUM; // make spie(1) and spp(0)
    pt_regs->sepc = (reg_t)start_routine;
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;

    // init switchto_context_t by copying from father pcb;
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++)
        pt_switchto->regs[i] = 0;

    // init some special regs
    pt_switchto->regs[0] = (reg_t)ret_from_exception;    // ra
    pt_switchto->regs[1] = (reg_t)tcb[free_tcb].user_sp; // sp

    tcb[free_tcb].kernel_sp = (ptr_t)pt_switchto;

    *thread = tcb[free_tcb].pid;
    // add new thread to ready_queue
    list_add(&ready_queue, &(tcb[free_tcb].list));
}

int pthread_join(pthread_t thread)
{
    // FIXME: How to know if thread is finished?
    int cpu_id = get_current_cpu_id();
    int current_tcb = 0;
    for (int i = 0; i < NUM_MAX_THREAD; i++)
        if (tcb[i].pid == thread && tcb[i].is_used == 1)
        {
            current_tcb = i;
            break;
        }
    if (current_tcb == 0)
        return 0;
    if (tcb[current_tcb].status != TASK_EXITED)
        do_block(&current_running[cpu_id]->list, &tcb[current_tcb].wait_list);
    return thread;
}