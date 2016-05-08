//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    not eax
    not rax
    not dword ptr [rdi]
    not qword ptr [rdi]

    neg eax
    neg rax
    neg dword ptr [rdi]
    neg qword ptr [rdi]

    mul eax
    mul rax
    mul dword ptr [rdi]
    mul qword ptr [rdi]

    imul eax
    imul rax
    imul dword ptr [rdi]
    imul qword ptr [rdi]

    div eax
    div rax
    div dword ptr [rdi]
    div qword ptr [rdi]

    idiv eax
    idiv rax
    idiv dword ptr [rdi]
    idiv qword ptr [rdi]

    ret

