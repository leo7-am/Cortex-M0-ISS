@ main(void) is not used: entry is _start.
@
@ CP1 program (Member A): HW3 problem 3 rewritten in Thumb.
@ Reads a dividend and a divisor, rejects negative numbers and a zero divisor,
@ then prints the quotient and remainder computed by shift-and-subtract
@ (long division). Same messages and checks as the RARS version (3.s).

        .syntax unified
        .cpu    cortex-m0
        .thumb

        .section .vectors, "a"
        .word   0x20040000              @ initial sp: top of RAM
        .word   _start                  @ reset vector

        .text
        .thumb_func
        .global _start
_start:
        ldr     r0, =msg_dividend
        svc     #0x11                   @ PUTS     (print_str)
        svc     #0x15                   @ GETINT   (read_int) -> r0
        movs    r4, r0                  @ r4 = dividend   (s0 in RARS)

        ldr     r0, =msg_divisor
        svc     #0x11
        svc     #0x15
        movs    r5, r0                  @ r5 = divisor    (s1 in RARS)

        cmp     r4, #0
        blt     negative                @ dividend < 0
        cmp     r5, #0
        blt     negative                @ divisor < 0
        beq     zero                    @ divisor == 0 (same flags as the cmp above)

        movs    r0, r4
        movs    r1, r5
        bl      udiv                    @ r0 = quotient, r1 = remainder
        movs    r4, r0                  @ r4 = quotient   (s2 in RARS)
        movs    r5, r1                  @ r5 = remainder  (s3 in RARS)

        ldr     r0, =msg_quotient
        svc     #0x11
        movs    r0, r4
        svc     #0x12                   @ PUTINT   (print_int)
        ldr     r0, =msg_newline
        svc     #0x11

        ldr     r0, =msg_remainder
        svc     #0x11
        movs    r0, r5
        svc     #0x12
        ldr     r0, =msg_newline
        svc     #0x11
        b       exit

negative:
        ldr     r0, =msg_negative
        svc     #0x11
        b       exit

zero:
        ldr     r0, =msg_zero
        svc     #0x11

exit:
        movs    r0, #0
        svc     #0x00                   @ EXIT 0

@ udiv: unsigned division by shift-and-subtract.
@   in:  r0 = dividend, r1 = divisor (both checked non-negative, divisor != 0)
@   out: r0 = quotient, r1 = remainder
@ For each dividend bit from 31 down to 0: shift the remainder left and bring
@ that bit in; if remainder >= divisor, subtract and set that quotient bit.
@ RARS pulls the bit out with srl + andi. Thumb shifts it out of the top of
@ r0 into the Carry flag (lsls) and adds it into the remainder (adcs).
        .thumb_func
udiv:
        push    {r4, lr}
        movs    r2, #0                  @ r2 = quotient   (t0 in RARS)
        movs    r3, #0                  @ r3 = remainder  (t1 in RARS)
        movs    r4, #32                 @ r4 = bits left
udiv_loop:
        lsls    r2, r2, #1              @ quotient <<= 1, making room for this bit
        lsls    r0, r0, #1              @ C = next dividend bit (bit 31 first)
        adcs    r3, r3                  @ remainder = (remainder << 1) | C
        cmp     r3, r1
        bcc     udiv_next               @ remainder < divisor (unsigned): skip
        subs    r3, r3, r1              @ remainder -= divisor
        adds    r2, #1                  @ set this quotient bit
udiv_next:
        subs    r4, #1
        bne     udiv_loop
        movs    r0, r2
        movs    r1, r3
        pop     {r4, pc}

        .section .rodata
msg_dividend:   .asciz  "Enter the dividend: "
msg_divisor:    .asciz  "Enter the divisor: "
msg_negative:   .asciz  "Error: enter non-negative numbers only.\n"
msg_zero:       .asciz  "Error: the divisor cannot be 0.\n"
msg_quotient:   .asciz  "Quotient = "
msg_remainder:  .asciz  "Remainder = "
msg_newline:    .asciz  "\n"
