require('anchor')({width = 480, height = 270, title = "Primitives Test"})

--[[
-- SHAPES TEST (commented out)

-- Setup layer
game = an:layer('game')
game.camera = nil  -- No camera, draw in screen space

-- Colors
white = color(255, 255, 255)
red = color(255, 100, 100)
green = color(100, 255, 100)
blue = color(100, 100, 255)
yellow = color(255, 255, 100)
cyan = color(100, 255, 255)
magenta = color(255, 100, 255)
orange = color(255, 180, 100)
bg = color(40, 44, 52)

-- Primitives test - static display
an:late_action('draw', function(self)
  -- Background
  game:rectangle(0, 0, 480, 270, bg())

  -- Row 1: Filled shapes (y = 50)
  -- Rectangle
  game:rectangle(10, 30, 50, 40, red())

  -- Circle
  game:circle(95, 50, 18, green())

  -- Triangle
  game:triangle(130, 70, 165, 70, 147, 30, blue())

  -- Line (capsule-style with width)
  game:line(180, 35, 230, 65, 6, yellow())

  -- Capsule
  game:capsule(250, 35, 300, 65, 10, cyan())

  -- Polygon (hexagon)
  game:polygon({
    345, 30,   -- top
    365, 40,   -- top-right
    365, 60,   -- bottom-right
    345, 70,   -- bottom
    325, 60,   -- bottom-left
    325, 40    -- top-left
  }, magenta())

  -- Rounded rectangle
  game:rounded_rectangle(390, 30, 60, 40, 10, orange())

  -- Row 2: Outline shapes (y = 140)
  -- Rectangle outline
  game:rectangle_line(10, 120, 50, 40, red(), 2)

  -- Circle outline
  game:circle_line(95, 140, 18, green(), 2)

  -- Triangle outline
  game:triangle_line(130, 160, 165, 160, 147, 120, blue(), 2)

  -- Line (thin, acts as simple line)
  game:line(180, 125, 230, 155, 2, yellow())

  -- Capsule outline
  game:capsule_line(250, 125, 300, 155, 10, cyan(), 2)

  -- Polygon outline (hexagon)
  game:polygon_line({
    345, 120,  -- top
    365, 130,  -- top-right
    365, 150,  -- bottom-right
    345, 160,  -- bottom
    325, 150,  -- bottom-left
    325, 130   -- top-left
  }, magenta(), 2)

  -- Rounded rectangle outline
  game:rounded_rectangle_line(390, 120, 60, 40, 10, orange(), 2)

  -- Row 3: Mixed / transformed (y = 230)
  -- Rotated rectangle
  game:push(50, 220, math.pi/6, 1, 1)
  game:rectangle(-25, -15, 50, 30, orange())
  game:pop()

  -- Scaled circle
  game:push(130, 220, 0, 1.5, 0.75)
  game:circle(0, 0, 15, white())
  game:pop()

  -- Rotated triangle
  game:push(200, 220, math.pi/4, 1, 1)
  game:triangle(-15, 15, 15, 15, 0, -15, red())
  game:pop()

  -- Multiple overlapping
  game:circle(280, 220, 25, color(255, 0, 0, 128)())
  game:circle(300, 220, 25, color(0, 255, 0, 128)())
  game:circle(290, 200, 25, color(0, 0, 255, 128)())

  -- Rotated rounded rectangle
  game:push(400, 220, math.pi/8, 1, 1)
  game:rounded_rectangle(-25, -15, 50, 30, 8, cyan())
  game:pop()

  -- Pentagon
  game:polygon({
    380, 195,  -- top
    405, 210,  -- top-right
    395, 240,  -- bottom-right
    365, 240,  -- bottom-left
    355, 210   -- top-left
  }, yellow())

  -- Octagon outline
  game:polygon_line({
    450, 195,  -- top
    465, 200,  -- top-right 1
    470, 215,  -- right
    465, 230,  -- bottom-right
    450, 235,  -- bottom
    435, 230,  -- bottom-left
    430, 215,  -- left
    435, 200   -- top-left
  }, cyan(), 1)
end)

-- Draw function
draw = function()
  game:render()
  game:draw()
end

-- END SHAPES TEST
--]]

-- =============================================================================
-- BASE TEST
-- =============================================================================

-- =============================================================================
-- ENGINE STATE TESTS
-- =============================================================================
print("=== ENGINE STATE TESTS ===")
print("Static values (set at init):")
print("  an.width = " .. tostring(an.width))
print("  an.height = " .. tostring(an.height))
print("  an.dt = " .. tostring(an.dt))
print("  an.platform = " .. tostring(an.platform))

-- =============================================================================
-- COLOR TESTS (Phase 1: Basic creation, RGB, packed value)
-- =============================================================================
print("=== COLOR TESTS ===")

-- Test 1: Creation with defaults
c1 = color()
print("Test 1 - Default color: r=" .. tostring(c1.r) .. " g=" .. tostring(c1.g) .. " b=" .. tostring(c1.b) .. " a=" .. tostring(c1.a))
print("  Expected: r=255 g=255 b=255 a=255")

-- Test 2: Creation with RGB
c2 = color(255, 0, 0)
print("Test 2 - Red color: r=" .. tostring(c2.r) .. " g=" .. tostring(c2.g) .. " b=" .. tostring(c2.b) .. " a=" .. tostring(c2.a))
print("  Expected: r=255 g=0 b=0 a=255")

-- Test 3: Creation with RGBA
c3 = color(0, 255, 0, 128)
print("Test 3 - Green transparent: r=" .. tostring(c3.r) .. " g=" .. tostring(c3.g) .. " b=" .. tostring(c3.b) .. " a=" .. tostring(c3.a))
print("  Expected: r=0 g=255 b=0 a=128")

