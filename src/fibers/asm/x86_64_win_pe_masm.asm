; This is just Boost.Context in masm
; https://github.com/boostorg/context

; /*************************************************************************************
; * ---------------------------------------------------------------------------------- *
; * |     0   |     1   |     2    |     3   |     4   |     5   |     6   |     7   | *
; * ---------------------------------------------------------------------------------- *
; * |    0x0  |    0x4  |    0x8   |    0xc  |   0x10  |   0x14  |   0x18  |   0x1c  | *
; * ---------------------------------------------------------------------------------- *
; * |                          SEE registers (XMM6-XMM15)                            | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |     8   |    9    |    10    |    11   |    12   |    13   |    14   |    15   | *
; * ---------------------------------------------------------------------------------- *
; * |   0x20  |  0x24   |   0x28   |   0x2c  |   0x30  |   0x34  |   0x38  |   0x3c  | *
; * ---------------------------------------------------------------------------------- *
; * |                          SEE registers (XMM6-XMM15)                            | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    16   |    17   |    18   |    19    |    20   |    21   |    22   |    23   | *
; * ---------------------------------------------------------------------------------- *
; * |   0x40  |   0x44  |   0x48  |   0x4c   |   0x50  |   0x54  |   0x58  |   0x5c  | *
; * ---------------------------------------------------------------------------------- *
; * |                          SEE registers (XMM6-XMM15)                            | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    24   |   25    |    26    |   27    |    28   |    29   |    30   |    31   | *
; * ---------------------------------------------------------------------------------- *
; * |   0x60  |   0x64  |   0x68   |   0x6c  |   0x70  |   0x74  |   0x78  |   0x7c  | *
; * ---------------------------------------------------------------------------------- *
; * |                          SEE registers (XMM6-XMM15)                            | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    32   |   32    |    33    |   34    |    35   |    36   |    37   |    38   | *
; * ---------------------------------------------------------------------------------- *
; * |   0x80  |   0x84  |   0x88   |   0x8c  |   0x90  |   0x94  |   0x98  |   0x9c  | *
; * ---------------------------------------------------------------------------------- *
; * |                          SEE registers (XMM6-XMM15)                            | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    39   |   40    |    41    |   42    |    43   |    44   |    45   |    46   | *
; * ---------------------------------------------------------------------------------- *
; * |   0xa0  |   0xa4  |   0xa8   |   0xac  |   0xb0  |   0xb4  |   0xb8  |   0xbc  | *
; * ---------------------------------------------------------------------------------- *
; * | fc_mxcsr|fc_x87_cw|     <alignment>    |       fbr_strg    |      fc_dealloc   | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    47   |   48    |    49    |   50    |    51   |    52   |    53   |    54   | *
; * ---------------------------------------------------------------------------------- *
; * |   0xc0  |   0xc4  |   0xc8   |   0xcc  |   0xd0  |   0xd4  |   0xd8  |   0xdc  | *
; * ---------------------------------------------------------------------------------- *
; * |        limit      |         base       |         R12       |         R13       | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    55   |   56    |    57    |   58    |    59   |    60   |    61   |    62   | *
; * ---------------------------------------------------------------------------------- *
; * |   0xe0  |   0xe4  |   0xe8   |   0xec  |   0xf0  |   0xf4  |   0xf8  |   0xfc  | *
; * ---------------------------------------------------------------------------------- *
; * |        R14        |         R15        |         RDI       |        RSI        | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    63   |   64    |    65    |   66    |    67   |    68   |    69   |    70   | *
; * ---------------------------------------------------------------------------------- *
; * |  0x100  |  0x104  |  0x108   |  0x10c  |  0x110  |  0x114  |  0x118  |  0x11c  | *
; * ---------------------------------------------------------------------------------- *
; * |        RBX        |         RBP        |  vgc_fiber *fiber |        RIP        | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    71   |   72    |    73    |   74    |    75   |    76   |    77   |    78   | *
; * ---------------------------------------------------------------------------------- *
; * |  0x120  |  0x124  |  0x128   |  0x12c  |  0x130  |  0x134  |  0x138  |  0x13c  | *
; * ---------------------------------------------------------------------------------- *
; * |                                    home space                                  | *
; * ---------------------------------------------------------------------------------- *
; * ---------------------------------------------------------------------------------- *
; * |    79   |   80    |    81    |   82    |    83   |    84   |    85   |    86   | *
; * ---------------------------------------------------------------------------------- *
; * |  0x140  |  0x144  |  0x148   |  0x14c  |  0x150  |  0x154  |  0x158  |  0x15c  | *
; * ---------------------------------------------------------------------------------- *
; * |    void *data     |     void *ctx      |  fiber_data *fd   |     <alignment>   | *
; * ---------------------------------------------------------------------------------- *
; **************************************************************************************/

EXTERN _exit:PROC
.code

