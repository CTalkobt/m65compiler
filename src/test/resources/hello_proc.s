; Test PROC/ENDPROC
.org $2000

start:
    CALL my_proc, B#$41, W#$1234
    RTS

PROC my_proc, B#val1, W#val2
    ; val2 is pushed last (W#$1234), so it's at offset 2
    ; val1 is pushed first (B#$41), so it's at offset 2 + sizeof(val2) = 4
    LDA val2, s  ; Low byte of $1234
    JSR $FFD2
    LDA val1, s  ; The byte $41 ('A')
    JSR $FFD2
ENDPROC