-- Test 4: Modify in place
c4 = color(100, 100, 100)
c4.r = 200
c4.g = 50
print("Test 4 - Modified: r=" .. tostring(c4.r) .. " g=" .. tostring(c4.g) .. " b=" .. tostring(c4.b) .. " a=" .. tostring(c4.a))
print("  Expected: r=200 g=50 b=100 a=255")

-- Test 5: Packed value via __call
c5 = color(255, 128, 64, 255)
packed = c5()
expected = rgba(255, 128, 64, 255)
print("Test 5 - Packed value: " .. tostring(packed))
print("  Expected (rgba): " .. tostring(expected))
print("  Match: " .. tostring(packed == expected))

-- =============================================================================
-- Phase 2: HSL properties
-- =============================================================================
print("\n--- Phase 2: HSL Properties ---")

-- Test 6: Read HSL from pure red (h=0, s=1, l=0.5)
c6 = color(255, 0, 0)
print("Test 6 - Red HSL: h=" .. tostring(c6.h) .. " s=" .. tostring(c6.s) .. " l=" .. tostring(c6.l))
print("  Expected: h=0 s=1 l=0.5")

-- Test 7: Read HSL from pure green (h=120, s=1, l=0.5)
c7 = color(0, 255, 0)
print("Test 7 - Green HSL: h=" .. tostring(c7.h) .. " s=" .. tostring(c7.s) .. " l=" .. tostring(c7.l))
print("  Expected: h=120 s=1 l=0.5")

-- Test 8: Read HSL from pure blue (h=240, s=1, l=0.5)
c8 = color(0, 0, 255)
print("Test 8 - Blue HSL: h=" .. tostring(c8.h) .. " s=" .. tostring(c8.s) .. " l=" .. tostring(c8.l))
print("  Expected: h=240 s=1 l=0.5")

-- Test 9: Read HSL from gray (achromatic, s=0)
c9 = color(128, 128, 128)
print("Test 9 - Gray HSL: h=" .. tostring(c9.h) .. " s=" .. tostring(c9.s) .. " l=" .. tostring(string.format('%.3f', c9.l)))
print("  Expected: h=0 s=0 l=~0.502")

-- Test 10: Set lightness to make red lighter
c10 = color(255, 0, 0)
c10.l = 0.75
print("Test 10 - Lightened red: r=" .. tostring(c10.r) .. " g=" .. tostring(c10.g) .. " b=" .. tostring(c10.b) .. " (l=" .. tostring(c10.l) .. ")")
print("  Expected: ~r=255 g=128 b=128 (pink-ish)")

-- Test 11: Set lightness to make red darker
c11 = color(255, 0, 0)
c11.l = 0.25
print("Test 11 - Darkened red: r=" .. tostring(c11.r) .. " g=" .. tostring(c11.g) .. " b=" .. tostring(c11.b) .. " (l=" .. tostring(c11.l) .. ")")
print("  Expected: ~r=128 g=0 b=0 (dark red)")

-- Test 12: Shift hue from red to green
c12 = color(255, 0, 0)
c12.h = 120
print("Test 12 - Hue shifted to green: r=" .. tostring(c12.r) .. " g=" .. tostring(c12.g) .. " b=" .. tostring(c12.b) .. " (h=" .. tostring(c12.h) .. ")")
print("  Expected: r=0 g=255 b=0")

-- Test 13: Desaturate
c13 = color(255, 0, 0)
c13.s = 0
print("Test 13 - Desaturated red: r=" .. tostring(c13.r) .. " g=" .. tostring(c13.g) .. " b=" .. tostring(c13.b) .. " (s=" .. tostring(c13.s) .. ")")
print("  Expected: ~r=128 g=128 b=128 (gray at same lightness)")

-- Test 14: Round-trip RGB -> HSL -> RGB
c14 = color(100, 150, 200)
orig_r, orig_g, orig_b = c14.r, c14.g, c14.b
c14.h = c14.h  -- trigger HSL -> RGB conversion
print("Test 14 - Round-trip: original=" .. tostring(orig_r) .. "," .. tostring(orig_g) .. "," .. tostring(orig_b) .. " after=" .. tostring(c14.r) .. "," .. tostring(c14.g) .. "," .. tostring(c14.b))
print("  Expected: same values (or very close)")

-- =============================================================================
-- Phase 3: Arithmetic operators
-- =============================================================================
print("\n--- Phase 3: Arithmetic Operators ---")

-- Test 15: Multiply by scalar (darken)
c15 = color(200, 100, 50)
c15 = c15 * 0.5
print("Test 15 - Multiply by 0.5: r=" .. tostring(c15.r) .. " g=" .. tostring(c15.g) .. " b=" .. tostring(c15.b))
print("  Expected: r=100 g=50 b=25")

-- Test 16: Divide by scalar (darken)
c16 = color(200, 100, 50)
c16 = c16 / 2
print("Test 16 - Divide by 2: r=" .. tostring(c16.r) .. " g=" .. tostring(c16.g) .. " b=" .. tostring(c16.b))
print("  Expected: r=100 g=50 b=25")

-- Test 17: Add scalar (lighten all channels)
c17 = color(100, 100, 100)
c17 = c17 + 50
print("Test 17 - Add 50: r=" .. tostring(c17.r) .. " g=" .. tostring(c17.g) .. " b=" .. tostring(c17.b))
print("  Expected: r=150 g=150 b=150")

-- Test 18: Subtract scalar
c18 = color(100, 100, 100)
c18 = c18 - 30
print("Test 18 - Subtract 30: r=" .. tostring(c18.r) .. " g=" .. tostring(c18.g) .. " b=" .. tostring(c18.b))
print("  Expected: r=70 g=70 b=70")

