; Test 45GS02 specific features
.org $2000

start:
    ; Test Z register
    LDZ #$01
    INZ
    DEZ
    PHZ
    PLZ

    ; Test base-page indirect Z
    LDA ($10), z
    STA ($12), z

    ; Test flat memory indirect Z (EOM prefix $EA)
    LDA [$10], z
    STA [$12], z

    ; Test stack-relative indirect indexed Y
    LDA ($02, sp), y

    RTS
