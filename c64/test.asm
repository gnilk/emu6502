    * = $4000 "main"
start:
    dec value
    dec value
    dec value
    dec value
    dec value
    brk
subroutine:
    lda #$40
    rts
    * = $4100
value: .byte 01