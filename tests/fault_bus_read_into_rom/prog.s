@ main(void) is not used: this image enters at _start.
@ fault_bus_read_into_rom (Farah): READ whose buffer is in ROM is a BUS fault at the SVC
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
	movs r0, #0
	ldr r1, =msg             @ msg is in .rodata, which is ROM
	movs r2, #3
	svc #0x22                @ stdin has bytes, so READ must write ROM: FAULT BUS
	movs r0, #0
	svc #0x00
	.ltorg
	.section .rodata
msg:	.ascii "abc"
