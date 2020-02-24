//!driver = test-driver-gen.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // TEST instructions, Intel Vol. 2B 4-428
    test [rdi], rax
    test [rdi], r9
    test [rdi], eax
    test [rdi], r9d
    test [rdi], ax
    test [rdi], r9w
    test [rdi], al
    test [rdi], r9b

    test al, r9b
    test r10b, bl
    test cl, dl
    test sil, dil
    test eax, r9d
    test r10d, ebx
    test rax, r9
    test r10, rbx

    test al, 0x10
    test ax, 0x1000
    test eax, 0xabcdef00
    test rax, 0x0bcdef00
    test bl, 0x10
    test bx, 0x1000
    test ebx, 0xabcdef00
    test rbx, 0x0bcdef00
    test r9b, 0x10
    test r9w, 0x1000
    test r9d, 0xabcdef00
    test r9, 0x0bcdef00
    test byte ptr [rax], 0x10
    test word ptr [rax], 0x310
    test dword ptr [rax], 0x310
    test qword ptr [rax], 0x310

    test al, sil
    test sil, al
    test al, ah
    test ah, bl
    test bl, bh
    test cl, ch
    test dl, dh
    ret
