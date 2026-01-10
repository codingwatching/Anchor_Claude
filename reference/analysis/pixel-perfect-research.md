# Pixel-Perfect Rendering with Smooth Movement Research

## The Problem

When rendering pixel art at a fixed low resolution (e.g., 480x270) and upscaling to display (e.g., 1440x810 at 3x), there's a fundamental tradeoff:

1. **Pixel-perfect appearance** requires snapping object positions to integers
2. **Smooth movement** requires fractional positions

Snapping positions causes jagged/jittery movement because objects "jump" between pixel positions rather than moving smoothly.

## The Sub-Pixel Offset Technique (Common Solution)

This technique is used by Unity's 2D Pixel Perfect package, Godot's smooth pixel camera implementations, and others.

### How It Works

1. **Render the game** at the pixel-perfect resolution with objects snapped to integer positions
2. **Track the fractional position** of the camera (or focal point)
3. **Offset the final screen blit** by the fractional amount multiplied by the scale factor

### Example

- Game resolution: 480x270
- Screen resolution: 1440x810 (3x scale)
- Camera position: 100.7
- Fractional part: 0.7
- Screen offset: 0.7 × 3 = 2.1 screen pixels

The entire rendered image shifts by 2.1 pixels on screen, creating smooth perceived motion while the internal game stays pixel-snapped.

### Implementation (Godot approach)

From https://github.com/voithos/godot-smooth-pixel-camera-demo:

1. Create a SubViewport that's **1 pixel larger on each side** than the target resolution (e.g., 482x272 instead of 480x270)
2. Render the game with objects at integer positions
3. Track two camera positions:
   - "Virtual" true position (fractional)
   - Snapped integer position
4. Calculate "pixel snap delta" = true position - snapped position
5. Apply this delta as an offset when blitting to screen

The extra 1-pixel border allows the fractional offset to work without showing empty edges.

### Implementation (Unity approach)

From Unity's 2D Pixel Perfect package:

