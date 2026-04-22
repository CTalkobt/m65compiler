.segment "data"
* = $2000
data_val: .byte $AA

.segment "text"
* = $1000
start:
    lda data_val
    rts

.segment "data"
    .byte $BB

.segment "bss"
* = $3000
bss_val: .word $0000
