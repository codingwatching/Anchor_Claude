require('anchor.class')

--[[
  Mutable color with RGB and HSL access.

  Usage:
    red = color(255, 0, 0)
    red.r = 200              -- modify in place
    red.l = 0.8              -- set lightness, recomputes RGB
    layer:circle(x, y, r, red())  -- get packed value

  Color is a standalone class (not a child object).
  All properties (r, g, b, a, h, s, l) are readable and writable.
  RGB and HSL stay synchronized automatically.

  Properties:
    r, g, b  - RGB components (0-255)
    a        - alpha (0-255)
    h        - hue (0-360)
    s        - saturation (0-1)
    l        - lightness (0-1)

  Operators (mutate in place, return self):
    color * number     - multiply RGB by scalar
    color * color      - multiply RGB component-wise
    color / number     - divide RGB by scalar
    color + number     - add to all RGB
    color + color      - add RGB component-wise
    color - number     - subtract from all RGB
    color - color      - subtract RGB component-wise

  Methods:
    clone    - create independent copy
    invert   - flip RGB values (255 - value)
    mix      - linear interpolation toward another color
]]

-- Internal: RGB (0-255) to HSL (h: 0-360, s: 0-1, l: 0-1)
function rgb_to_hsl(r, g, b)
  r, g, b = r/255, g/255, b/255
  local max = math.max(r, g, b)
  local min = math.min(r, g, b)
  local l = (max + min)/2

  if max == min then
    return 0, 0, l  -- achromatic
  end

  local d = max - min
  local s = l > 0.5 and d/(2 - max - min) or d/(max + min)

  local h_offset = g < b and 6 or 0
  local h
  if max == r then
    h = ((g - b)/d + h_offset)/6
  elseif max == g then
    h = ((b - r)/d + 2)/6
  else
    h = ((r - g)/d + 4)/6
  end

  return h*360, s, l
end

-- Internal: HSL to RGB (0-255)
function hsl_to_rgb(h, s, l)
  if s == 0 then
    local v = math.floor(l*255 + 0.5)
    return v, v, v  -- achromatic
  end

  h = h/360
  local q = l < 0.5 and l*(1 + s) or l + s - l*s
  local p = 2*l - q

  local function hue_to_rgb(t)
    if t < 0 then t = t + 1 end
    if t > 1 then t = t - 1 end
    if t < 1/6 then
      return p + (q - p)*6*t
    elseif t < 1/2 then
      return q
    elseif t < 2/3 then
      return p + (q - p)*(2/3 - t)*6
    else
      return p
    end
  end

  local r = math.floor(hue_to_rgb(h + 1/3)*255 + 0.5)
  local g = math.floor(hue_to_rgb(h)*255 + 0.5)
  local b = math.floor(hue_to_rgb(h - 1/3)*255 + 0.5)
  return r, g, b
end

-- Color uses a custom metatable instead of class:extend() because it needs
-- custom __index/__newindex for property access (r, g, b, a, h, s, l).
color = {}

local color_mt = {
  __call = function(_, r, g, b, a)
    local self = setmetatable({}, color)
    self:new(r, g, b, a)
    return self
  end,
}
setmetatable(color, color_mt)

--[[
  Creates a new color.

  Usage:
    white = color()                    -- default white (255, 255, 255, 255)
    red = color(255, 0, 0)             -- opaque red
    transparent_blue = color(0, 0, 255, 128)  -- semi-transparent blue

  Parameters:
    r - red component 0-255 (default 255)
    g - green component 0-255 (default 255)
    b - blue component 0-255 (default 255)
    a - alpha component 0-255 (default 255)

  Returns: color instance
]]
function color:new(r, g, b, a)
  r = r or 255
  g = g or 255
  b = b or 255
  a = a or 255
  rawset(self, 'data', {r = r, g = g, b = b, a = a, h = 0, s = 0, l = 0})
  self:sync_hsl()
end

-- Internal: recompute HSL from current RGB
function color:sync_hsl()
  local h, s, l = rgb_to_hsl(self.data.r, self.data.g, self.data.b)
  self.data.h = h
  self.data.s = s
  self.data.l = l
end

-- Internal: recompute RGB from current HSL
function color:sync_rgb()
  local r, g, b = hsl_to_rgb(self.data.h, self.data.s, self.data.l)
  self.data.r = r
  self.data.g = g
  self.data.b = b
end

-- Internal: property getter for r, g, b, a, h, s, l
color.__index = function(self, key)
  if key == 'r' then return self.data.r
  elseif key == 'g' then return self.data.g
  elseif key == 'b' then return self.data.b
  elseif key == 'a' then return self.data.a
  elseif key == 'h' then return self.data.h
  elseif key == 's' then return self.data.s
  elseif key == 'l' then return self.data.l
  else
    return rawget(color, key)
  end
end

-- Internal: property setter for r, g, b, a, h, s, l with auto-sync
color.__newindex = function(self, key, value)
  if key == 'r' then
    self.data.r = value
    self:sync_hsl()
  elseif key == 'g' then
    self.data.g = value
    self:sync_hsl()
  elseif key == 'b' then
    self.data.b = value
    self:sync_hsl()
  elseif key == 'a' then
    self.data.a = value
  elseif key == 'h' then
    self.data.h = value % 360
    self:sync_rgb()
  elseif key == 's' then
    self.data.s = math.max(0, math.min(1, value))
    self:sync_rgb()
  elseif key == 'l' then
    self.data.l = math.max(0, math.min(1, value))
    self:sync_rgb()
  else
    rawset(self, key, value)
  end
end

