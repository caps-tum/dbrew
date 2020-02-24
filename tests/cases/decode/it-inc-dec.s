//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // INC instructions, Intel Vol. 2A 3-422
    inc byte ptr [rdx]
    inc al
    inc r11b
    inc word ptr [rdx]
    inc ax
    inc r11w
    inc dword ptr [rdx]
    inc eax
    inc r11d
    inc qword ptr [rdx]
    inc rax
    inc r11

    // DEC instructions, Intel Vol. 2A 3-258
    dec byte ptr [rdx]
    dec al
    dec r11b
    dec word ptr [rdx]
    dec ax
    dec r11w
    dec dword ptr [rdx]
    dec eax
    dec r11d
    dec qword ptr [rdx]
    dec rax
    dec r11

    ret
