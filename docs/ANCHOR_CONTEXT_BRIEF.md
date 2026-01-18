# Anchor Context Brief

*This document captures the reasoning, philosophy, and decision-making style behind Anchor's design. Read this to understand *why* decisions were made and *how* to evaluate new ideas.*

---

## The Developer

a327ex is a solo indie game developer who has shipped successful games (BYTEPATH, SNKRX). Works primarily by intuition — choices are made because they "feel right" or "look nice" before rational justification. Rational thinking fills in the parts intuition doesn't have opinions on.

This means:
- Aesthetic judgments are valid and often primary ("this syntax looks better")
- Exploring the possibility space matters more than committing early
- Conversation is the design tool — talking through options surfaces what feels right
- Specs are artifacts of conversation, not contracts to implement literally

When evaluating features: "Would this feel right to use?" matters as much as "Is this technically sound?"

---

## Core Design Values

### Locality Above All

Code should be understandable by looking at one place. This serves:
- Human cognition (limited working memory)
- LLM cognition (finite context window)
- Solo developer workflow (must understand own code months later)

A class definition keeps everything together — properties, behaviors, and relationships are all visible in one place:

```yuescript
class player extends object
  new: (@x, @y) =>
    super!
    @\tag 'player'
    @\add collider 'player', 'dynamic', 'circle', 16
    @\add timer!

  update: (dt) =>
    @x += @vx * dt
    game\circle @x, @y, 16, white!
```

Compare to systems where you define a class in one file, register components elsewhere, wire up behaviors in a third place. Same expressiveness, but the understanding is scattered.

**When evaluating features:** Does this keep related things together, or scatter them?

### No Bureaucracy

Imports, exports, dependency injection, configuration objects, registration systems — these are bureaucracy. They may be necessary in large teams, but for solo development they're friction.

Anchor prefers:
- Globals that are just available (`an`, common classes)
- Direct mutation over message passing
- Explicit calls over implicit event systems
- Things that work without setup

**When evaluating features:** Does this require ceremony to use, or does it just work?

---

## Language Boundary Philosophy

### Why YueScript

Chosen because it "looked nice." That's the real reason. MoonScript syntax with additional features, compiles to Lua. The class system, the `@` for self, the significant whitespace, the operator overloading — it produces code that's pleasant to read and write.

The `-r` compiler flag rewrites output to match source line numbers, so Lua errors point to correct YueScript lines. No source map parsing needed.

### Why C (Not C++, Rust, etc.)

Simplicity. C has fewer concepts to manage. The entire Lua C API is designed for C. No RAII, no templates, no borrow checker — just functions and data.

C++ could work but adds complexity without clear benefit for this scope. Rust would be interesting but the Lua interop story is less mature.

### The Boundary Rule

Start with everything in Lua/YueScript. Only move to C when profiling shows actual bottlenecks.

This avoids premature optimization. The tree structure, timers, springs, actions — all start as Lua objects. If timer updates become a bottleneck (unlikely), move the hot loop to C while keeping the Lua interface.

The gameplay programmer never touches C. The engine layer (also in YueScript) wraps C calls. Raw pointers flow from C to Lua and back; Lua is responsible for calling destroy functions.

**When evaluating features:** Should this be C (performance-critical, low-level) or YueScript (game logic, flexible)?

---

## How to Evaluate New Features

When reviewing other engines/frameworks, ask:

1. **Does Anchor already have this?** (Check `reference/archives/ANCHOR.md`, the object tree might already cover it)

2. **Does this fit the locality principle?** (Keeps related code together vs. scatters it)

3. **What's the bureaucracy cost?** (Setup, configuration, boilerplate)

4. **Is this C-level or YueScript-level?** (Performance-critical vs. game logic)

5. **Does the developer need this?** (Solo indie action games, not AAA engines)

6. **Would it feel right to use?** (Aesthetic/intuitive judgment matters)

Features to flag:
- "Worth stealing" — fits philosophy, solves real problem
- "Interesting but doesn't fit" — clever but violates principles
- "Anchor already has this" — covered by existing design
- "Worth discussing" — unclear fit, needs conversation
