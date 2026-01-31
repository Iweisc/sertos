section .text

global context_switch
context_switch:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    
    mov [rcx], r15
    mov [rcx + 8], r14
    mov [rcx + 16], r13
    mov [rcx + 24], r12
    mov [rcx + 32], r11
    mov [rcx + 40], r10
    mov [rcx + 48], r9
    mov [rcx + 56], r8
    mov [rcx + 64], rbp
    mov [rcx + 72], rdi
    mov [rcx + 80], rsi
    mov [rcx + 88], rdx
    mov [rcx + 96], rcx
    mov [rcx + 104], rbx
    mov [rcx + 112], rax
    
    lea rax, [rel .return]
    mov [rcx + 120], rax
    
    mov rax, cs
    mov [rcx + 128], rax
    
    pushfq
    pop rax
    mov [rcx + 136], rax
    
    mov [rcx + 144], rsp
    
    mov rax, ss
    mov [rcx + 152], rax
    
    mov r15, [rdx]
    mov r14, [rdx + 8]
    mov r13, [rdx + 16]
    mov r12, [rdx + 24]
    mov r11, [rdx + 32]
    mov r10, [rdx + 40]
    mov r9, [rdx + 48]
    mov r8, [rdx + 56]
    mov rbp, [rdx + 64]
    mov rdi, [rdx + 72]
    mov rsi, [rdx + 80]
    mov rbx, [rdx + 104]
    mov rax, [rdx + 112]
    
    mov rsp, [rdx + 144]
    
    push qword [rdx + 152]
    push qword [rdx + 144]
    push qword [rdx + 136]
    push qword [rdx + 128]
    push qword [rdx + 120]
    
    mov rcx, [rdx + 96]
    mov rdx, [rdx + 88]
    
    iretq

.return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

global enter_usermode
enter_usermode:
    mov rsp, rsi
    
    push 0x23
    push rsi
    push rdx
    push 0x1B
    push rdi
    
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    iretq
