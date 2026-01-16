

require('anchor')


W, H = 480, 270


an:add(camera())
an.camera:add(shake())
an:add(spring())
an.spring:add('camera_rotation', 0, 2, 0.5)


game = an:layer('game')
game_2 = an:layer('game_2')
bg = an:layer('bg')
shadow = an:layer('shadow')
game_outline = an:layer('game_outline')
game_2_outline = an:layer('game_2_outline')
ui = an:layer('ui')
ui.camera = nil


an:font('main', 'assets/LanaPixel.ttf', 11)
an:image('ball', 'assets/slight_smile.png')
an:shader('shadow', 'shaders/shadow.frag')
an:shader('outline', 'shaders/outline.frag')


an:sound('death', 'assets/player_death.ogg')
an:music('track1', 'assets/speder2_01.ogg')
an:music('track2', 'assets/speder2_02.ogg')
an:music('track3', 'assets/speder2_03.ogg')


an:playlist_set({ 'track1', 'track2', 'track3' })


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


an:physics_init()
an:physics_set_gravity(0, 500)
an:physics_set_meter_scale(64)


an:physics_tag('ball')
an:physics_tag('wall')
an:physics_tag('impulse_block')
an:physics_tag('slowing_zone')
an:physics_collision('ball', 'wall')
an:physics_collision('ball', 'ball')
an:physics_collision('ball', 'impulse_block')
an:physics_sensor('ball', 'slowing_zone')
an:physics_hit('ball', 'wall')


bg_color = rgba(231, 232, 233, 255)
green = rgba(122, 179, 87, 255)
blue = rgba(85, 172, 238, 255)
blue_transparent = rgba(85, 172, 238, 128)
yellow = rgba(255, 204, 77, 255)
red = rgba(221, 46, 68, 255)
orange = rgba(244, 144, 12, 255)
purple = rgba(170, 142, 214, 255)
black = rgba(0, 0, 0, 255)
white = rgba(255, 255, 255, 255)


ground_width = W * 0.9
ground_height = 12
ground_x = (W - ground_width) / 2
ground_y = H - 20 - ground_height / 2

wall_width = 12
wall_top = H * 0.1
wall_height = ground_y - wall_top
left_wall_x = ground_x
right_wall_x = ground_x + ground_width - wall_width;do


local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self, layer)if 








self.rounded_top then local radius = 
self.w / 2
layer:circle(self.x, self.y - self.h / 2 + radius, radius, self.color)return 
layer:rectangle(self.x - self.w / 2, self.y - self.h / 2 + radius, self.w, self.h - radius, self.color)elseif 
self.rounded_left then local radius = 
self.h / 2
layer:circle(self.x - self.w / 2 + radius, self.y, radius, self.color)return 
layer:rectangle(self.x - self.w / 2 + radius, self.y - self.h / 2, self.w - radius, self.h, self.color)else return 

layer:rectangle(self.x - self.w / 2, self.y - self.h / 2, self.w, self.h, self.color)end end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y, w, h, color, rounded_top, rounded_left)if color == nil then color = green end;if rounded_top == nil then rounded_top = false end;if rounded_left == nil then rounded_left = false end;self.w = w;self.h = h;self.color = color;self.rounded_top = rounded_top;self.rounded_left = rounded_left;_class_0.__parent.__init(self)self:tag('drawable')self:add(collider('wall', 'static', 'box', self.w, self.h))self.collider:set_position(x, y)return self.collider:set_friction(1)end, __base = _base_0, __name = "wall", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;wall = _class_0 end


an:add(wall(ground_x + ground_width / 2, ground_y + ground_height / 2, ground_width, ground_height))
an:add(wall(left_wall_x + wall_width / 2, wall_top + wall_height / 2, wall_width, wall_height, green, true))
an:add(wall(right_wall_x + wall_width / 2, wall_top + wall_height / 2, wall_width, wall_height))


ceiling_width = ground_width / 2
ceiling_height = ground_height
ceiling_x = right_wall_x + wall_width - ceiling_width / 2
ceiling_y = wall_top + ceiling_height / 2
an:add(wall(ceiling_x, ceiling_y, ceiling_width, ceiling_height, green, false, true))


