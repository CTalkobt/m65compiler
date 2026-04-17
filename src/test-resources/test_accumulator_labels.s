.org $2000
a:
    NEG A      // Should be Accumulator mode ($42 1A for INC A, $42 for NEG)
    NEG a      // Should be Absolute mode ($42 CE ...)
    LDA a, s   // Should be Stack Relative ($E2 ...)
    INC a      // Should be Absolute ($EE ...)
    INC A      // Should be Accumulator ($1A)
