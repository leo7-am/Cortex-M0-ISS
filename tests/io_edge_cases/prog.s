@ main(void) is not used: this image enters at _start.
@ io_edge_cases (Farah): bad descriptors, zero-byte transfers, PUTINT and PUTHEX extremes
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
	movs r0, #3              @ WRITE to descriptor 3 -> -9
	ldr r1, =msg
	movs r2, #4
	svc #0x23
	svc #0x12
	ldr r0, =10
	svc #0x10
	movs r0, #1              @ zero-byte WRITE with a bad pointer -> 0, no fault
	ldr r1, =0x90000000
	movs r2, #0
	svc #0x23
	svc #0x12
	ldr r0, =10
	svc #0x10
	movs r0, #1              @ READ from descriptor 1 -> -9
	ldr r1, =buf
	movs r2, #4
	svc #0x22
	svc #0x12
	ldr r0, =10
	svc #0x10
	movs r0, #2              @ WRITE 4 bytes to stderr -> 4
	ldr r1, =msg
	movs r2, #4
	svc #0x23
	svc #0x12
	ldr r0, =10
	svc #0x10
	ldr r0, =0x80000000      @ most negative int
	svc #0x12
	ldr r0, =10
	svc #0x10
	ldr r0, =0xdeadbeef      @ PUTHEX: 0x and 8 uppercase digits
	svc #0x13
	ldr r0, =10
	svc #0x10
	movs r0, #7
	svc #0x00                @ exit status 7
	.ltorg
	.section .rodata
msg:	.ascii "err\n"
	.data
buf:	.space 8
