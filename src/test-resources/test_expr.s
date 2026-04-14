; Test EXPR and constant folding
.org $2000

start:
    ; Constant folding: 3 + 5 -> 8
    EXPR .A, 3 + 5
    STA $10

    ; Register reference and non-constant
    LDX #$02
    EXPR .A, .X + 10
    STA $11

    ; 16-bit register pairs
    LDA #$10
    LDX #$20
    EXPR .AX, .AX + .AX
    STA $12
    STX $13

    ; Test flag reference: .A - P.C
    SEC
    LDA #$05
    EXPR .A, .A - P.C ; Should subtract 1 if Carry is set
    STA $14

    CLC
    LDA #$05
    EXPR .A, .A - P.C ; Should subtract 0 if Carry is clear
    STA $15

    RTS
