# CLAUDE.md

Guidelines for Claude Code instances working on Anchor and games built with it.

---

## Read First

Read `docs/ANCHOR.md` before doing anything. It covers:
- The object tree model (tree-based ownership, automatic cleanup)
- How objects, timers, springs, and colliders work
- The action-based vs rules-based spectrum
- Technical implementation details (OpenGL rendering, Box2D physics)

**For the reasoning behind these decisions** — why the engine works the way it does, how to evaluate new features, the developer's working style — see the [Anchor Context Brief](#anchor-context-brief) at the end of this document.

---

## Session Workflow

**Every Claude Code session follows this workflow.**

### During Session

- Work normally on requested tasks
- Update `docs/ANCHOR.md` when APIs or architecture changes
- Update `docs/ANCHOR_IMPLEMENTATION_PLAN.md` when tasks are completed
- Update this file (`CLAUDE.md`) when new patterns or conventions are established

### End of Session

**`/end`** → run the full workflow below (save transcript, commit, push)

When running the full workflow, complete all steps before committing (one commit per session):

1. **Locate the session transcript** - Find the most recent JSONL file:
   ```bash
   ls -t ~/.claude/projects/E--a327ex-Anchor/*.jsonl | grep -v agent | head -1
   ```

2. **Convert to Markdown** - Run the converter with a title-based filename:
   ```bash
   python scripts/jsonl-to-markdown.py [transcript.jsonl] website/logs/title-slug.md
   ```
   Use a lowercase, hyphenated version of the title (e.g., `engine-phase-1.md`, `windows-setup.md`). Date-based filenames don't work with Blot.

3. **Read the converted log** to review the full session, especially if the conversation was compacted. This ensures the summary covers everything, not just what's in current context.

4. **Write a detailed summary** covering what was accomplished throughout the entire session:
   - Organize sections in chronological order (matching conversation flow)
   - Give weight to one-off fixes, attempts, and problems solved
   - Include specific details: error messages, what was tried, what worked
   - This helps future-me look back and see what was accomplished and tried

5. **Create a short title** (max 30 characters) based on the summary.

6. **Show title + summary to user** and wait for approval. The user may want to change the title or edit details in the summary before proceeding.

7. **Prepend title + summary** to the log file (replace the default header). Format:

   # Title Here

   ## Summary

   [Summary content here]

   ---

8. **Sync context files**:
   ```bash
   cp .claude/CLAUDE.md docs/* website/context/
   ```

9. **Commit everything** with title as subject line, full summary as body, and co-authorship.

   **CRITICAL: Copy the summary text directly from the log file.** Do not retype or paraphrase it. The commit body must be character-for-character identical to what's in the log file (between `## Summary` and `---`). Read the log file and copy that exact text.

   ```bash
   git add -A
   git commit -m "Title

   [COPY-PASTE the exact summary from the log file here]

   🤖 Generated with [Claude Code](https://claude.com/claude-code)

   Co-Authored-By: Claude <noreply@anthropic.com>"
   ```

   **Why this matters:** The log file is the source of truth. If you rewrite the summary for the commit, they'll diverge — different wording, missing details, inconsistent formatting. This has happened before. Don't let it happen again.

   **Always include** the robot line and `Co-Authored-By` so commits show as authored by both of us.

10. **Push to GitHub**:
    ```bash
    git push origin main
    ```

11. **Push website to Blot**:
    ```bash
    git subtree push --prefix=website blot master
    ```

12. **Confirm** completion to the user

### Log File Format

```markdown
# Short Title (Max 30 Chars)

## Summary

Opening paragraph: 1-2 sentences describing the session's main theme or focus.

**Category 1:**
- Bullet point with specific detail
- Another bullet point
  - Sub-bullet for related details

**Category 2:**
- Bullet point
- Another bullet point

---

[Transcript content follows]
```

**Format rules:**

1. **Title**: Short, descriptive (max 30 characters). Examples: "Windows Setup", "Anchor Interview", "Log Generator Script Fix"

2. **Opening paragraph**: One or two sentences summarizing what the session was about. Sets context before the detailed breakdown.

