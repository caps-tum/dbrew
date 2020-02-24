	.text
	.globl	f1
	.type	f1, @function
f1:
	push %rbp
	mov %rsp, %rbp
	mov %rdi, %rax
	leave
	ret
