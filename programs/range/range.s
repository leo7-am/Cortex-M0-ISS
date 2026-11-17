@ main(void) is not used: entry is _start.
@
@ CP1 program (Member B): HW3 problem 1 rewritten in Thumb.
@ Reads the first value, the last value and a step, then prints how many values
@ first, first+step, ... up to last there are and their sum (3, 11, 2 -> count 5,
@ sum 35). A step of 0 or less is rejected. Same messages as the RARS version (1.s).
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
        ldr     r0, =prompt_first
        bl      read_int
        movs    r4, r0
        ldr     r0, =prompt_last
        bl      read_int
        movs    r5, r0
        ldr     r0, =prompt_step
        bl      read_int
        movs    r6, r0
        cmp     r6, #0
        ble     bad_step

        movs    r0, r4
        movs    r1, r5
        movs    r2, r6
        bl      count_sum
        movs    r4, r0
        movs    r5, r1

        ldr     r0, =msg_count
        movs    r1, r4
        bl      print_result
        ldr     r0, =msg_sum
        movs    r1, r5
        bl      print_result
        movs    r0, #0
        svc     #0x00

bad_step:
        ldr     r0, =msg_bad
        svc     #0x11
        movs    r0, #0
        svc     #0x00

@ r0 = prompt, returns the integer typed in r0
        .thumb_func
read_int:
        svc     #0x11
        svc     #0x15
        bx      lr

@ r0 = label, r1 = value, prints "label value\n"
        .thumb_func
print_result:
        svc     #0x11
        movs    r0, r1
        svc     #0x12
        movs    r0, #10
        svc     #0x10
        bx      lr

@ r0 = first, r1 = last, r2 = step, returns r0 = count, r1 = sum
        .thumb_func
count_sum:
        push    {r4, lr}
        movs    r3, #0
        movs    r4, #0
cs_loop:
        cmp     r0, r1
        bgt     cs_done
        adds    r3, #1
        adds    r4, r4, r0
        adds    r0, r0, r2
        b       cs_loop
cs_done:
        movs    r0, r3
        movs    r1, r4
        pop     {r4, pc}

        .ltorg

        .section .rodata
prompt_first:   .asciz "Enter the first value: "
prompt_last:    .asciz "Enter the last value: "
prompt_step:    .asciz "Enter the step: "
msg_count:      .asciz "Count: "
msg_sum:        .asciz "Sum: "
msg_bad:        .asciz "Step must be positive\n"