; void *ctx vgc_make(void *base, void *limit, vgc_proc proc);
vgc_make PROC FRAME
  .endprolog
  mov rax, rcx                ; Grab the bottom of the context stack
  sub rax, 0160h              ; Reserve space on the context stack

  stmxcsr [rax + 0a0h]        ; Save MMX control/status word
  fnstcw [rax + 0a4h]         ; Save x87 control word

  mov [rax + 0100h], r8       ; Save proc address at RBX

  ; Save the required fields for the TIB
  ; https://en.wikipedia.org/wiki/Win32_Thread_Information_Block

  mov [rax + 0c8h], rcx       ; Save the stack base address
  mov [rax + 0c0h], rdx       ; Save the stack limit address

  ; Our stack isn't "real" and can't be expanded, therefore the deallocation
  ; stack and the stack limit are the same
  mov [rax + 0b8h], rdx       ; Save the deallocation stack address

  ; ToDo: I don't know if this is necessary or what the consequences of loading
  ; trash into fiber-local storage is. Boost zeros this out so we follow their
  ; example
  xor rcx, rcx
  mov [rax + 0b0h], rcx       ; Zero out fiber-local storage

  ; End of TIB fields

  lea rcx, [rax + 0140h]      ; Calculate and save the address for returning
  mov [rax + 0110h], rcx      ; the fiber struct when yielding

  lea rcx, trampoline         ; Calculate and save the address of trampoline
  mov [rax + 0118h], rcx

  lea rcx, finish             ; Calculate and save the address of finish at RBP
  mov [rax + 0108h], rcx

  ret                         ; Return pointer to context

; Fix the stack before jumping into the passed vgc_proc
trampoline:
  push rbp                    ; Set finish as return addr
  jmp rbx                     ; Jump to the context function

; Kill the process if a context returns from the bottom stack frame
finish:
  xor rcx, rcx                ; Exit code is zero
  call _exit
  hlt
vgc_make ENDP

; vgc_fiber vgc_jump(vgc_fiber)
; rcx | return pointer
; rdx | vgc_fiber *fiber
vgc_jump PROC FRAME
  .endprolog
  sub rsp, 0118h               ; Allocate stack space

  ; Save all non-volatile floating-point registers
  movaps [rsp], xmm6
  movaps [rsp + 010h], xmm7
  movaps [rsp + 020h], xmm8
  movaps [rsp + 030h], xmm9
  movaps [rsp + 040h], xmm10
  movaps [rsp + 050h], xmm11
  movaps [rsp + 060h], xmm12
  movaps [rsp + 070h], xmm13
  movaps [rsp + 080h], xmm14
  movaps [rsp + 090h], xmm15

  stmxcsr [rsp + 0a0h]        ; Save MMX control/status word
  fnstcw [rsp + 0a4h]         ; Save x87 control word

  mov r10, gs:[030h]          ; Load the linear address of the TIB

  mov rax, [r10 + 020h]       ; Save fiber-local storage
  mov [rsp + 0b0h], rax

  mov rax, [r10 + 01478h]     ; Save deallocation stack
  mov [rsp + 0b8h], rax

  mov rax, [r10 + 010h]       ; Save stack limit
  mov [rsp + 0c0h], rax

  mov rax, [r10 + 08h]        ; Save stack base
  mov [rsp + 0c8h], rax

  ; Save all non-volatile integer registers
  mov [rsp + 0d0h], r12
  mov [rsp + 0d8h], r13
  mov [rsp + 0e0h], r14
  mov [rsp + 0e8h], r15
  mov [rsp + 0f0h], rdi
  mov [rsp + 0f8h], rsi
  mov [rsp + 0100h], rbx
  mov [rsp + 0108h], rbp

  mov [rsp + 0110h], rcx      ; Save the return address for the fiber struct

  mov r8, rsp                 ; Store current stack pointer
  mov rsp, [rdx + 08h]        ; Switch into the destination stack

  ; Restore all non-volaile floating-point registers
  movaps xmm6, [rsp]
  movaps xmm7, [rsp + 010h]
  movaps xmm8, [rsp + 020h]
  movaps xmm9, [rsp + 030h]
  movaps xmm10, [rsp + 040h]
  movaps xmm11, [rsp + 050h]
  movaps xmm12, [rsp + 060h]
  movaps xmm13, [rsp + 070h]
  movaps xmm14, [rsp + 080h]
  movaps xmm15, [rsp + 090h]

  ldmxcsr [rsp + 0a0h]        ; Restore MMX control/status word
  fldcw [rsp + 0a4h]          ; Restore x87 control word

  mov rax, [rsp + 0b0h]       ; Restore fiber-local storage
  mov [r10 + 020h], rax

  mov rax, [rsp + 0b8h]       ; Restore deallocation stack
  mov [r10 + 01478h], rax

  mov rax, [rsp + 0c0h]       ; Restore stack limit
  mov [r10 + 010h], rax

  mov rax, [rsp + 0c8h]       ; Restore stack base
  mov [r10 + 08h], rax

  ; Restore all non-volatile integer registers
  mov r12, [rsp + 0d0h]
  mov r13, [rsp + 0d8h]
  mov r14, [rsp + 0e0h]
  mov r15, [rsp + 0e8h]
  mov rdi, [rsp + 0f0h]
  mov rsi, [rsp + 0f8h]
  mov rbx, [rsp + 0100h]
  mov rbp, [rsp + 0108h]

  mov rax, [rsp + 0110h]      ; Restore address for fiber struct
  mov r9, [rsp + 0118h]       ; Restore return address

  mov [rax + 08h], r8         ; Restore parent context
  mov r8, [rdx]               ; Restore user data pointer
  mov [rax], r8
  mov r8, [rdx + 010h]        ; Restore fiber data struct
  mov [rax + 010h], r8

  mov rcx, rax                ; vgc_fiber is first argument of vgc_proc

  add rsp, 0120h
  jmp r9
vgc_jump ENDP
END
