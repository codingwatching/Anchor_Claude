# Box2D 3.x Research & Anchor Physics API Proposal

## Box2D 3.x Research Summary

### Key Architectural Differences from Box2D 2.x

1. **ID-based handles** instead of pointers (`b2BodyId`, `b2ShapeId`, etc.)
2. **Events instead of callbacks** - retrieved after `b2World_Step()`, not during
3. **No fixtures** - shapes attach directly to bodies
4. **64-bit filter categories/masks** - massive increase from 16-bit
5. **Sub-stepping** - `b2World_Step(worldId, timeStep, subStepCount)` instead of iterations
6. **Deterministic multithreading** - events are buffered safely
7. **Sensors queryable** - can get current overlaps, not just begin/end events

### Box2D ID Structure

Box2D 3.x IDs are small structs (8 bytes) containing index, world reference, and generation counter:

```c
typedef struct b2BodyId {
    int32_t index1;      // internal index
    uint16_t world0;     // world reference
    uint16_t generation; // version for stale detection
} b2BodyId;
```

These can be stored directly in Lua userdata - no internal mapping array needed.

### Event Types
- **SensorBeginTouchEvent/SensorEndTouchEvent** - overlap detection
- **ContactBeginTouchEvent/ContactEndTouchEvent** - physical collision
- **ContactHitEvent** - high-speed impact with `approachSpeed`
- Events must be explicitly enabled per-shape via `enableSensorEvents`, `enableContactEvents`, `enableHitEvents`

### Spatial Queries
- `b2World_CastRayClosest()` - returns first hit
- `b2World_CastRay()` - callback for all hits
- `b2World_OverlapAABB()` - callback for shapes in rectangle
- `b2World_OverlapShape()` - callback for shapes overlapping arbitrary shape
- `b2World_CastShape()` - sweep a shape along a path
- All queries use `b2QueryFilter` with `categoryBits`/`maskBits`

---

## Proposed Anchor Physics API

### Design Philosophy

1. **Tag-based collision** - strings for readability, mapped internally to bit categories
2. **Events are per-frame arrays** - query after update, not callbacks
3. **Meter scaling handled internally** - Lua works in pixels, C converts to meters
4. **Pass-through IDs** - Box2D ID structs passed directly to/from Lua as userdata, no transformation

---

### World Setup

```lua
-- Initialize physics (called once at startup)
physics_init()

-- Configuration
physics_set_gravity(0, 500)           -- in pixels/sec² (converted to meters internally)
physics_set_meter_scale(64)           -- 64 pixels = 1 meter (default, tunable)

-- Collision tag registration (must happen before creating bodies)
-- Internally assigns each tag a unique bit in the 64-bit category
physics_register_tag('player')
physics_register_tag('enemy')
physics_register_tag('wall')
physics_register_tag('bullet')
physics_register_tag('pickup')
physics_register_tag('trigger')

-- Enable collision between tag pairs (physical collision with response)
physics_enable_collision('ball', 'wall')
physics_enable_collision('ball', 'paddle')
physics_enable_collision('player', 'enemy')

-- Enable sensor events between tag pairs (overlap detection, no physics response)
physics_enable_sensor('player', 'pickup')
physics_enable_sensor('player', 'trigger')

-- Enable hit events (high-speed impacts) - must also have collision enabled
physics_enable_hit('ball', 'wall')

-- Disable collision entirely between tag pairs (they pass through each other)
physics_disable_collision('player', 'player_bullet')
```

---

### Body Creation

```lua
-- Create a body (returns body userdata containing b2BodyId)
-- body_type: 'static', 'dynamic', 'kinematic'
local body = physics_create_body(body_type, x, y)

-- Attach shapes to body (returns shape userdata containing b2ShapeId)
local shape = physics_add_circle(body, tag, radius)
local shape = physics_add_box(body, tag, width, height)
local shape = physics_add_polygon(body, tag, {x1, y1, x2, y2, x3, y3, ...})
local shape = physics_add_capsule(body, tag, length, radius)

-- Shape can be a sensor (no collision response)
local sensor = physics_add_circle(body, tag, radius, {sensor = true})

-- Multiple shapes per body
local player_body = physics_create_body('dynamic', 100, 100)
physics_add_circle(player_body, 'player', 16)           -- hitbox
physics_add_circle(player_body, 'player_feet', 8, {     -- ground sensor
    sensor = true,
    offset_y = 16
})

-- Destroy
physics_destroy_body(body)
```

