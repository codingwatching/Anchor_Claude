# Camera Systems Research

Research across 13 game engines to understand common behaviors and unique/elegant patterns.

---

## Common Behaviors (Found in Most Engines)

### 1. Position & Transform
- **Position** (x, y or scroll position)
- **Zoom/Scale** (typically 1.0 = normal, 2.0 = 2x magnification)
- **Rotation** (angle in degrees or radians)
- **Origin** (pivot point for rotation/zoom, usually center)

### 2. Follow Target
- Track a game object's position
- **Lerp/smoothing** for gradual catch-up (0.0-1.0 interpolation factor)
- **Offset** from target (fixed displacement)

### 3. Bounds/Limits
- Rectangular constraints on camera movement
- Camera stops at edges rather than showing empty space
- Separate limits for left/right/top/bottom

### 4. Deadzone
- Rectangular area where target movement doesn't trigger camera movement
- Camera only moves when target reaches deadzone edge
- Prevents jittery micro-adjustments

### 5. Screen Shake
- Oscillating displacement (position and/or rotation)
- Intensity + duration parameters
- Often with direction control (both/horizontal/vertical)

### 6. Effects
- **Fade**: Gradually fill screen with color
- **Flash**: Quick color overlay that fades out
- **Zoom effect**: Animated zoom over duration

---

## Engine-by-Engine Analysis

### HaxeFlixel (FlxCamera)

**Unique: Follow Styles (Preset Deadzones)**

Six built-in follow modes with pre-configured deadzones:

| Style | Deadzone Shape | Best For |
|-------|---------------|----------|
| `LOCKON` | None | Direct tracking |
| `PLATFORMER` | Narrow & tall (1/8 × 1/3 of screen) | Side-scrollers |
| `TOPDOWN` | Medium square (1/4 of max dimension) | Overhead games |
| `TOPDOWN_TIGHT` | Small square (1/8 of max dimension) | Tighter overhead tracking |
| `SCREEN_BY_SCREEN` | Full screen | Zelda-style room transitions |
| `NO_DEAD_ZONE` | None, centered | Direct centered tracking |

**Unique: Follow Lead**
- `followLead`: Offset that anticipates target movement direction
- Camera looks ahead in the direction of travel

**Other Features:**
- `pixelPerfectRender`: Round positions for crisp pixel art
- `pixelPerfectShake`: Round shake offsets
- Multiple simultaneous cameras supported
- Shake with axis control (X only, Y only, or both)

