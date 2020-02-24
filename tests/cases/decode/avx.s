//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    vaddss xmm2, xmm0, xmm1
    vaddsd xmm2, xmm0, xmm1
    vaddps xmm2, xmm0, xmm1
    vaddpd xmm2, xmm0, xmm1
    vaddss xmm2, xmm0, [rax]
    vaddsd xmm2, xmm0, [rax]
    vaddps xmm2, xmm0, [rax]
    vaddpd xmm2, xmm0, [rax]

    vaddps ymm2, ymm0, ymm1
    vaddpd ymm2, ymm0, ymm1
    vaddps ymm2, ymm0, [rax]
    vaddpd ymm2, ymm0, [rax]

    vmulss xmm2, xmm0, xmm1
    vmulsd xmm2, xmm0, xmm1
    vmulps xmm2, xmm0, xmm1
    vmulpd xmm2, xmm0, xmm1
    vmulss xmm2, xmm0, [rax]
    vmulsd xmm2, xmm0, [rax]
    vmulps xmm2, xmm0, [rax]
    vmulpd xmm2, xmm0, [rax]

    vmulps ymm2, ymm0, ymm1
    vmulpd ymm2, ymm0, ymm1
    vmulps ymm2, ymm0, [rax]
    vmulpd ymm2, ymm0, [rax]

    vxorps xmm2, xmm0, xmm1
    vxorpd xmm2, xmm0, xmm1
    vxorps xmm2, xmm0, [rax]
    vxorpd xmm2, xmm0, [rax]
    vxorps ymm2, ymm0, ymm1
    vxorpd ymm2, ymm0, ymm1
    vxorps ymm2, ymm0, [rax]
    vxorpd ymm2, ymm0, [rax]

    vmovss  xmm0, [rax]
    vmovsd  xmm0, [rax]
    vmovaps xmm0, [rax]
    vmovapd xmm0, [rax]
    vmovups xmm0, [rax]
    vmovupd xmm0, [rax]
    vmovdqu xmm0, [rax]
    vmovdqa xmm0, [rax]
    vmovss  [rax], xmm0
    vmovsd  [rax], xmm0
    vmovaps [rax], xmm0
    vmovapd [rax], xmm0
    vmovups [rax], xmm0
    vmovupd [rax], xmm0
    vmovdqu [rax], xmm0
    vmovdqa [rax], xmm0

    vmovaps ymm0, [rax]
    vmovapd ymm0, [rax]
    vmovups ymm0, [rax]
    vmovupd ymm0, [rax]
    vmovdqu ymm0, [rax]
    vmovdqa ymm0, [rax]
    vmovaps [rax], ymm0
    vmovapd [rax], ymm0
    vmovups [rax], ymm0
    vmovupd [rax], ymm0
    vmovdqu [rax], ymm0
    vmovdqa [rax], ymm0

    vmovntdq [rax], xmm0
    vmovntdq [rax], ymm0

    ret
