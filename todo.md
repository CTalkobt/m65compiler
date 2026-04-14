# TODO: cc45 Optimizer

## Dead Code Removal
- [ ] Detect and remove code following a `return` statement in the same block.
- [ ] Remove redundant `ENDPROC` (RTN) instructions if a `return` has already performed the stack cleanup.

## Arithmetic Optimizations
- [ ] Constant folding (e.g., `1 + 2` -> `3`).
- [ ] Strength reduction (e.g., `x * 2` -> `x << 1`).

## Register Allocation
- [ ] Improve usage of A, X, Y, and Z registers to minimize stack traffic.
