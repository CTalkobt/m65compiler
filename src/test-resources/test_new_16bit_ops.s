.org $2000

test_abs:
    ; Positive .AX
    lda #$34
    ldx #$12
    abs.16 .ax
    ; Expected: $1234 in .AX

    ; Negative .AX
    lda #$2E
    ldx #$FF  ; -210
    abs.16 .ax
    ; Expected: 210 ($00D2) in .AX

    ; Memory
    lda #$CE
    sta $3000
    lda #$FF
    sta $3001 ; -50 at $3000
    abs.16 $3000
    ; Expected: 50 ($0032) at $3000

test_rol_ror:
    sec
    lda #$01
    ldx #$80
    rol.16 .ax
    ; $1234 -> wait, $8001
    ; Carry was 1. 
    ; Low byte $01 << 1 | C(1) = $03, Carry=$00
    ; High byte $80 << 1 | C(0) = $00, Carry=$01
    ; Result: .AX = $0003, C=1

    clc
    lda #$00
    ldx #$01
    ror.16 .ax
    ; $0100 ->
    ; Carry was 0.
    ; High byte $01 >> 1 | C(0) = $00, Carry=$01
    ; Low byte $00 >> 1 | C(1) = $80, Carry=$00
    ; Result: .AX = $0080, C=0

test_cmp:
    lda #$00
    ldx #$10
    cmp.16 .ax, #$1000
    ; Expected: Z=1

    lda #$55
    ldx #$AA
    sta $3002
    stx $3003
    lda #$55
    ldx #$AA
    cmp.16 .ax, $3002
    ; Expected: Z=1

    lda #$00
    ldx #$20
    cmp.16 .ax, #$1000
    ; Expected: Z=0, N=0, C=1 (unsigned $2000 > $1000)
