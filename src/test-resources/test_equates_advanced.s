MY_VAL = 10 + 5
FORWARD = LATER * 2

start:
    LDA #MY_VAL    // #15
    LDX #FORWARD   // #40
    RTS

PROC local_test
    LOCAL_CONST = $42
    LDA #LOCAL_CONST
ENDPROC

LATER = 20
