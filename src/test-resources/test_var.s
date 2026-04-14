; Test .var directive
.org $2000

.var my_x = $10
start:
    LDA #my_x ; Should be $10
    .var my_x++
    LDX #my_x ; Should be $11
    .var my_x = $20
    LDY #my_x ; Should be $20
    RTS
