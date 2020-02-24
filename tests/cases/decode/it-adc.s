//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // ADC instructions, Intel Vol. 2A 3-24
    adc [rdi], rax
    adc [rdi], r9
    adc [rdi], eax
    adc [rdi], r9d
    adc [rdi], ax
    adc [rdi], r9w
    adc [rdi], al
    adc [rdi], r9b
    adc rax, [rdi]
    adc r9, [rdi]
    adc eax, [rdi]
    adc r9d, [rdi]
    adc ax, [rdi]
    adc r9w, [rdi]
    adc al, [rdi]
    adc r9b, [rdi]
    adc al, 0x10
    adc ax, 0x1000
    adc eax, 0xabcdef00
    adc rax, 0x0bcdef00
    adc byte ptr [rax], 0x10
    adc word ptr [rax], 0x310
    adc dword ptr [rax], 0x310
    adc qword ptr [rax], 0x310

    ret
