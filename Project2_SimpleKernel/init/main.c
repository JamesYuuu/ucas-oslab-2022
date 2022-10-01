/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();

// Task info array
#define TASK_ADDRESS 0x58000000
task_info_t tasks[TASK_MAXNUM];
short task_num;

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    bios_sdread(TASK_ADDRESS, 1, 0);
    task_num=*((short*)(TASK_ADDRESS+512-10));
    int info_address=*((int*)(TASK_ADDRESS+512-8));
    int block_id=info_address/512;
    int offset=info_address%512;
    int block_num=(info_address+sizeof(task_info_t)*task_num-1)/512+1-block_id;
    bios_sdread(TASK_ADDRESS, block_num, block_id);
    for (short i=0;i<task_num;i++)
    {
        tasks[i]=*((task_info_t*)(unsigned long)(TASK_ADDRESS+offset));
        load_task_img(tasks[i].task_name,task_num);
        offset+=sizeof(task_info_t);
    }
}

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
    pt_regs->regs[1]=(reg_t)entry_point; //ra
    pt_regs->regs[2]=(reg_t)user_stack;  //sp
    pt_regs->regs[4]=(reg_t)pcb;         //tp
    // special registers
    pt_regs->sstatus=SR_SPIE & ~SR_SPP;  // make spie(1) and spp(0)
    pt_regs->sepc=(reg_t)entry_point;
    pt_regs->sbadaddr=0;
    pt_regs->scause=0;

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

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (short i=0;i<task_num;i++)
    {
        pcb[i].pid=i+1;
        pcb[i].kernel_sp=allocKernelPage(1);
        pcb[i].user_sp=allocUserPage(1);
        pcb[i].status=TASK_READY;
        pcb[i].cursor_x=pcb[i].cursor_y=0;
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, tasks[i].entry_point, &pcb[i]);
        list_add(&ready_queue,&(pcb[i].list));
    }
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running=&pid0_pcb;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]=(long (*)())do_sleep;
    syscall[SYSCALL_YIELD]=(long (*)())do_scheduler;
    syscall[SYSCALL_WRITE]=(long (*)())screen_write;
    syscall[SYSCALL_CURSOR]=(long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]=(long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE]=(long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]=(long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]=(long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]=(long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]=(long (*)())do_mutex_lock_release;
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}