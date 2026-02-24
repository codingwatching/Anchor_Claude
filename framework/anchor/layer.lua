require('anchor.class')

--[[
  Layer class wraps the C layer handle for drawing.

  Layers are FBOs (framebuffer objects) that accumulate draw commands during the frame.
  Commands are deferred and processed at frame end via layer_render() with GL batching.
  Draw order is FIFO — no z-ordering, call order determines render order.

  Usage:
    an:layer('game')                           -- register layer
    game = an.layers.game                      -- access layer
    game:rectangle(100, 100, 50, 30, color)    -- queue rectangle
    game:draw()                                -- composite to screen

  Properties:
    self.name   - string, layer identifier
    self.handle - C layer pointer
]]
layer = class:extend()

--[[
  Creates a new layer with the given name.

  Usage:
    layer('game')
    layer('ui')

  Behavior:
    - Calls layer_create() which gets or creates a named layer in C
    - Stores the C handle for subsequent draw calls
]]
function layer:new(name)
  self.name = name
  self.handle = layer_create(self.name)
  self.parallax_x = 1
  self.parallax_y = 1
  self.camera = an.camera
end

--[[
  Queues a filled rectangle at (x, y).

  Usage:
    layer:rectangle(100, 100, 50, 30, rgba(255, 0, 0, 255))

  Parameters:
    x, y  - top-left position
    w, h  - width and height
    color - packed RGBA (use rgba() helper)
]]
function layer:rectangle(x, y, w, h, color)
  layer_rectangle(self.handle, x, y, w, h, color)
end

--[[
  Queues a filled circle centered at (x, y).

  Usage:
    layer:circle(200, 150, 25, rgba(0, 255, 0, 255))

  Parameters:
    x, y   - center position
    radius - circle radius
    color  - packed RGBA (use rgba() helper)
]]
function layer:circle(x, y, radius, color)
  layer_circle(self.handle, x, y, radius, color)
end

--[[
  Queues a rectangle outline at (x, y).

  Usage:
    layer:rectangle_line(100, 100, 50, 30, rgba(255, 0, 0, 255))
    layer:rectangle_line(100, 100, 50, 30, rgba(255, 0, 0, 255), 2)  -- 2px line

  Parameters:
    x, y       - top-left position
    w, h       - width and height
    color      - packed RGBA (use rgba() helper)
    line_width - outline thickness (default: 1)
]]
function layer:rectangle_line(x, y, w, h, color, line_width)
  layer_rectangle_line(self.handle, x, y, w, h, color, line_width or 1)
end

--[[
  Queues a circle outline centered at (x, y).

  Usage:
    layer:circle_line(200, 150, 25, rgba(0, 255, 0, 255))
    layer:circle_line(200, 150, 25, rgba(0, 255, 0, 255), 2)  -- 2px line

  Parameters:
    x, y       - center position
    radius     - circle radius
    color      - packed RGBA (use rgba() helper)
    line_width - outline thickness (default: 1)
]]
function layer:circle_line(x, y, radius, color, line_width)
  layer_circle_line(self.handle, x, y, radius, color, line_width or 1)
end

--[[
  Queues a line from (x1, y1) to (x2, y2).

  Usage:
    layer:line(100, 100, 200, 150, 2, rgba(255, 255, 255, 255))

  Parameters:
    x1, y1 - start position
    x2, y2 - end position
    width  - line thickness
    color  - packed RGBA (use rgba() helper)
]]
function layer:line(x1, y1, x2, y2, width, color)
  layer_line(self.handle, x1, y1, x2, y2, width, color)
end

--[[
  Queues a filled capsule (stadium shape) from (x1, y1) to (x2, y2).

  Usage:
    layer:capsule(100, 100, 200, 100, 10, rgba(0, 128, 255, 255))

  Parameters:
    x1, y1 - start center position
    x2, y2 - end center position
    radius - capsule radius (half-width)
    color  - packed RGBA (use rgba() helper)
]]
function layer:capsule(x1, y1, x2, y2, radius, color)
  layer_capsule(self.handle, x1, y1, x2, y2, radius, color)
