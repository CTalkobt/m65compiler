    .org $2000
start:
    lda #$41 ; 'A'
    FILL $3000, #100
    FILL .AX, #50
    FILL ($02), #20
    FILL.SP #4, #10
    FILL.SP .X, .Y
    
    ; Test .XY length
    ldx #10
    ldy #2
    FILL $4000, .XY ; Fill 512+10 bytes

    ; Test MOVE
    ldx #$00
    ldy #$01 ; 256 bytes
    MOVE $3000, $4000
    MOVE .AZ, $5000
    MOVE $6000, .AZ
    
    MOVE.SP $7000, #4
    MOVE.SP #8, $8000
    
    rts
