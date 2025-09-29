

section .data
    char_one     db '1'
    char_zero    db '0'
    char_space   db ' '
    char_newline db 10       ; newline character '\n'

section .text
    global print_one
    global print_zero
    global print_space
    global print_newline
    global print_numbers
    global print_table_of_numbers

print_one:
    push rax
    push rdi
    push rsi
    push rdx

    mov rax, 1
    mov rdi, 1
    mov rsi, char_one
    mov rdx, 1
    syscall

    pop rdx
    pop rsi
    pop rdi
    pop rax
    ret

print_zero:
    push rax
    push rdi
    push rsi
    push rdx

    mov rax, 1
    mov rdi, 1
    mov rsi, char_zero
    mov rdx, 1
    syscall

    pop rdx
    pop rsi
    pop rdi
    pop rax
    ret

print_space:
    push rax
    push rdi
    push rsi
    push rdx
    push rcx

    mov rax, 1
    mov rdi, 1
    mov rsi, char_space
    mov rdx, 1
    syscall

    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rax
    ret

print_newline:
    push rax
    push rdi
    push rsi
    push rdx
    push rcx

    mov rax, 1
    mov rdi, 1
    mov rsi, char_newline
    mov rdx, 1
    syscall

    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rax
    ret

print_numbers:
    push rdx
    push rax
    push rbx
    push rcx
    push rdi

    mov rdx, 64
    loop2:
        mov rax, 4
        loop:
            sub rdx, 1
            sub rax, 1
            mov rcx, rdx
            mov rbx, 1
            shl rbx, cl
            and rbx, rdi
            cmp rbx, 0
            je print0
            call print_one
            jmp continue
            print0:
                call print_zero
                
            continue:
                cmp rax, 0
                jne loop
        
        call print_space
        cmp rdx, 0
        jne loop2

        pop rdi
        pop rcx
        pop rbx
        pop rax
        pop rdx
        ret

print_table_of_numbers:
    push rdi
    push rsi
    push rdx
    push rcx
    push rax
    
    mov rax, rdi
    mov rcx, 0
    shr rdx, 6
    
    loop3:
        mov rdi, [rax + rcx * 8]
        call print_numbers
        call print_newline
        inc rcx
        cmp rcx, rdx
        jne loop3


    call print_newline

    mov rax, rsi
    mov rcx , 0
    shl rdx,1
    loop4:
        mov rdi, [rax + rcx * 8]
        call print_numbers
        call print_newline
        inc rcx
        cmp rcx, rdx
        jne loop4
    
    pop rax
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    call print_newline
    call print_newline
    ret