3. **Category headers**: Bold text with colon (`**Category Name:**`). Group related work together. Examples:
   - System/environment work: "Windows Terminal Configuration", "NeoVim/Neovide Setup"
   - Feature work: "The Crucible Design (Chosen)", "Operator Redesign"
   - Bug fixes: "The Bug", "The Fix", "Linux-to-Windows Migration Fixes"
   - Documentation: "Documentation Updates", "Engine Documentation Cleanup"

4. **Bullet points**: Specific, detailed items under each category. Include:
   - What was done
   - Error messages encountered
   - What was tried, what worked, what didn't work
   - File names, function names, line numbers when relevant
   - Sub-bullets for grouping related details

5. **Chronological order**: Categories should roughly follow conversation flow. This helps future readers trace what happened and in what order.

6. **Separator**: Single `---` between summary and transcript

**Examples of good category headers:**
- "Implementation Plan Interview" — for a specific activity
- "Consistency Fixes (multiple rounds, including pre-compaction work)" — with clarifying context
- "Continuation Session (Log Formatting Fixes)" — noting session boundaries
- "The Crucible Design (Chosen)" — marking decisions made

### Accidental Closure

If the terminal closes unexpectedly:
- The JSONL transcript exists with everything up to that point
- User can resume with `claude --continue`
- Complete the end-of-session ritual when ready

### After Claude Code Updates

Run the patch script to fix the "File has been unexpectedly modified" bug on Windows:

```bash
./scripts/patch-claude-code.sh
```

This patches cli.js to disable broken timestamp validation. Must be rerun after every Claude Code update.

### `/push-website`

Quick push of website changes to Blot (no commit message):

```bash
git add -A
git commit --allow-empty-message -m ""
git subtree push --prefix=website blot master
```

---

## The Project

**Anchor** — a game engine being rewritten from Lua/LÖVE to C/Lua with SDL2, OpenGL, and Box2D.

---

## Working Style

### Incremental Steps

Don't build large systems autonomously. Instead:
1. Complete one small piece
2. Show it / let them test it
3. Get feedback
4. Then proceed

Once trust is established, this shifts to larger tasks.

### Build Order

1. Get moment-to-moment gameplay working first (physics, core mechanics)
2. Then surrounding systems (metagame, UI, progression)
3. Polish and juice come throughout, not as a final phase

### Juice and Feel

**Do not invent juice independently.** The developer has specific taste developed over years of frame-by-frame analysis.

When implementing something that needs juice:
- Ask what the juice should be
- Implement the mechanical version first, let the developer add juice
- Follow explicit juice instructions if given

### Long Responses with Code

When providing answers that are:
- Long enough to be awkward in terminal UI
- Contain multiple code examples or detailed analysis

Create a markdown file in `reference/` and open it in NeoVim with MarkdownPreview:

```bash
# Write the response to a descriptive filename
# Then open with neovim and preview
~/bin/neovim.exe reference/your-filename.md -- -c "MarkdownPreview"
```

This gives proper formatting for technical documentation. Use descriptive filenames like `anchor-loop-analysis.md`, `timer-system-notes.md`, etc.

---

## Code Patterns

### Comment Style

Use minimal single-line comments. Avoid multi-line decorative banners:

```c
// Bad
//----------------------------------------------------------
// Layer
//----------------------------------------------------------

// Good
// Layer
```

### Single-Letter Aliases

Anchor provides single-letter aliases that look like runes:

```lua
E = object                                        -- Entity/object
U = function(name_or_fn, fn) ... end              -- U(fn) or U('name', fn)
L = function(name_or_fn, fn) ... end              -- L(fn) or L('name', fn)
X = function(name, fn) return {[name] = fn} end   -- eXplicit/named
-- A is a method alias: self:A('tag') == self:all('tag')
```

In YueScript:
```yuescript
E = object
U = (name_or_fn, fn) -> ...  -- U(fn) or U('name', fn)
L = (name_or_fn, fn) -> ...  -- L(fn) or L('name', fn)
X = (name, fn) -> {[name]: fn}
```

**Future single-letter aliases** should prefer these characters, chosen for their angular, runic appearance (symmetrical, minimal roundness):

```
E, X, A, T, L, V, U, Y, I, H
```

### Object Construction