end

--[[
  Queues a capsule outline from (x1, y1) to (x2, y2).

  Usage:
    layer:capsule_line(100, 100, 200, 100, 10, rgba(0, 128, 255, 255))
    layer:capsule_line(100, 100, 200, 100, 10, rgba(0, 128, 255, 255), 2)

  Parameters:
    x1, y1     - start center position
    x2, y2     - end center position
    radius     - capsule radius (half-width)
    color      - packed RGBA (use rgba() helper)
    line_width - outline thickness (default: 1)
]]
function layer:capsule_line(x1, y1, x2, y2, radius, color, line_width)
  layer_capsule_line(self.handle, x1, y1, x2, y2, radius, color, line_width or 1)
end

--[[
  Queues a filled triangle with vertices at (x1,y1), (x2,y2), (x3,y3).

  Usage:
    layer:triangle(100, 100, 150, 50, 200, 100, rgba(255, 128, 0, 255))

  Parameters:
    x1, y1 - first vertex
    x2, y2 - second vertex
    x3, y3 - third vertex
    color  - packed RGBA (use rgba() helper)
]]
function layer:triangle(x1, y1, x2, y2, x3, y3, color)
  layer_triangle(self.handle, x1, y1, x2, y2, x3, y3, color)
end

--[[
  Queues a triangle outline with vertices at (x1,y1), (x2,y2), (x3,y3).

  Usage:
    layer:triangle_line(100, 100, 150, 50, 200, 100, rgba(255, 128, 0, 255))
    layer:triangle_line(100, 100, 150, 50, 200, 100, rgba(255, 128, 0, 255), 2)

  Parameters:
    x1, y1     - first vertex
    x2, y2     - second vertex
    x3, y3     - third vertex
    color      - packed RGBA (use rgba() helper)
    line_width - outline thickness (default: 1)
]]
function layer:triangle_line(x1, y1, x2, y2, x3, y3, color, line_width)
  layer_triangle_line(self.handle, x1, y1, x2, y2, x3, y3, color, line_width or 1)
end

--[[
  Queues a filled polygon (up to 8 vertices).

  Usage:
    -- Hexagon centered at (200, 135)
    layer:polygon({
      200, 100,   -- top
      240, 117,   -- top-right
      240, 153,   -- bottom-right
      200, 170,   -- bottom
      160, 153,   -- bottom-left
      160, 117    -- top-left
    }, rgba(128, 0, 255, 255))

  Parameters:
    vertices - table of {x1, y1, x2, y2, ...} (3-8 vertices)
    color    - packed RGBA (use rgba() helper)
]]
function layer:polygon(vertices, color)
  layer_polygon(self.handle, vertices, color)
end

--[[
  Queues a polygon outline (up to 8 vertices).

  Usage:
    layer:polygon_line({200, 100, 240, 117, 240, 153, 200, 170, 160, 153, 160, 117}, rgba(128, 0, 255, 255))
    layer:polygon_line(vertices, rgba(128, 0, 255, 255), 2)  -- 2px line

  Parameters:
    vertices   - table of {x1, y1, x2, y2, ...} (3-8 vertices)
    color      - packed RGBA (use rgba() helper)
    line_width - outline thickness (default: 1)
]]
function layer:polygon_line(vertices, color, line_width)
  layer_polygon_line(self.handle, vertices, color, line_width or 1)
end

--[[
  Queues a filled rounded rectangle at (x, y).

  Usage:
    layer:rounded_rectangle(100, 100, 50, 30, 8, rgba(255, 0, 0, 255))

  Parameters:
    x, y   - top-left position
    w, h   - width and height
    radius - corner radius
    color  - packed RGBA (use rgba() helper)
]]
function layer:rounded_rectangle(x, y, w, h, radius, color)
  layer_rounded_rectangle(self.handle, x, y, w, h, radius, color)
end

