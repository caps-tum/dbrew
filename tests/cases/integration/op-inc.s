    .text
	.globl	f1
	.type	f1, @function
f1:
	mov %edi, %eax
	inc %eax
	dec %eax
	ret
