    * = $4000 "main"
start:  
    lda #$10
    sec
    asl
    asl value
    brk
subroutine:

    rts
    * = $4100
value: .byte $80