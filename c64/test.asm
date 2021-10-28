    * = $4000 "main"
start:

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
