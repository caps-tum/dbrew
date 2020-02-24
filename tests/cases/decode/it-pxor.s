//!driver = test-driver-decode.c
.intel_syntax noprefix
	.text
	.globl	f1
	.type	f1, @function
f1:
	pxor	mm1, mm2
	pxor	mm1, [rdi]
	pxor	mm1, [r9]

	pxor	xmm3, xmm4
	pxor	xmm3, xmm9
	pxor	xmm3, [rdi]
	pxor	xmm9, [rdi]
	pxor	xmm3, [r9]
	pxor	xmm9, [r9]

	ret
