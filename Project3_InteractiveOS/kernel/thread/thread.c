#include <type.h>
#include <printk.h>
#include <os/sched.h>
#include <os/list.h>
#include <csr.h>
#include <os/mm.h>
#include <asm.h>

#define THREAD_STACK_SIZE 1024
#define MAX_THREAD_NUM 16

pcb_t tcb[MAX_THREAD_NUM];
int thread_id = 0;
extern void ret_from_exception();

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
    pt_switchto->regs[0]=(reg_t)ret_from_exception;        //ra
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