```lua
thing = class:class_new(object)
function thing:new(x, y, args)
    self:object('thing', args)
    self.x, self.y = x, y
    
    -- Add children
    self + collider('thing', 'dynamic', 'circle', 10)
    self + spring(1, 200, 10)
end

function thing:update(dt)
    self.x, self.y = self.collider:get_position()

    game:push(self.x, self.y, 0, self.spring.x, self.spring.x)
        game:circle(self.x, self.y, 10, self.color)
    game:pop()
end
```

### Inline Objects

```lua
-- With operators and single-letter alias
E() ^ {x = 100, y = 100}
   / function(self, dt)
         self.x = self.x + 50 * dt
         game:circle(self.x, self.y, 10, color)
     end
   >> arena
```

### Collision Handling (Rules-Based, in Arena)

Box2D 3.1 provides two event types:
- **Sensor events** — overlap detection, no physics response (for triggers, pickups)
- **Contact events** — physical collision with response (for walls, bouncing)

```lua
function arena:update(dt)
    -- Physical collisions (objects bounce)
    for _, c in ipairs(an:physics_get_contact_enter('player', 'enemy')) do
        local player, enemy = c.a, c.b
        player:take_damage(enemy.damage)
    end
    
    -- Sensor overlaps (objects pass through)
    for _, c in ipairs(an:physics_get_sensor_enter('player', 'pickup')) do
        local player, pickup = c.a, c.b
        pickup:collect()
    end
    
    -- High-speed impacts (for sounds/particles)
    for _, c in ipairs(an:physics_get_contact_hit('ball', 'wall')) do
        local speed = c.approach_speed
        an:sound_play(bounce_sound, {volume = speed / 100})
    end
end
```

### Timers and Springs

```lua
-- Timer is a child object
self + timer()

-- Timer methods: after(duration, [name], callback)
self.timer:after(0.5, function() self:explode() end)                    -- Anonymous
self.timer:after(0.15, 'flash', function() self.flashing = false end)   -- Named

-- Tween (provided by timer)
self.timer:tween(0.3, self, {x = 100}, math.cubic_out)

-- Springs are child objects (each spring object IS one spring)
self + spring(1, 200, 10)   -- initial, stiffness, damping (defaults to name 'spring')
self.spring:pull(0.5)

-- Use spring in drawing
game:push(self.x, self.y, 0, self.spring.x, self.spring.x)
```

### Death

```lua
-- kill() recursively sets .dead = true on self and all children
-- Actual removal happens at end-of-frame
self:kill()
```

---

## Naming Conventions

- Engine API: `an:something()`
- Layers: `game`, `effects`, `ui`
- Containers: named for contents (`enemies`, `projectiles`)
- Timer/tween tags: describe what they prevent (`'movement'`, `'attack_cooldown'`)

---

## File Organization

- Use `--{{{ }}}` fold markers for sections
- Keep files under ~1000 lines
- Single file per major system until unwieldy
- Comment non-obvious code

---

## Decision Points

**Architecture decisions:** Ask. The developer has opinions about action-based vs rules-based.

**Implementation details:** Proceed with best judgment, explain what you did.

**Juice/feel decisions:** Ask or leave placeholder. Don't guess.

**Performance optimization:** Don't prematurely optimize. Get it working first.

---

## Key Technical Decisions

### Engine vs Objects

**Engine (`an`):** C-backed services. Physics, rendering, input, audio, RNG. Always available, don't die.

**Objects:** Lua tables in a tree. Have state, update, die. When parent dies, children die immediately.

### Timers, Springs, Colliders

These are **engine objects** — child objects that wrap C-side resources. They die when their parent dies. No manual cleanup tracking.

Engine objects are named after themselves by default, so `@ + timer()` creates a child named `'timer'`, accessible via `@timer`. This pattern applies to all engine objects (timers, springs, colliders, and any future ones).

```lua
-- Engine objects as children (default names)
self + timer()                          -- Creates self.timer
self + spring(1, 200, 10)               -- Creates self.spring
self + collider('player', 'dynamic', 'circle', 10)  -- Creates self.collider

-- Multiple of same type (explicit names)
self + spring('attack', 1, 200, 10)     -- Creates self.attack
self + spring('hit', 1, 300, 15)        -- Creates self.hit
```

### Layers

Layers are **engine infrastructure**, created at startup. They're not tree objects. They don't die.

```lua
game = an:layer('game')        -- Once, at init
game:circle(x, y, r, color)    -- Use anywhere
```

