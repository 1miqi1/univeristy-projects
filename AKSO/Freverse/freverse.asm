
global _start

;-----Stałe symboliczne----
SYS_OPEN equ 2
FILENAME_OFFEST equ 16
O_RDWR equ 2
STAT_SIZE equ 144
SYS_FSTAT equ 5
ST_SIZE_OFFSET equ 48
FILE_OP_ERROR equ 0
PROT_READ_WRITE equ 3
MAP_SHARED equ 1
SYS_MAP equ 9
MS_SYNC equ 4
SYS_MSYNC equ 26
SYS_MUNMAP equ 11
SYS_CLOSE equ 3
EXIT_ERROR equ 1
SYS_EXIT equ 60


section .data
err_code:   dq 1

section .bss
fd:         resq 1      ; identyfikator/deksryptor pliku
size:       resq 1      ; rozmiar pliku
map_addr:   resq 1      ; adres mapy

section .text

_start:
    ; argc w [rsp] : sprawdzamy ilośc podanych argumentów
    mov rdi, [rsp]
    cmp rdi, 2
    jne exit_err

    ; open(argv[1], O_RDWR) : otwieramy plik 
    mov rax, SYS_OPEN                 
    mov rdi, [rsp + FILENAME_OFFEST]        
    mov rsi, O_RDWR                 
    syscall
    cmp rax, 0
    jl exit_err                 ; obsługa błędu wywołania systemowego
    mov [fd], rax

    ; fstat(fd, buf) : tworzymy obiekt struktury stat fstat(fd, buf)
    sub rsp, STAT_SIZE          
    mov rdi, rax                
    mov rsi, rsp                
    mov rax, SYS_FSTAT          
    syscall
    cmp rax, 0
    js close_and_exit_err       ; obsługa błędu wywołania systemowego

    ; size = st_size (offset 48)
    mov rax, [rsp + ST_SIZE_OFFSET]
    mov [size], rax

    ; przypadek graniczny size < 2
    cmp rax, 2
    jb cleanup_and_exit_ok

    ; mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : mapujemy plik do przestrzeni adresowej
    mov rdi, 0
    mov rsi, [size]
    mov rdx, PROT_READ_WRITE          
    mov r10, MAP_SHARED          
    mov r8, [fd]
    mov r9, 0
    mov rax, SYS_MAP          
    syscall
    cmp rax, 0              
    js cleanup_and_exit_err     ; obsługa błędu wywołania systemowego
    mov [map_addr], rax

    ; rsi = początek pliku , rdi = koniec pliku 
    mov rsi, rax
    mov rcx, [size]
    dec rcx
    lea rdi, [rsi + rcx]

; pętla zamieniająca (i)-ty bit z (size-i)-tym bitem
reverse_loop:
    cmp rsi, rdi
    jge reverse_done

    mov al, [rsi]
    mov bl, [rdi]
    mov [rsi], bl
    mov [rdi], al

    inc rsi
    dec rdi
    jmp reverse_loop

; synchronizujemy zmiany i odmapowujuemy pamięc
reverse_done:
    ; msync(addr, size, MS_SYNC)
    mov rdi, [map_addr]
    mov rsi, [size]
    mov rdx, MS_SYNC
    mov rax, SYS_MSYNC
    syscall

    ; munmap(addr, size)
    mov rdi, [map_addr]
    mov rsi, [size]
    mov rax, SYS_MUNMAP
    syscall

; zwalniamy przyporządkowane miejsce na stosie i zamykamy plik
cleanup_and_exit_ok:
    add rsp, STAT_SIZE
    mov rdi, [fd]
    mov rax, SYS_CLOSE      
    syscall
    mov rdi, 0
    jmp exit

; zwalniamy przyporządkowane miejsce na stosie
cleanup_and_exit_err:
    add rsp, STAT_SIZE
; zamykamy plik
close_and_exit_err:
    mov rdi, [fd]
    mov rax, SYS_CLOSE
    syscall
; kończymy program z kodem błędu = 1
exit_err:
    mov rdi, EXIT_ERROR
; kończymy program 
exit:
    mov rax, SYS_EXIT
    syscall