-- Test 19: Add another color
c19 = color(100, 50, 25)
c19 = c19 + color(50, 50, 50)
print("Test 19 - Add color(50,50,50): r=" .. tostring(c19.r) .. " g=" .. tostring(c19.g) .. " b=" .. tostring(c19.b))
print("  Expected: r=150 g=100 b=75")

-- Test 20: Subtract another color
c20 = color(200, 150, 100)
c20 = c20 - color(50, 50, 50)
print("Test 20 - Subtract color(50,50,50): r=" .. tostring(c20.r) .. " g=" .. tostring(c20.g) .. " b=" .. tostring(c20.b))
print("  Expected: r=150 g=100 b=50")

-- Test 21: Clamping at 0
c21 = color(50, 50, 50)
c21 = c21 - 100
print("Test 21 - Clamp at 0: r=" .. tostring(c21.r) .. " g=" .. tostring(c21.g) .. " b=" .. tostring(c21.b))
print("  Expected: r=0 g=0 b=0")

-- Test 22: Clamping at 255
c22 = color(200, 200, 200)
c22 = c22 + 100
print("Test 22 - Clamp at 255: r=" .. tostring(c22.r) .. " g=" .. tostring(c22.g) .. " b=" .. tostring(c22.b))
print("  Expected: r=255 g=255 b=255")

-- Test 23: Chaining (returns self)
c23 = color(100, 100, 100)
result = c23 + 50
print("Test 23 - Chaining returns self: " .. tostring(result == c23))
print("  Expected: true")

-- Test 24: HSL updates after arithmetic
c24 = color(255, 0, 0)  -- pure red, l=0.5
c24 = c24 * 0.5          -- darken
print("Test 24 - HSL updates: l=" .. tostring(string.format('%.2f', c24.l)) .. " (was 0.5)")
print("  Expected: l=0.25 (darker)")

-- =============================================================================
-- Phase 4: clone, invert, mix
-- =============================================================================
print("\n--- Phase 4: clone, invert, mix ---")

-- Test 25: Clone creates independent copy
c25 = color(100, 150, 200)
c25_copy = c25:clone()
c25.r = 50
print("Test 25 - Clone independent: original r=" .. tostring(c25.r) .. ", copy r=" .. tostring(c25_copy.r))
print("  Expected: original r=50, copy r=100")

-- Test 26: Clone preserves alpha
c26 = color(100, 100, 100, 128)
c26_copy = c26:clone()
print("Test 26 - Clone preserves alpha: " .. tostring(c26_copy.a))
print("  Expected: 128")

-- Test 27: Invert RGB
c27 = color(100, 150, 200)
c27:invert()
print("Test 27 - Invert: r=" .. tostring(c27.r) .. " g=" .. tostring(c27.g) .. " b=" .. tostring(c27.b))
print("  Expected: r=155 g=105 b=55")

-- Test 28: Invert returns self
c28 = color(100, 100, 100)
result28 = c28:invert()
print("Test 28 - Invert returns self: " .. tostring(result28 == c28))
print("  Expected: true")

-- Test 29: Mix halfway
c29 = color(0, 0, 0)
c29:mix(color(100, 200, 100), 0.5)
print("Test 29 - Mix 50%: r=" .. tostring(c29.r) .. " g=" .. tostring(c29.g) .. " b=" .. tostring(c29.b))
print("  Expected: r=50 g=100 b=50")

-- Test 30: Mix at t=0 (no change)
c30 = color(100, 100, 100)
c30:mix(color(200, 200, 200), 0)
print("Test 30 - Mix t=0: r=" .. tostring(c30.r) .. " g=" .. tostring(c30.g) .. " b=" .. tostring(c30.b))
print("  Expected: r=100 g=100 b=100")

-- Test 31: Mix at t=1 (full target)
c31 = color(100, 100, 100)
c31:mix(color(200, 200, 200), 1)
print("Test 31 - Mix t=1: r=" .. tostring(c31.r) .. " g=" .. tostring(c31.g) .. " b=" .. tostring(c31.b))
print("  Expected: r=200 g=200 b=200")

-- Test 32: Mix interpolates alpha
c32 = color(100, 100, 100, 0)
c32:mix(color(100, 100, 100, 200), 0.5)
print("Test 32 - Mix alpha: a=" .. tostring(c32.a))
print("  Expected: a=100")

-- Test 33: Mix returns self
c33 = color(100, 100, 100)
result33 = c33:mix(color(200, 200, 200), 0.5)
print("Test 33 - Mix returns self: " .. tostring(result33 == c33))
print("  Expected: true")

print("=== END COLOR TESTS ===\n")

-- =============================================================================
-- ARRAY TESTS
-- =============================================================================
print("=== ARRAY TESTS ===")

-- Test: array.all
print("\n--- array.all ---")
print("all {1,2,3} > 0: " .. tostring(array.all({1, 2, 3}, function(v) return v > 0 end)))
print("  Expected: true")
print("all {1,2,3} < 2: " .. tostring(array.all({1, 2, 3}, function(v) return v < 2 end)))
print("  Expected: false")
print("all {} any: " .. tostring(array.all({}, function(v) return v == 0 end)))
print("  Expected: true (vacuous)")

-- Test: array.any
print("\n--- array.any ---")
print("any {1,2,3} > 2: " .. tostring(array.any({1, 2, 3}, function(v) return v > 2 end)))
print("  Expected: true")
print("any {1,2,3} > 5: " .. tostring(array.any({1, 2, 3}, function(v) return v > 5 end)))
print("  Expected: false")

-- Test: array.average
print("\n--- array.average ---")
print("average {1, 3}: " .. tostring(array.average({1, 3})))
print("  Expected: 2")
print("average {-3, 3}: " .. tostring(array.average({-3, 3})))
print("  Expected: 0")

