# Phase Review Findings

Summary of decisions and clarifications from the phase review session.

---

## Definitive Decisions

These should be added to the documentation:

### Phase 4: Effects/Shaders
- **Shader hot reload:** Not needed. Restart-to-test is acceptable.
- No file watching or runtime shader recompilation required.

### Phase 5: Input
- **Action rebinding:** Runtime rebindable. Players can remap controls in-game.
- Implies need for key capture UI support and binding persistence.

### Phase 7: Physics
- **Pixel-to-meter scale:** Configurable at init time. Will check old Anchor values for reference.
- **Joints:** Start with collision detection only. Add joints (hinges, ropes, etc.) if/when needed for specific games.
- **Collision tags:** String API for readability, mapping to Box2D's internal representation. Details TBD during implementation.

### Phase 8: Random
- **Deterministic replays:** Yes, important.
- Implies:
  - Save/restore RNG state functions
  - Consider separate RNG streams (gameplay vs cosmetic effects)
  - Consistent seeding mechanism

### Phase 9: Text
- **Font support:** Both bitmap fonts AND TTF rendering.
- Bitmap for pixel-perfect game text, TTF for scalable UI text.
- **Error screen:** Full stack trace (file:line for each call).

### Phase 10: Object System
- **Update order:** Creation order (first added = first updated).
- **Timer callbacks:** Receive just `self` (may explore alternatives during implementation).
- **Spring model:** Will test different approaches during implementation.

### Phase 11: Distribution
- **Asset packing:** Single executable with embedded assets (zip-append or similar).

### General
- **Frame counters:** Expose `an.frame`, `an.step`, `an.game_time` to Lua.

---

## Still Undecided

These remain open and will be decided during implementation:

| Topic | Notes |
|-------|-------|
| Audio library | miniaudio vs SoLoud - decide based on actual needs |
| Tree root structure | Whether `an` IS the root or references a separate root object |
| Web audio unlock | Auto-handle vs manual `an:unlock_audio()` |

---

## Recommended Documentation Updates

### ANCHOR_IMPLEMENTATION_PLAN.md

Add to **Phase 5 (Input)**:
```markdown
- Action bindings must be rebindable at runtime (for options menus)
- Need key capture support for rebinding UI
```

Add to **Phase 7 (Physics)**:
```markdown
- Pixel-to-meter scale configurable at init (check old Anchor values)
- Start with collision detection only; joints added as needed
- String-based collision tags (maps to Box2D internally)
```

Add to **Phase 8 (Random)**:
```markdown
- Deterministic replay support required
- Save/restore RNG state functions
- Consider separate streams for gameplay vs cosmetic RNG
```

Add to **Phase 9 (Text)**:
```markdown
- Support both bitmap fonts (pixel-perfect) and TTF (scalable)
- Error screen shows full stack trace
```

Add to **Phase 10 (Object System)**:
```markdown
- Sibling update order: creation order (first added = first updated)
- Timer callbacks receive self only (for now)
```

Add/update **Phase 11** or create if missing:
```markdown
## Phase 11: Distribution & Polish
- Single executable with embedded assets (zip-append)
- Web build edge cases (audio unlock, gamepad, keyboard)
- Final robustness testing
```

Add to **General/Engine State**:
```markdown
- Expose to Lua: an.frame, an.step, an.game_time
```

### ANCHOR.md

Add to **Key Technical Decisions** or **Engine vs Objects**:
```markdown
### Frame Counters

The engine exposes timing state to Lua:
- `an.frame` — render frame count (increments each rendered frame)
- `an.step` — physics step count (increments each 120Hz tick)
- `an.game_time` — accumulated game time in seconds

Useful for deterministic effects, debugging, and replay systems.
```

Add to **Timers** section when written:
```markdown
Timer callbacks receive only `self`:
```lua
self.timer:after(0.5, function(self)
    self:explode()
end)
```
```

---

## Questions Deferred to Implementation

These need discussion when we reach the relevant phase:

1. **Physics scale values** - Check what values were used in previous Anchor pixel games
2. **Box2D collision tag mapping** - Exact implementation of string-to-mask conversion
3. **Spring equation parameters** - Test damped harmonic vs other approaches
4. **Tree root design** - How `an` relates to tree structure