--[[
  Queues a rounded rectangle outline at (x, y).

  Usage:
    layer:rounded_rectangle_line(100, 100, 50, 30, 8, rgba(255, 0, 0, 255))
    layer:rounded_rectangle_line(100, 100, 50, 30, 8, rgba(255, 0, 0, 255), 2)  -- 2px line

  Parameters:
    x, y       - top-left position
    w, h       - width and height
    radius     - corner radius
    color      - packed RGBA (use rgba() helper)
    line_width - outline thickness (default: 1)
]]
function layer:rounded_rectangle_line(x, y, w, h, radius, color, line_width)
  layer_rounded_rectangle_line(self.handle, x, y, w, h, radius, color, line_width or 1)
end

--[[
  Queues a horizontal gradient filled rectangle at (x, y).

  Usage:
    layer:rectangle_gradient_h(100, 100, 200, 50, red(), blue())

  Parameters:
    x, y    - top-left position
    w, h    - width and height
    color1  - left color (packed RGBA)
    color2  - right color (packed RGBA)
]]
function layer:rectangle_gradient_h(x, y, w, h, color1, color2)
  layer_rectangle_gradient_h(self.handle, x, y, w, h, color1, color2)
end

--[[
  Queues a vertical gradient filled rectangle at (x, y).

  Usage:
    layer:rectangle_gradient_v(100, 100, 200, 50, red(), blue())

  Parameters:
    x, y    - top-left position
    w, h    - width and height
    color1  - top color (packed RGBA)
    color2  - bottom color (packed RGBA)
]]
function layer:rectangle_gradient_v(x, y, w, h, color1, color2)
  layer_rectangle_gradient_v(self.handle, x, y, w, h, color1, color2)
end

--[[
  Queues an image (texture) centered at (x, y).

  Usage:
    layer:image(an.images.player, 100, 100)
    layer:image(an.images.player, 100, 100, rgba(255, 255, 255, 128))  -- semi-transparent
    layer:image(an.images.player, 100, 100, 0xFFFFFFFF, rgba(255, 0, 0, 255))  -- red flash

  Parameters:
    img   - image object (from an:image)
    x, y  - center position
    color - tint/multiply color (default: white/opaque)
    flash - additive flash color (default: none)

  Note: Use push/pop for rotation and scaling.
]]
function layer:image(img, x, y, color, flash)
  layer_draw_texture(self.handle, img.handle, x, y, color or 0xFFFFFFFF, flash or 0)
end

--[[
  Queues a spritesheet frame centered at (x, y).

  Usage:
    layer:spritesheet(an.spritesheets.hit, 1, 100, 100)
    layer:spritesheet(an.spritesheets.hit, 3, 100, 100, rgba(255, 255, 255, 128))  -- tinted
    layer:spritesheet(an.spritesheets.hit, 5, 100, 100, 0xFFFFFFFF, rgba(255, 0, 0, 255))  -- flash

  Parameters:
    sheet - spritesheet object (from an:spritesheet)
    frame - frame index (1-based, left-to-right, top-to-bottom)
    x, y  - center position
    color - tint/multiply color (default: white/opaque)
    flash - additive flash color (default: none)

  Note: Use push/pop for rotation and scaling.
]]
function layer:spritesheet(sheet, frame, x, y, color, flash)
  layer_draw_spritesheet_frame(self.handle, sheet.handle, frame, x, y, color or 0xFFFFFFFF, flash or 0)
end

--[[
  Queues an animation's current frame centered at (x, y).

  Usage:
    layer:animation(animation_object, 100, 100)
    layer:animation(animation_object, 100, 100, rgba(255, 255, 255, 128))  -- tinted
    layer:animation(animation_object, 100, 100, 0xFFFFFFFF, rgba(255, 0, 0, 255))  -- flash

  Parameters:
    animation_object - animation object
    x, y             - center position
    color            - tint/multiply color (default: white/opaque)
    flash            - additive flash color (default: none)

  Note: Use push/pop for rotation and scaling.
]]
function layer:animation(animation_object, x, y, color, flash)
  layer_draw_spritesheet_frame(self.handle, animation_object.spritesheet.handle, animation_object.frame, x, y, color or 0xFFFFFFFF, flash or 0)
end

