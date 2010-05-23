.globl mcount
mcount:
        subq    $56,%rsp
        movq    %rax,(%rsp)
        movq    %rcx,8(%rsp)
        movq    %rdx,16(%rsp)
        movq    %rsi,24(%rsp)
        movq    %rdi,32(%rsp)
        movq    %r8,40(%rsp)
        movq    %r9,48(%rsp)

        movq $1, %rdi
        movq 56(%esp), %rsi
        call add_trace@PLT

        movq    48(%rsp),%r9
        movq    40(%rsp),%r8
        movq    32(%rsp),%rdi
        movq    24(%rsp),%rsi
        movq    16(%rsp),%rdx
        movq    8(%rsp),%rcx
        movq    (%rsp),%rax
        addq    $56,%rsp
        ret
