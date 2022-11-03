/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

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
