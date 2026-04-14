;-----Stałe symboliczne----
BIT_MASK      equ 63           ; Stała używana do ustalania x%63
SHIFT         equ 6            ; Stałą używana do przesunięć bitowych

; ------------------------------------------------
; Makro zmieniające bit w tablicy Q o indeksie rcx
; ------------------------------------------------
%macro CHANGE_BIT 0
    mov     r11, rcx            
    mov     r10, rcx            
    and     r10, BIT_MASK       ; r10 = rcx % 64 (indeks bitu w słowie)
    shr     r11, SHIFT          ; r11 = rcx / 64 (indeks słowa w tablicy Q_{j-1})
    btc     [rdi + r11*8], r10  ; zmiana bitu r10 w słowie Q_{j-1}[(rcx)/64]
%endmacro

; ----------------------------------------------------
; Makro obliczające wartość T_{j-1}[i]
; Zakładamy, że:
; - rcx = n - j + 1 
; - r9  = (n - j + 1)/64
; Użycie innych rejestrów:
; - r10 - wartość górnego bloku Q_{j-1} / całego bloku T_{j-1}
; - r11 - wartość dolnego bloku Q_{j-1}
; ----------------------------------------------------
%macro FIND_VALUE 0
    inc     rcx                 
    xor     r10, r10            
    xor     r11, r11            

    ; Obliczanie górnej części T_{j-1}[i]
    cmp     r8, r9              ; sprawdzamy czy i < (n-j+1)/64
    jb      %%calculation       ; T_{j-1}[i] nie zachacza o Q

    mov     r11, r8             
    sub     r11, r9             
    cmp     r11, rdx            ; czy i - (n-j+1)/64 >= n/64?
    jae     %%lower             ; T_{j-1}[i] nie zachacza z góry o Q

    mov     r10, [rdi + 8*r11]  ; T_{j-1}[i] zachacza o Q[m] : r10 = Q[m] = Q[i - (n-j+1)/64]

    ; Obliczanie dolnej części T_{j-1}[i]
%%lower:
    cmp     r11, 1              ; czy m < 1?
    jb      %%zero              ; T_{j-1}[i] nie zachacza z dołu o Q

    dec     r11                 
    cmp     r11, rdx            ; czy m-1 >= n/64?
    jae     %%zero              ; T_{j-1}[i] nie zachacza o Q

    mov     r11, [rdi + r11*8]  ; r11 = Q[m-1]

    jmp     %%calculation       ; przechodzimy do łączenia bitów

%%zero:
    xor     r11, r11            

%%calculation:
    mov     r9, rcx             
    and     cl, BIT_MASK        ; reszta z n-j+1 % 64
    shld    r10, r11, cl        ; r10 = (Q[m] << n-j+1%64) | (Q[m-1] >> (64-(n-j+1) % 64))

    ; Przywracamy r9,rcx to stanu "wejściowego"
    mov     rcx, r9             
    shr     r9, SHIFT           
    dec     rcx

    ; Obsługa przypadku granicznego
    cmp     rcx, 0              ; czy n=j?
    jne     %%end          
    cmp     r8, 0               ; czy i == 0?
    jne     %%end         
    add     r10, 1              ; dodajemy 4^0 do T_{j-1}[0], bo nie dodaliśmy przy ustawianiu T na początku funkcji

%%end:    
%endmacro

section .text
    global nsqrt

; --------------------------------------------
; Główna funkcja: nsqrt
; Rejestry:
; rdi = Q, rsi = X, rdx = n ( w kodzie rdx = n/64 - liczba bloków)
; rcx = iterator (n-j), rax = CF
; r8  = iterator pętli wewnętrznych
; r9, r10, r11 = rejestry pomocnicze
; -----------------------------------------
nsqrt:
    mov     rcx, rdx            ; rcx = n, bo zaczynamy iteracje od n-1
    shr     rdx, SHIFT          ; rdx = n/64

    ; Czyszczenie tablicy Q 
    xor     rax, rax            ; iterator pętli
    xor     r8, r8              ; indeks bloku

.clear_loop:
    mov     [rdi + r8*8], rax   ; Q[r8] = 0
    inc     r8                   
    cmp     r8, rdx             
    jb      .clear_loop        


    ; --------------------------------------------
    ; Obliczamy R_j, Q_j
    ; --------------------------------------------
.main_loop:                     
    dec     rcx                 

    cmp     rcx, 0              ; patrzymy, czy nie natrafiliśmy na przypadek graniczny
    je      .Init               

    dec     rcx                 
    CHANGE_BIT                  ; ustaw bit w Q (dodaj 2^{n-j-1}), bo T=2^{n-j+1}(Q+2^{n-j-1})
    inc     rcx                 

    ; Inicjalizujemy zmienne w pętli
.Init:
    mov     r9, rcx             
    inc     r9                  
    shr     r9, SHIFT           ; r9 = (n-j+1)/64

    mov     r8, rdx             
    shl     r8, 1               ; r8 = (2n/64)

    ; --------------------------------------------
    ; Sprawdzamy R_{j-1} ? T_{j-1}
    ; --------------------------------------------
.compare_loop:
    dec     r8                  
    FIND_VALUE                  ; obliczamy T_{j-1}[i]

.comp_end:
    cmp     r10, [rsi + 8*r8]   ; porównujemy T_{j-1}[i] z R_{j-1}[i]
    jb      .R_greater          
    ja      .Back_T             
    cmp     r8, r9        
    ja      .compare_loop       ; jesli i = (n-j+1)/64, to oznacza, ze T_{j-1} <= R_{j-1}, bo wcześniejsze bloki T_{j-1} są zerami 

    ; --------------------------------------------
    ; Przypadek: R_{j-1} >= T_{j-1}
    ; Odejmujemy T_{j-1} od R_{j-1}  
    ; --------------------------------------------
.R_greater:
    mov     r8, r9              ; zaczynamy odejmowanie od (n-j+1)/64, bo wcześniejsze bity T_{j-1} są zerami
    xor     rax, rax            ; CF = 0, al przechowuje wartość flagi CF po odjęciu kolejnych bloków R_{j-1}, T_{j-1}

.sub_loop:
    FIND_VALUE                  ; obliczamy T[i]

.sub_end:
    cmp     al, 0               ; instrukcja by zachować flagi 
    je      .no_carry           
    stc                         ; ustawienie CF = 1
.no_carry:
    sbb     [rsi + 8*r8], r10   ; R_{j}[i]=R_{j-1}[i]-T_{j-1}[i]-CF
    setc    al                  
    inc     r8                  
    mov     r11, rdx            
    shl     r11, 1              
    cmp     r8, r11             ; czy koniec i = 2n/64 
    jb      .sub_loop           

    CHANGE_BIT                  ; ustawiamy bit q_j na 1

.Back_T:
    cmp     rcx, 0              ; patrzymy, czy nie obsługujemy przypadku granicznego
    je      .main_end           
    dec     rcx                 
    CHANGE_BIT                  ; cofamy zmianę bitu
    inc     rcx                 

.main_end:
    cmp     rcx, 0              
    ja      .main_loop          
    shl     rdx, SHIFT          
    ret                         