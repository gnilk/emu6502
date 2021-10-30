    * = $4000 "main"
start:
    lda #$42
    asl
    rol
    rol
    rol

    brk
subroutine:
    lda #$40
    rts
    * = $4100
