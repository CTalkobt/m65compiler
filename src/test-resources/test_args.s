; Test Dynamic ARGn offsets
.org $2000

start:
    ; ARG1=$41, ARG2=$1234
    CALL my_proc, B#$41, W#$1234
    RTS

PROC my_proc, p1, p2
    ; p2 (W) pushed last -> ARG2 = 2
    ; p1 (B) pushed first -> ARG1 = 4
    LDA ARG1, s ; Should be $41
    JSR $FFD2
    LDA ARG2, s ; Should be $34 (low byte of $1234)
    JSR $FFD2
ENDPROC
