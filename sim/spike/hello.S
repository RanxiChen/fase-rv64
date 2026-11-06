    .section .rodata
message:
    .ascii "FASE_SPIKE_HOST_HELLO\n"
message_end:
    .equ MESSAGE_LENGTH, message_end-message
    .section .text
    .globl _start
_start:
    li a0, 1
    lla a1, message
    li a2, MESSAGE_LENGTH
    li a7, 64
    ecall
    li t0, MESSAGE_LENGTH
    bne a0, t0, failed
    li a0, 0
    li a7, 93
    ecall
failed:
    ebreak
