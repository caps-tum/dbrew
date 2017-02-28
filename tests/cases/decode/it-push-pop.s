//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // PUSH instructions, Intel Vol. 2B 4-272
    push word ptr [rdx]
    push qword ptr [rdx]
    push dx
    push rdx
    // imm8/16/32, extended to 64bit pushed
    push 0x10
    push 0x1000
    push 0x10000000
    // imm8/16, 16bits pushed
    push word ptr 0x10
    push word ptr 0x1000

    // rex.W push $0x78563412: push imm32 extended to 64bit, rex.W ignored
    // .byte 0x48, 0x68, 0x12, 0x34, 0x56, 0x78

    // push fs
    // push gs

    // POP instructions, Intel Vol. 2B 4-183
    pop word ptr [rdx]
    pop qword ptr [rdx]
    pop dx
    pop rdx

    pushfw
    popfw
    pushfq
    popfq

    ret
