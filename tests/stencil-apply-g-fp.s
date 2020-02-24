	.text
	.globl	f1
	.type	f1, @function
f1:
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, -24(%rbp)
	movl	%esi, -28(%rbp)
	movq	%rdx, -40(%rbp)
	pxor	%xmm0, %xmm0
	movsd	%xmm0, -16(%rbp)
.L5:
	movq	-40(%rbp), %rax
	movsd	8(%rax), %xmm0
	movsd	%xmm0, -8(%rbp)
	pxor	%xmm0, %xmm0
	ucomisd	-8(%rbp), %xmm0
	jp	.L2
	pxor	%xmm0, %xmm0
	ucomisd	-8(%rbp), %xmm0
	je	.L7
.L2:
	movq	-40(%rbp), %rax
	movl	(%rax), %edx
	movq	-40(%rbp), %rax
	movl	4(%rax), %eax
	imull	-28(%rbp), %eax
	addl	%edx, %eax
	cltq
	leaq	0(,%rax,8), %rdx
	movq	-24(%rbp), %rax
	addq	%rdx, %rax
	movsd	(%rax), %xmm0
	mulsd	-8(%rbp), %xmm0
	movsd	-16(%rbp), %xmm1
	addsd	%xmm1, %xmm0
	movsd	%xmm0, -16(%rbp)
	addq	$16, -40(%rbp)
	jmp	.L5
.L7:
	movsd	-16(%rbp), %xmm0
	popq	%rbp
	ret

