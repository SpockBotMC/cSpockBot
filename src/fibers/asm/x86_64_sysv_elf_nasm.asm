; This is just Boost.Context in nasm
; https://github.com/boostorg/context


; ****************************************************************************************
; *                                                                                      *
; *  ----------------------------------------------------------------------------------  *
; *  |    0    |    1    |    2    |    3    |    4     |    5    |    6    |    7    |  *
; *  ----------------------------------------------------------------------------------  *
; *  |   0x0   |   0x4   |   0x8   |   0xc   |   0x10   |   0x14  |   0x18  |   0x1c  |  *
; *  ----------------------------------------------------------------------------------  *
; *  | fc_mxcsr|fc_x87_cw|        R12        |         R13        |        R14        |  *
; *  ----------------------------------------------------------------------------------  *
; *  ----------------------------------------------------------------------------------  *
; *  |    8    |    9    |   10    |   11    |    12    |    13   |    14   |    15   |  *
; *  ----------------------------------------------------------------------------------  *
; *  |   0x20  |   0x24  |   0x28  |  0x2c   |   0x30   |   0x34  |   0x38  |   0x3c  |  *
; *  ----------------------------------------------------------------------------------  *
; *  |        R15        |        RBX        |         RBP        | vgc_fiber *return |  *
; *  ----------------------------------------------------------------------------------  *
; *  ----------------------------------------------------------------------------------  *
; *  |    16   |    17   |    18   |    19   |    20    |    21   |    22   |    23   |  *
; *  ----------------------------------------------------------------------------------  *
; *  |   0x40  |   0x44  |   0x48  |   0x4c  |   0x50   |   0x54  |   0x58  |   0x5c  |  *
; *  ----------------------------------------------------------------------------------  *
; *  |        RIP        |     void *data    |     void *ctx      |  fiber_data *fd   |  *
; *  ----------------------------------------------------------------------------------  *
; *  ----------------------------------------------------------------------------------  *
; *  |    24   |   25    |    26   |   27    |    28    |    29   |    30   |    31   |  *
; *  ----------------------------------------------------------------------------------  *
; *  |   0x60  |   0x64  |   0x68  |   0x6c  |   0x70   |   0x74  |   0x78  |   0x7c  |  *
; *  ----------------------------------------------------------------------------------  *
; *  |    <alignment>    |                                                            |  *
; *  ----------------------------------------------------------------------------------  *
; *                                                                                      *
; ****************************************************************************************

section .text

; void *ctx vgc_make(void *base, void *limit, vgc_proc proc);
global vgc_make
vgc_make:
  mov rax, rdi                ; Grab the bottom of the context stack
  sub rax, 68h                ; Reserve space on the context stack

  stmxcsr [rax]               ; Save MMX control/status word
  fnstcw [rax + 4h]           ; Save x87 control word

  mov [rax + 28h], rdx        ; Store proc address at RBX

  lea rcx, [rax + 48h]        ; Calculate/store the address of return ptr
  mov [rax + 38h], rcx

  lea rcx, [rel finish]       ; Calculate/store the address of finish at RBP
  mov [rax + 30h], rcx

  lea rcx, [rel trampoline]   ; Calculate/store the address of trampoline
  mov [rax + 40h], rcx

  ret                         ; Return pointer to context

; Fix the stack before jumping into the passed vgc_proc
trampoline:
  push rbp                    ; Set finish as return addr
  jmp rbx                     ; Jump to the context function

; Kill the process if a context returns from the bottom stack frame
finish:
  xor rdi, rdi                ; Exit code is zero
  mov rax, 60d                ; Call _exit
  syscall
  hlt

; vgc_fiber vgc_jump(vgc_fiber)
global vgc_jump
vgc_jump:
  mov rsi, [rsp + 8h]         ; Grab data pointer
  mov rdx, [rsp + 10h]        ; Grab context pointer
  mov rcx, [rsp + 18h]        ; Grab fiber data pointer

  sub rsp, 40h                ; Allocate stack space

  stmxcsr [rsp]               ; Save MMX control/status word
  fnstcw [rsp + 4h]           ; Save x87 control word

  mov [rsp +  8h], r12        ; Save R12
  mov [rsp + 10h], r13        ; Save R13
  mov [rsp + 18h], r14        ; Save R14
  mov [rsp + 20h], r15        ; Save R15
  mov [rsp + 28h], rbx        ; Save RBX
  mov [rsp + 30h], rbp        ; Save RBP

  mov [rsp + 38h], rdi        ; Save return fiber struct address

  mov rdi, rsp                ; Store the current stack pointer
  mov rsp, rdx                ; Switch into the destination stack

  ldmxcsr [rsp]               ; Restore MMX control/status word
  fldcw [rsp + 4h]            ; Restore x87 control word

  mov r12, [rsp +  8h]        ; Restore R12
  mov r13, [rsp + 10h]        ; Restore R13
  mov r14, [rsp + 18h]        ; Restore R14
  mov r15, [rsp + 20h]        ; Restore R15
  mov rbx, [rsp + 28h]        ; Restore RBX
  mov rbp, [rsp + 30h]        ; Restore RBP
  mov rax, [rsp + 38h]        ; Restore fiber return struct
  mov rdx, [rsp + 40h]        ; Restore return address

  mov [rax], rsi              ; Restore user data pointer
  mov [rax + 8h], rdi         ; Restore parent context
  mov [rax + 10h], rcx        ; Restore fiber data struct

  ; Note, don't use ret
  ; The Return Stack Buffer will miss every time due to the context switch
  ; Much better to use the general indirect branch predictor and only miss
  ; _most_ of the time.
  add rsp, 48h
  jmp rdx