impulse_width = ground_width * 0.1
impulse_height = ground_height
impulse_x = left_wall_x + wall_width
impulse_y = ground_y - impulse_height;do

local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self, layer)












layer:push(self.x, self.y, 0, self.spring.main.x, self.spring.main.x)
layer:rectangle(-self.w / 2, -self.h / 2, self.w, self.h, self.flash and white or blue)return 
layer:pop()end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y, w, h)self.w = w;self.h = h;_class_0.__parent.__init(self)self:tag('impulse_block')self.flash = false;self:add(timer())self:add(spring())self:add(collider('impulse_block', 'static', 'box', self.w, self.h))self.collider:set_position(x + self.w / 2, y + self.h / 2)self.collider:set_friction(1)return self.collider:set_restitution(1)end, __base = _base_0, __name = "impulse_block", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;impulse_block = _class_0 end

an:add(impulse_block(impulse_x, impulse_y, impulse_width, impulse_height))


box_interior_width = ground_width - 2 * wall_width
box_interior_height = wall_height
slowing_zone_width = box_interior_width / 6
slowing_zone_height = box_interior_height / 3
ceiling_left_edge = ceiling_x - ceiling_width / 2
slowing_zone_x = ceiling_left_edge
slowing_zone_y = wall_top + ceiling_height + slowing_zone_height / 2;do

local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self, layer)return 







layer:rectangle(self.x - self.w / 2, self.y - self.h / 2, self.w, self.h, blue_transparent)end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y, w, h)self.w = w;self.h = h;_class_0.__parent.__init(self)self:tag('slowing_zone')self:add(collider('slowing_zone', 'static', 'box', self.w, self.h, { sensor = true }))return self.collider:set_position(x, y)end, __base = _base_0, __name = "slowing_zone", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;slowing_zone = _class_0 end

an:add(slowing_zone(slowing_zone_x, slowing_zone_y, slowing_zone_width, slowing_zone_height))


ball_radius = 10
ball_scale = ball_radius * 2 / an.images.ball.width;do

local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self, layer)local angle = 















self.collider:get_angle()local scale = 
ball_scale * self.spring.main.x
layer:push(self.x, self.y, angle, scale, scale)
layer:image(an.images.ball, 0, 0, nil, self.flash and white or nil)return 
layer:pop()end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y)self.x = x;self.y = y;_class_0.__parent.__init(self)self:tag('ball')self:tag('drawable')self.impulsed = false;self.original_speed = 0;self.flash = false;self:add(timer())self:add(spring())self:add(collider('ball', 'dynamic', 'circle', ball_radius))self.collider:set_position(self.x, self.y)self.collider:set_restitution(1)return self.collider:set_friction(1)end, __base = _base_0, __name = "ball", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;ball = _class_0 end


audio_crossfade_enabled = false



