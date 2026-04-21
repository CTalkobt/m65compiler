.org $2000
start:
    ; Test Load-after-Store
    LDA #$10
    STA $02
    LDA #$10    ; Should be removed (identical immediate)
    
    LDX #$20
    STX $03
    LDX $03     ; Should be removed (load from same address)
    
    ; Test Register Transfers
    LDA #$40
    TAX
    TXA         ; Should be removed
    
    ; Test JMP to BRA conversion
    JMP target  ; Should become BRA target (2 bytes)
    EOM
    
target:
    ; Test @ labels (internal optimization window)
    LDA #$55
@internal:
    LDA #$55    ; Should be removed (tracking continues across @ label)
    RTS
    
final:
    LDA #$55
other:
    LDA #$55    ; Should NOT be removed (standard label is a barrier)
    RTS