### Draw Order

Draw order within a layer is **submission order** (when Lua calls draw functions), not tree order. This keeps drawing flexible — an object can draw to multiple layers, in any order.

### C/Lua Bindings

C exposes **plain functions** that take and return simple values or raw pointers (lightuserdata). No metatables, no userdata with methods, no global tables on the C side.

```lua
-- Raw C bindings (dumb, minimal)
local layer = layer_create('game')
layer_rectangle(layer, 10, 10, 50, 50, 0xFF0000FF)
layer_circle(layer, 100, 100, 25, rgba(255, 128, 0, 255))
```

The nice OOP API (`game:rectangle(...)`) is built later in YueScript on top of these primitives. This keeps the C side simple and puts the abstraction in YueScript where it belongs.

```yuescript
-- YueScript wrapper (built on raw bindings)
class Layer
  new: (name) => @_ptr = layer_create(name)
  rectangle: (x, y, w, h, color) => layer_rectangle(@_ptr, x, y, w, h, color)

game = Layer 'game'
game\rectangle 10, 10, 50, 50, 0xFF0000FF
```

---

## Common Mistakes to Avoid

1. **Creating layers as tree objects** — Layers are engine infrastructure, not gameplay objects

2. **Expecting tree order to control draw order** — It doesn't. Submission order does.

3. **Manual cleanup of timers/springs** — Not needed. They're children, they die with parent.

4. **Checking `if self.parent.dead`** — Not needed. If parent died, you're already dead.

5. **Inventing juice** — Don't. Ask or leave mechanical.

6. **Big autonomous changes** — Work incrementally, get feedback.

7. **Confusing sensors vs contacts** — Sensors are for overlap detection (triggers), contacts are for physical collisions (bouncing).

8. **Redundant owner/target fields** — If an object is a child, use `@parent` to reference the parent. Don't create redundant fields like `@owner = parent` in the constructor.

9. **Named actions in class constructors** — For class-based objects, prefer class methods (`update:`, `draw:`) over named actions created in constructors (`@ / X 'update', ...`). Named actions with X are for inline anonymous objects or behavior that needs to be dynamically added/removed at runtime.

10. **Using link when tree would work** — If the dependency is vertical (one object conceptually belongs to another), use the tree. Link is for horizontal dependencies where tree structure doesn't fit.

### Tree Limitations

The tree handles parent-child dependendies automatically (children die with parents, `@parent` provides references). However, sibling or cousin dependencies - like an enemy checking if the player is dead - still require explicit checks. This isn't a failure to use the system; horizontal relationships aren't captured by vertical tree structure.

---

## Keeping Documentation Updated

**This is critical.** These documents must stay current as the engine is implemented.

After any session that results in:
- New patterns or conventions
- Changed APIs
- Architectural decisions
- Implementation details that future Claude instances need to know

**Remind the developer** to update `ANCHOR.md` and/or `CLAUDE.md`. Proactively suggest specific additions or changes.

The **Patterns** section below should grow as we implement the engine. When we establish how something actually works in practice (not just in theory), add it here with concrete code examples.

### What to Update Where

**ANCHOR.md:**
- Technical architecture changes
- API changes (what functions exist, what they do)
- New engine services or object types
- Changes to the object tree model
- Rendering, physics, audio implementation details

**CLAUDE.md:**
- New code patterns with examples
- New conventions or naming rules
- Changed working style preferences
- New common mistakes discovered
- Updates to the Patterns section below

---

## Patterns

*This section will grow as we implement the engine. Add concrete, tested patterns here — not theoretical ones.*

### Inline Single-Use Code

If a function is only called once, inline it instead of extracting it. This preserves locality — you see everything in one place without jumping around.

```lua
-- Bad: splits up code that's only used in one place
function arena:new(args)
    self:object('arena', args)
    self:setup_physics()
    self:setup_input()
    self:create_objects()
end

-- Good: everything visible in constructor
function arena:new(args)
    self:object('arena', args)
    
    -- Physics setup
    an:physics_set_gravity(0, 0)
    an:physics_enable_contact_between('player', {'enemy', 'wall'})
    an:physics_enable_sensor_between('player', {'pickup'})
    
    -- Input setup
    an:input_bind('move_left', {'key:a', 'key:left'})
    an:input_bind('move_right', {'key:d', 'key:right'})
    
    -- Create objects
    self + player(240, 135)
         + { wall('top'), wall('bottom'), goal('left'), goal('right') }
end
```