-- Test: array.sum
print("\n--- array.sum ---")
print("sum {1, 2, 3}: " .. tostring(array.sum({1, 2, 3})))
print("  Expected: 6")
print("sum with func: " .. tostring(array.sum({{a = 1}, {a = 4}}, function(v) return v.a end)))
print("  Expected: 5")

-- Test: array.max
print("\n--- array.max ---")
print("max {1, 5, 3}: " .. tostring(array.max({1, 5, 3})))
print("  Expected: 5")
print("max {-2, 0, -10}: " .. tostring(array.max({-2, 0, -10})))
print("  Expected: 0")
max_obj = array.max({{a = 1}, {a = 4}, {a = 2}}, function(v) return v.a end)
print("max with func: " .. tostring(max_obj.a))
print("  Expected: 4")

-- Test: array.count
print("\n--- array.count ---")
print("count {1, 1, 2}: " .. tostring(array.count({1, 1, 2})))
print("  Expected: 3")
print("count {1, 1, 2}, 1: " .. tostring(array.count({1, 1, 2}, 1)))
print("  Expected: 2")
print("count with func (v > 3): " .. tostring(array.count({1, 2, 3, 4, 5}, function(v) return v > 3 end)))
print("  Expected: 2")

-- Test: array.has
print("\n--- array.has ---")
print("has {1, 2, 3}, 2: " .. tostring(array.has({1, 2, 3}, 2)))
print("  Expected: true")
print("has {1, 2, 3}, 5: " .. tostring(array.has({1, 2, 3}, 5)))
print("  Expected: false")
print("has with func (v > 2): " .. tostring(array.has({1, 2, 3}, function(v) return v > 2 end)))
print("  Expected: true")

-- Test: array.index
print("\n--- array.index ---")
print("index {2, 1, 2}, 2: " .. tostring(array.index({2, 1, 2}, 2)))
print("  Expected: 1")
print("index {2, 1, 2}, 1: " .. tostring(array.index({2, 1, 2}, 1)))
print("  Expected: 2")
print("index with func (v > 2): " .. tostring(array.index({1, 2, 3, 4}, function(v) return v > 2 end)))
print("  Expected: 3")

-- Test: array.get
print("\n--- array.get ---")
print("get {4, 3, 2, 1}, 1: " .. tostring(array.get({4, 3, 2, 1}, 1)))
print("  Expected: 4")
print("get {4, 3, 2, 1}, -1: " .. tostring(array.get({4, 3, 2, 1}, -1)))
print("  Expected: 1")
get_range = array.get({4, 3, 2, 1}, 1, 3)
print("get {4, 3, 2, 1}, 1, 3: " .. tostring(table.tostring(get_range)))
print("  Expected: {4, 3, 2}")
get_neg = array.get({4, 3, 2, 1}, -2, -1)
print("get {4, 3, 2, 1}, -2, -1: " .. tostring(table.tostring(get_neg)))
print("  Expected: {2, 1}")

-- Test: array.get_circular_buffer_index
print("\n--- array.get_circular_buffer_index ---")
print("circular {'a','b','c'}, 1: " .. tostring(array.get_circular_buffer_index({'a', 'b', 'c'}, 1)))
print("  Expected: 1")
print("circular {'a','b','c'}, 0: " .. tostring(array.get_circular_buffer_index({'a', 'b', 'c'}, 0)))
print("  Expected: 3")
print("circular {'a','b','c'}, 4: " .. tostring(array.get_circular_buffer_index({'a', 'b', 'c'}, 4)))
print("  Expected: 1")

-- Test: array.delete
print("\n--- array.delete ---")
del_t = {1, 2, 1, 3, 1}
del_count = array.delete(del_t, 1)
print("delete {1,2,1,3,1}, 1: removed " .. tostring(del_count) .. ", result " .. tostring(table.tostring(del_t)))
print("  Expected: removed 3, result {2, 3}")

-- Test: array.remove
print("\n--- array.remove ---")
rem_t = {3, 2, 1}
rem_val = array.remove(rem_t, 1)
print("remove {3, 2, 1}, 1: removed " .. tostring(rem_val) .. ", result " .. tostring(table.tostring(rem_t)))
print("  Expected: removed 3, result {2, 1}")

-- Test: array.reverse
print("\n--- array.reverse ---")
rev_t = {1, 2, 3, 4}
array.reverse(rev_t)
print("reverse {1, 2, 3, 4}: " .. tostring(table.tostring(rev_t)))
print("  Expected: {4, 3, 2, 1}")
rev_t2 = {1, 2, 3, 4}
array.reverse(rev_t2, 1, 2)
print("reverse {1, 2, 3, 4}, 1, 2: " .. tostring(table.tostring(rev_t2)))
print("  Expected: {2, 1, 3, 4}")

-- Test: array.rotate
print("\n--- array.rotate ---")
rot_t = {1, 2, 3, 4}
array.rotate(rot_t, 1)
print("rotate {1, 2, 3, 4}, 1: " .. tostring(table.tostring(rot_t)))
print("  Expected: {4, 1, 2, 3}")
rot_t2 = {1, 2, 3, 4}
array.rotate(rot_t2, -1)
print("rotate {1, 2, 3, 4}, -1: " .. tostring(table.tostring(rot_t2)))
print("  Expected: {2, 3, 4, 1}")

