.org $2000

MY_ADDR = $123456

start:
    // Constants
    lda lo($AABBCC)    // $CC
    ldx hi($AABBCC)    // $BB
    ldy bank($AABBCC)  // $AA
    
    // Equates
    lda lo(MY_ADDR)    // $56
    ldx hi(MY_ADDR)    // $34
    ldy bank(MY_ADDR)  // $12

    // Labels
    lda lo(start)      // $00
    ldx hi(start)      // $20
    ldy bank(start)    // $00

    rts