### Inline Objects with Operators

The operator chain reads left-to-right: create → configure → add behavior → flow to parent.

**With property table:**
```lua
E('ball') ^ {x = 240, y = 135, vx = 100, vy = 100}
          / function(self, dt)
                self.x = self.x + self.vx * dt
                game:circle(self.x, self.y, 8, an.colors.white)
            end
          >> arena

-- Later: arena.ball exists
-- Adding another 'ball' kills the old one
```

**With build function (for method calls):**
```lua
E() ^ function(self)
          self.x, self.y = 100, 200
          self + timer()
      end
    / function(self, dt)
          game:circle(self.x, self.y, 10, color)
      end
    >> arena
```

**With multiple action phases:**
```lua
E() ^ {x = 100, y = 200}
   / U(function(self, dt)
         -- early: before main updates
     end)
   / function(self, dt)
         -- main update
     end
   / L(function(self, dt)
          -- late: after main updates, good for drawing
      end)
   >> arena
```

**Multiple `^` operators:** When chaining multiple `^`, group all initialization before action operators:
```lua
-- Good: all init together, then actions
E() ^ {x = 100, y = 200}
   ^ function(self) self + timer() end
   / function(self, dt) ... end
   >> arena

-- Bad: init split across the chain
E() ^ {x = 100}
   / function(self, dt) ... end
   ^ function(self) self + timer() end -- init after action, harder to follow
   >> arena
```

### Named vs Anonymous

The tree automatically manages named children: adding a child with a name that already exists kills the old child first. This provides cancellation and replacement without explicit tag systems.

**Objects:**
```lua
-- Anonymous
E() ^ {x = 100} >> arena

-- Named (arena.ball points to this)
E('ball') ^ {x = 100} >> arena
```

**Timer callbacks:**
```lua
-- Anonymous
self.timer:after(0.5, function() self.can_shoot = true end)

-- Named (self.timer.flash points to this, can be killed/replaced)
self.timer:after(0.15, 'flash', function() self.flashing = false end)

-- Cancel
if self.timer.flash then self.timer.flash:kill() end

-- Replace (old 'flash' killed automatically)
self.timer:after(0.15, 'flash', function() self.flashing = false end)
```

**Actions:**
```lua
-- Anonymous actions with phase helpers
E() / U(early_fn) / update_fn / L(late_fn) >> arena

-- Named actions with phase helpers
self / U('water_sim', fn)   -- named early
self / X('update', fn)      -- named main
self / L('draw', fn)        -- named late

-- Named with methods (alternative)
self:action('update', fn)
self:early_action('water_sim', fn)
self:late_action('water_draw', fn)

-- Cancel
self.water_sim:kill()
```

### Hierarchy Building with `+`

Use the `+` operator to build object hierarchies declaratively (parent-centric style):

```lua
-- In constructor
function arena:new(args)
    self:object('arena', args)

    self + timer()
         + paddle('left', 30, 135)
         + paddle('right', 450, 135)
         + ball()
         + { wall('top', 240, 5),
             wall('bottom', 240, 265),
             goal('left', -5, 135),
             goal('right', 485, 135) }
end

-- Dynamically adding during gameplay
function arena:spawn_enemy(x, y)
    self + enemy(x, y)
end

-- Multiple at once
function arena:spawn_wave()
    self + {
        enemy(100, 100),
        enemy(200, 100),
        enemy(300, 100),
    }
end
```

### Pipeline Style with `>>`

Use `>>` for object-centric pipeline construction:

```lua
-- Object flows into parent
E('ball') ^ {x = 100, y = 200}
          / function(self, dt)
                game:circle(self.x, self.y, 10, color)
            end
          >> arena
```

**Choosing between `+` and `>>`:**
- `+` is parent-centric: "arena gains these children"
- `>>` is object-centric: "this object flows into arena"

Use `+` for adding multiple siblings, `>>` for inline object construction.

### Mixing `+` with Inline Objects

When combining `+` with inline object construction, parentheses are needed:

