# Anchor Implementation Plan

C-first approach with YueScript as the scripting layer. Minimize plain Lua; write game-facing code in YueScript from the start.

**Target Game:** Emoji Ball Battles — Minesweeper dungeon exploration + physics auto-battler combat.

---

## Build Strategy

### YueScript Compilation

**Decision: Build-time compilation**

Compile `.yue` → `.lua` during build, not at runtime. This:
- Eliminates runtime dependency on YueScript compiler
- Produces debuggable `.lua` files for error inspection
- Simplifies the C side (just loads standard Lua)
- Can add runtime compilation later for hot reloading

**Build flow:**
```
game.yue ──► yue -r ──► game.lua ──► embedded in executable / loaded at runtime
                ↑
                └── -r flag preserves line numbers for debugging
```

**Directory structure:**
```
anchor/
├── src/                    # C source
├── yue/                    # YueScript engine code (compiled to lua/)
│   ├── object.yue
│   ├── timer.yue
│   ├── spring.yue
│   ├── collider.yue
│   └── init.yue            # Sets up E, X, A aliases
├── lua/                    # Compiled Lua output (git-ignored or committed)
├── game/                   # Game YueScript (compiled to game_lua/)
│   └── main.yue
├── game_lua/               # Compiled game Lua
├── assets/
├── build.bat               # Windows build (includes yue compilation)
└── build.sh                # Web build (Emscripten)
```

---

## Phase 1: C Skeleton + Web Build

**Goal:** Window opens, renders a colored background, processes input, runs Lua. Works on both Windows and Web.

### 1.1 Project Setup
- [ ] Create directory structure
- [ ] Set up build.bat for Windows (cl.exe or gcc)
- [ ] Set up build.sh for Web (Emscripten)
- [ ] Download/configure dependencies:
  - SDL2 (window, input, audio)
  - Lua 5.4 (scripting)
  - stb_image (texture loading)
  - stb_truetype (font loading)

### 1.2 Main Loop
- [ ] SDL2 initialization
- [ ] Create SDL_Texture for framebuffer presentation
- [ ] Fixed timestep game loop (60 FPS target)
- [ ] Basic input polling
- [ ] Clean shutdown
- [ ] Web: `emscripten_set_main_loop` integration

### 1.3 Lua Integration
- [ ] Initialize Lua state
- [ ] Load and run a test `.lua` file
- [ ] Protected call wrapper with error display
- [ ] Error state (show error on screen, keep running)

### 1.4 Web-Specific Setup
- [ ] Emscripten SDK configured
- [ ] Asset preloading strategy (embed or fetch)
- [ ] HTML shell template
- [ ] Verify builds run in browser

### 1.5 Verification
```c
// C side calls Lua init, then update each frame
// Lua can print to console
// Errors display on screen instead of crashing
// Works identically on Windows and Web
```

**Deliverable:** Window that runs Lua code with error handling, on both platforms.

**Important:** Every subsequent phase must be verified on both Windows and Web before moving on.

---

## Phase 2: Rendering

**Goal:** Draw shapes and sprites to screen with layers using software rendering.

### 2.1 Framebuffer and Primitives
- [ ] Framebuffer struct (pixels, width, height, stride)
- [ ] framebuffer_create, framebuffer_destroy, framebuffer_clear
- [ ] Color utilities (pack/unpack RGBA, blend_over)
- [ ] draw_rect_fill, draw_rect_outline
- [ ] draw_line (Bresenham)
- [ ] draw_circle_fill, draw_circle_outline

### 2.2 Sprite System
- [ ] Sprite loading (stb_image)
- [ ] blit_alpha (axis-aligned with alpha blending)
- [ ] blit_scaled (runtime scaling)
- [ ] Precomputed rotation generation
- [ ] RotatedSprite structure (base + rotated variants)
- [ ] blit_rotated_scaled (select precomputed rotation, apply scale)

### 2.3 Layer System
- [ ] Layer struct (framebuffer + command buffer)
- [ ] Command buffer (batched draw commands)
- [ ] layer_flush (process commands, draw to framebuffer)
- [ ] Transform stack (push/pop)
- [ ] Layer composition to screen (alpha blend)

### 2.4 Effect System

**Scope limited to what Emoji Ball Battles needs:**

