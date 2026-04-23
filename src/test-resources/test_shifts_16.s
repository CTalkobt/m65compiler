.org $2000

test_lsl:
    lda #$55
    ldx #$AA
    lsl.16 .ax
    ; Expected: .AX = $AA AA ($55<<1 = $AA, $AA<<1 | 0 = $154 -> $54, wait)
    ; $55 = 0101 0101 -> <<1 = 1010 1010 ($AA), Carry=0
    ; $AA = 1010 1010 -> ROL with C=0 -> 0101 0100 ($54), Carry=1
    ; So .AX should be $54AA

test_lsr:
    lda #$55
    ldx #$AA
    lsr.16 .ax
    ; $AA = 1010 1010 -> LSR -> 0101 0101 ($55), Carry=0
    ; $55 = 0101 0101 -> ROR with C=0 -> 0010 1010 ($2A), Carry=1
    ; So .AX should be $552A

test_asr_neg:
    lda #$00
    ldx #$80
    asr.16 .ax
    ; $80 = 1000 0000 -> ASR -> 1100 0000 ($C0), Carry=0
    ; $00 = 0000 0000 -> ROR with C=0 -> 0000 0000 ($00)
    ; So .AX should be $C000

test_asr_pos:
    lda #$00
    ldx #$70
    asr.16 .ax
    ; $70 = 0111 0000 -> ASR -> 0011 1000 ($38), Carry=0
    ; $00 = 0000 0000 -> ROR with C=0 -> 0000 0000 ($00)
    ; So .AX should be $3800

test_mem:
    lda #$55
    sta $3000
    lda #$AA
    sta $3001
    lsl.16 $3000
    ; Result should be $54AA at $3000

    lda #$55
    sta $3002
    lda #$AA
    sta $3003
    lsr.16 $3002
    ; Result should be $552A at $3002