--[[
  Queues text at position (x, y).

  Usage:
    layer:text("Hello!", an.fonts.main, 100, 50, rgba(255, 255, 255, 255))
    layer:text("Score: 100", "main", 100, 50, rgba(255, 255, 255, 255))  -- font name string

  Parameters:
    text  - string to render
    f     - font object or font name string
    x, y  - position (top-left of text)
    color - packed RGBA
]]
function layer:text(text, f, x, y, color)
  local font_name
  if type(f) == 'string' then
    font_name = f
  else
    font_name = f.name
  end
  layer_draw_text(self.handle, text, font_name, x, y, color)
end

--[[
  Pushes a transform onto the layer's transform stack.

  Usage:
    layer:push(240, 135, math.pi/4, 2, 2)   -- translate, rotate 45°, scale 2x
    layer:image(img, 0, 0)                   -- draws at (240,135), rotated, scaled
    layer:pop()

  Parameters:
    x, y - translation
    r    - rotation in radians
    sx   - scale X
    sy   - scale Y

  Behavior:
    - Builds TRS matrix: Translate(x,y) * Rotate(r) * Scale(sx,sy)
    - Multiplies with current transform (transforms compose)
    - All subsequent draws use this transform until pop
    - Max stack depth: 32
]]
function layer:push(x, y, r, sx, sy)
  layer_push(self.handle, x, y, r, sx, sy)
end

--[[
  Pops the top transform from the stack.

  Usage:
    layer:push(100, 100, 0, 1, 1)
    layer:rectangle(0, 0, 50, 50, color)
    layer:pop()

  Behavior:
    - Restores previous transform
    - If stack is empty (depth 0), does nothing
]]
function layer:pop()
  layer_pop(self.handle)
end

--[[
  Sets the blend mode for subsequent draw commands.

  Usage:
    layer:set_blend_mode('additive')   -- for glows, particles
    layer:set_blend_mode('alpha')      -- default blending

  Parameters:
    mode - 'alpha' (default) or 'additive'

  Behavior:
    - 'alpha': result = src * src.a + dst * (1 - src.a)
    - 'additive': result = src * src.a + dst (good for glows)
]]
function layer:set_blend_mode(mode)
  layer_set_blend_mode(self.handle, mode)
end

--[[
  Queues this layer to be composited to the screen.

  Usage:
    layer:draw()           -- draw at (0, 0)
    layer:draw(10, 20)     -- draw with offset

  Parameters:
    x, y - screen offset (default: 0, 0)

  Behavior:
    - Adds layer to the layer_draw_queue
    - At frame end, all queued layers are composited to screen in order
    - Layer is cleared after compositing (ready for next frame)
]]
function layer:draw(x, y)
  layer_draw(self.handle, x or 0, y or 0)
end

--[[
  Applies a shader to the layer's current contents.

  Usage:
    layer:apply_shader(an.shaders.blur)
    layer:apply_shader(an.shaders.outline)

  Parameters:
    shader - shader handle (from an:shader or an:shader_string)

  Behavior:
    - Applies shader via ping-pong rendering (reads from layer, writes result back)
    - Multiple shaders can be chained (call apply_shader multiple times)
    - Set uniforms before calling apply_shader
]]
function layer:apply_shader(shader)
  layer_apply_shader(self.handle, shader)
end

--[[
  Sets a float uniform on a shader for this layer.

  Usage:
    layer:shader_set_float(an.shaders.blur, 'u_radius', 5.0)

  Parameters:
    shader - shader handle
    name   - uniform name in shader
    value  - float value
]]
function layer:shader_set_float(shader, name, value)
  layer_shader_set_float(self.handle, shader, name, value)
end

--[[
  Sets a vec2 uniform on a shader for this layer.

  Usage:
    layer:shader_set_vec2(an.shaders.outline, 'u_pixel_size', 1/480, 1/270)

  Parameters:
    shader - shader handle
    name   - uniform name in shader
    x, y   - vec2 components
]]
function layer:shader_set_vec2(shader, name, x, y)
  layer_shader_set_vec2(self.handle, shader, name, x, y)
end