- [ ] Effect types enum and params struct
- [ ] effect_apply dispatcher
- [ ] **Outline effect** (ball outlines, UI elements)
- [ ] **Tint effect** (hit flash, color modulation)
- [ ] **Brightness effect** (damage feedback, highlights)
- [ ] Per-layer effect configuration

**Deferred:** Blur and other advanced effects. Add later if needed.

### 2.5 Lua Bindings
```lua
-- Exposed to Lua
game = an:layer('game')
game:circle(x, y, radius, color)
game:rectangle(x, y, w, h, rx, ry, color)
game:line(x1, y1, x2, y2, color, width)
game:draw_image(img, x, y, r, sx, sy, ox, oy, color)
game:push(x, y, r, sx, sy)
game:pop()
game:set_effect('outline', {color = 0x000000FF, thickness = 1})
game:set_effect('tint', {color = 0xFF0000FF})
game:set_effect('brightness', {value = 1.5})
game:clear_effect()
```

**Deliverable:** Can draw shapes and sprites from Lua with transform stack and effects.

**Verify on both Windows and Web before proceeding.**

---

## Phase 3: Input

**Goal:** Action-based input with keyboard and mouse bindings.

### 3.1 Input State Tracking
- [ ] Keyboard state (down, pressed, released)
- [ ] Mouse state (position, buttons)
- [ ] Per-frame state transitions

### 3.2 Binding System
- [ ] Action → input mapping
- [ ] Multiple inputs per action
- [ ] Input string parsing (`key:a`, `mouse:1`, `key:space`)

### 3.3 Lua Bindings
```lua
an:input_bind('move_up', {'key:w', 'key:up'})
an:input_bind('move_down', {'key:s', 'key:down'})
an:input_bind('move_left', {'key:a', 'key:left'})
an:input_bind('move_right', {'key:d', 'key:right'})
an:input_bind('confirm', {'mouse:1', 'key:space', 'key:return'})

if an:is_pressed('move_up') then ... end
if an:is_down('move_left') then ... end
if an:is_released('confirm') then ... end

local mx, my = an:mouse_position()
```

**Deliverable:** Keyboard and mouse input usable from Lua.

**Verify on both Windows and Web before proceeding.**

---

## Phase 4: Audio

**Goal:** Play sounds and music.

### 4.1 SDL_mixer Setup
- [ ] Audio initialization
- [ ] Sound loading (WAV, OGG)
- [ ] Music loading (OGG)

### 4.2 Playback
- [ ] Sound playback (fire and forget)
- [ ] Sound volume control
- [ ] Music playback (loop, stop)
- [ ] Music volume control
- [ ] Web: Handle audio context (requires user interaction to start)

### 4.3 Lua Bindings
```lua
local hit_sfx = an:sound_load('hit.ogg')
local mine_sfx = an:sound_load('mine.ogg')
an:sound_play(hit_sfx)
an:sound_volume(hit_sfx, 0.5)

local bgm = an:music_load('bgm.ogg')
an:music_play(bgm)
an:music_stop()
an:music_volume(0.5)
```

**Deliverable:** Audio playable from Lua.

**Verify on both Windows and Web before proceeding.**

---

## Phase 5: Physics

**Goal:** Box2D 3.1 integration with sensor and contact events for ball combat.

### 5.1 World Setup
- [ ] Box2D world creation
- [ ] Gravity configuration
- [ ] Fixed timestep stepping

### 5.2 Body Management
- [ ] Body creation (static, dynamic, kinematic)
- [ ] Shape types: circle, rectangle, polygon
- [ ] Return raw pointer/handle to Lua (Lua manages lifetime, passes pointer back to C calls)

### 5.3 Collision System (Box2D 3.1)

Box2D 3.1 separates sensors (overlap detection) from contacts (physical collision):

**Sensors:**
- Created by setting `isSensor = true` on shape definition
- No physics response (objects pass through)
- Generate begin/end overlap events
- Retrieved via `b2World_GetSensorEvents()`

**Contacts:**
- Normal non-sensor shapes
- Physics response (objects bounce)
- Generate begin/end touch events plus hit events for high-speed collisions
- Retrieved via `b2World_GetContactEvents()`

Implementation:
- [ ] Collision tag system
- [ ] Enable/disable sensor between tags
- [ ] Enable/disable contact between tags
- [ ] Sensor event buffering (enter, exit)
- [ ] Contact event buffering (enter, exit, hit)
- [ ] Query functions (raycast, AABB, circle, point)

