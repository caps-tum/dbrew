// test for segment-overwrite prefix (fs:)
// this is gcc-generated assembly of
//
//  __thread int tls_i32 = 1;
//  __thread long long tls_i64 = 1;
//
//  int f1(int i)
//  {
//  	return tls_i32 + tls_i64 + i;
//  }

    .text
    .globl  f1
    .type   f1, @function
f1:
    push    %rbp
    mov     %rsp,%rbp
    mov     %edi,-0x4(%rbp)
    mov     %fs:-0x8,%rax
    mov     %eax,%edx
    mov     %fs:-0x10,%eax
    add     %eax,%edx
    mov     -0x4(%rbp),%eax
    add     %edx,%eax
    pop     %rbp
    ret