-- Test: array.shuffle (just check it doesn't error and length preserved)
print("\n--- array.shuffle ---")
shuf_t = {1, 2, 3, 4, 5}
array.shuffle(shuf_t)
print("shuffle {1,2,3,4,5}: length=" .. tostring(#shuf_t) .. ", sum=" .. tostring(array.sum(shuf_t)))
print("  Expected: length=5, sum=15")

-- Test: array.random
print("\n--- array.random ---")
rand_t = {10, 20, 30, 40, 50}
rand_single = array.random(rand_t)
print("random {10,20,30,40,50}: " .. tostring(rand_single) .. " (should be one of 10,20,30,40,50)")
rand_multi = array.random(rand_t, 3)
print("random 3 elements: " .. tostring(table.tostring(rand_multi)) .. " (3 unique values)")
print("  Length: " .. tostring(#rand_multi) .. ", Expected: 3")

-- Test: array.remove_random
print("\n--- array.remove_random ---")
rem_rand_t = {1, 2, 3, 4, 5}
rem_rand_val = array.remove_random(rem_rand_t)
print("remove_random: removed " .. tostring(rem_rand_val) .. ", remaining " .. tostring(table.tostring(rem_rand_t)))
print("  Remaining length: " .. tostring(#rem_rand_t) .. ", Expected: 4")

-- Test: array.flatten
print("\n--- array.flatten ---")
flat1 = array.flatten({1, 2, {3, 4}})
print("flatten {1, 2, {3, 4}}: " .. tostring(table.tostring(flat1)))
print("  Expected: {1, 2, 3, 4}")
flat2 = array.flatten({1, {2, {3, {4}}}})
print("flatten {1, {2, {3, {4}}}}: " .. tostring(table.tostring(flat2)))
print("  Expected: {1, 2, 3, 4}")
flat3 = array.flatten({1, {2, {3, {4}}}}, 1)
print("flatten level=1: " .. tostring(table.tostring(flat3)))
print("  Expected: {1, 2, {3, {4}}} (nested tables preserved)")

-- Test: array.join
print("\n--- array.join ---")
print("join {1, 2, 3}: '" .. tostring(array.join({1, 2, 3})) .. "'")
print("  Expected: '123'")
print("join {1, 2, 3}, ', ': '" .. tostring(array.join({1, 2, 3}, ', ')) .. "'")
print("  Expected: '1, 2, 3'")

-- Test: table.copy
print("\n--- table.copy ---")
orig = {a = 1, b = {c = 2}}
copy = table.copy(orig)
copy.b.c = 999
print("deep copy: orig.b.c=" .. tostring(orig.b.c) .. ", copy.b.c=" .. tostring(copy.b.c))
print("  Expected: orig.b.c=2, copy.b.c=999")

-- Test: table.tostring
print("\n--- table.tostring ---")
print("tostring {1, 2, 3}: " .. tostring(table.tostring({1, 2, 3})))
print("  Expected: {[1] = 1, [2] = 2, [3] = 3}")

print("\n=== END ARRAY TESTS ===\n")

-- Screen dimensions (must be before camera)
W, H = 480, 270

-- Create camera first (layers will reference it)
an:add(camera())
an.camera:add(shake())
an:add(spring())
an.spring:add('camera_rotation', 0, 2, 0.5)  -- 2 Hz, moderate bounce

-- Setup layers
game = an:layer('game')
game_2 = an:layer('game_2')
bg = an:layer('bg')
shadow = an:layer('shadow')
game_outline = an:layer('game_outline')
game_2_outline = an:layer('game_2_outline')
ui = an:layer('ui')
ui.camera = nil  -- UI layer stays in screen space

-- Resources
an:font('main', 'assets/LanaPixel.ttf', 11)
an:image('ball', 'assets/slight_smile.png')
an:shader('shadow', 'shaders/shadow.frag')
an:shader('outline', 'shaders/outline.frag')

-- Audio resources
an:sound('death', 'assets/player_death.ogg')
an:music('track1', 'assets/speder2_01.ogg')
an:music('track2', 'assets/speder2_02.ogg')
an:music('track3', 'assets/speder2_03.ogg')

-- Spritesheet resources
an:spritesheet('hit', 'assets/hit1.png', 96, 48)

-- Animation test objects (three loop modes)
test_anim_loop = animation('hit', 0.1, 'loop')
test_anim_once = animation('hit', 0.1, 'once', {
  [0] = function(self) print("once animation completed!") end,
})
test_anim_bounce = animation('hit', 0.1, 'bounce')

-- Setup playlist
an:playlist_set({'track1', 'track2', 'track3'})

-- Print audio test controls
print("=== AUDIO TEST CONTROLS ===")
print("1 - Play death sound")
print("2 - Play track1 directly")
print("3 - Stop music")
print("4 - Start playlist")
print("5 - Playlist next")
print("6 - Playlist prev")
print("7 - Toggle shuffle")
print("8 - Toggle crossfade (0 or 2 seconds)")
print("9 - Crossfade to track2 (2 seconds)")
print("0 - Stop playlist")
print("===========================")
print("")
print("=== TIME SCALE CONTROLS ===")
print("F1 - Slow to 0.5 (instant)")
print("F2 - Slow to 0.5 with 0.5s recovery (typical player hit)")
print("F3 - Slow to 0.1 with 1s elastic recovery")
print("F4 - Cancel slow")
print("F5 - Hit stop 0.1s")
print("F6 - Hit stop 0.2s (UI excluded)")
print("F7 - Print time scale info")
print("F8 - Reset 'once' animation")
print("===========================")

-- Initialize physics
an:physics_init()
an:physics_set_gravity(0, 500)
an:physics_set_meter_scale(64)

-- Register tags and collisions
an:physics_tag('ball')
an:physics_tag('wall')
an:physics_tag('impulse_block')
an:physics_tag('slowing_zone')
an:physics_collision('ball', 'wall')
an:physics_collision('ball', 'ball')
an:physics_collision('ball', 'impulse_block')
an:physics_sensor('ball', 'slowing_zone')
an:physics_hit('ball', 'wall')

-- Colors (twitter emoji theme)
bg_color = color(231, 232, 233)
green = color(122, 179, 87)
blue = color(85, 172, 238)
blue_transparent = color(85, 172, 238, 128)
yellow = color(255, 204, 77)
red = color(221, 46, 68)
orange = color(244, 144, 12)
purple = color(170, 142, 214)
black = color(0, 0, 0)
white = color(255, 255, 255)

-- Wall dimensions
ground_width = W*0.9
ground_height = 12
ground_x = (W - ground_width)/2
ground_y = H - 20 - ground_height/2

wall_width = 12
wall_top = H*0.1
wall_height = ground_y - wall_top
left_wall_x = ground_x
right_wall_x = ground_x + ground_width - wall_width

-- Create wall colliders (static bodies)
wall = object:extend()

function wall:new(x, y, w, h, clr, rounded_top, rounded_left)
  object.new(self)
  self:tag('drawable')
  self.w = w
  self.h = h
  self.color = clr or green
  self.rounded_top = rounded_top or false
  self.rounded_left = rounded_left or false
  self:add(collider('wall', 'static', 'box', self.w, self.h))
  self.collider:set_position(x, y)
  self.collider:set_friction(1)
end

function wall:draw(layer)
  if self.rounded_top then
    local radius = self.w/2
    layer:circle(self.x, self.y - self.h/2 + radius, radius, self.color())
    layer:rectangle(self.x - self.w/2, self.y - self.h/2 + radius, self.w, self.h - radius, self.color())
  elseif self.rounded_left then
    local radius = self.h/2
    layer:circle(self.x - self.w/2 + radius, self.y, radius, self.color())
    layer:rectangle(self.x - self.w/2 + radius, self.y - self.h/2, self.w - radius, self.h, self.color())
  else
    layer:rectangle(self.x - self.w/2, self.y - self.h/2, self.w, self.h, self.color())
  end
end

-- Physics positions are center-based
an:add(wall(ground_x + ground_width/2, ground_y + ground_height/2, ground_width, ground_height))
an:add(wall(left_wall_x + wall_width/2, wall_top + wall_height/2, wall_width, wall_height, green, true))
an:add(wall(right_wall_x + wall_width/2, wall_top + wall_height/2, wall_width, wall_height))

-- Ceiling (half width, right-aligned, rounded left)
ceiling_width = ground_width/2
ceiling_height = ground_height
ceiling_x = right_wall_x + wall_width - ceiling_width/2
ceiling_y = wall_top + ceiling_height/2
an:add(wall(ceiling_x, ceiling_y, ceiling_width, ceiling_height, green, false, true))

-- Impulse block (blue rectangle at bottom left corner, standing on ground)
impulse_width = ground_width*0.1
impulse_height = ground_height
impulse_x = left_wall_x + wall_width
impulse_y = ground_y - impulse_height

impulse_block = object:extend()

function impulse_block:new(x, y, w, h)
  object.new(self)
  self:tag('impulse_block')
  self.w = w
  self.h = h
  self.flash = false
  self:add(timer())
  self:add(spring())
  self:add(collider('impulse_block', 'static', 'box', self.w, self.h))
  self.collider:set_position(x + self.w/2, y + self.h/2)
  self.collider:set_friction(1)
  self.collider:set_restitution(1)
end

function impulse_block:draw(layer)
  layer:push(self.x, self.y, 0, self.spring.main.x, self.spring.main.x)
  layer:rectangle(-self.w/2, -self.h/2, self.w, self.h, self.flash and white() or blue())
  layer:pop()
end

an:add(impulse_block(impulse_x, impulse_y, impulse_width, impulse_height))

-- Slowing zone (under ceiling left edge, 1/3 height)
box_interior_width = ground_width - 2*wall_width
box_interior_height = wall_height
slowing_zone_width = box_interior_width/6
slowing_zone_height = box_interior_height/3
ceiling_left_edge = ceiling_x - ceiling_width/2
slowing_zone_x = ceiling_left_edge
slowing_zone_y = wall_top + ceiling_height + slowing_zone_height/2

slowing_zone = object:extend()

function slowing_zone:new(x, y, w, h)
  object.new(self)
  self:tag('slowing_zone')
  self.w = w
  self.h = h
  self:add(collider('slowing_zone', 'static', 'box', self.w, self.h, {sensor = true}))
  self.collider:set_position(x, y)
end

function slowing_zone:draw(layer)
  layer:rectangle(self.x - self.w/2, self.y - self.h/2, self.w, self.h, blue_transparent())
end

an:add(slowing_zone(slowing_zone_x, slowing_zone_y, slowing_zone_width, slowing_zone_height))

-- Ball class
ball_radius = 10
ball_scale = ball_radius*2/an.images.ball.width

ball = object:extend()

function ball:new(x, y)
  object.new(self)
  self.x = x
  self.y = y
  self:tag('ball')
  self:tag('drawable')
  self.impulsed = false
  self.original_speed = 0
  self.flash = false
  self:add(timer())
  self:add(spring())
  self:add(collider('ball', 'dynamic', 'circle', ball_radius))
  self.collider:set_position(self.x, self.y)
  self.collider:set_restitution(1)
  self.collider:set_friction(1)
end

function ball:draw(layer)
  local angle = self.collider:get_angle()
  local scale = ball_scale*self.spring.main.x
  layer:push(self.x, self.y, angle, scale, scale)
  layer:image(an.images.ball, 0, 0, nil, self.flash and white() or nil)
  layer:pop()
end

-- Audio test state
audio_crossfade_enabled = false

-- Spawn ball on K, impulse on P, camera movement with WASD/arrows
-- Audio tests: 1-0 keys (see controls printed above)
an:action(function(self, dt)
  -- Audio tests
  if an:key_is_pressed('1') then
    an:sound_play('death')
    print("Sound: death")
  end

  if an:key_is_pressed('2') then
    an:music_play('track1')
    print("Music: track1")
  end

  if an:key_is_pressed('3') then
    an:music_stop()
    print("Music: stopped")
  end

  if an:key_is_pressed('4') then
    an:playlist_play()
    print("Playlist: started")
  end

  if an:key_is_pressed('5') then
    an:playlist_next()
    print("Playlist: next -> " .. an:playlist_current_track())
  end

  if an:key_is_pressed('6') then
    an:playlist_prev()
    print("Playlist: prev -> " .. an:playlist_current_track())
  end

  if an:key_is_pressed('7') then
    an:playlist_shuffle(not an.playlist_shuffle_enabled)
    print("Playlist shuffle: " .. tostring(an.playlist_shuffle_enabled))
  end

  if an:key_is_pressed('8') then
    audio_crossfade_enabled = not audio_crossfade_enabled
    if audio_crossfade_enabled then
      an:playlist_set_crossfade(2)
      print("Playlist crossfade: 2 seconds")
    else
      an:playlist_set_crossfade(0)
      print("Playlist crossfade: instant")
    end
  end

  if an:key_is_pressed('9') then
    an:music_crossfade('track2', 2)
    print("Music: crossfade to track2")
  end

  if an:key_is_pressed('0') then
    an:playlist_stop()
    print("Playlist: stopped")
  end

  if an:key_is_pressed('k') then
    local spawn_x = left_wall_x + wall_width + ball_radius + 20
    local spawn_y = wall_top - ball_radius - 5
    local new_ball = ball(spawn_x, spawn_y)
    an:add(new_ball)
    -- an.camera:follow(new_ball, 0.9, 0.1)
  end

  if an:key_is_pressed('p') then
    for _, b in ipairs(an:all('ball')) do
      b.collider:apply_impulse(200, 0)
    end
  end

  if an:key_is_pressed('r') then
    an.spring:pull('camera_rotation', math.pi/12)  -- 15 degrees
  end

  if an:key_is_pressed('t') then
    an.camera.shake:trauma(1, 1)
  end

  if an:key_is_pressed('y') then
    an.camera.shake:push(random_float(0, 2*math.pi), 20)
  end

  if an:key_is_pressed('u') then
    an.camera.shake:shake(15, 0.5)
  end

  if an:key_is_pressed('i') then
    an.camera.shake:sine(random_float(0, 2*math.pi), 15, 8, 0.5)
  end

  if an:key_is_pressed('o') then
    an.camera.shake:square(random_float(0, 2*math.pi), 15, 8, 0.5)
  end

  if an:key_is_pressed('h') then
    an.camera.shake:handcam(not an.camera.shake.handcam_enabled)
  end

  an.camera.rotation = an.spring.camera_rotation.x

  -- Screen -> world test: click on ball to flash + jiggle
  if an:mouse_is_pressed(1) then
    for _, b in ipairs(an:query_point(an.camera.mouse.x, an.camera.mouse.y, 'ball')) do
      b.flash = true
      b.timer:after(0.15, 'flash', function() b.flash = false end)
      b.spring:pull('main', 0.2, 5, 0.8)
    end
  end

  local camera_speed = 200
  if an:key_is_down('w') or an:key_is_down('up') then
    an.camera.y = an.camera.y - camera_speed*dt
  end
  if an:key_is_down('s') or an:key_is_down('down') then
    an.camera.y = an.camera.y + camera_speed*dt
  end
  if an:key_is_down('a') or an:key_is_down('left') then
    an.camera.x = an.camera.x - camera_speed*dt
  end
  if an:key_is_down('d') or an:key_is_down('right') then
    an.camera.x = an.camera.x + camera_speed*dt
  end

  -- Engine state test (press 'e' to print dynamic values)
  if an:key_is_pressed('e') then
    print("=== ENGINE STATE (dynamic) ===")
    print("  an.frame = " .. tostring(an.frame))
    print("  an.step = " .. tostring(an.step))
    print("  an.time = " .. tostring(string.format('%.2f', an.time)))
    print("  an.window_width = " .. tostring(an.window_width))
    print("  an.window_height = " .. tostring(an.window_height))
    print("  an.scale = " .. tostring(an.scale))
    print("  an.fullscreen = " .. tostring(an.fullscreen))
    print("  an.fps = " .. tostring(string.format('%.1f', an.fps)))
    print("  an.draw_calls = " .. tostring(an.draw_calls))
  end

  -- Time scale tests
  if an:key_is_pressed('f1') then
    an:slow(0.5)
    print("Slow: 0.5 (instant)")
  end

  if an:key_is_pressed('f2') then
    an:slow(0.5, 0.5)
    print("Slow: 0.5 with 0.5s cubic_in_out recovery (typical player hit)")
  end

  if an:key_is_pressed('f3') then
    an:slow(0.1, 1, math.elastic_out)
    print("Slow: 0.1 with 1s elastic recovery")
  end

  if an:key_is_pressed('f4') then
    an:cancel_slow()
    print("Slow: cancelled")
  end

  if an:key_is_pressed('f5') then
    an:hit_stop(0.1)
    print("Hit stop: 0.1s")
  end

  if an:key_is_pressed('f6') then
    an:hit_stop(0.2, {except = 'ui'})
    print("Hit stop: 0.2s (UI excluded)")
  end

  if an:key_is_pressed('f7') then
    print("=== TIME SCALE STATE ===")
    print("  an.time_scale = " .. tostring(an.time_scale))
    print("  an.dt = " .. tostring(an.dt))
    print("  an.unscaled_dt = " .. tostring(an.unscaled_dt))
    print("  an.hit_stop_active = " .. tostring(an.hit_stop_active))
  end

  -- Animation tests
  if an:key_is_pressed('f8') then
    test_anim_once:reset()
    print("Reset 'once' animation")
  end

  -- Update test animations
  test_anim_loop:update(dt)
  test_anim_once:update(dt)
  test_anim_bounce:update(dt)
end)

-- Handle collisions
an:early_action('handle_collisions', function(self)
  for _, event in ipairs(an:collision_begin_events('ball', 'impulse_block')) do
    local ball = event.a
    local block = event.b
    if not ball.impulsed then
      ball.impulsed = true
      ball.collider:apply_impulse(random_float(20, 40), 0)
      block.flash = true
      block.timer:after(0.15, 'flash', function() block.flash = false end)
      block.spring:pull('main', 0.2, 5, 0.8)
    end
  end

  for _, event in ipairs(an:sensor_begin_events('ball', 'slowing_zone')) do
    local ball = event.a
    local vx, vy = ball.collider:get_velocity()
    ball.original_speed = math.sqrt(vx*vx + vy*vy)
    ball.collider:set_velocity(vx*0.1, vy*0.1)
    ball.collider:set_gravity_scale(0.1)
  end

  for _, event in ipairs(an:sensor_end_events('ball', 'slowing_zone')) do
    local ball = event.a
    local vx, vy = ball.collider:get_velocity()
    local current_speed = math.sqrt(vx*vx + vy*vy)
    if current_speed > 0 then
      local scale = ball.original_speed/current_speed
      ball.collider:set_velocity(vx*scale, vy*scale)
    end
    ball.collider:set_gravity_scale(1)
  end

  for _, event in ipairs(an:hit_events('ball', 'wall')) do
    local ball = event.a
    if event.approach_speed > 300 then
      ball.flash = true
      ball.timer:after(0.15, 'flash', function() ball.flash = false end)
      ball.spring:pull('main', 0.2, 5, 0.8)
    end
  end
end)

-- Queue draw commands during update
an:late_action('draw', function(self)
  -- Draw background
  bg:rectangle(0, 0, W, H, bg_color())

  -- Draw all drawable objects to game layer
  for _, obj in ipairs(an:all('drawable')) do
    obj:draw(game)
  end

  -- Draw impulse blocks to game_2 layer
  for _, obj in ipairs(an:all('impulse_block')) do
    obj:draw(game_2)
  end

  -- Draw slowing zone to ui layer
  for _, zone in ipairs(an:all('slowing_zone')) do
    zone:draw(ui)
  end

  -- World -> screen test: draw UI marker above each ball
  for _, b in ipairs(an:all('ball')) do
    local screen_x, screen_y = an.camera:to_screen(b.x, b.y)
    ui:circle(screen_x, screen_y - 20, 5, red())
  end

  -- Animation test: three loop modes side by side
  ui:text("loop", 'main', 60, H - 90, white())
  ui:animation(test_anim_loop, 80, H - 55)

  ui:text("once", 'main', 180, H - 90, white())
  ui:animation(test_anim_once, 200, H - 55)

  ui:text("bounce", 'main', 290, H - 90, white())
  ui:animation(test_anim_bounce, 320, H - 55)

  -- Audio status display (check current channel OR if crossfade is in progress)
  local is_playing = music_is_playing(an.playlist_channel) or (an.crossfade_state and music_is_playing(an.crossfade_state.to_channel))
  local playing_status = is_playing and "PLAYING" or "STOPPED"
  local shuffle_status = an.playlist_shuffle_enabled and "ON" or "OFF"
  local crossfade_status = an.playlist_crossfade_duration > 0 and tostring(an.playlist_crossfade_duration) .. "s" or "OFF"
  local current_track = #an.playlist > 0 and an:playlist_current_track() or "none"

  -- Build shuffle order string
  local shuffle_order = ""
  if an.playlist_shuffle_enabled and #an.playlist_shuffled > 0 then
    local order_parts = {}
    for _, i in ipairs(an.playlist_shuffled) do
      table.insert(order_parts, tostring(i))
    end
    shuffle_order = " Order: [" .. table.concat(order_parts, ",") .. "]"
  end

  ui:text("Track: " .. tostring(current_track) .. " [" .. tostring(an.playlist_index) .. "/" .. tostring(#an.playlist) .. "]", 'main', 5, 5, white())
  ui:text("Status: " .. tostring(playing_status) .. " | Shuffle: " .. tostring(shuffle_status) .. tostring(shuffle_order) .. " | Crossfade: " .. tostring(crossfade_status), 'main', 5, 18, white())
end)

-- Global draw function - called by C after update
-- Handles: render source layers, create derived layers, composite to screen
draw = function()
  -- 1. Render source layers (process queued commands to FBOs)
  bg:render()
  game:render()
  game_2:render()
  ui:render()

  -- 2. Create derived layers (copy through shaders)
  shadow:clear()
  shadow:draw_from(game, an.shaders.shadow)
  shadow:draw_from(game_2, an.shaders.shadow)

  shader_set_vec2_immediate(an.shaders.outline, "u_pixel_size", 1/W, 1/H)
  game_outline:clear()
  game_outline:draw_from(game, an.shaders.outline)
  game_2_outline:clear()
  game_2_outline:draw_from(game_2, an.shaders.outline)

  -- 3. Composite to screen (visual back-to-front order)
  bg:draw()
  shadow:draw(4, 4)
  game_outline:draw()
  game:draw()
  game_2_outline:draw()
  game_2:draw()
  ui:draw()
end

-- =============================================================================
-- END BASE TEST
-- =============================================================================