### 5.4 Lua Bindings
```lua
an:physics_set_gravity(0, GRAVITY)  -- Value TBD through playtesting

-- Configure which tags interact
an:physics_enable_contact_between('player_ball', {'enemy_ball', 'wall'})
an:physics_enable_sensor_between('player', {'pickup', 'trigger'})

-- Query contact events each frame (physical collisions)
for _, c in ipairs(an:physics_get_contact_enter('player_ball', 'enemy_ball')) do
    local player, enemy = c.a, c.b
    local nx, ny = c.nx, c.ny
    -- Deal damage based on collision
end

-- Query sensor events each frame (overlap detection)
for _, s in ipairs(an:physics_get_sensor_enter('player', 'pickup')) do
    local player, pickup = s.a, s.b
    pickup:collect()
end

-- Query high-speed impacts
for _, h in ipairs(an:physics_get_contact_hit('ball', 'wall')) do
    local speed = h.approach_speed
    an:sound_play(bounce_sfx, speed / 100)
end

-- On-demand spatial queries
local enemies = an:physics_query_circle(x, y, radius, {'enemy'})
local hit = an:physics_raycast_closest(x1, y1, x2, y2, {'wall', 'enemy'})
local all_hits = an:physics_raycast(x1, y1, x2, y2, {'wall'})
local in_area = an:physics_query_aabb(x, y, w, h, {'pickup'})

-- Body management (called from collider wrapper)
local body_id = an:physics_create_body(tag, body_type, shape_type, ...)
an:physics_destroy_body(body_id)
an:physics_get_position(body_id)
an:physics_set_velocity(body_id, vx, vy)
```

**Deliverable:** Physics simulation with sensor/contact events and spatial queries from Lua.

**Verify on both Windows and Web before proceeding.**

---

## Phase 6: Random

**Goal:** Seedable PRNG for determinism (dungeon generation, physics seeding).

### 6.1 Implementation
- [ ] PCG or xorshift PRNG
- [ ] Seed function
- [ ] Float, int, angle, sign functions

### 6.2 Lua Bindings
```lua
an:random_seed(12345)
local x = an:random_float(0, 100)
local i = an:random_int(1, 10)
local r = an:random_angle()  -- 0 to 2π
local s = an:random_sign()   -- -1 or 1
```

**Deliverable:** Deterministic random from Lua.

**Verify on both Windows and Web before proceeding.**

---

## Phase 6.5: Text Rendering

**Goal:** Draw text for UI (mine numbers, HP, gold, shop prices). Emojis are drawn as images, not text.

### 6.5.1 Approach: TTF with Baked Atlas (stb_truetype)

Load TTF files directly, bake to texture atlas at load time.

- [ ] Include stb_truetype.h (single header, like stb_image)
- [ ] Font loading:
  - Load TTF file into memory
  - Call `stbtt_BakeFontBitmap()` to render glyphs to atlas
  - Store baked character data (positions, advances)
  - Create texture from atlas
- [ ] Support multiple sizes (bake each size as separate atlas)
- [ ] Draw characters as textured quads using baked metrics

### 6.5.2 Lua Bindings
```lua
an:font_load('default', 'assets/fonts/myfont.ttf', 24)  -- name, path, size
an:font_load('large', 'assets/fonts/myfont.ttf', 48)    -- same TTF, different size
an:font_get_text_width('default', 'HP: 3')
layer:draw_text('HP: 3', 'default', x, y)
layer:draw_text('99', 'large', x, y, r, sx, sy)  -- with optional transform
```

### 6.5.3 Implementation Notes

```c
// Baking at load time (~50-100 lines)
unsigned char ttf_buffer[1<<20];
unsigned char atlas_bitmap[512*512];
stbtt_bakedchar cdata[96];  // ASCII 32-127

fread(ttf_buffer, 1, 1<<20, fopen(path, "rb"));
stbtt_BakeFontBitmap(ttf_buffer, 0, font_size, atlas_bitmap, 512, 512, 32, 96, cdata);
// Create texture from atlas_bitmap
// Store cdata for drawing
```

**Deliverable:** Can load TTF fonts and draw text from Lua.

**Verify on both Windows and Web before proceeding.**

---

## Phase 7: YueScript Object System

**Goal:** Full object tree in YueScript with operators and YueScript class integration.

