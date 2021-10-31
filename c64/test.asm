    * = $4000 "main"
start:
    nop
    ldy #$40
    sty $4100
    ldy #$00
    ldy $4100
    brk
    * = $4100
