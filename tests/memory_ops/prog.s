@ main(void) is not used: entry is _start.
@
@ Test (Member A): every load/store form, LDM/STM, PUSH/POP and POP {pc}.
@ Besides those it uses only MOVS #imm and SVC, so it runs before the ALU exists.
@ Each check loads a value into r0 and prints it with PUTHEX, one per line.

        .syntax unified
        .cpu    cortex-m0
        .thumb

        .section .vectors, "a"
        .word   0x20040000
        .word   _start

        .text
        .thumb_func
        .global _start
_start:
        ldr     r1, =0x20000100         @ LDR literal: base address in RAM
        ldr     r2, =0x12345678
        str     r2, [r1, #4]            @ STR  [rn, #imm]
        ldr     r0, [r1, #4]            @ LDR  [rn, #imm]    12345678
        svc     #0x13
        movs    r0, #10
        svc     #0x10

        movs    r3, #4
        ldr     r0, [r1, r3]            @ LDR  [rn, rm]      12345678
        svc     #0x13
        movs    r0, #10
        svc     #0x10

        strb    r2, [r1, #1]            @ STRB [rn, #imm]    stores 78
        ldrb    r0, [r1, #1]            @ LDRB [rn, #imm]    00000078
        svc     #0x13
        movs    r0, #10
        svc     #0x10

        strh    r2, [r1, #2]            @ STRH [rn, #imm]    stores 5678
        ldrh    r0, [r1, #2]            @ LDRH [rn, #imm]    00005678
        svc     #0x13
        movs    r0, #10
        svc     #0x10

        movs    r3, #8
        ldr     r2, =0xffff8081
        str     r2, [r1, r3]            @ STR  [rn, rm]
        ldrsb   r0, [r1, r3]            @ LDRSB              ffffff81
        svc     #0x13
        movs    r0, #10
        svc     #0x10
        ldrsh   r0, [r1, r3]            @ LDRSH              ffff8081
        svc     #0x13
        movs    r0, #10
        svc     #0x10
        ldrh    r0, [r1, r3]            @ LDRH [rn, rm]      00008081
        svc     #0x13
        movs    r0, #10
        svc     #0x10
        ldrb    r0, [r1, r3]            @ LDRB [rn, rm]      00000081
        svc     #0x13
        movs    r0, #10
        svc     #0x10

        movs    r3, #12
        movs    r4, #0xab
        strb    r4, [r1, r3]            @ STRB [rn, rm]
        movs    r3, #14
        ldr     r4, =0xcdef
        strh    r4, [r1, r3]            @ STRH [rn, rm]
        ldr     r0, [r1, #12]           @ cdef00ab
        svc     #0x13
        movs    r0, #10
        svc     #0x10

        movs    r0, #7
        ldr     r2, =0x22222222
        push    {r0, r1, r2, lr}        @ PUSH (lr = ffffffff after reset)
        ldr     r0, [sp, #8]            @ LDR  [sp, #imm]    22222222
        svc     #0x13
        movs    r0, #10
        svc     #0x10
        movs    r5, #0x55
        str     r5, [sp, #0]            @ STR  [sp, #imm]    replaces the pushed r0
        pop     {r0, r1, r2, r3}        @ POP: r0 = 55, r3 = ffffffff
        svc     #0x13                   @ 00000055
        movs    r0, #10
        svc     #0x10
        str     r3, [r1, #16]
        ldr     r0, [r1, #16]           @ ffffffff (the popped lr)
        svc     #0x13
        movs    r0, #10
        svc     #0x10

        ldr     r2, =0x11111111
        ldr     r4, =0x44444444
        ldr     r6, =0x66666666
        stmia   r1!, {r2, r4, r6}       @ STM: stores 3 words, r1 += 12
        ldr     r0, =0x20000100
        ldmia   r0!, {r2, r3, r4}       @ LDM, base not in list: r0 += 12
        svc     #0x13                   @ 2000010c
        movs    r0, #10
        svc     #0x10
        ldr     r0, =0x20000100
        ldmia   r0, {r0, r1}            @ LDM, base in list: r0 = loaded word
        svc     #0x13                   @ 11111111
        movs    r0, #10
        svc     #0x10

        ldr     r0, =done               @ address of done, bit 0 set
        push    {r0}
        pop     {pc}                    @ POP {pc}: a branch to done
        movs    r0, #1                  @ skipped
        svc     #0x00

        .thumb_func
done:
        movs    r0, #0
        svc     #0x00                   @ EXIT 0