This is where we write YueScript, not Lua.

### 7.1 YueScript Build Integration
- [ ] Install YueScript compiler (`luarocks install yuescript` or standalone)
- [ ] Add yue compilation step to build.bat/build.sh (use `-r` flag for line number preservation)
- [ ] Verify .yue → .lua compilation works
- [ ] Verify error line numbers match .yue source

### 7.2 Base Object Class (`yue/object.yue`)

**Ask the developer before implementing.** The object class needs:
- Constructor with name/args handling
- `kill()` for death propagation
- `add_child()` for tree management
- `tag()` for tagging
- `all()` / `A()` for queries
- Operators (`^`, `%`, `/`, `//`, `+`, `>>`) with `__inherited` propagation

The exact method signatures, timer/spring/collider integration, and helper methods should be discussed with the developer before writing code.

### 7.3 Timer, Spring, Collider, Action Objects

**Ask the developer before implementing.** These will be child objects added via `@ + collider ...` etc., but the exact:
- Constructor signatures
- Which methods to expose
- How they integrate with the update loop
- Memory management patterns (raw C pointers)

...should be decided in conversation when we reach this phase.

### 7.4 Init and Aliases (`yue/init.yue`)

```yuescript
-- Single-letter aliases
export E = object
export X = (name, fn) -> {[name]: fn}
-- A is a method alias on object

-- Factory functions use operators: @ + collider 'tag', 'dynamic', 'circle', 12
```

### 7.5 Update Loop Integration

**Ask the developer before implementing.** The C side calls into Lua/YueScript for updates, but the exact:
- How phases (early, action, late) are handled
- Tree traversal order
- Dead object cleanup timing
- Integration with C's main loop

...involves assumptions that should be validated before writing code.

**Deliverable:** Full object system in YueScript with operators, timers, springs, colliders, and phased actions.

---

## Phase 8: Emoji Ball Battles - Core Game

**Goal:** Build the actual game, not a throwaway test. This phase implements Emoji Ball Battles Layers 0-2.

### 8.1 Layer 0: Physics Combat (`game/combat.yue`)

Two balls fighting in an arena with gravity.

```yuescript
-- Combat arena
class Arena extends object
  new: (w, h) =>
    super 'arena'
    @w, @h = w, h
    -- Create walls (static bodies)
    @ + collider 'wall', 'static', 'rect', 0, 0, w, 10      -- top
    @ + collider 'wall', 'static', 'rect', 0, h-10, w, 10   -- bottom
    @ + collider 'wall', 'static', 'rect', 0, 0, 10, h      -- left
    @ + collider 'wall', 'static', 'rect', w-10, 0, 10, h   -- right

-- Ball entity
class Ball extends object
  new: (x, y, hp, damage, team) =>
    super 'ball'
    @x, @y = x, y
    @hp, @max_hp = hp, hp
    @damage = damage
    @team = team
    @radius = 20
    
    @ + collider team .. '_ball', 'dynamic', 'circle', @radius
    @ + spring 'hit', 1, 200, 10  -- For hit feedback
    
    @ / (dt) =>
      @x, @y = @collider\get_position!
    
    @ // (dt) =>
      s = @hit.x
      game\push @x, @y, 0, s, s
      game\circle @x, @y, @radius, @team == 'player' and {0, 1, 0, 1} or {1, 0, 0, 1}
      game\pop!
      -- HP bar
      game\rectangle @x - 15, @y - 30, 30 * (@hp / @max_hp), 4, 0, 0, {0, 1, 0, 1}

  take_damage: (amount) =>
    @hp -= amount
    @hit\pull 0.3
    an\sound_play hit_sfx
    if @hp <= 0
      @\kill!

-- Combat manager
class Combat extends object
  new: (player_ball, enemy_ball) =>
    super 'combat'
    @arena = Arena(480, 270) >> @
    @player = player_ball >> @
    @enemy = enemy_ball >> @
    @result = nil
    
    @ / (dt) =>
      -- Check contact events (physical collisions)
      for c in *an\physics_get_contact_enter 'player_ball', 'enemy_ball'
        @player\take_damage @enemy.damage
        @enemy\take_damage @player.damage
      
      -- Check win/lose
      if @player.dead
        @result = 'lose'
      elseif @enemy.dead
        @result = 'win'
```

### 8.2 Layer 1-2: Dungeon Navigation (`game/dungeon.yue`)

