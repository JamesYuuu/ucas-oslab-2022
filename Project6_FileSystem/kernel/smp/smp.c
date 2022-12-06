#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/irq.h>

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_init(&kernel_lock);
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    // // Wake up the other cores
    disable_interrupt();
    send_ipi(NULL);
    asm volatile("csrw 0x144,zero");  // clear CSR_SIP to avoid ipi interrupt
    enable_interrupt();
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_acquire(&kernel_lock);
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_release(&kernel_lock);
}
