@ main(void) is not used: this image enters at _start.
@ getint_rules (Farah): GETINT skips white space, takes a sign, and leaves non-digits unread
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
	svc #0x15                @ "  -42" -> -42
	svc #0x12
	ldr r0, =10
	svc #0x10
	svc #0x15                @ "+7" -> 7
	svc #0x12
	ldr r0, =10
	svc #0x10
	svc #0x15                @ "x" is not a number -> 0, and x stays unread
	svc #0x12
	ldr r0, =10
	svc #0x10
	svc #0x14                @ so GETCHAR now returns 'x'
	svc #0x10
	ldr r0, =10
	svc #0x10
	svc #0x15                @ "-" with no digits -> 0, the sign stays unread
	svc #0x12
	ldr r0, =10
	svc #0x10
	svc #0x14                @ GETCHAR returns '-'
	svc #0x10
	svc #0x14                @ end of file -> -1
	svc #0x12
	ldr r0, =10
	svc #0x10
	movs r0, #0
	svc #0x00
	.ltorg
