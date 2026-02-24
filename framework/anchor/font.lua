require('anchor.class')

--[[
  Font class wraps a named C font for text rendering.

  Fonts in the C engine are identified by name string, not handle.
  This class registers the font and provides query methods for metrics.

  Usage:
    an:font('main', 'assets/LanaPixel.ttf', 11)   -- register font
    f = an.fonts.main                              -- access font
    layer:text("Hello", f, 100, 50, color)         -- draw text

  Properties:
    self.name   - font identifier string
    self.size   - font size in pixels
    self.height - line height in pixels
]]
font = class:extend()

--[[
  Creates and registers a font.

  Usage:
    font('main', 'assets/font.ttf', 16)

  Parameters:
    name - identifier for this font (used in C calls)
    path - path to TTF file
    size - font size in pixels

  Behavior:
    - Calls font_load() to load and register the font in C
    - Caches the line height
]]
function font:new(name, path, size)
  self.name = name
  self.size = size
  font_load(self.name, path, self.size)
  self.height = font_get_height(self.name)
end

--[[
  Returns the width of a text string in pixels.

  Usage:
    local width = f:text_width("Hello, world!")

  Returns: number (pixel width)
]]
function font:text_width(text)
  return font_get_text_width(self.name, text)
end

--[[
  Returns the advance width of a single character.

  Usage:
    local width = f:char_width(65)   -- 'A'

  Parameters:
    codepoint - Unicode codepoint (number)

  Returns: number (pixel width)
]]
function font:char_width(codepoint)
  return font_get_char_width(self.name, codepoint)
end

--[[
  Returns detailed metrics for a glyph.

  Usage:
    local metrics = f:glyph_metrics(65)   -- 'A'
    print(metrics.width, metrics.advance)

  Parameters:
    codepoint - Unicode codepoint (number)

  Returns: table with fields:
    width    - glyph bitmap width
    height   - glyph bitmap height
    advance  - horizontal advance (spacing to next glyph)
    bearingX - horizontal offset from cursor to glyph left edge
    bearingY - vertical offset from baseline to glyph top edge
]]
function font:glyph_metrics(codepoint)
  return font_get_glyph_metrics(self.name, codepoint)
end
