//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // SHL/SAL instructions, Intel Vol. 2B 4-342
    shl al, 1
    shl r10b, 1
    shl byte ptr [rax], 1
    shl al, cl
    shl r10b, cl
    shl byte ptr [rax], cl
    shl al, 5
    shl r10b, 5
    shl byte ptr [rax], 5
    shl ax, 1
    shl r10w, 1
    shl word ptr [rax], 1
    shl ax, cl
    shl r10w, cl
    shl word ptr [rax], cl
    shl ax, 5
    shl r10w, 5
    shl word ptr [rax], 5
    shl eax, 1
    shl r10d, 1
    shl dword ptr [rax], 1
    shl eax, cl
    shl r10d, cl
    shl dword ptr [rax], cl
    shl eax, 5
    shl r10d, 5
    shl dword ptr [rax], 5
    shl rax, 1
    shl r10, 1
    shl qword ptr [rax], 1
    shl rax, cl
    shl r10, cl
    shl qword ptr [rax], cl
    shl rax, 5
    shl r10, 5
    shl qword ptr [rax], 5

    // SHR instructions, Intel Vol. 2B 4-342
    shr al, 1
    shr r10b, 1
    shr byte ptr [rax], 1
    shr al, cl
    shr r10b, cl
    shr byte ptr [rax], cl
    shr al, 5
    shr r10b, 5
    shr byte ptr [rax], 5
    shr ax, 1
    shr r10w, 1
    shr word ptr [rax], 1
    shr ax, cl
    shr r10w, cl
    shr word ptr [rax], cl
    shr ax, 5
    shr r10w, 5
    shr word ptr [rax], 5
    shr eax, 1
    shr r10d, 1
    shr dword ptr [rax], 1
    shr eax, cl
    shr r10d, cl
    shr dword ptr [rax], cl
    shr eax, 5
    shr r10d, 5
    shr dword ptr [rax], 5
    shr rax, 1
    shr r10, 1
    shr qword ptr [rax], 1
    shr rax, cl
    shr r10, cl
    shr qword ptr [rax], cl
    shr rax, 5
    shr r10, 5
    shr qword ptr [rax], 5

    // SAR instructions, Intel Vol. 2B 4-342
    sar al, 1
    sar r10b, 1
    sar byte ptr [rax], 1
    sar al, cl
    sar r10b, cl
    sar byte ptr [rax], cl
    sar al, 5
    sar r10b, 5
    sar byte ptr [rax], 5
    sar ax, 1
    sar r10w, 1
    sar word ptr [rax], 1
    sar ax, cl
    sar r10w, cl
    sar word ptr [rax], cl
    sar ax, 5
    sar r10w, 5
    sar word ptr [rax], 5
    sar eax, 1
    sar r10d, 1
    sar dword ptr [rax], 1
    sar eax, cl
    sar r10d, cl
    sar dword ptr [rax], cl
    sar eax, 5
    sar r10d, 5
    sar dword ptr [rax], 5
    sar rax, 1
    sar r10, 1
    sar qword ptr [rax], 1
    sar rax, cl
    sar r10, cl
    sar qword ptr [rax], cl
    sar rax, 5
    sar r10, 5
    sar qword ptr [rax], 5

    ret
