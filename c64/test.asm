    * = $4000 "main"
start:
    bpl subroutine
    bmi subroutine
    bvc subroutine
    bvs subroutine
    bcc subroutine
    bcs subroutine
    bne subroutine
    beq subroutine

    brk
subroutine:
    brk
    rts
    * = $4100
value: .byte $80