#include <asm.h>

ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la s1,__bss_start
    la s2,__BSS_END__
    bge s1,s2,end

loop:
    sw zero,(s1)
    addi s1,s1,4
    blt s1,s2,loop

end:
    /* TODO: [p1-task3] enter main function */
    addi sp,sp,-16
    sw ra,12(sp)
    call main

    /* TODO: [p1-task3] finish task and return to the kernel */
    lw ra,12(sp)
    addi sp,sp,16
    ret

    /* NOTE: You need to replace this with new mechanism in p3-task2! */

// while(1) loop, unreachable here
/*
loop:
    wfi
    j loop
*/

END(_start)