```lua
arena + paddle('left', 30, 135)
      + paddle('right', 450, 135)
      + (E('ball') ^ {x = 240, y = 135}
                   / function(self, dt)
                         game:circle(self.x, self.y, 8, an.colors.white)
                     end)
```

### Bulk Operations with `all()` or `A()`

Use `all()` or the `A()` alias to operate on multiple objects:

```lua
-- Iteration for method calls or property setting
for _, enemy in ipairs(arena:A('enemy')) do
    enemy:take_damage(10)
end

-- Count checks
if #arena:A('enemy') == 0 then
    arena:next_wave()
end

-- Combined with conditionals
for _, item in ipairs(arena:A('item')) do
    if item.rarity == 'legendary' then
        item:sparkle()
    end
end
```

### Named Timers Pattern

Use named timer callbacks when you need to cancel or replace an ongoing timer:

```lua
function enemy:flash()
    self.flashing = true
    self.timer:after(0.15, 'flash', function()
        self.flashing = false
    end)
end

-- Calling flash() again replaces the old callback
-- So rapid hits extend the flash rather than stacking callbacks

function enemy:stun(duration)
    -- Cancel any existing stun
    if self.timer.stun then self.timer.stun:kill() end

    self.stunned = true
    self.timer:after(duration, 'stun', function()
        self.stunned = false
    end)
end
```

### Named Actions Pattern

Use named actions with the `U`, `L`, and `X` helpers when you need to cancel or replace ongoing behavior:

```lua
function arena:new()
    self:object('arena')

    -- Named early action
    self / U('water_sim', function(self, dt)
        -- propagate spring velocities
    end)

    -- Named late action
    self / L('water_draw', function(self, dt)
        -- build water surface polyline
    end)
end

function arena:pause_water()
    if self.water_sim then self.water_sim:kill() end
    if self.water_draw then self.water_draw:kill() end
end

function arena:resume_water()
    self / U('water_sim', function(self, dt)
        -- propagate spring velocities
    end)
    self / L('water_draw', function(self, dt)
        -- build water surface polyline
    end)
end
```

**YueScript equivalent:**
```yuescript
class arena extends object
  new: =>
    super 'arena'

    @ / U 'water_sim', (dt) =>
      -- propagate spring velocities

    @ / L 'water_draw', (dt) =>
      -- build water surface polyline

  pause_water: =>
    @water_sim\kill! if @water_sim
    @water_draw\kill! if @water_draw
```

### Placeholder: Spring Usage
```lua
-- Pattern will be documented once spring system is implemented
```

### Placeholder: Collider Setup
```lua
-- Pattern will be documented once collider system is implemented
```

### Placeholder: Arena Collision Handling
```lua
-- Pattern will be documented once physics queries are implemented
```

### Placeholder: Layer Usage
```lua
-- Pattern will be documented once rendering is implemented
```

### Horizontal Links

For sibling or cousin dependencies that the tree can't express:
```lua
-- Enemy dies when player dies (siblings under arena)
function enemy:new(x, y)
    self:object()
    self:link(self.arena.player)
end

-- With callback for custom death behavior
function enemy:new(x, y)
    self:object()
    self:link(self.arena.player, function()
        self:flee()  -- scatter instead of dying
    end)
end

-- Tether between two units (can't be child of both)
function tether:new(a, b)
    self:object()
    self:link(a)
    self:link(b)
end
```

**Don't use link when tree works:**
```lua
-- Bad: shield could just be a child
self:link(owner)

-- Good: use the tree
owner + shield()
```

*When implementing each system, replace the placeholder with the actual working pattern, including any gotchas or non-obvious details discovered during implementation.*

---

## Anchor Context Brief

*This section captures the reasoning, philosophy, and decision-making style behind Anchor's design. Read this to understand *why* decisions were made and *how* to evaluate new ideas.*

### The Developer

a327ex is a solo indie game developer who has shipped successful games (BYTEPATH, SNKRX). Works primarily by intuition — choices are made because they "feel right" or "look nice" before rational justification. Rational thinking fills in the parts intuition doesn't have opinions on.

This means:
- Aesthetic judgments are valid and often primary ("this syntax looks better")
- Exploring the possibility space matters more than committing early
- Conversation is the design tool — talking through options surfaces what feels right
- Specs are artifacts of conversation, not contracts to implement literally

When evaluating features: "Would this feel right to use?" matters as much as "Is this technically sound?"

