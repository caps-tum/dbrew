//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // MOV instructions, Intel Vol. 2A 3-530
    mov [rsi], dl
    mov [rsi], ah
    mov [rsi], r10b
    mov [rsi], dx
    mov [rsi], r10w
    mov [rsi], edx
    mov [rsi], r10d
    mov [rsi], rdx
    mov [rsi], r10
    mov dl, [rsi]
    mov ah, [rsi]
    mov r10b, [rsi]
    mov dx, [rsi]
    mov r10w, [rsi]
    mov edx, [rsi]
    mov r10d, [rsi]
    mov rdx, [rsi]
    mov r10, [rsi]

    mov dl, 0x10
    mov ah, 0x10
    mov r10b, 0x10
    mov dx, 0x1000
    mov r10w, 0x1000
    mov edx, 0x10000000
    mov r10d, 0x10000000
    movabs rdx, 0x1000000000000000
    movabs r10, 0x1000000000000000
    mov byte ptr [rdi], 0x10
    mov byte ptr [r10], 0x10
    mov word ptr [rdi], 0x1000
    mov word ptr [r10], 0x1000
    mov dword ptr [rdi], 0x10000000
    mov dword ptr [r10], 0x10000000
    mov qword ptr [rdi], 0x10000000
    mov qword ptr [r10], 0x10000000

    ret
