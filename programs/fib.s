@ main(void) is not used: entry is _start.
@
@ CP1 program (Member C: Farah Habib): HW3 problem 5(a) rewritten in Thumb
@ Reads n (0 to 46), computes fib(n) with the recursive function
@     int fib(unsigned int n) { if (n < 2) return n; else return fib(n - 1) + fib(n - 2); }
@ and prints "fib(n) = result". Same prompts and messages as the RARS version.
@ Each comment shows the RISC-V instruction(s) from the RARS program that the line replaces.
@ RARS ecall (service in a7) -> Thumb svc (service number inside the instruction):
@   print string (4) -> svc #0x11, print int (1) -> svc #0x12, read int (5) -> svc #0x15,
@   exit (10) -> svc #0x00

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
read_n:
        ldr     r0, =p_n                @ la   a0, p_n
        svc     #0x11                   @ li a7, 4 / ecall      (PUTS = print string)
        svc     #0x15                   @ li a7, 5 / ecall      (GETINT = read int) -> r0
        cmp     r0, #0                  @ bltz a0, bad_n
        blt     bad_n                   @   (negative input is not a valid n)
        cmp     r0, #46                 @ li   t0, 46
        bgt     bad_n                   @ bgt  a0, t0, bad_n
        movs    r4, r0                  @ mv   s0, a0           (r4 = n, plays s0)

        movs    r0, r4                  @ mv   a0, s0
        bl      fib                     @ call fib              (r0 = fib(n))
        movs    r5, r0                  @ mv   s1, a0           (r5 = result, plays s1)

        ldr     r0, =m_res1             @ la   a0, m_res1
        svc     #0x11                   @ li a7, 4 / ecall      (print "fib(")
        movs    r0, r4                  @ mv   a0, s0
        svc     #0x12                   @ li a7, 1 / ecall      (PUTINT = print int: n)
        ldr     r0, =m_res2             @ la   a0, m_res2
        svc     #0x11                   @ li a7, 4 / ecall      (print ") = ")
        movs    r0, r5                  @ mv   a0, s1
        svc     #0x12                   @ li a7, 1 / ecall      (print fib(n))
        ldr     r0, =nl                 @ la   a0, nl
        svc     #0x11                   @ li a7, 4 / ecall      (print "\n")

        movs    r0, #0                  @ (exit status 0; RARS exit has none)
        svc     #0x00                   @ li a7, 10 / ecall     (EXIT)

bad_n:
        ldr     r0, =e_n                @ la   a0, e_n
        svc     #0x11                   @ li a7, 4 / ecall
        b       read_n                  @ j    read_n

@ fib(r0 = n, unsigned) -> r0 = fib(n)
@ Stack frame (12 bytes, same as RARS): lr, r4 = n, r5 = fib(n - 1).
@ RARS saves ra/s0/s1 with three sw; Thumb does it with one push and
@ restores them and returns with one pop (popping the saved lr into pc).
        .thumb_func
fib:
        cmp     r0, #2                  @ li   t0, 2
        bcc     fib_base                @ bltu a0, t0, fib_base (unsigned n < 2: return n)

        push    {r4, r5, lr}            @ addi sp, sp, -12 / sw ra, 8(sp) / sw s0, 4(sp) / sw s1, 0(sp)

        movs    r4, r0                  @ mv   s0, a0           (n must survive both calls)
        subs    r0, r4, #1              @ addi a0, s0, -1
        bl      fib                     @ call fib              (r0 = fib(n - 1))
        movs    r5, r0                  @ mv   s1, a0
        subs    r0, r4, #2              @ addi a0, s0, -2
        bl      fib                     @ call fib              (r0 = fib(n - 2))
        adds    r0, r0, r5              @ add  a0, a0, s1       (fib(n - 1) + fib(n - 2))

        pop     {r4, r5, pc}            @ lw s1 / lw s0 / lw ra / addi sp, sp, 12 / ret
fib_base:
        bx      lr                      @ ret                   (r0 already holds n)

        .section .rodata
p_n:    .asciz  "Enter n (0 to 46): "
e_n:    .asciz  "n must be between 0 and 46 (fib(47) does not fit in an int). Try again.\n"
m_res1: .asciz  "fib("
m_res2: .asciz  ") = "
nl:     .asciz  "\n"
