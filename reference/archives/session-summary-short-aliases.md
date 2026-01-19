# Horizontal Links & Short Aliases

## Summary

This session implemented horizontal links (death notification between objects) and short single-letter method aliases. After implementing links, we explored custom operators but discovered YueScript limitations made them impractical, leading to the short alias approach.

**Horizontal Links Implementation:**
- Design decisions:
  - Callbacks run immediately during `kill()`, not deferred to cleanup
  - Default behavior: linker dies when target dies (if no callback provided)
  - Callback receives only `self` - use closures if target reference needed
  - No named references created - links are just death notifications
- Bidirectional storage (`@links` and `target.linked_from`) enables efficient cleanup
- Circular links handled safely by checking `dead` flag before processing
- Added 10 tests covering callbacks, default kill, circular links, cleanup

**Operators Abandoned:**
- Discovered YueScript doesn't allow standalone operator expressions as statements
- `obj ^ {x: 100}` fails as a statement - only works in expression context
- Lua's `^` is right-associative, breaking chaining like `obj ^ {a:1} ^ {b:2}`
- Created `reference/operators-vs-methods.md` comparing approaches
- Decided short methods achieve similar brevity without language hacks

**Naming Iterations:**
- First: S, B, E, X, L, A, F
- Then: E, T, V, Y, X, Z, A, F, L
- Then: T, R, U, E, X, L, A, F, K
- Tested W, Y, I, N, H for "set" spread across examples
- Final: T (object), Y (set), U (build), E (early), X (action), L (late), A (add), F (flow), K (link)

**Implementation:**
- Added `set`, `build`, `flow_to` as proper documented methods
- Aliases point to these methods (not inline implementations)
- Global `T = object` added at end of object.yue
- Removed `^` operator since we're not using operators
- Fixed aliases to use explicit parameters instead of varargs

**Test Timing Fix:**
- E, X, L alias test showed only "E" in order - X and L missing
- Cause: `an`'s action (running the test check) executes before child's actions in same frame
- Solution: added wait frame so all phases complete before checking order

**Files Modified:**
- `game/object.yue` - link method, set/build/flow_to, aliases, removed ^ operator
- `main.yue` - link tests, alias tests, removed ^ operator tests
- `reference/operators-vs-methods.md` - comparison document
- `docs/PHASE_10_PROGRESS.md` - updated with all new features

**Final State:**
- 42 tests, all passing
- Short aliases: T, Y, U, E, X, L, A, F, K
