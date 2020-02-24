//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // MUL instructions, Intel Vol. 2A 3-614
    mul al
    mul r9b
    mul ax
    mul eax
    mul rax

    // IMUL instructions, Intel Vol. 2A 3-415
    imul al
    imul r9b
    imul ax
    imul eax
    imul rax
    imul rax, rax
    imul eax, eax
    imul ax, ax
    imul rax, rax, 0x100
    imul eax, eax, 0x100
    imul ax, ax, 0x100
    imul rax, rax, 0x10
    imul eax, eax, 0x10
    imul ax, ax, 0x10

    // DIV instructions, Intel Vol. 2A 3-260
    div al
    div r9b
    div ax
    div eax
    div rax

    // DIV instructions, Intel Vol. 2A 3-412
    idiv al
    idiv r9b
    idiv ax
    idiv eax
    idiv rax

    ret
