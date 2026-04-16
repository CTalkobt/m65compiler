.org $2000

MY_CONST = $42
MY_WIDE = $1234

start:
    // CALL with equate (byte)
    CALL print_byte, MY_CONST
    // CALL with label (address, word)
    CALL print_word, my_data
    RTS

PROC print_byte, B#val
    LDA val
    JSR $FFD2
ENDPROC

PROC print_word, W#ptr
    LDA ptr     // Low byte
    LDA ptr+1   // High byte
ENDPROC

my_data:
    .word $AABB
