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
#include <os/smp.h>

extern void ret_from_exception();

// Task info array
#define TASK_ADDRESS 0xffffffc058000000

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
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
    short task_num;
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    bios_sdread(kva2pa(TASK_ADDRESS), 1, 0);
    task_num = *((short *)(TASK_ADDRESS + 512 - 10));
    int info_address = *((int *)(TASK_ADDRESS + 512 - 8));
    int block_id = info_address / 512;
    int offset = info_address % 512;
    int block_num = (info_address + sizeof(task_info_t) * task_num - 1) / 512 + 1 - block_id;
    bios_sdread(kva2pa(TASK_ADDRESS), block_num, block_id);
    for (short i = 0; i < task_num; i++)
    {
        tasks[i] = *((task_info_t *)(unsigned long)(TASK_ADDRESS + offset));
        offset += sizeof(task_info_t);
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
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[2] = (reg_t)user_stack; // sp
    pt_regs->regs[4] = (reg_t)pcb;        // tp
    // special registers
    pt_regs->sstatus = SR_SPIE & ~SR_SPP | SR_SUM; // make spie(1) and spp(0)
    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 2; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pt_switchto->regs[0] = (reg_t)ret_from_exception; // ra
    pt_switchto->regs[1] = (reg_t)user_stack;         // sp
    pcb->kernel_sp = (ptr_t)pt_switchto;
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    // Note that tasks[0] is shell and we only need to load shell
    for (int i = 0; i < TASK_MAXNUM; i++)
    {
        pcb[i].kernel_stack_base = allocPage()->kva + PAGE_SIZE;
        pcb[i].user_stack_base = (ptr_t)USER_STACK_ADDR;
        list_init(&pcb[i].wait_list);
        list_init(&pcb[i].mm_list);
    }
    pcb[0].pid = 1;
    pcb[0].kernel_sp = pcb[0].kernel_stack_base;
    pcb[0].user_sp = pcb[0].user_stack_base;
    pcb[0].status = TASK_READY;
    pcb[0].cursor_x = pcb[0].cursor_y = 0;
    pcb[0].thread_num = 0;
    pcb[0].is_used = 1;
    pcb[0].mask = CORE_BOTH;
    // alloc page dir
    mm_page_t *page_dir = allocPage();
    clear_pgdir(page_dir->kva);
    list_add(&pcb[0].mm_list,&page_dir->list);
    page_dir->fixed = 1;
    pcb[0].pgdir = page_dir->kva;
    // copy kernel page table to user page table
    share_pgtable(pcb[0].pgdir, PGDIR_KVA);
    // alloc user stack
    alloc_page_helper(USER_STACK_ADDR - NORMAL_PAGE_SIZE, &pcb[0]);
    // alloc user page table and copy task_image to kva
    int page_num = tasks[0].memsz / NORMAL_PAGE_SIZE + 1;
    uintptr_t prev_kva;
    for (int j = 0; j < page_num; j++)
    {
        uintptr_t va = tasks[0].entry_point + j * NORMAL_PAGE_SIZE;
        uintptr_t kva = alloc_page_helper(va, &pcb[0]);
        load_task_img(0, kva, prev_kva, j);
        prev_kva = kva;
    }
    // init pcb stack
    init_pcb_stack(pcb[0].kernel_sp, pcb[0].user_sp, tasks[0].entry_point, &pcb[0]);
    list_add(&ready_queue, &(pcb[0].list));
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running[0] = &pid0_pcb;
    current_running[1] = &pid1_pcb;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    // syscalls for processes
    syscall[SYSCALL_EXEC]    = (long (*)())do_exec;
    syscall[SYSCALL_EXIT]    = (long (*)())do_exit;
    syscall[SYSCALL_SLEEP]   = (long (*)())do_sleep;
    syscall[SYSCALL_KILL]    = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_PS]      = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID]  = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD]   = (long (*)())do_scheduler;

    // syscalls for taskset
    syscall[SYSCALL_TASKSET]    = (long (*)())do_taskset;
    syscall[SYSCALL_TASKSET_P]  = (long (*)())do_taskset_p;

    // syscalls for input and output
    syscall[SYSCALL_WRITE]   = (long (*)())screen_write;
    syscall[SYSCALL_READCH]  = (long (*)())bios_getchar;
    syscall[SYSCALL_CURSOR]  = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_CLEAR]   = (long (*)())screen_clear;

    // syscalls for timer
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]     = (long (*)())get_ticks;

    // syscalls for mutex lock
    syscall[SYSCALL_LOCK_INIT]    = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]     = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;

    // syscalls for barriers
    syscall[SYSCALL_BARR_INIT]    = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]    = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;

    // syscalls for condition variable
    syscall[SYSCALL_COND_INIT]      = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT]      = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]    = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]   = (long (*)())do_condition_destroy;

    // syscalls for mailbox
    syscall[SYSCALL_MBOX_OPEN]  = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]  = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]  = (long (*)())do_mbox_recv;

    // syscalls for create_thread
    syscall[SYSCALL_PTHREAD_CREATE] = (long (*)())pthread_create;
    syscall[SYSCALL_PTHREAD_JOIN]   = (long (*)())pthread_join;

    // syscalls for shared memory
    syscall[SYSCALL_SHM_GET] = (long (*)())shm_page_get;
    syscall[SYSCALL_SHM_DT]  = (long (*)())shm_page_dt;

    // syscalls for snapshot
    syscall[SYSCALL_SNAPSHOT_SHOT]    = (long (*)())do_snapshot_shot;
    syscall[SYSCALL_SNAPSHOT_RESTORE] = (long (*)())do_snapshot_restore;
    syscall[SYSCALL_GETPA]            = (long (*)())do_getpa;

}

void cancel_mapping()
{
    // Cancel temporary mapping by setting the pmd to 0
    PTE *pg_dir = (PTE *)PGDIR_KVA;
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu)
    {
        uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
        del_attribute(&pg_dir[vpn2], _PAGE_PRESENT);
    }

    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
}

int main(void)
{
    int cpu_id = get_current_cpu_id();

    // note that core 1 only need to setup exception and the first time interrupt
    // other initialization should be done by core 0
    if (cpu_id == 1)
    {
        // Cancel previous mapping for boot.c
        cancel_mapping();
        setup_exception();
        set_timer(get_ticks() + TIMER_INTERVAL);
        while (1)
        {
            enable_preempt();
            asm volatile("wfi");
        }
    }

    // FIXME: delete after double core
    // Cancel previous mapping for boot.c
    cancel_mapping();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init memory management (0_0)
    init_memory();
    init_disk();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init barrier mechanism (｡•ˇ‸ˇ•｡)
    init_barriers();
    printk("> [INIT] Barrier mechanism initialization succeeded.\n");

    // Init condition variable mechanism (｡•ˇ‸ˇ•｡)
    init_conditions();
    printk("> [INIT] Condition variable mechanism initialization succeeded.\n");

    // Init mailbox mechanism (｡•ˇ‸ˇ•｡)
    init_mbox();
    printk("> [INIT] Mailbox mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // init the lock
    smp_init();

    // wakeup_other_hart()
    // wakeup_other_hart();

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    set_timer(get_ticks() + TIMER_INTERVAL);

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}