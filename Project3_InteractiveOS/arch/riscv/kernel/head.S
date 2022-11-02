/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

#define KERNEL_STACK_1        0x50502000

.section ".entry_function","ax"
ENTRY(_start)
    /* Mask all interrupts */
    csrw CSR_SIE, zero
    csrw CSR_SIP, zero
    // If cpu is core1 then no need to clear BSS
    bnez a0, core1
    /* TODO: [p1-task2] clear BSS for flat non-ELF images */
    // check riscv.lds for __bss_start and __BSS_END__
    la s1,__bss_start
    la s2,__BSS_END__
    bge s1,s2,end

loop:
    sw zero,(s1)
    addi s1,s1,4
    blt s1,s2,loop

end:
    /* TODO: [p1-task2] setup C environment */
    li sp,KERNEL_STACK
    mv tp,zero
    call main

core1:
    li sp,KERNEL_STACK_1
    mv tp,zero
    call main

END(_start)
