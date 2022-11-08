#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence
	
	// a0 is mhartid
	bnez a0, secondary
	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li a0,BIOS_PUTSTR
	la a1,msg
	call bios_func_entry
	
	// TODO: [p1-task2] call BIOS to read kernel in SD card
	li a0,BIOS_SDREAD
	la a1,kernel		//mem_address
	la a4,os_size_loc
	lh a2,(a4)			//num_of_blocks
	li a3,1				//block_id
	call bios_func_entry

	// TODO: [p1-task4] load task-related arguments and pass them to kernel

	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	la s0,kernel
	jr s0

secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	// mask all interrupts
	csrw CSR_SIE, zero
	// let stvec pointer to kernel_main
	la s0, kernel
  	csrw CSR_STVEC, s0
	// enable software interrupts for ipi
	li s0,SIE_SSIE
	csrs CSR_SIE, s0
	li s0,SR_SIE
	csrw CSR_SSTATUS, s0

wait_for_wakeup:
	wfi
	j wait_for_wakeup

// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"
