; Hello World for Mega65 (45GS02)
.org $2000

start:
    LDX #$00
loop:
    LDA hello_msg, x
    BEQ done
    JSR $FFD2 ; CHROUT
    INX
    BRA loop
done:
    RTS

hello_msg:
    .text "HELLO, WORLD!"
    .byte 13, 0 ; CR, NULL
