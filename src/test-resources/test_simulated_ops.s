.org $2000

start:
    ; 16-bit Load/Store (Legacy syntax)
    LDW .AX, $1234
    STW .AX, $02
    LDW .AX, $02
    
    ; 16-bit Load/Store (Preferred syntax)
    LDAX $1234
    STAX $02
    LDAX $02
    
    ; 16-bit Immediate Load
    LDAX #$5678
    LDAY #$9ABC
    LDAZ #$DE00
    LDAX #<target
    
    ; Stack-relative Load/Store
    LDAX 2, s
    STAX 4, s
    LDAY 6, s
    STAY 8, s
    LDAZ 10, s
    STAZ 12, s

    ; New SP-relative syntax
    LDW.SP .AX, 2
    STW.SP .AX, 4
    STW.SP #$1234, 6

    ; 16-bit Add/Sub
    ADD.16 .AX, $1000
    SUB.16 .AX, $0500
    ADD.16 .AX, $02
    
    ; 16-bit Neg/Not
    NEG.16
    NOT.16
    
    ; 16-bit Compare
    CPW .AX, $1D34
    CPW .AX, $02
    
    ; Register Swaps
    SWAP A, X
    SWAP A, Y
    SWAP A, Z
    SWAP X, Y
    SWAP X, Z
    SWAP Y, Z
    
    ; Register Zeroing
    ZERO A, X, Y, Z
    
    ; Truthiness checks
    CHKZERO.8
    CHKZERO.16
    CHKNONZERO.8
    CHKNONZERO.16
    
    ; Branching and Select
    BRANCH.16 BEQ, target
    SELECT .A, 1, 2
    
    ; Pointer and Stack
    PTRSTACK 2
    PTRDEREF $02
    
    ; Flat Memory (28-bit)
    LDW.F $12345
    STW.F $12345
    INC.F $12345
    
target:
    RTS
