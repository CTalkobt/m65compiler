.segmentOrder code, data, bss
.org $2000

.code
start:
    lda data_val
    sta bss_val
    rts

.data
data_val: .byte $42
reserved_data: .res 4, $AA

.bss
bss_val: .res 1
reserved_bss: .res 10
