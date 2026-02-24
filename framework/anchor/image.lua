require('anchor.class')

--[[
  Image class wraps a C texture handle.

  Images are GPU textures loaded from files. They're drawn via layer:image().
  The class caches width/height so you don't need C calls to query them.

  Usage:
    an:image('player', 'assets/player.png')   -- register image
    img = an.images.player                    -- access image
    layer:image(img, 100, 100)                -- draw centered at (100, 100)

  Properties:
    self.handle - C texture pointer
    self.width  - texture width in pixels
    self.height - texture height in pixels
]]
image = class:extend()

--[[
  Creates an image wrapper from a C texture handle.

  Usage:
    img = image(handle)   -- typically called by an:image, not directly

  Behavior:
    - Stores the C handle
    - Queries and caches width/height from C
]]
function image:new(handle)
  self.handle = handle
  self.width = texture_get_width(self.handle)
  self.height = texture_get_height(self.handle)
end