---

### Body Properties

```lua
-- Position and rotation
local x, y = physics_get_position(body)
local angle = physics_get_angle(body)
physics_set_position(body, x, y)
physics_set_angle(body, angle)
physics_set_transform(body, x, y, angle)

-- Velocity
local vx, vy = physics_get_velocity(body)
local av = physics_get_angular_velocity(body)
physics_set_velocity(body, vx, vy)
physics_set_angular_velocity(body, av)

-- Forces and impulses
physics_apply_force(body, fx, fy)
physics_apply_force_at(body, fx, fy, px, py)  -- at point
physics_apply_impulse(body, ix, iy)
physics_apply_impulse_at(body, ix, iy, px, py)
physics_apply_torque(body, torque)
physics_apply_angular_impulse(body, impulse)

-- Properties
physics_set_linear_damping(body, damping)
physics_set_angular_damping(body, damping)
physics_set_gravity_scale(body, scale)
physics_set_fixed_rotation(body, true)
physics_set_bullet(body, true)  -- CCD for fast-moving bodies

-- User data (link back to game object via integer ID)
physics_set_user_data(body, object_id)  -- integer ID, not Lua table
local object_id = physics_get_user_data(body)
```

---

### Per-Frame Event Queries

```lua
-- In update(), after physics step:

-- Collision events (physical collisions with response)
for _, event in ipairs(physics_get_collision_begin('ball', 'wall')) do
    local ball_body = event.body_a
    local wall_body = event.body_b
    local ball_id = physics_get_user_data(ball_body)
    -- look up object from ID and call method
end

for _, event in ipairs(physics_get_collision_end('ball', 'wall')) do
    -- Bodies separated
end

-- Hit events (high-speed impacts, includes approach speed)
for _, event in ipairs(physics_get_hit('ball', 'wall')) do
    local speed = event.approach_speed
    local point_x, point_y = event.point_x, event.point_y
    local normal_x, normal_y = event.normal_x, event.normal_y
    sound_play(bounce, speed / 100, 1.0)
end

-- Sensor events (overlap detection, no physics response)
for _, event in ipairs(physics_get_sensor_begin('player', 'pickup')) do
    local player_body = event.body_a
    local pickup_body = event.body_b
    local pickup_id = physics_get_user_data(pickup_body)
    -- look up pickup object and collect it
end

for _, event in ipairs(physics_get_sensor_end('player', 'trigger')) do
    -- Player left trigger zone
end

-- Current sensor overlaps (not just events)
local overlaps = physics_get_sensor_overlaps(sensor_shape)
for _, other_shape in ipairs(overlaps) do
    local other_body = physics_shape_get_body(other_shape)
    -- ...
end
```

---

### Spatial Queries (Overlap)

All overlap queries return arrays of bodies matching the filter tags.

```lua
-- Point query (what's at this exact point?)
local bodies = physics_query_point(x, y, {'clickable', 'enemy'})

-- Circle overlap
local bodies = physics_query_circle(x, y, radius, {'enemy'})

-- Axis-aligned box overlap
local bodies = physics_query_aabb(x, y, width, height, {'enemy'})

-- Rotated box overlap
local bodies = physics_query_box(x, y, width, height, angle, {'enemy'})

-- Capsule overlap (pill shape)
local bodies = physics_query_capsule(x1, y1, x2, y2, radius, {'enemy'})

-- Arbitrary convex polygon overlap
local bodies = physics_query_polygon(x, y, {dx1, dy1, dx2, dy2, dx3, dy3, ...}, {'enemy'})
```

---

### Spatial Queries (Casts/Sweeps)

All cast queries return hit info: `body`, `shape`, `point_x`, `point_y`, `normal_x`, `normal_y`, `fraction`.
The `_all` variants return arrays of hits (unordered). The non-`_all` variants return the closest hit.

