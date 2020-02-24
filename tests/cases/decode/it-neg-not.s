//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // NEG instructions, Intel Vol. 2B 4-7
    neg byte ptr [rdx]
    neg al
    neg r11b
    neg word ptr [rdx]
    neg ax
    neg r11w
    neg dword ptr [rdx]
    neg eax
    neg r11d
    neg qword ptr [rdx]
    neg rax
    neg r11

    // NOT instructions, Intel Vol. 2B 4-10
    not byte ptr [rdx]
    not al
    not r11b
    not word ptr [rdx]
    not ax
    not r11w
    not dword ptr [rdx]
    not eax
    not r11d
    not qword ptr [rdx]
    not rax
    not r11

    ret
