    .org $2000
start:
    lda #$41 ; 'A'
    FILL $3000, #100
    FILL .AX, #50
    FILL ($02), #20
    FILL.SP #4, #10
    FILL.SP .X, .Y
    rts
