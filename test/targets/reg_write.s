.global main

.section .data

hex_format: .asciz "%#x"

.section .text

.macro trap
    movq $62, %rax # 62 is PID for kill
    movq %r12, %rdi
    movq $5, %rsi # 5 is PID for SIGTRAP
    syscall
.endm

main:
    push    %rbp
    movq    %rsp, %rbp

    # Get PID
    movq    $39, %rax
    syscall
    movq    %rax, %r12

    trap

    # Print contents of rsi
    leaq    hex_format(%rip), %rdi
    movq    $0, %rax
    call    printf@plt
    movq    $0, %rdi
    call    fflush@plt
    trap

    popq    %rbp
    movq    $0, %rax
    ret