1. Enable "Upscale Render Texture" - renders to a texture at reference resolution
2. Pixel Snapping snaps sprites to grid at render-time (doesn't affect Transform positions)
3. The fractional camera offset is applied when displaying the render texture

Key quote: "Take the fractional part of the camera position, multiply that fractional part by the scale (aka number of skips) and you get a pseudo subpixel offset that you can apply to the UI Image."

## Current Anchor Implementation Status

### What We Have

1. **Shader-level texel snapping for sprites**:
```glsl
ivec2 texSize = textureSize(u_texture, 0);
vec2 snappedUV = (floor(vUV * vec2(texSize)) + 0.5) / vec2(texSize);
vec4 texColor = texture(u_texture, snappedUV);
```

2. **Shader-level radius snapping for circles** (in rough mode):
```glsl
if (u_aa_width == 0.0) {
    radius = floor(radius + 0.5);
}
```

3. **C-level center position snapping** (causes jitter):
```c
// In process_circle and process_sprite:
float snapped_cx = floorf(wcx);  // or floorf(wcx + 0.5f) for round
float snapped_cy = floorf(wcy);
float offset_x = snapped_cx - wcx;
float offset_y = snapped_cy - wcy;
// Apply offset to all quad corners
```

### The Issue

The C-level position snapping fixes the visual pixel-perfect appearance at 144Hz (which looked too smooth without it), but causes jagged movement, especially diagonally.

### Findings from Testing

- At 60Hz fixed timestep: looks correct without C-level snapping
- At 144Hz fixed timestep: looks too smooth without C-level snapping
- With C-level snapping at 144Hz: looks pixel-perfect but movement is jagged

The 144Hz issue is because objects move in smaller increments (~0.69 pixels vs ~1.67 at 60Hz), spending more time at sub-pixel positions.

## Proposed Implementation for Anchor

### Option 1: Viewport Offset Technique

1. Change game layer size from 480x270 to 482x272 (add 1px border)
2. Remove C-level position snapping from process_circle and process_sprite
3. Keep shader-level snapping (texel centers, radius)
4. When blitting game layer to screen:
   - Calculate fractional offset from some reference (player position, camera, or average of visible objects)
   - Offset the UV coordinates or viewport by fractional × scale

### Option 2: Accept Sub-Pixel Rendering

Many successful pixel art games (like Stardew Valley) don't enforce strict pixel-perfect rendering. They allow sub-pixel movement for smooth motion.

1. Remove C-level position snapping
2. Keep shader-level texel snapping for sprites
3. Accept that fast-moving objects may have slight sub-pixel rendering

### Option 3: Hybrid Approach

1. Make snapping optional per-object or per-layer
2. Static objects snap, moving objects don't
3. Or: snap when velocity is below threshold

## Implemented Solution: Decoupled Physics/Rendering

After testing all options, the solution that works is **decoupling physics from rendering**:

- **Physics/input**: 144Hz (PHYSICS_RATE) — responsive feel, precise collision
- **Rendering**: 60Hz (RENDER_RATE) — chunky pixel-art movement

### Why This Works

At 60Hz rendering, objects move ~1.67 pixels between frames (at 100px/sec velocity), naturally landing on integer pixel positions more frequently. The 144Hz physics rate provides responsive input and smooth simulation, while the 60Hz render rate gives the desired chunky visual aesthetic.

### What Didn't Work

1. **Viewport offset technique**: Only works with camera-relative rendering. With a fixed camera and independently moving objects, there's no single fractional offset to apply.

2. **C-level position snapping at 144Hz**: Fixed the "too smooth" look but caused jagged diagonal movement because snapped X and Y positions jump at different times.

3. **Pure 60Hz fixed timestep**: Works visually but loses the input responsiveness of 144Hz.

### C-Level Position Snapping (Partial Solution - Reference)

This code fixes the "too smooth" visual issue at 144Hz but causes jagged diagonal movement. Kept here for reference:

```c
// In process_circle and process_sprite, after transforming to world coords:
// Snap center to pixel grid for pixel-perfect rendering
float wcx, wcy;
transform_point(cmd->transform, x, y, &wcx, &wcy);
float snapped_cx = floorf(wcx);
float snapped_cy = floorf(wcy);
float offset_x = snapped_cx - wcx;
float offset_y = snapped_cy - wcy;
wx0 += offset_x; wy0 += offset_y;
wx1 += offset_x; wy1 += offset_y;
wx2 += offset_x; wy2 += offset_y;
wx3 += offset_x; wy3 += offset_y;
```

This snaps the center point to the pixel grid and shifts all quad corners by the same offset. The result is pixel-perfect positioning but jagged movement because X and Y snap at different times during diagonal motion.

### Current Implementation

```c
#define PHYSICS_RATE (1.0 / 144.0)  // 144Hz physics/input
#define RENDER_RATE  (1.0 / 60.0)   // 60Hz rendering

// Main loop:
// 1. SDL_PollEvent (once per frame iteration)
// 2. Physics loop (while physics_lag >= PHYSICS_RATE)
// 3. Render (if render_lag >= RENDER_RATE)
```

**Shader-level snapping** (still in use):
- Sprites: texel center snapping for crisp pixels
- Circles: radius snapping in rough mode

**C-level position snapping**: Removed (not needed with decoupled rates).

## Sources

- Unity 2D Pixel Perfect docs: https://docs.unity3d.com/Packages/com.unity.2d.pixel-perfect@1.0/manual/index.html
- Godot smooth pixel camera: https://github.com/voithos/godot-smooth-pixel-camera-demo
- Game Developer article: https://www.gamedeveloper.com/programming/pixel-perfect-rendering-in-unity
- Unity forum on Stardew Valley approach: https://discussions.unity.com/t/smooth-looking-pixel-art-in-motion/907566
- LÖVE forum on pixel art scrolling: https://love2d.org/forums/viewtopic.php?t=93570

## Key Code Locations in anchor.c

- `PHYSICS_RATE`, `RENDER_RATE`: lines 42-43
- `physics_lag`, `render_lag`: lines 778-779
- `main_loop_iteration()`: lines 982+, decoupled loop structure
- `process_circle()`: ~line 437, no position snapping
- `process_sprite()`: ~line 565, no position snapping
- Fragment shader sprite sampling: texel center snapping
- Fragment shader circle radius snapping: in rough mode only
