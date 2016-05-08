//!run = {outfile} --var 0 1
    .text
    .globl  f1
    .type   f1, @function
f1:
    test %rdi, %rdi
    jz 1f
    xor %eax, %eax
    ret
1:
    mov $1, %eax
    ret
