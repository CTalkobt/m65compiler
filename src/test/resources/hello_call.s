; Test Symbol Table Sizes
.org $2000

MY_CONST = $42  ; Should be a byte
MY_WIDE = $1234 ; Should be a word

start:
    ; CALL with equate (byte)
    CALL print_byte, MY_CONST
    ; CALL with label (address, word)
    CALL print_word, my_data
    RTS

print_byte:
    LDA 2, s ; Should be at offset 2
    JSR $FFD2
    ENDCALL 1

print_word:
    LDA 2, s ; Low byte
    LDA 3, s ; High byte
    RTS

my_data:
    .byte $AA
