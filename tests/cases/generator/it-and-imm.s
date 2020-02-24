//!driver = test-driver-gen.c
.intel_syntax noprefix
	.text
	.globl	f1
	.type	f1, @function
f1:
        and al,  4
        and ax,  4
        and ax,  400
        and eax, 4
        and eax, 400
        and eax, 400000
        and rax, 4
        and rax, 400
        and rax, 400000

	and al,  -4
	and ax,  -4
	and ax,  -400
	and eax, -4
	and eax, -400
	and eax, -400000
	and rax, -4
	and rax, -400
	and rax, -400000
	ret
