//!args=--nobytes
    .intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    mov rbx, 0x1234567
    mov [wdata], rbx
    // rip-relative addressing, AT&T: lea wdata(%rip),%rdx
    lea rdx,[rip+wdata]
    mov qword ptr [rdx],1
    // TODO: add test with movabs + 64bit immediate
    // uncomment if accessing read-only mem range is handled
    // mov rax, [rdata+8]
    mov rax, [wdata]
    ret