Grid-based Minesweeper dungeon.

```yuescript
class Dungeon extends object
  new: (width, height, mine_count) =>
    super 'dungeon'
    @w, @h = width, height  -- Values TBD through playtesting
    @tiles = {}
    @player_x, @player_y = 0, 0
    @player_hp = 3
    
    -- Generate grid
    for y = 0, @h - 1
      @tiles[y] = {}
      for x = 0, @w - 1
        @tiles[y][x] = {
          revealed: false
          is_mine: false
          adjacent_mines: 0
          is_boss: false
        }
    
    -- Place mines (avoiding start and adjacent to start)
    @\place_mines mine_count
    
    -- Place boss (furthest corner)
    @tiles[@h - 1][@w - 1].is_boss = true
    
    -- Calculate adjacent mine counts
    @\calculate_numbers!
    
    -- Reveal start
    @tiles[0][0].revealed = true
    
    -- Input handling
    @ / (dt) =>
      if an\is_pressed 'move_up' then @\try_move 0, -1
      if an\is_pressed 'move_down' then @\try_move 0, 1
      if an\is_pressed 'move_left' then @\try_move -1, 0
      if an\is_pressed 'move_right' then @\try_move 1, 0
    
    -- Rendering
    @ // (dt) =>
      for y = 0, @h - 1
        for x = 0, @w - 1
          @\draw_tile x, y
      -- Draw player
      px, py = @player_x * 32 + 16, @player_y * 32 + 16
      game\circle px, py, 10, {0, 0.8, 1, 1}
      -- Draw HP
      game\draw_text 'HP: ' .. @player_hp, 'default', 10, 10

  try_move: (dx, dy) =>
    nx, ny = @player_x + dx, @player_y + dy
    return if nx < 0 or nx >= @w or ny < 0 or ny >= @h
    
    @player_x, @player_y = nx, ny
    tile = @tiles[ny][nx]
    
    if not tile.revealed
      tile.revealed = true
      if tile.is_mine
        @player_hp -= 1
        an\sound_play mine_sfx
        if @player_hp <= 0
          -- Game over
    
    if tile.is_boss
      -- Transition to combat
      @\start_boss_fight!

  draw_tile: (x, y) =>
    tile = @tiles[y][x]
    sx, sy = x * 32, y * 32
    
    if not tile.revealed
      game\rectangle sx, sy, 30, 30, 2, 2, {0.3, 0.3, 0.4, 1}
      game\draw_text '?', 'default', sx + 10, sy + 8
    elseif tile.is_mine
      game\rectangle sx, sy, 30, 30, 2, 2, {0.5, 0.1, 0.1, 1}
      game\draw_text '💣', 'default', sx + 6, sy + 6
    elseif tile.is_boss
      game\rectangle sx, sy, 30, 30, 2, 2, {0.6, 0.5, 0.1, 1}
      game\draw_text '⭐', 'default', sx + 6, sy + 6
    else
      game\rectangle sx, sy, 30, 30, 2, 2, {0.2, 0.2, 0.25, 1}
      if tile.adjacent_mines > 0
        game\draw_text tostring(tile.adjacent_mines), 'default', sx + 10, sy + 8
```

### 8.3 Main Game Loop (`game/main.yue`)

```yuescript
init = ->
  -- Resolution and scale TBD
  an\anchor_start 'Emoji Ball Battles', 640, 360, 2, 2
  
  an\input_bind 'move_up', {'key:w', 'key:up'}
  an\input_bind 'move_down', {'key:s', 'key:down'}
  an\input_bind 'move_left', {'key:a', 'key:left'}
  an\input_bind 'move_right', {'key:d', 'key:right'}
  an\input_bind 'confirm', {'mouse:1', 'key:space'}
  
  -- Gravity value TBD through playtesting
  an\physics_set_gravity 0, GRAVITY
  
  -- Configure contact events for ball combat
  an\physics_enable_contact_between 'player_ball', {'enemy_ball', 'wall'}
  an\physics_enable_contact_between 'enemy_ball', {'wall'}
  
  game = an\layer 'game'
  game\set_effect 'outline', {color = 0x000000FF, thickness = 1}
  
  -- Load sounds
  export hit_sfx = an\sound_load 'hit.ogg'
  export mine_sfx = an\sound_load 'mine.ogg'
  
  -- Dungeon size TBD through playtesting
  an + Dungeon DUNGEON_WIDTH, DUNGEON_HEIGHT, MINE_COUNT
```

