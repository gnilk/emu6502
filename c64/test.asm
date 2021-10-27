    * = $4000 "main"
start:
    lda #$f0
    pha
    clc
    adc #$20
    php
    adc #$00
    clc
    plp
    sta value
    pla
    sta value2
    brk
subroutine:

    rts
    * = $4100
value: .byte 0
value2: .byte 0