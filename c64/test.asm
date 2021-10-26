    * = $4000 "main"
start:
    lda #$01
    sta $d020
    lda $d020
    sta $d021
    jsr subroutine
    lda #$04
    sta $d023
    brk 
subroutine:
    lda #$03
    sta $d022
    rts    