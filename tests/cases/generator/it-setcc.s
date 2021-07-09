//!driver = test-driver-gen.c
.intel_syntax noprefix
        .text
        .global f1
        .type f1,@function
f1:
        seta al
        setae al
        setb al
        setbe al
        sete al
        setg al
        setge al
        setl al
        setle al
        setne al
        setno al
        setnp al
        setns al
        seto al
        setp al
        sets al

        seta [rax]
        setae [rax]
        setb [rax]
        setbe [rax]
        sete [rax]
        setg [rax]
        setge [rax]
        setl [rax]
        setle [rax]
        setne [rax]
        setno [rax]
        setnp [rax]
        setns [rax]
        seto [rax]
        setp [rax]
        sets [rax]

        ret
