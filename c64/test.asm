    * = $4000 "main"
start:
    lda #$01
    lda value
    lda value,x
    lda value,y
    lda $01
    lda $80,x
    lda ($80,x)
    lda ($80),y
    brk

    sta $4100
    sta $4100,x
    sta $4100,y
    sta $80
    sta $80,x
    sta ($80,x)
    sta ($80),y
    brk


    rti
    rts
    jsr subroutine


    bpl subroutine
    bmi subroutine
    bvc subroutine
    bvs subroutine
    bcc subroutine
    bcs subroutine
    bne subroutine
    beq subroutine
    tay
    iny
    inx

    clc
    sec
    cli
    sei
    tya
    clv
    cld
    sed

    php
    plp
    pha
    pla

    lda #$01
    bpl subroutine
    rol
    bmi subroutine
    sta $4100
    beq subroutine
    rol
    bne subroutine
    sta $4101
    bcc subroutine
    rol
    bcs subroutine
    sta $4101
    brk
subroutine:

    rts
    * = $4100
value: .byte $42