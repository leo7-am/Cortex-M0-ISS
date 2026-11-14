@ main(void) is not used: this image enters at _start.
@ fault_unaligned_rom_store (Farah): a misaligned store to ROM is UNALIGNED, not BUS
	.syntax unified
	.cpu cortex-m0
	.thumb
	.section .vectors, "a"
	.word 0x20040000
	.word _start
	.text
	.thumb_func
	.global _start
_start:
	movs r1, #2              @ address 2: in ROM and not word-aligned
	movs r0, #5
	str r0, [r1, #0]         @ FAULT UNALIGNED at this instruction
	movs r0, #0
	svc #0x00
	.ltorg
