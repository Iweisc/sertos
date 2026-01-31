section .text

extern syscall_handler_asm

global syscall_entry
syscall_entry:
    swapgs
    
    mov [gs:0x10], rsp
    mov rsp, [gs:0x08]
    
    push rcx
    push r11
    
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    mov rcx, r10
    
    sub rsp, 32
    call syscall_handler_asm
    add rsp, 32
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 8
    
    pop r11
    pop rcx
    
    mov rsp, [gs:0x10]
    
    swapgs
    
    o64 sysret
