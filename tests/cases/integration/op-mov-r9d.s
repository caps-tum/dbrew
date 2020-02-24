.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    mov r10d, 0x12345678
    mov r11w, r10w
    mov r12b, r10b
    mov r9d, 1
    mov rax, r9
    add rax, r11
    add rax, r12
    ret