--[[
  Sets a vec4 uniform on a shader for this layer.

  Usage:
    layer:shader_set_vec4(an.shaders.tint, 'u_color', 1.0, 0.5, 0.0, 1.0)

  Parameters:
    shader       - shader handle
    name         - uniform name in shader
    x, y, z, w   - vec4 components
]]
function layer:shader_set_vec4(shader, name, x, y, z, w)
  layer_shader_set_vec4(self.handle, shader, name, x, y, z, w)
end

--[[
  Sets an int uniform on a shader for this layer.

  Usage:
    layer:shader_set_int(an.shaders.effect, 'u_mode', 2)

  Parameters:
    shader - shader handle
    name   - uniform name in shader
    value  - integer value
]]
function layer:shader_set_int(shader, name, value)
  layer_shader_set_int(self.handle, shader, name, value)
end

--[[
  Gets the layer's current texture handle.

  Usage:
    tex = layer:get_texture()
    layer:shader_set_int(shader, "u_texture", tex)

  Returns: texture ID for use in shader uniforms

  Behavior:
    - Returns the layer's texture handle
    - Typically used to pass a layer's contents as a shader uniform
]]
function layer:get_texture()
  return layer_get_texture(self.handle)
end

--[[
  Clears the layer's contents and resets effect state.

  Usage:
    layer:reset_effects()

  Behavior:
    - Clears all drawn contents from the layer
    - Resets any effect processing state
]]
function layer:reset_effects()
  layer_reset_effects(self.handle)
end

--[[
  Clears the layer's FBO contents to transparent black.

  Usage:
    layer:clear()

  Behavior:
    - Immediately clears the layer's framebuffer
    - Use before draw_from if you want to replace contents (not accumulate)
]]
function layer:clear()
  layer_clear(self.handle)
end

--[[
  Renders queued draw commands to this layer's FBO.

  Usage:
    game:render()

  Behavior:
    - Binds this layer's FBO
    - Clears to transparent black
    - Processes all queued draw commands (rectangles, circles, images, text)
    - Clears the command queue

  Note: Call this in draw() for each layer that has draw commands.
  Layers used only as effect targets (shadow, outline) don't need render.
]]
function layer:render()
  layer_render(self.handle)
end

--[[
  Draws another layer's texture to this layer's FBO.

  Usage:
    shadow:draw_from(game)                          -- copy game to shadow
    shadow:draw_from(game, an.shaders.shadow)       -- copy through shader
    outline:draw_from(game, an.shaders.outline)     -- copy through shader

  Parameters:
    source - source layer to copy from
    shader - optional shader to apply during copy

  Behavior:
    - Binds this layer's FBO as render target
    - Draws source layer's texture as a fullscreen quad
    - If shader provided, applies it during the draw
    - Uses alpha blending, so multiple sources accumulate
    - Call clear() first if you want to replace instead of accumulate
]]
function layer:draw_from(source, shader)
  layer_draw_from(self.handle, source.handle, shader)
end

--[[
  Start writing to stencil buffer (mask mode).

  Usage:
    layer:stencil_mask()
    layer:rectangle(x, y, w, h, white())   -- draws to stencil only, not visible
    layer:stencil_test()
    layer:image(heart, 0, 0)               -- only draws where stencil was set
    layer:stencil_off()

  Behavior:
    - Subsequent draws write to stencil buffer only (not visible on screen)
    - Use to define a mask shape, then call stencil_test() to use the mask
]]
function layer:stencil_mask()
  layer_stencil_mask(self.handle)
end

--[[
  Start testing against stencil buffer.

  Usage:
    layer:stencil_test()
    layer:image(heart, 0, 0)   -- only draws where stencil is set

  Behavior:
    - Subsequent draws only appear where stencil buffer has been written
    - Call after stencil_mask() and drawing your mask shape
]]
function layer:stencil_test()
  layer_stencil_test(self.handle)
end

--[[
  Disable stencil, return to normal drawing.

  Usage:
    layer:stencil_off()

  Behavior:
    - Disables stencil test, subsequent draws render normally
    - Call when done with masked drawing
]]
function layer:stencil_off()
  layer_stencil_off(self.handle)
end