an:action(function(self, dt)if 

key_is_pressed('1') then
an:sound_play('death')
print("Sound: death")end;if 

key_is_pressed('2') then
print("Before play: ch0 vol=" .. tostring(music_get_volume(0)) .. ", ch1 vol=" .. tostring(music_get_volume(1)))
an:music_play('track1')
print("After play: ch0 vol=" .. tostring(music_get_volume(0)) .. ", playing=" .. tostring(music_is_playing(0)))end;if 

key_is_pressed('3') then
print("Before stop: ch0 vol=" .. tostring(music_get_volume(0)) .. ", ch1 vol=" .. tostring(music_get_volume(1)))
an:music_stop()
print("After stop: ch0 vol=" .. tostring(music_get_volume(0)) .. ", ch1 vol=" .. tostring(music_get_volume(1)))end;if 

key_is_pressed('4') then
an:playlist_play()
print("Playlist: started")end;if 

key_is_pressed('5') then
an:playlist_next()
print("Playlist: next -> " .. an:playlist_current_track())end;if 

key_is_pressed('6') then
an:playlist_prev()
print("Playlist: prev -> " .. an:playlist_current_track())end;if 

key_is_pressed('7') then
an:playlist_shuffle(not an.playlist_shuffle_enabled)
print("Playlist shuffle: " .. tostring(an.playlist_shuffle_enabled))end;if 

key_is_pressed('8') then
audio_crossfade_enabled = not audio_crossfade_enabled;if 
audio_crossfade_enabled then
an:playlist_set_crossfade(2)
print("Playlist crossfade: 2 seconds")else

an:playlist_set_crossfade(0)
print("Playlist crossfade: instant")end end;if 

key_is_pressed('9') then
print("Before crossfade: ch0 vol=" .. tostring(music_get_volume(0)) .. ", ch1 vol=" .. tostring(music_get_volume(1)))
an:music_crossfade('track2', 2)
print("After crossfade: ch0 vol=" .. tostring(music_get_volume(0)) .. ", ch1 vol=" .. tostring(music_get_volume(1)))
print("Crossfade state: " .. tostring(an.crossfade_state and 'exists' or 'nil'))end;if 

key_is_pressed('0') then
an:playlist_stop()
print("Playlist: stopped")end;if 
key_is_pressed('k') then local spawn_x = 
left_wall_x + wall_width + ball_radius + 20;local spawn_y = 
wall_top - ball_radius - 5;local new_ball = 
ball(spawn_x, spawn_y)
an:add(new_ball)end;if 


key_is_pressed('p') then local _list_0 = 
an:all('ball')for _index_0 = 1, #_list_0 do local b = _list_0[_index_0]
b.collider:apply_impulse(200, 0)end end;if 

key_is_pressed('r') then
an.spring:pull('camera_rotation', math.pi / 12)end;if 

key_is_pressed('t') then
an.camera.shake:trauma(1, 1)end;if 

key_is_pressed('y') then
an.camera.shake:push(random_float(0, 2 * math.pi), 20)end;if 

key_is_pressed('u') then
an.camera.shake:shake(15, 0.5)end;if 

key_is_pressed('i') then
an.camera.shake:sine(random_float(0, 2 * math.pi), 15, 8, 0.5)end;if 

key_is_pressed('o') then
an.camera.shake:square(random_float(0, 2 * math.pi), 15, 8, 0.5)end;if 

key_is_pressed('h') then
an.camera.shake:handcam(not an.camera.shake.handcam_enabled)end

an.camera.rotation = an.spring.camera_rotation.x;if 


mouse_is_pressed(1) then local _list_0 = 
an:query_point(an.camera.mouse.x, an.camera.mouse.y, 'ball')for _index_0 = 1, #_list_0 do local b = _list_0[_index_0]
b.flash = true
b.timer:after(0.15, 'flash', function()b.flash = false end)
b.spring:pull('main', 0.2, 5, 0.8)end end;local camera_speed = 

200;if 
key_is_down('w') or key_is_down('up') then local _obj_0 = 
an.camera;_obj_0.y = _obj_0.y - (camera_speed * dt)end;if 
key_is_down('s') or key_is_down('down') then local _obj_0 = 
an.camera;_obj_0.y = _obj_0.y + (camera_speed * dt)end;if 
key_is_down('a') or key_is_down('left') then local _obj_0 = 
an.camera;_obj_0.x = _obj_0.x - (camera_speed * dt)end;if 
key_is_down('d') or key_is_down('right') then local _obj_0 = 
an.camera;_obj_0.x = _obj_0.x + (camera_speed * dt)end end)


