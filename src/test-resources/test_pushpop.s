.org $2000

start:
    push a
    push x
    push y
    push z
    pop z
    pop y
    pop x
    pop a

    push .ax
    pop .ax

    push .ay
    pop .ay

    push .az
    pop .az

    push .axyz
    pop .q

    rts