### 8.4 Verification Checklist
- [ ] Combat: Two balls spawn and fight with gravity
- [ ] Combat: Contact deals damage, HP bars update
- [ ] Combat: Hit feedback (spring squash, sound, screen shake)
- [ ] Combat: Ball death ends combat with win/lose result
- [ ] Dungeon: Grid renders with fog of war
- [ ] Dungeon: WASD/arrows move player
- [ ] Dungeon: Tile reveals on entry
- [ ] Dungeon: Mine numbers display correctly (8-neighbor count)
- [ ] Dungeon: Stepping on mine deals damage, plays sound
- [ ] Dungeon: HP reaches 0 ends run
- [ ] Dungeon: Reaching boss triggers combat
- [ ] Integration: Combat victory returns to dungeon (next floor)
- [ ] Integration: Combat defeat ends run
- [ ] Both Windows and Web verified

**Deliverable:** Playable Emoji Ball Battles with dungeon exploration and physics combat.

---

## Summary: Build Order

| Phase | Focus | Deliverable |
|-------|-------|-------------|
| 1 | C Skeleton + Web | Window + Lua + error handling (Windows & Web) |
| 2 | Rendering | Software renderer, shapes, sprites, layers, effects (outline, tint, brightness) |
| 3 | Input | Keyboard, mouse bindings |
| 4 | Audio | Sounds and music |
| 5 | Physics | Box2D 3.1 with sensor/contact events and spatial queries |
| 6 | Random | Seedable PRNG |
| 6.5 | Text | TTF font loading, glyph rendering |
| 7 | YueScript | Object tree, operators, timers, springs |
| 8 | Game | Emoji Ball Battles Layers 0-2 (combat + dungeon) |

**Critical:** Every phase must be verified on both Windows and Web before proceeding.

**Deferred (add later if needed):**
- Gamepad support (Steam release prep)
- Steam Input (Steam release prep)
- Blur and advanced effects
- Hot reloading
- Debug tooling
- SDF text rendering (for website)

**Estimated time:** Phases 1-6.5 (C foundation) are the bulk of the work. Phase 7 (YueScript) requires developer consultation for implementation details. Phase 8 is the actual game.

---

## Decisions Made

1. **Hot reloading:** Deferred. Add later if needed.

2. **Debug tooling:** Deferred. Add later if needed.

3. **Asset pipeline:** None. Load directly from disk.

4. **YueScript line numbers:** Use `-r` flag to rewrite compiled output to match original line numbers. Lua errors will report correct YueScript lines natively.

5. **Collider/spring pattern:** Use operators (`@ + collider ...`, `@ + spring ...`).

6. **Resource lifetimes:**
   - Textures: Live forever
   - Sounds: Live forever
   - Sound instances: Die when playback ends
   - Physics bodies: Explicit destroy, freed at end of frame
   - General rule: Explicit destroy calls, except where automatic cleanup is obvious

7. **Module organization:** Global, no bureaucracy. YueScript files don't need elaborate import/export ceremony. Engine globals (`an`, `E`, `X`, `A`, etc.) are just available.

8. **Camera:** Implemented in YueScript using push/pop transform stack. Not a C-level feature. See existing camera.lua for reference patterns (parallax, shake, world↔screen coordinate conversion).

9. **Text effects:** The per-character effect system stays in YueScript. C handles glyph rendering, Lua handles effect logic. Batching limited by effect flexibility (each character can have unique transform).

10. **Gamepad:** Deferred until Steam release preparation. Keyboard + mouse sufficient for development.

11. **Effects scope:** Only outline, tint, brightness implemented initially. Blur and other effects added if needed.

12. **Phase 8 is the game:** No throwaway test game. Phase 8 builds Emoji Ball Battles directly.

13. **Box2D 3.1 events:** Use sensor events for overlap detection (triggers, pickups) and contact events for physical collisions (bouncing, damage). Use hit events for high-speed impact feedback (sounds, particles).

---

## Next Steps

1. Set up project directory structure
2. Download dependencies (SDL2, Lua 5.4, Box2D 3.1, stb_image, stb_truetype)
3. Create minimal build.bat that compiles and runs
4. Implement Phase 1 (C skeleton)