### Core Design Values

#### Locality Above All

Code should be understandable by looking at one place. This serves:
- Human cognition (limited working memory)
- LLM cognition (finite context window)
- Solo developer workflow (must understand own code months later)

The operator syntax exists because it lets you declare an object, its properties, its behaviors, and its place in the tree *in one expression*:

```yuescript
E 'player' ^ {x: 100, y: 100}
  / X 'movement', (dt) => @x += @vx * dt
  >> arena
```

Compare to systems where you define a class in one file, register components elsewhere, wire up behaviors in a third place. Same expressiveness, but the understanding is scattered.

**When evaluating features:** Does this keep related things together, or scatter them?

#### No Bureaucracy

Imports, exports, dependency injection, configuration objects, registration systems — these are bureaucracy. They may be necessary in large teams, but for solo development they're friction.

Anchor prefers:
- Globals that are just available (`an`, `E`, `U`, `L`, `X`, `A`)
- Direct mutation over message passing
- Explicit calls over implicit event systems
- Things that work without setup

**When evaluating features:** Does this require ceremony to use, or does it just work?

#### Alien But Right

The operator syntax (`^`, `/`, `+`, `>>`) and single-letter aliases (`E`, `U`, `L`, `X`, `A`) are deliberately unusual. Inspired by Hoon's rune system — symbols that seem foreign at first but become natural with use.

The goal isn't familiarity, it's terseness and visual distinctiveness. `E 'player'` is weirder than `Object.new('player')` but it's shorter and once learned, instantly recognizable.

Single-letter aliases follow a preference for angular, symmetrical letters: E, X, A, T, L, V, U, Y, I, H. Avoid round letters (O, Q, C, G).

**When evaluating features:** Does this earn its complexity? Is the weirdness worth it?

### Language Boundary Philosophy

#### Why YueScript

Chosen because it "looked nice." That's the real reason. MoonScript syntax with additional features, compiles to Lua. The class system, the `@` for self, the significant whitespace, the operator overloading — it produces code that's pleasant to read and write.

The `-r` compiler flag rewrites output to match source line numbers, so Lua errors point to correct YueScript lines. No source map parsing needed.

#### Why C (Not C++, Rust, etc.)

Simplicity. C has fewer concepts to manage. The entire Lua C API is designed for C. No RAII, no templates, no borrow checker — just functions and data.

C++ could work but adds complexity without clear benefit for this scope. Rust would be interesting but the Lua interop story is less mature.

#### The Boundary Rule

Start with everything in Lua/YueScript. Only move to C when profiling shows actual bottlenecks.

This avoids premature optimization. The tree structure, timers, springs, actions — all start as Lua objects. If timer updates become a bottleneck (unlikely), move the hot loop to C while keeping the Lua interface.

The gameplay programmer never touches C. The engine layer (also in YueScript) wraps C calls. Raw pointers flow from C to Lua and back; Lua is responsible for calling destroy functions.

**When evaluating features:** Should this be C (performance-critical, low-level) or YueScript (game logic, flexible)?

### Deliberate Deferrals

Some features are intentionally left for later:

- **High-res text layer:** Bitmap fonts at game resolution first. High-res text layer (rendering at window resolution) adds quality for UI text. Slot in later without changing text effect architecture.
  
- **Hot reloading:** Would require runtime YueScript compilation. Nice for iteration but not essential. Add when the pain of restart-to-test becomes significant.

- **Debug tooling:** Console overlay, variable inspector, etc. Add when needed for specific debugging problems.

- **Steam Input:** Basic SDL2 gamepad first. Steam Input when preparing for actual Steam release.

These aren't forgotten — they're scoped out of initial build to maintain focus.

**When evaluating features:** Is this essential now, or a later enhancement?

### How to Evaluate New Features

When reviewing other engines/frameworks, ask:

1. **Does Anchor already have this?** (Check ANCHOR.md, the object tree might already cover it)

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

### Communication Style

When the implementation plan says "Ask the developer before implementing," it means:
- Present options, not conclusions
- Surface tradeoffs explicitly
- Let the conversation find what feels right
- Don't assume the spec is final — it's a starting point

The developer appreciates honesty, critique, and contradiction. Sycophancy is unwelcome. If something seems wrong, say so.
