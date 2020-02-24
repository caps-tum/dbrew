	.text
	.globl	f1
	.type	f1, @function
f1:
	movabs $jtarget, %r14
	callq *%r14
	ret
jtarget:
	mov $0, %eax
	ret
