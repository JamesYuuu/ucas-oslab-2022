#include <asm.h>

#define KERNEL_STACK		0x50500000
#define KERNEL_ADDRESS      0x50201000

ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la s1,__bss_start
    la s2,__BSS_END__
    bge s1,s2,end

end:
    /* TODO: [p1-task3] enter main function */
    li sp,KERNEL_STACK
    call main

    /* TODO: [p1-task3] finish task and return to the kernel */
	la a0,KERNEL_ADDRESS
	jr a0
    /* NOTE: You need to replace this with new mechanism in p3-task2! */

// while(1) loop, unreachable here
loop:
    sw zero,(s1)
    addi s1,s2,4
    blt s1,s2,loop

END(_start)