**Sources:** [FlxCamera API](https://api.haxeflixel.com/flixel/FlxCamera.html), [FlxCameraFollowStyle](https://api.haxeflixel.com/flixel/FlxCameraFollowStyle.html)

---

### Unity (Cinemachine)

**Unique: Virtual Camera Architecture**

Modular component system where behaviors are composed:
- **Body** components: Handle positioning (Transposer, Framing Transposer, Orbital)
- **Aim** components: Handle targeting (Composer, Group Composer)
- **Extension** components: Add noise, collision detection, constraints
- **Brain**: Blends between virtual cameras automatically

**Unique: Follow Zoom**
- Automatically adjusts FOV to keep target at constant screen size
- Target stays same visual size regardless of distance

**Unique: Group Composer**
- Frames multiple targets simultaneously
- Auto-adjusts zoom to keep all targets visible
- Configurable framing margins

**Unique: Dead Zones (Soft Zones)**
- Rectangular or circular regions
- Graduated response: no movement in center, increasing urgency toward edges

**Sources:** [Cinemachine Documentation](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineFollowZoom.html), [Unity Cinemachine](https://unity.com/features/cinemachine)

---

### Godot (Camera2D)

**Unique: Rotation Smoothing**
- Separate smoothing for position AND rotation
- `position_smoothing_enabled` + `position_smoothing_speed`
- `rotation_smoothing_enabled` + `rotation_smoothing_speed`

**Unique: Limit Smoothing**
- `limit_smoothed`: Camera smoothly decelerates when approaching limits
- Instead of hard stop, eases into boundary

**Features:**
- Drag margins (percentage-based deadzone)
- Process mode selection (idle vs physics for determinism)
- Simple child-of-player setup for basic following

**Sources:** [Camera2D Documentation](https://docs.godotengine.org/en/stable/classes/class_camera2d.html)

---

### Construct 3

**Unique: Multi-Object Averaging**
- Multiple objects with ScrollTo behavior → camera centers between ALL of them
- Automatic group framing without code

**Third-Party: Advanced Camera Plugin**

Three camera modes:
1. **Scroll-To**: Standard smooth follow
2. **BoxTrap**: Bounding box deadzone, only moves when player exits box
3. **Grid Camera**: Zelda-style discrete room transitions

All modes support:
- Multiple instance tracking with real-time list updates
- Easing functions for movement, rotation, zoom

**Sources:** [Scroll To Behavior](https://www.construct.net/en/make-games/manuals/construct-3/behavior-reference/scroll-to)

---

### Heaps.io (h2d.Camera)

**Unique: Scene Integration**
- Camera is integral part of h2d.Scene
- Accessed via `scene.camera` or `scene.cameras` array
- Multiple cameras per scene supported

**Unique: ScaleMode System**
- `Zoom(level)`: Direct zoom control
- Auto-calculated zoom based on target resolution vs window size
- Maintains aspect ratio automatically

**Sources:** [Heaps Camera Documentation](https://heaps.io/documentation/2d-camera.html), [Camera API](https://heaps.io/api/h2d/Camera.html)

---

### Cute Framework

**Architecture Note:**
- Camera transforms managed via a stack structure (`cam_stack`)
- Transform matrix affects all subsequent draw operations
- Push/pop pattern for nested transforms

**Sources:** [Cute Framework Renderer](https://randygaul.github.io/cute_framework/topics/renderer/)

---

### Phaser 3

**Unique: Independent X/Y Zoom**
- `setZoom(x, y)` allows non-uniform scaling
- Horizontal and vertical zoom can differ

**Unique: Comprehensive Effect System**

| Effect | Description |
|--------|-------------|
| `fade()` | Gradual fill/clear |
| `flash()` | Quick overlay |
| `pan()` | Animated position change |
| `rotateTo()` | Animated rotation |
| `shake()` | Oscillating displacement |
| `zoomTo()` | Animated zoom change |

All effects support: duration, easing, callbacks, force parameter to override active effects.

**Unique: World View Rectangle**
- `camera.worldView` gives exact world-space bounds currently visible
- Accounts for zoom, scroll, rotation, viewport size

**Features:**
- `roundPixels`: Integer coordinate rounding for pixel art
- Per-camera `alpha` for transparency effects
- Custom origins for rotation pivot

**Sources:** [Phaser Cameras Concepts](https://docs.phaser.io/phaser/concepts/cameras), [Camera API](https://docs.phaser.io/api-documentation/class/cameras-scene2d-camera)

---

### p5play

**Unique: On/Off Toggle**
- `camera.on()`: Subsequent draws affected by camera transform
- `camera.off()`: Subsequent draws in screen space (for UI)
- Explicit control over what's affected by camera

**Unique: Dual Mouse Coordinates**
- `mouse.x`: World-space position (affected by camera)
- `mouse.canvasPos.x`: Screen-space position (unaffected)
- Simplifies UI vs world interaction

**Unique: Async Zoom**
- `zoomTo(target, speed)` returns promise
- `await camera.zoomTo(2, 0.1)` for sequential animations

**Sources:** [p5play Camera](https://p5play.org/learn/camera.html), [Camera Documentation](https://p5play.org/docs/Camera.html)

---

### PixiJS (pixi-viewport)

**Unique: Plugin Architecture**

Viewport is built from composable plugins:
- `.drag()`: Click and drag to pan
- `.pinch()`: Two-finger pinch to zoom
- `.wheel()`: Mouse wheel zoom
- `.decelerate()`: Momentum after drag release
- `.bounce()`: Elastic bounce at edges
- `.clamp()`: Hard limits on movement
- `.clampZoom()`: Limits on zoom range
- `.snap()`: Snap to specific positions
- `.follow()`: Track a target

Each plugin is optional and configurable.

**Sources:** [pixi-viewport GitHub](https://github.com/pixijs-userland/pixi-viewport), [API Documentation](https://viewport.pixijs.io/jsdoc/)

---

### Defold (defold-orthographic)

**Unique: Selective Axis Following**
- `follow_horizontal`: Enable/disable X tracking
- `follow_vertical`: Enable/disable Y tracking
- Follow only in one dimension (e.g., platformer with fixed Y)

**Unique: Recoil Effect**
- Linear decay offset (not oscillating like shake)
- Perfect for weapon kickback or impact feedback
- Separate from shake system

**Unique: Auto-Zoom for Resolution**
- Camera automatically adjusts zoom based on display resolution
- Content stays properly framed regardless of screen size
- Two modes: min (content fits within bounds) or max (content fills bounds)

**Unique: Comprehensive Coordinate Conversion**
- World ↔ Screen ↔ Window conversions
- Supports GUI adjust modes (FIT/ZOOM/STRETCH)

**Features:**
- Shake with direction control + completion callback
- Deadzone with edge-triggered following
- Multi-camera support with ordering

**Sources:** [defold-orthographic GitHub](https://github.com/britzl/defold-orthographic)

---

### KaboomJS / Kaplay

**Unique: Minimalist API**
```js
camPos(x, y)      // Set/get position
camScale(x, y)    // Set/get zoom
camRot(angle)     // Set/get rotation
shake(intensity)  // One-liner shake
```

**Unique: Fixed Component**
- `fixed()` component makes objects immune to camera transform
- Added to game objects, not camera
- Objects with `fixed()` render in screen space (perfect for UI)

**Unique: Coordinate Helpers**
- `toScreen(worldPos)`: Convert world → screen
- `toWorld(screenPos)`: Convert screen → world

**Sources:** [Kaboom.js](https://kaboomjs.com/), [Kaplay camPos](https://kaplayjs.com/doc/ctx/camPos/)

---

### GameMaker

**Unique: View System**
- Cameras exist separately from views
- Views are the "window" into the camera
- Multiple views can share one camera, or each view can have its own

**Unique: View Border (Smart Deadzone)**
- `view_hborder` / `view_vborder`: Distance from edge before camera follows
- Speed of -1 = instant snap
- Built-in object following with border-based deadzone

**Shake Implementation:**
- Typically done via `camera_set_view_angle()` with random values
- Or by offsetting `view_xview`/`view_yview` with random amounts

**Sources:** [GameMaker Camera Tutorial](https://gamemaker.io/en/tutorials/cameras-and-views)

---

### MonoGame (MonoGame.Extended)

**Unique: Transformation Matrix Approach**
- Camera provides a matrix to SpriteBatch
- Matrix = Translation × Rotation × Scale × Origin
- All transforms composed into single matrix multiplication

**Unique: Zoom-Aware Bounds Clamping**
- `IsZoomClampedToWorldBounds`: Prevents zooming out beyond world bounds
- Automatically adjusts max zoom based on world size

**Unique: Zoom to Point**
- `ZoomIn(worldPosition)` / `ZoomOut(worldPosition)`
- Zoom centers on specified point (e.g., mouse cursor)
- Point stays fixed on screen during zoom

**Unique: Small World Handling**
- If world is smaller than viewport, camera auto-centers
- Graceful fallback instead of edge-clamping artifacts

**Sources:** [MonoGame.Extended Camera](https://www.monogameextended.net/docs/features/camera/orthographic-camera/)

---

## Unique & Elegant Patterns Summary

### Follow Styles / Presets
- **HaxeFlixel**: 6 preset deadzones (PLATFORMER, TOPDOWN, etc.)
- **Construct 3**: BoxTrap, Grid Camera modes
- Reduces boilerplate for common game types

### Look-Ahead / Lead
- **HaxeFlixel**: `followLead` property
- Camera anticipates movement direction
- Shows more of what's coming, less of what's behind

### Trauma-Based Shake
- **Godot, Bevy**: Trauma accumulates, decays over time
- Shake intensity = trauma² or trauma³ (exponential feels better)
- Noise-based displacement (not pure random)

### Multi-Target Auto-Framing
- **Unity Cinemachine**: Group Composer
- **Construct 3**: Multi-ScrollTo averaging
- **Godot**: Multi-target camera recipe
- Auto-zoom to keep all targets visible

### Selective Axis Following
- **Defold**: `follow_horizontal`, `follow_vertical` flags
- Follow X but not Y (or vice versa)
- Useful for platformers (fixed vertical position)

### On/Off Toggle for UI
- **p5play**: `camera.on()` / `camera.off()`
- **KaboomJS**: `fixed()` component
- Explicit control over what's camera-affected

### Plugin/Component Architecture
- **Unity Cinemachine**: Composable Body/Aim/Extension components
- **PixiJS viewport**: Chain of optional plugins
- Mix and match behaviors

### Zoom to Point
- **MonoGame.Extended**: Zoom toward cursor position
- Point stays fixed on screen during zoom
- Intuitive for mouse-driven zoom

### Resolution-Independent Auto-Zoom
- **Defold**: Camera auto-adjusts for display resolution
- **Heaps**: ScaleMode system
- Content stays properly framed on any screen

---

## Recommended Features for Anchor

Based on this research, here are features worth considering:

### Essential
1. **Position, zoom, rotation** with smoothing/lerp
2. **Follow target** with offset
3. **Bounds** to constrain movement
4. **Deadzone** (rectangular area of no-follow)
5. **Shake** with intensity, duration, direction

### Valuable Additions
6. **Follow styles/presets** (platformer, topdown, lockon)
7. **Look-ahead/lead** (anticipate movement direction)
8. **Selective axis following** (X only, Y only)
9. **On/off toggle** for UI rendering
10. **Zoom to point** (zoom toward cursor)

### Nice to Have
11. **Multi-target framing** with auto-zoom
12. **Trauma-based shake** (accumulate, decay, exponential)
13. **Recoil effect** (linear decay offset, not oscillating)
14. **Rotation smoothing** (separate from position smoothing)

---

## Sources

- [HaxeFlixel FlxCamera](https://api.haxeflixel.com/flixel/FlxCamera.html)
- [Unity Cinemachine](https://unity.com/features/cinemachine)
- [Godot Camera2D](https://docs.godotengine.org/en/stable/classes/class_camera2d.html)
- [Construct 3 Scroll To](https://www.construct.net/en/make-games/manuals/construct-3/behavior-reference/scroll-to)
- [Heaps.io Camera](https://heaps.io/documentation/2d-camera.html)
- [Cute Framework](https://randygaul.github.io/cute_framework/)
- [Phaser Cameras](https://docs.phaser.io/phaser/concepts/cameras)
- [p5play Camera](https://p5play.org/learn/camera.html)
- [pixi-viewport](https://github.com/pixijs-userland/pixi-viewport)
- [defold-orthographic](https://github.com/britzl/defold-orthographic)
- [KaboomJS](https://kaboomjs.com/)
- [GameMaker Cameras](https://gamemaker.io/en/tutorials/cameras-and-views)
- [MonoGame.Extended Camera](https://www.monogameextended.net/docs/features/camera/orthographic-camera/)
- [GDC: Juicing Your Cameras With Math](https://www.youtube.com/watch?v=tu-Qe66AvtY)
- [GMTK: How to Make a Good 2D Camera](https://gmtk.substack.com/p/how-to-make-a-good-2d-camera)
