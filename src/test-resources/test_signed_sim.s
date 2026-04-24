    .org $2000
    
test_sxt8:
    lda #$7F
    sxt.8
    ; Expected: .AX = $007F
    
    lda #$80
    sxt.8
    ; Expected: .AX = $FF80
    
test_adds16:
    ldax #$0001
    add.s16 .ax, #$0002
    ; Expected: .AX = $0003
    
test_cmps16:
    ldax #$8000
    cmp.s16 .ax, #$7FFF
    ; Signed comparison: $8000 (-32768) < $7FFF (32767)
    
test_shifts_s16:
    ldax #$8000
    asr.s16 .ax
    ; Expected: .AX = $C000
    
    ldax #$7000
    asr.s16 .ax
    ; Expected: .AX = $3800