--[[
  Returns packed RGBA integer for C drawing functions.

  Usage:
    layer:circle(x, y, r, red())
    layer:rectangle(0, 0, w, h, my_color())

  Returns: packed RGBA integer
]]
color.__call = function(self)
  return rgba(math.floor(self.data.r + 0.5), math.floor(self.data.g + 0.5), math.floor(self.data.b + 0.5), math.floor(self.data.a + 0.5))
end

--[[
  Multiply RGB by scalar or another color's RGB.

  Usage:
    red = red * 0.5           -- darken by half
    result = color1 * color2  -- component-wise multiply

  Behavior:
    - Scalar: multiplies each RGB component by the number
    - Color: multiplies component-wise (r*r/255, g*g/255, b*b/255)
    - Values are clamped to 0-255
    - HSL is recomputed after modification
    - Mutates in place

  Returns: self (for chaining)
]]
color.__mul = function(self, other)
  if type(other) == 'number' then
    self.data.r = math.max(0, math.min(255, self.data.r*other))
    self.data.g = math.max(0, math.min(255, self.data.g*other))
    self.data.b = math.max(0, math.min(255, self.data.b*other))
  else
    self.data.r = math.max(0, math.min(255, self.data.r*other.r/255))
    self.data.g = math.max(0, math.min(255, self.data.g*other.g/255))
    self.data.b = math.max(0, math.min(255, self.data.b*other.b/255))
  end
  self:sync_hsl()
  return self
end

--[[
  Divide RGB by scalar.

  Usage:
    red = red / 2  -- darken by half

  Behavior:
    - Divides each RGB component by the number
    - Values are clamped to 0-255
    - HSL is recomputed after modification
    - Mutates in place

  Returns: self (for chaining)
]]
color.__div = function(self, other)
  if type(other) == 'number' then
    self.data.r = math.max(0, math.min(255, self.data.r/other))
    self.data.g = math.max(0, math.min(255, self.data.g/other))
    self.data.b = math.max(0, math.min(255, self.data.b/other))
    self:sync_hsl()
  end
  return self
end

--[[
  Add scalar or another color's RGB.

  Usage:
    red = red + 50            -- brighten all channels by 50
    result = color1 + color2  -- component-wise add

  Behavior:
    - Scalar: adds to each RGB component
    - Color: adds component-wise (r+r, g+g, b+b)
    - Values are clamped to 0-255
    - HSL is recomputed after modification
    - Mutates in place

  Returns: self (for chaining)
]]
color.__add = function(self, other)
  if type(other) == 'number' then
    self.data.r = math.max(0, math.min(255, self.data.r + other))
    self.data.g = math.max(0, math.min(255, self.data.g + other))
    self.data.b = math.max(0, math.min(255, self.data.b + other))
  else
    self.data.r = math.max(0, math.min(255, self.data.r + other.r))
    self.data.g = math.max(0, math.min(255, self.data.g + other.g))
    self.data.b = math.max(0, math.min(255, self.data.b + other.b))
  end
  self:sync_hsl()
  return self
end

--[[
  Subtract scalar or another color's RGB.

  Usage:
    red = red - 50            -- darken all channels by 50
    result = color1 - color2  -- component-wise subtract

  Behavior:
    - Scalar: subtracts from each RGB component
    - Color: subtracts component-wise (r-r, g-g, b-b)
    - Values are clamped to 0-255
    - HSL is recomputed after modification
    - Mutates in place

  Returns: self (for chaining)
]]
color.__sub = function(self, other)
  if type(other) == 'number' then
    self.data.r = math.max(0, math.min(255, self.data.r - other))
    self.data.g = math.max(0, math.min(255, self.data.g - other))
    self.data.b = math.max(0, math.min(255, self.data.b - other))
  else
    self.data.r = math.max(0, math.min(255, self.data.r - other.r))
    self.data.g = math.max(0, math.min(255, self.data.g - other.g))
    self.data.b = math.max(0, math.min(255, self.data.b - other.b))
  end
  self:sync_hsl()
  return self
end

--[[
  Create an independent copy of this color.

  Usage:
    copy = red:clone()
    copy.r = 100  -- original red is unchanged

  Returns: new color instance with same RGBA values
]]
function color:clone()
  return color(self.data.r, self.data.g, self.data.b, self.data.a)
end

--[[
  Invert RGB values (255 - value).

  Usage:
    red:invert()  -- red becomes cyan

  Behavior:
    - Each RGB component becomes (255 - value)
    - Alpha is unchanged
    - HSL is recomputed after modification
    - Mutates in place

  Returns: self (for chaining)
]]
function color:invert()
  self.data.r = 255 - self.data.r
  self.data.g = 255 - self.data.g
  self.data.b = 255 - self.data.b
  self:sync_hsl()
  return self
end

--[[
  Linear interpolation toward another color.

  Usage:
    red:mix(blue, 0.5)   -- halfway between red and blue
    red:mix(blue, 0)      -- stays red
    red:mix(blue, 1)      -- becomes blue

  Parameters:
    other - target color to interpolate toward
    t     - interpolation factor 0-1 (default 0.5)

  Behavior:
    - Interpolates all four components (RGBA)
    - t=0 means no change, t=1 means fully target color
    - HSL is recomputed after modification
    - Mutates in place

  Returns: self (for chaining)
]]
function color:mix(other, t)
  t = t or 0.5
  self.data.r = self.data.r + (other.r - self.data.r)*t
  self.data.g = self.data.g + (other.g - self.data.g)*t
  self.data.b = self.data.b + (other.b - self.data.b)*t
  self.data.a = self.data.a + (other.a - self.data.a)*t
  self:sync_hsl()
  return self
end
