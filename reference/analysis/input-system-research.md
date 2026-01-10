# Input System Research Across Game Engines

Comprehensive analysis of input systems across 17+ game engines/frameworks.

---

## Executive Summary

### Common Patterns (Nearly Universal)

1. **Three-state model**: `pressed` (just this frame), `down` (held), `released` (just released)
2. **Action/verb abstraction**: Map physical inputs to named actions
3. **Multiple bindings per action**: One action can have keyboard + gamepad + mouse bindings
4. **Previous/current frame comparison**: Standard edge detection pattern

### Standout Features Worth Considering

1. **Hold duration / long press detection** (Unity, libGDX, GameMaker Input)
2. **Double-tap / multi-tap detection** (Unity, GameMaker Input, your sequence system)
3. **Input device change detection** (Unity, Unreal, GameMaker Input)
4. **Analog vs digital action distinction** (HaxeFlixel, Unity)
5. **Frame count instead of boolean** (p5play) - very elegant
6. **Composite bindings** for WASD→Vector2 (Unity, Godot)
7. **Chords** - multiple buttons pressed together (GameMaker Input)

### What We Might Be Missing

1. **`get_axis()` / `get_vector()` helpers** - Godot's approach is clean
2. **Hold duration tracking** - how long has this been held?
3. **Input type detection** - was last input keyboard or gamepad?
4. **Rebinding capture mode** - "press any key to bind"

---

## Detailed Engine Analysis

### Godot 4 (Most Elegant API)

**Standout features:**
- `Input.is_action_just_pressed(action)` - pressed this frame
- `Input.is_action_pressed(action)` - currently held
- `Input.is_action_just_released(action)` - released this frame
- `Input.get_action_strength(action)` - 0.0 to 1.0 (analog)
- `Input.get_axis(negative, positive)` - returns -1 to 1 from two actions
- `Input.get_vector(neg_x, pos_x, neg_y, pos_y)` - returns Vector2 from four actions

**Why it's good:** The `get_axis()` and `get_vector()` helpers eliminate boilerplate. Instead of:
```lua
local h = (is_down('right') and 1 or 0) - (is_down('left') and 1 or 0)
local v = (is_down('down') and 1 or 0) - (is_down('up') and 1 or 0)
```
You write:
```gdscript
var move = Input.get_vector("left", "right", "up", "down")
```

**InputMap runtime API:**
- `InputMap.add_action(action)` - create action at runtime
- `InputMap.action_add_event(action, event)` - add binding
- `InputMap.action_erase_event(action, event)` - remove binding
- `InputMap.has_action(action)` - check if exists