```lua
-- Raycast (line)
local hit = physics_raycast(x1, y1, x2, y2, {'wall', 'enemy'})
local hits = physics_raycast_all(x1, y1, x2, y2, {'wall', 'enemy'})

if hit then
    local body = hit.body
    local shape = hit.shape
    local point_x, point_y = hit.point_x, hit.point_y
    local normal_x, normal_y = hit.normal_x, hit.normal_y
    local fraction = hit.fraction  -- 0-1, where along the path
end

-- Circle cast (sweep a circle along a path)
local hit = physics_cast_circle(x, y, radius, dx, dy, {'wall'})
local hits = physics_cast_circle_all(x, y, radius, dx, dy, {'wall'})

-- Box cast (sweep an axis-aligned box)
local hit = physics_cast_aabb(x, y, width, height, dx, dy, {'wall'})
local hits = physics_cast_aabb_all(x, y, width, height, dx, dy, {'wall'})

-- Rotated box cast (sweep a rotated box)
local hit = physics_cast_box(x, y, width, height, angle, dx, dy, {'wall'})
local hits = physics_cast_box_all(x, y, width, height, angle, dx, dy, {'wall'})

-- Capsule cast (sweep a capsule)
local hit = physics_cast_capsule(x1, y1, x2, y2, radius, dx, dy, {'wall'})
local hits = physics_cast_capsule_all(x1, y1, x2, y2, radius, dx, dy, {'wall'})

-- Polygon cast (sweep a convex polygon)
local hit = physics_cast_polygon(x, y, {dx1, dy1, dx2, dy2, ...}, dx, dy, {'wall'})
local hits = physics_cast_polygon_all(x, y, {dx1, dy1, dx2, dy2, ...}, dx, dy, {'wall'})
```

---

### Collision Filter Queries

```lua
-- Check if two tags collide
local collides = physics_tags_collide('player', 'enemy')

-- Get all tags a tag collides with
local tags = physics_get_collision_tags('player')
```

---

### World Control

```lua
-- Manual stepping (if not using engine's built-in stepping)
physics_step(dt)  -- Usually called by engine automatically

-- Enable/disable physics
physics_set_enabled(true)  -- Pause/resume simulation
```

---

## Implementation Notes

### Tag-to-bit mapping
- Each registered tag gets a unique bit (1, 2, 4, 8, 16, ...)
- With 64-bit categories, supports up to 64 unique tags
- `physics_enable_collision('a', 'b')` sets both `a.mask |= b.category` and `b.mask |= a.category`

### Meter scaling
- Store `PIXELS_PER_METER` (default 64)
- All Lua positions/velocities are in pixels
- Convert: `meters = pixels / PIXELS_PER_METER`
- This keeps gameplay code intuitive while Box2D stays in its happy range

### Body/Shape IDs
- Box2D returns `b2BodyId`/`b2ShapeId` structs
- Pass directly to Lua as userdata, Lua passes back, use directly with Box2D
- No transformation, no mapping - just pass through
- Box2D handles validation via `b2Body_IsValid()`, `b2Shape_IsValid()`

### Event buffering
- After `b2World_Step()`, iterate all events and store them in arrays keyed by tag pairs
- Events cleared at start of next frame
- Shape IDs validated with `b2Shape_IsValid()` before accessing

### User data flow
- Each body stores an integer ID (not a Lua table pointer)
- Lua object system maintains its own `id → object` lookup table
- This avoids passing Lua tables through C

---

## Sources

- [Box2D Documentation Overview](https://box2d.org/documentation/)
- [Box2D v3.1 Release Notes](https://box2d.org/documentation/md_release__notes__v310.html)
- [Box2D Events](https://box2d.org/documentation/group__events.html)
- [Box2D Simulation](https://box2d.org/documentation/md_simulation.html)
- [Box2D Collision](https://box2d.org/documentation/md_collision.html)
- [Box2D Shape Functions](https://box2d.org/documentation/group__shape.html)
- [Box2D Body Functions](https://box2d.org/documentation/group__body.html)
- [Box2D Migration Guide (v2 to v3)](https://box2d.org/documentation/md_migration.html)
- [Box2D GitHub - box2d.h](https://github.com/erincatto/box2d/blob/main/include/box2d/box2d.h)
- [Box2D GitHub - types.h](https://github.com/erincatto/box2d/blob/main/include/box2d/types.h)
- [Box2D GitHub - id.h](https://github.com/erincatto/box2d/blob/main/include/box2d/id.h)
