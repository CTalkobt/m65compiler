* = $2000
first:
    LDA #$01
    JMP second

.org $3000
second:
    LDA #$02
    JMP third

* = $4000
third:
    RTS