an:early_action('handle_collisions', function(self)local _list_0 = 
an:collision_begin_events('ball', 'impulse_block')for _index_0 = 1, #_list_0 do local event = _list_0[_index_0]local ball = 
event.a;local block = 
event.b;if not 
ball.impulsed then
ball.impulsed = true
ball.collider:apply_impulse(random_float(20, 40), 0)
block.flash = true
block.timer:after(0.15, 'flash', function()block.flash = false end)
block.spring:pull('main', 0.2, 5, 0.8)end end;local _list_1 = 

an:sensor_begin_events('ball', 'slowing_zone')for _index_0 = 1, #_list_1 do local event = _list_1[_index_0]local ball = 
event.a;local vx,vy = 
ball.collider:get_velocity()
ball.original_speed = math.sqrt(vx * vx + vy * vy)
ball.collider:set_velocity(vx * 0.1, vy * 0.1)
ball.collider:set_gravity_scale(0.1)end;local _list_2 = 

an:sensor_end_events('ball', 'slowing_zone')for _index_0 = 1, #_list_2 do local event = _list_2[_index_0]local ball = 
event.a;local vx,vy = 
ball.collider:get_velocity()local current_speed = 
math.sqrt(vx * vx + vy * vy)if 
current_speed > 0 then local scale = 
ball.original_speed / current_speed
ball.collider:set_velocity(vx * scale, vy * scale)end
ball.collider:set_gravity_scale(1)end;local _list_3 = 

an:hit_events('ball', 'wall')for _index_0 = 1, #_list_3 do local event = _list_3[_index_0]local ball = 
event.a;if 
event.approach_speed > 300 then
ball.flash = true
ball.timer:after(0.15, 'flash', function()ball.flash = false end)
ball.spring:pull('main', 0.2, 5, 0.8)end end end)


an:late_action('draw', function(self)

bg:rectangle(0, 0, W, H, bg_color)local _list_0 = 


an:all('drawable')for _index_0 = 1, #_list_0 do local obj = _list_0[_index_0]
obj:draw(game)end;local _list_1 = 


an:all('impulse_block')for _index_0 = 1, #_list_1 do local obj = _list_1[_index_0]
obj:draw(game_2)end;local _list_2 = 


an:all('slowing_zone')for _index_0 = 1, #_list_2 do local zone = _list_2[_index_0]
zone:draw(ui)end;local _list_3 = 


an:all('ball')for _index_0 = 1, #_list_3 do local b = _list_3[_index_0]local screen_x,screen_y = 
an.camera:to_screen(b.x, b.y)
ui:circle(screen_x, screen_y - 20, 5, red)end;local is_playing = 


music_is_playing(an.playlist_channel) or (an.crossfade_state and music_is_playing(an.crossfade_state.to_channel))local playing_status = 
is_playing and "PLAYING" or "STOPPED"local shuffle_status = 
an.playlist_shuffle_enabled and "ON" or "OFF"local crossfade_status = 
an.playlist_crossfade_duration > 0 and tostring(an.playlist_crossfade_duration) .. "s" or "OFF"local current_track = #
an.playlist > 0 and an:playlist_current_track() or "none"local shuffle_order = 


""if 
an.playlist_shuffle_enabled and #an.playlist_shuffled > 0 then
local order_parts;do local _accum_0 = {  }local _len_0 = 1;local _list_4 = an.playlist_shuffled;for _index_0 = 1, #_list_4 do local i = _list_4[_index_0]_accum_0[_len_0] = tostring(i)_len_0 = _len_0 + 1 end;order_parts = _accum_0 end
shuffle_order = " Order: [" .. table.concat(order_parts, ",") .. "]"end

ui:text("Track: " .. tostring(current_track) .. " [" .. tostring(an.playlist_index) .. "/" .. tostring(#an.playlist) .. "]", 'main', 5, 5, white)return 
ui:text("Status: " .. tostring(playing_status) .. " | Shuffle: " .. tostring(shuffle_status) .. tostring(shuffle_order) .. " | Crossfade: " .. tostring(crossfade_status), 'main', 5, 18, white)end)



draw = function()

bg:render()
game:render()
game_2:render()
ui:render()


shadow:clear()
shadow:draw_from(game, an.shaders.shadow)
shadow:draw_from(game_2, an.shaders.shadow)

shader_set_vec2(an.shaders.outline, "u_pixel_size", 1 / W, 1 / H)
game_outline:clear()
game_outline:draw_from(game, an.shaders.outline)
game_2_outline:clear()
game_2_outline:draw_from(game_2, an.shaders.outline)


bg:draw()
shadow:draw(4, 4)
game_outline:draw()
game:draw()
game_2_outline:draw()
game_2:draw()return 
ui:draw()end