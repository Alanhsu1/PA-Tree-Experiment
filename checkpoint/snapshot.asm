    .include data_model.h
    .cdecls C, LIST, "msp430.h"
    .cdecls C, LIST
    %{
        #include "FreeRTOS.h"
    %}

    .global snapshot_reg 

    .def snapshotReg
    .def restoreReg

    .text
; ==========================================================
; snapshotReg
; ==========================================================
snapshotReg: .asmfunc

    dint
    nop

    pushm_x     #14, r15             ; Push R2 - R15

    mov_x       &snapshot_reg, r9      

    pop_x        8(r9)
    pop_x       12(r9) 
    pop_x       16(r9) 
    pop_x       20(r9) 
    pop_x       24(r9) 
    pop_x       28(r9) 
    pop_x       32(r9) 
    pop_x       36(r9) 
    pop_x       40(r9) 
    pop_x       44(r9) 
    pop_x       48(r9) 
    pop_x       52(r9) 
    pop_x       56(r9) 
    pop_x       60(r9) 
    
    mov_x       sp, 4(r9)

    mov_x       36(r9), r9

    ret_x
    .endasmfunc

; ==========================================================
; restoreReg
; ==========================================================
restoreReg: .asmfunc

    dint
    nop

    mov_x       &snapshot_reg, r9

    push_x       4(r9)

    push_x      60(r9)
    push_x      56(r9)
    push_x      52(r9)
    push_x      48(r9)
    push_x      44(r9)
    push_x      40(r9)
    push_x      36(r9)
    push_x      32(r9)
    push_x      28(r9)
    push_x      24(r9)
    push_x      20(r9)
    push_x      16(r9)
    push_x      12(r9)
    push_x       8(r9)

    popm_x      #14, r15
    mov_x       @sp, sp

    ret_x
    .endasmfunc

    .end