Sources: [Godot Input](https://docs.godotengine.org/en/stable/classes/class_input.html), [InputMap](https://docs.godotengine.org/en/stable/classes/class_inputmap.html)

---

### Unity Input System (Most Feature-Rich)

**Core API:**
- `action.ReadValue<T>()` - get current value (any type)
- `action.WasPressedThisFrame()` - just pressed
- `action.WasReleasedThisFrame()` - just released
- `action.IsPressed()` - currently held

**Interactions (Built-in gesture detection):**
- `HoldInteraction` - triggers after holding for N seconds
- `TapInteraction` - must press and release within N seconds
- `SlowTapInteraction` - hold for minimum duration, then release
- `MultiTapInteraction` - double-tap, triple-tap, etc.
  - `tapCount` - how many taps required
  - `tapTime` - max time for each tap
  - `tapDelay` - max time between taps

**Composite Bindings:**
- `1DAxis` - two buttons → -1 to 1 axis
- `2DVector` - four buttons → Vector2 (WASD)
- Handles the math automatically

**Binding Groups:**
- Group bindings by control scheme ("Keyboard", "Gamepad")
- Switch active scheme at runtime

Sources: [Unity Actions](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.5/manual/Actions.html), [Interactions](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.4/manual/Interactions.html)

---

### HaxeFlixel (Clean Digital/Analog Split)

**Two action types:**
- `FlxActionDigital` - binary on/off (jump, shoot)
- `FlxActionAnalog` - floating point with x,y axes (move, aim)

**Trigger states:**
- `PRESSED` - held
- `JUST_PRESSED` - this frame
- `RELEASED` - not held
- `JUST_RELEASED` - this frame

**Analog axis options:**
- `X` - only x axis
- `Y` - only y axis
- `EITHER` - either axis
- `BOTH` - both axes (full 2D)

**Direct state access:**
```haxe
FlxG.keys.justPressed.SPACE
FlxG.keys.pressed.A
FlxG.mouse.justPressed
FlxG.gamepads.firstActive.pressed.A
```

Sources: [HaxeFlixel Actions](https://haxeflixel.com/documentation/actions/), [FlxActionAnalog](https://api.haxeflixel.com/flixel/input/actions/FlxActionAnalog.html)

---

### p5play (Elegant Frame Counter)

**Unique approach:** Instead of booleans, stores frame count.

```javascript
// kb.space = number of frames held (0 if not pressed)
if (kb.space == 1) { /* just pressed */ }
if (kb.space > 0) { /* being held */ }
if (kb.space == -1) { /* just released */ }
```

**State values:**
- `0` - not pressed
- `1` - just pressed this frame
- `>1` - held for N frames (actual count!)
- `-1` - just released
- `-2` - released after being held
- `-3` - pressed and released same frame

**Why it's clever:** The frame count gives you hold duration for free. You can do `if (kb.space > 30)` to check if held for half a second at 60fps.

**Unified API across devices:**
- `kb.presses('space')` - just pressed
- `kb.pressing('space')` - held
- `kb.released('space')` - just released
- Same pattern for `mouse`, `contros`

**Direction aliases:**
- `'up'`, `'down'`, `'left'`, `'right'` → WASD + arrows
- `'up2'`, `'down2'`, etc. → IJKL (player 2)

Sources: [p5play Input](https://p5play.org/learn/input.html), [InputDevice](https://p5play.org/docs/InputDevice.html)

---

### GameMaker Input Library (Most Complete Third-Party)

Used in Shovel Knight Pocket Dungeon, Samurai Gunn 2.

**Core "verbs" system:**
- Define named actions ("verbs")
- Bind multiple inputs to each verb
- Check with `input_check(verb)`, `input_check_pressed(verb)`, `input_check_released(verb)`

**Advanced features:**
- **Double taps:** Detect rapid double-press
- **Long holds:** Detect held for duration
- **Chords:** Multiple buttons pressed together
- **Combos:** Sequence of inputs (like fighting games)
- **Rapidfire:** Auto-repeat while held

**Input device detection:**
- Tracks which device made last input
- Events when device type changes
- For UI prompt switching (show keyboard or gamepad icons)

**Accessibility:**
- Built-in accessibility features
- Configurable thresholds

Sources: [Input Documentation](https://offalynne.github.io/Input/), [GitHub](https://github.com/offalynne/Input)

---

### Defold (Data-Driven Bindings)

**Unique approach:** Input bindings defined in data file, not code.

**game.input_binding file:**
```
key_trigger {
  input: KEY_SPACE
  action: "jump"
}
mouse_trigger {
  input: MOUSE_BUTTON_LEFT
  action: "shoot"
}
```

**In code:**
```lua
function on_input(self, action_id, action)
    if action_id == hash("jump") and action.pressed then
        -- jumped
    end
end
```

**Action object contains:**
- `pressed` - just pressed
- `released` - just released
- `repeated` - key repeat event
- `value` - analog value (0-1)
- `x`, `y` - mouse/touch position
- `dx`, `dy` - mouse/touch delta

**Key insight:** The same action name can be bound to multiple inputs in the data file - no code needed.

Sources: [Defold Input](https://defold.com/manuals/input/)

---

### Phaser 3 (Unified Pointer Model)

**Key insight:** Mouse and touch unified as "pointers."

```javascript
// Works for both mouse and touch
this.input.on('pointerdown', callback);
this.input.activePointer.x; // current pointer position
```

**Multiple pointers:**
- `mousePointer` - always the mouse
- `pointer1`, `pointer2`, etc. - touch pointers
- `activePointer` - most recent interaction

**Keyboard:**
```javascript
this.input.keyboard.on('keydown-SPACE', callback);
cursors = this.input.keyboard.createCursorKeys(); // arrow keys object
if (cursors.left.isDown) { ... }
```

Sources: [Phaser Input](https://docs.phaser.io/phaser/concepts/input)

---

### FNA/XNA (Classic Pattern)

**The original pattern many engines copied:**

```csharp
KeyboardState currentKeyboard;
KeyboardState previousKeyboard;
GamePadState currentGamepad;
GamePadState previousGamepad;

void Update() {
    previousKeyboard = currentKeyboard;
    currentKeyboard = Keyboard.GetState();

    // Just pressed
    if (currentKeyboard.IsKeyDown(Keys.Space) &&
        previousKeyboard.IsKeyUp(Keys.Space)) { ... }

    // Held
    if (currentKeyboard.IsKeyDown(Keys.Space)) { ... }
}
```

**Extensions:**
- `Keyboard.GetKeyFromScancodeEXT()` - proper keyboard layout support
- `Mouse.IsRelativeMouseModeEXT` - for FPS games
- `TextInputEXT` - text input handling

Sources: [FNA Extensions](https://fna-xna.github.io/docs/5:-FNA-Extensions/)

---

### Construct 3 / GDevelop (Visual Event Systems)

**Key insight:** Separate plugins for each input type.

**Construct 3:**
- Keyboard plugin
- Mouse plugin
- Touch plugin
- Gamepad plugin

Each has consistent conditions:
- "Key is down"
- "On key pressed"
- "On key released"

**GDevelop:**
- Mouse/touch unified by default
- Can be separated if needed
- Touch auto-converts to mouse events

**Touch/mouse unification toggle:**
> "When activated, any touch made on a touchscreen will also move the mouse cursor... By default, this is activated so you can simply use the mouse conditions to also support touchscreens."

Sources: [Construct 3 Keyboard](https://www.construct.net/en/make-games/manuals/construct-3/plugin-reference/keyboard), [GDevelop Mouse/Touch](https://wiki.gdevelop.io/gdevelop5/all-features/mouse-touch/)

---

### Bevy (ECS Resource Pattern)

**Input as resources:**
```rust
fn movement(keyboard: Res<ButtonInput<KeyCode>>) {
    if keyboard.just_pressed(KeyCode::Space) { ... }
    if keyboard.pressed(KeyCode::KeyA) { ... }
    if keyboard.just_released(KeyCode::Space) { ... }
}
```

**No built-in action mapping** - community plugins fill the gap:
- `leafwing-input-manager` - action mapping
- `bevy_enhanced_input` - Unity-style system

Sources: [Bevy Input](https://docs.rs/bevy/latest/bevy/input/index.html), [Bevy Cheatbook](https://bevy-cheatbook.github.io/input.html)

---

### Cute Framework (C)

**Simple, direct API:**
```c
if (cf_key_just_pressed(CF_KEY_SPACE)) { ... }
if (cf_key_down(CF_KEY_A)) { ... }
if (cf_mouse_just_pressed(CF_MOUSE_BUTTON_LEFT)) { ... }
```

Sources: [Cute Framework](https://randygaul.github.io/cute_framework/)

---

### KaboomJS / KAPLAY (Buttons API)

**Unified binding system:**
```javascript
// Define buttons (actions)
setButton("jump", [keyboard("space"), gamepad("south")]);

// Check
if (isButtonPressed("jump")) { ... }
if (isButtonDown("jump")) { ... }
if (isButtonReleased("jump")) { ... }
```

**Event-based:**
```javascript
onKeyPress("space", () => { ... });
onMouseDown("left", () => { ... });
onGamepadButtonPress("south", () => { ... });
```

Sources: [KAPLAY Input](https://kaplayjs.com/docs/guides/input/)

---

## Patterns & Recommendations

### 1. Get Axis / Get Vector (STRONGLY RECOMMENDED)

Add these helpers - they eliminate tons of boilerplate:

```lua
-- Returns -1, 0, or 1 based on two actions
function input_get_axis(negative, positive)
    local neg = is_down(negative) and 1 or 0
    local pos = is_down(positive) and 1 or 0
    return pos - neg
end

-- Returns x, y from four actions
function input_get_vector(left, right, up, down)
    return input_get_axis(left, right), input_get_axis(up, down)
end
```

Usage:
```lua
local move_x, move_y = input_get_vector('left', 'right', 'up', 'down')
```

### 2. Frame Count Instead of Boolean (CONSIDER)

p5play's approach is elegant. Instead of:
- `is_pressed()` → boolean
- `is_down()` → boolean

You could have:
- `input_frames(action)` → 0, 1, 2, 3... (frames held), -1 (just released)

Then:
- `frames == 1` means just pressed
- `frames > 0` means held
- `frames == -1` means just released
- `frames > 30` means held for 0.5s at 60fps

This gives hold duration for free.

### 3. Hold Duration / Long Press (USEFUL)

Either via frame count (above) or explicit:
```lua
input_get_hold_duration(action)  -- returns seconds held
```

### 4. Input Type Detection (IMPORTANT FOR UI)

Track which device made the last input:
```lua
input_get_last_type()  -- 'keyboard', 'mouse', 'gamepad'
input_on_type_changed(callback)  -- event when it changes
```

Essential for showing correct button prompts.

### 5. Rebinding Capture Mode (NEEDED FOR OPTIONS)

For rebind UI:
```lua
input_start_capture(callback)  -- enters capture mode
-- callback receives the control string when user presses something
-- e.g., callback('key:space') or callback('button:a')
input_stop_capture()  -- exits capture mode
```

### 6. Chords (NICE TO HAVE)

Multiple buttons pressed together:
```lua
input_bind_chord('sprint_jump', {'shift', 'space'})
if is_pressed('sprint_jump') then ... end
```

### 7. Any Key / Any Button (USEFUL)

For "Press any key to continue":
```lua
input_any_pressed()  -- true if any bound action was just pressed
input_any_key_pressed()  -- true if any keyboard key
input_any_button_pressed()  -- true if any gamepad button
```

---

## Revised API Proposal

Based on this research:

**Binding:**
```lua
input_bind(action, controls)
input_unbind(action, control)
input_unbind_all(action)
input_bind_all()                    -- all keys to same-named actions
input_bind_sequence(name, {...})    -- for combos
```

**Core Queries:**
```lua
is_pressed(action)                  -- just pressed
is_down(action)                     -- held (returns axis value if analog)
is_released(action)                 -- just released
```

**Axis Helpers (NEW):**
```lua
input_get_axis(negative, positive)  -- returns -1 to 1
input_get_vector(l, r, u, d)        -- returns x, y
```

**Hold Duration (NEW):**
```lua
input_get_hold_time(action)         -- seconds held (0 if not held)
-- OR use frame count approach
```

**Sequences:**
```lua
is_sequence_pressed(name)
is_sequence_down(name)
is_sequence_released(name)
```

**Mouse:**
```lua
mouse_position()                    -- x, y in game coords
mouse_delta()                       -- dx, dy this frame
mouse_set_visible(bool)
mouse_set_grabbed(bool)
```

**Device Detection (NEW):**
```lua
input_get_last_type()               -- 'keyboard', 'mouse', 'gamepad'
```

**Rebinding Support (NEW):**
```lua
input_start_capture()               -- enter capture mode
input_get_captured()                -- returns control string or nil
input_stop_capture()                -- exit capture mode
```

**Gamepad:**
```lua
gamepad_is_connected()
gamepad_get_axis(axis)              -- raw -1 to 1
input_set_deadzone(value)
```

**Utility:**
```lua
input_any_pressed()                 -- any action just pressed
key_is_pressed(key)                 -- raw key query
key_is_down(key)
key_is_released(key)
```

---

## Sources

- [Godot Input](https://docs.godotengine.org/en/stable/classes/class_input.html)
- [Unity Input System](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.5/manual/Actions.html)
- [HaxeFlixel Actions](https://haxeflixel.com/documentation/actions/)
- [p5play Input](https://p5play.org/learn/input.html)
- [GameMaker Input Library](https://offalynne.github.io/Input/)
- [Defold Input](https://defold.com/manuals/input/)
- [Phaser Input](https://docs.phaser.io/phaser/concepts/input)
- [Bevy Input](https://bevy-cheatbook.github.io/input.html)
- [FNA Extensions](https://fna-xna.github.io/docs/5:-FNA-Extensions/)
- [Construct 3 Keyboard](https://www.construct.net/en/make-games/manuals/construct-3/plugin-reference/keyboard)
- [GDevelop Input](https://wiki.gdevelop.io/gdevelop5/all-features/mouse-touch/)
- [KAPLAY Input](https://kaplayjs.com/docs/guides/input/)
- [Cute Framework](https://randygaul.github.io/cute_framework/)
- [Heaps Input](https://heaps.io/api/hxd/Key.html)
- [Ceramic Input](https://ceramic-engine.com/api-docs/clay-native/ceramic/Input/)
- [LÖVR Input](https://lovr.org/docs/lovr.system.isKeyDown)
