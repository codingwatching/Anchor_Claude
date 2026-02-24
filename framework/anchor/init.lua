--[[
  Anchor framework initialization.

  This file:
    1. Requires all framework classes (object, layer, image, font)
    2. Returns an init function that takes configuration
    3. The init function creates the root 'an' object and sets up the framework
    4. Defines the global update() function called by the C engine

  Usage:
    require('anchor') {
      width = 640,
      height = 360,
      title = "My Game",
      scale = 2,
      vsync = true,
      fullscreen = false,
      resizable = true,
      filter = "rough",
    }

  The 'an' object is the root of the object tree and the central access point
  for all framework resources (layers, images, fonts).
]]

-- Load framework classes (doesn't require engine to be initialized)
require('anchor.object')
require('anchor.layer')
require('anchor.image')
require('anchor.font')
require('anchor.timer')
require('anchor.math')
require('anchor.collider')
require('anchor.spring')
require('anchor.camera')
require('anchor.shake')
require('anchor.random')
require('anchor.color')
require('anchor.array')
require('anchor.spritesheet')
require('anchor.animation')

-- Return initialization function that takes config
return function(config)
  config = config or {}

  -- Apply configuration before engine_init
  if config.width and config.height then
    engine_set_game_size(config.width, config.height)
  end
  if config.title then
    engine_set_title(config.title)
  end
  if config.scale then
    engine_set_scale(config.scale)
  end
  if config.vsync ~= nil then
    engine_set_vsync(config.vsync)
  end
  if config.fullscreen ~= nil then
    engine_set_fullscreen(config.fullscreen)
  end
  if config.resizable ~= nil then
    engine_set_resizable(config.resizable)
  end
  if config.filter then
    set_filter_mode(config.filter)
  end

  -- Initialize engine (creates window, GL context, shaders)
  engine_init()

  --[[
    Root object for the framework.

    All game objects should be children of 'an' (directly or nested).
    Resource registries (layers, images, fonts) live here.

    Usage:
      an:layer('game')                  -- register layer
      an:image('player', 'path.png')    -- register image
      an:font('main', 'path.ttf', 16)  -- register font
      an:add(player)                    -- add child object
  ]]
  an = object('an')
  an.layers = {}
  an.images = {}
  an.fonts = {}
  an.shaders = {}
  an.sounds = {}
  an.tracks = {}
  an.spritesheets = {}
  an:add(random())
  an:add(timer())

  -- Engine state: static values (don't change during runtime)
  an.width = engine_get_width()
  an.height = engine_get_height()
  an.unscaled_dt = engine_get_unscaled_dt()
  an.platform = engine_get_platform()

  -- Time scale state
  an.time_scale = 1.0         -- Current time scale multiplier
  an.dt = an.unscaled_dt      -- Scaled dt (updated each frame)
  an.hit_stop_active = false   -- True during hitstop
  an.hit_stop_excluded_tags = {}  -- Tags that ignore hitstop

  --[[
    Registers a named layer.

    Usage:
      an:layer('game')
      an:layer('ui')

    Behavior:
      - Creates a layer object and stores it in an.layers[name]
      - Subsequent access via an.layers.game, an.layers.ui, etc.

    Returns: the created layer object
  ]]
  function an:layer(name)
    self.layers[name] = layer(name)
    return self.layers[name]
  end

  --[[
    Registers a named image (texture).

    Usage:
      an:image('player', 'assets/player.png')
      an:image('enemy', 'assets/enemy.png')

    Behavior:
      - Loads texture from path via texture_load()
      - Creates an image wrapper and stores in an.images[name]
      - Subsequent access via an.images.player, an.images.enemy, etc.

    Returns: the created image object
  ]]
  function an:image(name, path)
    local handle = texture_load(path)
    self.images[name] = image(handle)
    return self.images[name]
  end

  --[[
    Registers a named spritesheet.

    Usage:
      an:spritesheet('hit', 'assets/hit1.png', 96, 48)
      an:spritesheet('explosion', 'assets/explosion.png', 32, 32, 1)  -- with 1px padding

    Parameters:
      name         - string name for later access
      path         - file path to spritesheet image
      frame_width  - width of each frame in pixels
      frame_height - height of each frame in pixels
      padding      - (optional) pixels between frames, default 0

    Behavior:
      - Loads spritesheet from path with frame dimensions
      - Creates a spritesheet wrapper and stores in an.spritesheets[name]
      - Subsequent access via an.spritesheets.hit, etc.

    Returns: the created spritesheet object
  ]]
  function an:spritesheet(name, path, frame_width, frame_height, padding)
    padding = padding or 0
    local handle = spritesheet_load(path, frame_width, frame_height, padding)
    self.spritesheets[name] = spritesheet(handle)
    return self.spritesheets[name]
  end

  --[[
    Registers a named font.

    Usage:
      an:font('main', 'assets/LanaPixel.ttf', 11)
      an:font('title', 'assets/BigFont.ttf', 32)

    Behavior:
      - Loads font from path at given size
      - Creates a font wrapper and stores in an.fonts[name]
      - Subsequent access via an.fonts.main, an.fonts.title, etc.

    Returns: the created font object
  ]]
  function an:font(name, path, size)
    self.fonts[name] = font(name, path, size)
    return self.fonts[name]
  end

  --[[
    Registers a named shader from a file.

    Usage:
      an:shader('shadow', 'shaders/shadow.frag')
      an:shader('outline', 'shaders/outline.frag')

    Behavior:
      - Loads shader from file path via shader_load_file()
      - Stores shader handle in an.shaders[name]
      - Subsequent access via an.shaders.shadow, an.shaders.outline, etc.

    Returns: the shader handle
  ]]
  function an:shader(name, path)
    self.shaders[name] = shader_load_file(path)
    return self.shaders[name]
  end

  --[[
    Registers a named shader from source string.

    Usage:
      an:shader_string('invert', '...shader source...')

    Behavior:
      - Compiles shader from source string via shader_load_string()
      - Stores shader handle in an.shaders[name]
      - Subsequent access via an.shaders[name]

    Returns: the shader handle
  ]]
  function an:shader_string(name, source)
    self.shaders[name] = shader_load_string(source)
    return self.shaders[name]
  end

  --[[
    Registers a named sound effect.

    Usage:
      an:sound('hit', 'assets/hit.wav')
      an:sound('jump', 'assets/jump.ogg')

    Behavior:
      - Loads sound from path via sound_load()
      - Stores sound handle in an.sounds[name]
      - Subsequent access via an.sounds.hit, an.sounds.jump, etc.

    Returns: the sound handle
  ]]
  function an:sound(name, path)
    self.sounds[name] = sound_load(path)
    return self.sounds[name]
  end

  --[[
    Plays a sound effect by name.

    Usage:
      an:sound_play('hit')
      an:sound_play('hit', 0.5)        -- half volume
      an:sound_play('hit', 1, 1.2)     -- normal volume, higher pitch

    Parameters:
      name   - sound name (registered via an:sound)
      volume - (optional) volume 0-1, default 1
      pitch  - (optional) pitch multiplier, default 1
  ]]
  function an:sound_play(name, volume, pitch)
    volume = volume or 1
    pitch = pitch or 1
    sound_play(self.sounds[name], volume, pitch)
  end

  --[[
    Plays a sound effect by name and returns a handle for controlling it.

    Usage:
      local handle = an:sound_play_handle('wind_small_impact_1', 0.65, 0.8)

    Parameters:
      name   - sound name (registered via an:sound)
      volume - (optional) volume 0-1, default 1
      pitch  - (optional) pitch multiplier, default 1

    Returns: a sound handle that can be passed to sound_handle_set_pitch/sound_handle_set_volume
  ]]
  function an:sound_play_handle(name, volume, pitch)
    volume = volume or 1
    pitch = pitch or 1
    return sound_play_handle(self.sounds[name], volume, pitch)
  end

  --[[
    Sets the pitch of a playing sound by handle.

    Usage:
      an:sound_handle_set_pitch(handle, 1.2)
  ]]
  function an:sound_handle_set_pitch(handle, pitch)
    sound_handle_set_pitch(handle, pitch)
  end

  --[[
    Sets the volume of a playing sound by handle.

    Usage:
      an:sound_handle_set_volume(handle, 0.5)
  ]]
  function an:sound_handle_set_volume(handle, volume)
    sound_handle_set_volume(handle, volume)
  end

  --[[
    Sets the master volume for all sound effects.

    Usage:
      an:sound_set_volume(0.5)  -- half volume
  ]]
  function an:sound_set_volume(volume)
    sound_set_volume(volume)
  end

  --[[
    Registers a named music track.

    Usage:
      an:music('battle', 'assets/battle.ogg')
      an:music('menu', 'assets/menu.ogg')

    Behavior:
      - Loads music from path via music_load()
      - Stores music handle in an.tracks[name]
      - Subsequent access via an.tracks.battle, an.tracks.menu, etc.

    Returns: the music handle
  ]]
  function an:music(name, path)
    self.tracks[name] = music_load(path)
    return self.tracks[name]
  end

  --[[
    Plays a music track by name.

    Usage:
      an:music_play('battle')           -- play once
      an:music_play('battle', true)     -- loop
      an:music_play('battle', true, 1)  -- loop on channel 1

    Parameters:
      name    - music name (registered via an:music)
      loop    - (optional) whether to loop, default false
      channel - (optional) channel 0 or 1, default 0
  ]]
  function an:music_play(name, loop, channel)
    if loop == nil then loop = false end
    if channel == nil then channel = 0 end
    -- Cancel crossfade if we're playing directly on a channel it uses
    if self.crossfade_state and (channel == self.crossfade_state.from_channel or channel == self.crossfade_state.to_channel) then
      music_set_volume(self.crossfade_state.original_from_volume, self.crossfade_state.from_channel)
      music_set_volume(self.crossfade_state.original_to_volume, self.crossfade_state.to_channel)
      self.crossfade_state = nil
    end
    music_play(self.tracks[name], loop, channel)
  end

  --[[
    Stops music playback.

    Usage:
      an:music_stop()      -- stop all channels
      an:music_stop(0)     -- stop channel 0 only
      an:music_stop(1)     -- stop channel 1 only
  ]]
  function an:music_stop(channel)
    if channel == nil then channel = -1 end
    music_stop(channel)
    -- Cancel any in-progress crossfade and restore original volumes
    if self.crossfade_state then
      music_set_volume(self.crossfade_state.original_from_volume, self.crossfade_state.from_channel)
      music_set_volume(self.crossfade_state.original_to_volume, self.crossfade_state.to_channel)
      self.crossfade_state = nil
    end
    -- Reset playlist channel to 0 when stopping all channels
    if channel == -1 then
      self.playlist_channel = 0
    end
  end

  --[[
    Sets music volume.

    Usage:
      an:music_set_volume(0.5)          -- master volume
      an:music_set_volume(0.5, 0)       -- channel 0 volume
      an:music_set_volume(0.5, 1)       -- channel 1 volume

    Parameters:
      volume  - volume 0-1
      channel - (optional) channel 0 or 1, or -1 for master (default)
  ]]
  function an:music_set_volume(volume, channel)
    if channel == nil then channel = -1 end
    music_set_volume(volume, channel)
  end

  --[[
    Crossfades from current music to a new track.

    Usage:
      an:music_crossfade('battle', 2)       -- crossfade over 2 seconds
      an:music_crossfade('battle', 2, true) -- crossfade and loop

    Parameters:
      name     - music name (registered via an:music)
      duration - crossfade duration in seconds
      loop     - (optional) whether to loop new track, default false

    Behavior:
      - Starts new track on channel 1 at volume 0
      - Tweens channel 0 from 1 to 0
      - Tweens channel 1 from 0 to 1
      - Stops channel 0 when done and swaps channels
  ]]
  function an:music_crossfade(name, duration, loop)
    if loop == nil then loop = false end
    -- Determine channels: from current playlist channel, to the other one
    local from_channel = self.playlist_channel
    local to_channel = 1 - self.playlist_channel  -- toggles between 0 and 1

    -- Store original volumes before modifying
    local original_from_volume = music_get_volume(from_channel)
    local original_to_volume = music_get_volume(to_channel)

    -- Start new track on to_channel at volume 0
    music_set_volume(0, to_channel)
    music_play(self.tracks[name], loop, to_channel)

    -- Store crossfade state including original volumes for restoration if cancelled
    self.crossfade_state = {
      duration = duration,
      time = 0,
      from_channel = from_channel,
      to_channel = to_channel,
      original_from_volume = original_from_volume,
      original_to_volume = original_to_volume,
    }
  end

  -- Playlist state
  an.playlist = {}           -- array of track names
  an.playlist_index = 1      -- current index
  an.playlist_shuffled = {}  -- shuffled order (when shuffle enabled)
  an.playlist_shuffle_enabled = false
  an.playlist_crossfade_duration = 0  -- 0 = instant switch
  an.playlist_channel = 0             -- which channel playlist is currently using
  an.playlist_just_advanced = false   -- skip at_end check for one frame after advancing

  --[[
    Sets the playlist tracks.

    Usage:
      an:playlist_set({'menu', 'battle', 'boss'})

    Parameters:
      tracks - array of music names (registered via an:music)
  ]]
  function an:playlist_set(tracks)
    self.playlist = tracks
    self.playlist_index = 1
    self.playlist_shuffled = {}
    if self.playlist_shuffle_enabled then
      self:playlist_generate_shuffle()
    end
  end

  --[[
    Starts or resumes playlist playback.

    Usage:
      an:playlist_play()
  ]]
  function an:playlist_play()
    if #self.playlist == 0 then return end
    local track = self:playlist_current_track()
    if self.playlist_crossfade_duration > 0 then
      self:music_crossfade(track, self.playlist_crossfade_duration)
    else
      self:music_play(track, false, self.playlist_channel)
    end
    self.playlist_just_advanced = true
  end

  --[[
    Stops playlist playback.

    Usage:
      an:playlist_stop()
  ]]
  function an:playlist_stop()
    self:music_stop()
    self.playlist_channel = 0  -- reset to channel 0 for next start
  end

  --[[
    Advances to next track in playlist.

    Usage:
      an:playlist_next()
  ]]
  function an:playlist_next()
    if #self.playlist == 0 then return end
    self.playlist_index = (self.playlist_index % #self.playlist) + 1
    self:playlist_play()
  end

  --[[
    Goes to previous track in playlist.

    Usage:
      an:playlist_prev()
  ]]
  function an:playlist_prev()
    if #self.playlist == 0 then return end
    self.playlist_index = ((self.playlist_index - 2) % #self.playlist) + 1
    self:playlist_play()
  end

  --[[
    Enables or disables shuffle mode.

    Usage:
      an:playlist_shuffle(true)   -- enable
      an:playlist_shuffle(false)  -- disable

    Behavior:
      When enabled, tracks play in random order.
      Generates a new shuffle order each time the playlist loops.
  ]]
  function an:playlist_shuffle(enabled)
    self.playlist_shuffle_enabled = enabled
    if enabled then
      self:playlist_generate_shuffle()
    else
      self.playlist_shuffled = {}
    end
  end

  --[[
    Sets the crossfade duration for playlist transitions.

    Usage:
      an:playlist_set_crossfade(2)  -- 2 second crossfade
      an:playlist_set_crossfade(0)  -- instant switch (default)
  ]]
  function an:playlist_set_crossfade(duration)
    self.playlist_crossfade_duration = duration
  end

  -- Internal: generate shuffled order
  function an:playlist_generate_shuffle()
    self.playlist_shuffled = {}
    local indices = {}
    for i = 1, #self.playlist do
      table.insert(indices, i)
    end
    while #indices > 0 do
      local index = self.random:int(1, #indices)
      table.insert(self.playlist_shuffled, indices[index])
      table.remove(indices, index)
    end
  end

  -- Internal: get current track name respecting shuffle
  function an:playlist_current_track()
    if self.playlist_shuffle_enabled and #self.playlist_shuffled > 0 then
      return self.playlist[self.playlist_shuffled[self.playlist_index]]
    else
      return self.playlist[self.playlist_index]
    end
  end

  -- Crossfade update action
  an:early_action('crossfade', function(self, dt)
    if not self.crossfade_state then return end
    local crossfade = self.crossfade_state
    crossfade.time = crossfade.time + dt

    if crossfade.time >= crossfade.duration then
      -- Crossfade complete
      music_set_volume(1, crossfade.to_channel)
      music_stop(crossfade.from_channel)
      -- Restore from_channel's original volume so it's ready for future use
      music_set_volume(crossfade.original_from_volume, crossfade.from_channel)
      -- Swap playlist channel so it tracks the now-active channel
      self.playlist_channel = crossfade.to_channel
      self.crossfade_state = nil
    else
      -- Interpolate volumes
      local progress = crossfade.time / crossfade.duration
      music_set_volume(1 - progress, crossfade.from_channel)
      music_set_volume(progress, crossfade.to_channel)
    end
  end)

  -- Playlist auto-advance action
  an:early_action('playlist_auto_advance', function(self, dt)
    if #self.playlist == 0 then return end
    -- Skip check for one frame after advancing (music_at_end can still be true briefly)
    if self.playlist_just_advanced then
      self.playlist_just_advanced = false
      return
    end
    -- Check current playlist channel for end of track
    if music_at_end(self.playlist_channel) and not music_is_playing(self.playlist_channel) then
      -- Advance to next track
      self.playlist_index = (self.playlist_index % #self.playlist) + 1
      -- Regenerate shuffle if we looped back to start
      if self.playlist_index == 1 and self.playlist_shuffle_enabled then
        self:playlist_generate_shuffle()
      end
      self:playlist_play()
    end
  end)

  --[[
    Applies slow-motion effect with optional tween recovery.

    Usage:
      an:slow(0.5)                                    -- slow to 0.5, instant
      an:slow(0.5, 0.3)                               -- recover over 0.3s
      an:slow(0.2, 0.5, math.elastic_out)             -- with easing
      an:slow(0.2, 0.5, math.elastic_out, 'combat')   -- with tag for cancellation

    Parameters:
      amount   - time scale (0 = frozen, 1 = normal, 0.5 = half speed)
      duration - (optional) recovery duration in seconds (default 0)
      easing   - (optional) easing function for recovery (default math.cubic_in_out)
      tag      - (optional) string tag for cancellation via cancel_slow
  ]]
  function an:slow(amount, duration, easing, tag)
    duration = duration or 0
    easing = easing or math.cubic_in_out
    tag = tag or 'slow'
    self.time_scale = amount
    if duration > 0 then
      self.timer:tween(duration, tag, self, {time_scale = 1}, easing)
    end
  end

  --[[
    Cancels a slow-motion effect by tag.

    Usage:
      an:cancel_slow('combat')

    Immediately restores time_scale to 1.0 and cancels the tween.
  ]]
  function an:cancel_slow(tag)
    tag = tag or 'slow'
    self.timer:cancel(tag)
    self.time_scale = 1
  end

  --[[
    Applies hitstop (global freeze with tag-based exclusion).

    Usage:
      an:hit_stop(0.1)                              -- freeze everything for 0.1s
      an:hit_stop(0.05, {except = 'ui'})             -- 'ui' tagged objects use unscaled_dt
      an:hit_stop(0.1, {except = {'ui', 'particles'}}) -- multiple exclusions

    Parameters:
      duration - freeze duration in seconds (uses unscaled time)
      options  - (optional) table with:
                   except: tag or array of tags to exclude from hitstop

    During hitstop:
      - an.time_scale = 0, an.dt = 0
      - Physics is stepped with dt = 0 (maintains collisions, no movement)
      - Objects with excluded tags get unscaled_dt via get_dt_for
  ]]
  function an:hit_stop(duration, options)
    options = options or {}
    -- Handle exclusions
    local except = options.except
    if except then
      if type(except) == 'string' then
        self.hit_stop_excluded_tags = {[except] = true}
      else
        self.hit_stop_excluded_tags = {}
        for _, tag in ipairs(except) do
          self.hit_stop_excluded_tags[tag] = true
        end
      end
    else
      self.hit_stop_excluded_tags = {}
    end

    -- Only save pre_hitstop_time_scale if not already in hitstop
    if not self.hit_stop_active then
      self.pre_hitstop_time_scale = self.time_scale
    end

    -- Activate hitstop (engine sync happens in update loop)
    self.hit_stop_active = true
    self.hit_stop_remaining = duration
    self.time_scale = 0
  end

  --[[
    Returns the appropriate dt for an object based on hitstop state.

    Usage (internal, called by main loop):
      local dt = an:get_dt_for(object)

    Returns:
      - unscaled_dt if hitstop is active AND object has an excluded tag
      - an.dt otherwise (which is 0 during hitstop, scaled_dt otherwise)
  ]]
  function an:get_dt_for(object)
    if self.hit_stop_active and object.tags then
      for tag, _ in pairs(object.tags) do
        if self.hit_stop_excluded_tags[tag] then
          return self.unscaled_dt
        end
      end
    end
    return self.dt
  end

  -- Hitstop countdown action (uses unscaled time)
  an:early_action('hit_stop_countdown', function(self, dt)
    if not self.hit_stop_active then return end
    self.hit_stop_remaining = self.hit_stop_remaining - self.unscaled_dt
    if self.hit_stop_remaining <= 0 then
      self.hit_stop_active = false
      self.hit_stop_excluded_tags = {}
      self.time_scale = self.pre_hitstop_time_scale
    end
  end)

  -- Physics world state
  an.colliders = {}        -- body_handle -> collider (internal registry)
  an.collision_pairs = {}  -- tracks enabled pairs for queries
  an.sensor_pairs = {}
  an.hit_pairs = {}

  --[[
    Initializes the physics world.

    Usage:
      an:physics_init()

    Must be called before creating any colliders or setting physics properties.
  ]]
  function an:physics_init()
    physics_init()
  end

  --[[
    Sets the gravity vector for the physics world.

    Usage:
      an:physics_set_gravity(0, 500)   -- down
      an:physics_set_gravity(0, -500)  -- up
  ]]
  function an:physics_set_gravity(gx, gy)
    physics_set_gravity(gx, gy)
  end

  --[[
    Sets the meter scale (pixels per meter) for physics simulation.

    Usage:
      an:physics_set_meter_scale(32)
  ]]
  function an:physics_set_meter_scale(scale)
    physics_set_meter_scale(scale)
  end

  --[[
    Enables or disables physics simulation.

    Usage:
      an:physics_set_enabled(false)  -- pause physics
      an:physics_set_enabled(true)   -- resume physics
  ]]
  function an:physics_set_enabled(enabled)
    physics_set_enabled(enabled)
  end

  --[[
    Registers a physics tag for collision filtering.

    Usage:
      an:physics_tag('player')
      an:physics_tag('enemy')
      an:physics_tag('wall')

    Tags must be registered before enabling collisions between them.
  ]]
  function an:physics_tag(name)
    physics_register_tag(name)
  end

  --[[
    Enables solid collision between two tags.

    Usage:
      an:physics_collision('player', 'wall')
      an:physics_collision('enemy', 'wall')

    Both tags must be registered first via physics_tag.
  ]]
  function an:physics_collision(tag_a, tag_b)
    physics_enable_collision(tag_a, tag_b)
    table.insert(self.collision_pairs, {a = tag_a, b = tag_b})
  end

  --[[
    Enables sensor (overlap) detection between two tags.

    Usage:
      an:physics_sensor('player', 'pickup')

    Sensors detect overlap without physical response.
    Both tags must be registered first via physics_tag.
  ]]
  function an:physics_sensor(tag_a, tag_b)
    physics_enable_sensor(tag_a, tag_b)
    table.insert(self.sensor_pairs, {a = tag_a, b = tag_b})
  end

  --[[
    Enables hit events (collision with contact info) between two tags.

    Usage:
      an:physics_hit('bullet', 'enemy')

    Hit events include contact point, normal, and approach speed.
    Both tags must be registered first via physics_tag.
  ]]
  function an:physics_hit(tag_a, tag_b)
    physics_enable_hit(tag_a, tag_b)
    table.insert(self.hit_pairs, {a = tag_a, b = tag_b})
  end

  --[[
    Returns collision begin events between two tags this frame.

    Usage:
      for _, event in ipairs(an:collision_begin_events('player', 'enemy')) do
        event.a:take_damage(10)
        spawn_particles(event.point_x, event.point_y)
      end

    Returns array of:
      {a = <object>, b = <object>, shape_a = <handle>, shape_b = <handle>,
       point_x, point_y, normal_x, normal_y}
  ]]
  function an:collision_begin_events(tag_a, tag_b)
    local result = {}
    for _, event in ipairs(physics_get_collision_begin(tag_a, tag_b)) do
      local id_a = physics_get_user_data(event.body_a)
      local id_b = physics_get_user_data(event.body_b)
      local collider_a = self.colliders[id_a]
      local collider_b = self.colliders[id_b]
      if collider_a and collider_b then
        -- Normalize order: a should have tag_a, b should have tag_b
        if event.tag_a == tag_a and event.tag_b == tag_b then
          table.insert(result, {
            a = collider_a.parent,
            b = collider_b.parent,
            shape_a = event.shape_a,
            shape_b = event.shape_b,
            point_x = event.point_x,
            point_y = event.point_y,
            normal_x = event.normal_x,
            normal_y = event.normal_y,
          })
        elseif event.tag_a == tag_b and event.tag_b == tag_a then
          table.insert(result, {
            a = collider_b.parent,
            b = collider_a.parent,
            shape_a = event.shape_b,
            shape_b = event.shape_a,
            point_x = event.point_x,
            point_y = event.point_y,
            normal_x = -event.normal_x,
            normal_y = -event.normal_y,
          })
        end
      end
    end
    return result
  end

  --[[
    Returns collision end events between two tags this frame.

    Usage:
      for _, event in ipairs(an:collision_end_events('player', 'platform')) do
        event.a.on_ground = false
      end

    Returns array of:
      {a = <object>, b = <object>, shape_a = <handle>, shape_b = <handle>}
  ]]
  function an:collision_end_events(tag_a, tag_b)
    local result = {}
    for _, event in ipairs(physics_get_collision_end(tag_a, tag_b)) do
      local id_a = physics_get_user_data(event.body_a)
      local id_b = physics_get_user_data(event.body_b)
      local collider_a = self.colliders[id_a]
      local collider_b = self.colliders[id_b]
      if collider_a and collider_b then
        if event.tag_a == tag_a and event.tag_b == tag_b then
          table.insert(result, {
            a = collider_a.parent,
            b = collider_b.parent,
            shape_a = event.shape_a,
            shape_b = event.shape_b,
          })
        elseif event.tag_a == tag_b and event.tag_b == tag_a then
          table.insert(result, {
            a = collider_b.parent,
            b = collider_a.parent,
            shape_a = event.shape_b,
            shape_b = event.shape_a,
          })
        end
      end
    end
    return result
  end

  --[[
    Returns sensor begin events between two tags this frame.

    Usage:
      for _, event in ipairs(an:sensor_begin_events('player', 'pickup')) do
        event.a:collect(event.b)
        event.b:kill()
      end

    Returns array of:
      {a = <object>, b = <object>, shape_a = <handle>, shape_b = <handle>}
  ]]
  function an:sensor_begin_events(tag_a, tag_b)
    local result = {}
    for _, event in ipairs(physics_get_sensor_begin(tag_a, tag_b)) do
      local id_a = physics_get_user_data(event.sensor_body)
      local id_b = physics_get_user_data(event.visitor_body)
      local collider_a = self.colliders[id_a]
      local collider_b = self.colliders[id_b]
      if collider_a and collider_b then
        if event.sensor_tag == tag_a and event.visitor_tag == tag_b then
          table.insert(result, {
            a = collider_a.parent,
            b = collider_b.parent,
            shape_a = event.sensor_shape,
            shape_b = event.visitor_shape,
          })
        elseif event.sensor_tag == tag_b and event.visitor_tag == tag_a then
          table.insert(result, {
            a = collider_b.parent,
            b = collider_a.parent,
            shape_a = event.visitor_shape,
            shape_b = event.sensor_shape,
          })
        end
      end
    end
    return result
  end

  --[[
    Returns sensor end events between two tags this frame.

    Usage:
      for _, event in ipairs(an:sensor_end_events('player', 'zone')) do
        event.b:on_player_exit()
      end

    Returns array of:
      {a = <object>, b = <object>, shape_a = <handle>, shape_b = <handle>}
  ]]
  function an:sensor_end_events(tag_a, tag_b)
    local result = {}
    for _, event in ipairs(physics_get_sensor_end(tag_a, tag_b)) do
      local id_a = physics_get_user_data(event.sensor_body)
      local id_b = physics_get_user_data(event.visitor_body)
      local collider_a = self.colliders[id_a]
      local collider_b = self.colliders[id_b]
      if collider_a and collider_b then
        if event.sensor_tag == tag_a and event.visitor_tag == tag_b then
          table.insert(result, {
            a = collider_a.parent,
            b = collider_b.parent,
            shape_a = event.sensor_shape,
            shape_b = event.visitor_shape,
          })
        elseif event.sensor_tag == tag_b and event.visitor_tag == tag_a then
          table.insert(result, {
            a = collider_b.parent,
            b = collider_a.parent,
            shape_a = event.visitor_shape,
            shape_b = event.sensor_shape,
          })
        end
      end
    end
    return result
  end

  --[[
    Returns hit events between two tags this frame.

    Usage:
      for _, hit in ipairs(an:hit_events('bullet', 'enemy')) do
        hit.a:kill()
        hit.b:take_damage(10)
        spawn_particles(hit.point_x, hit.point_y)
      end

    Returns array of:
      {a = <object>, b = <object>, shape_a = <handle>, shape_b = <handle>,
       point_x, point_y, normal_x, normal_y, approach_speed}
  ]]
  function an:hit_events(tag_a, tag_b)
    local result = {}
    for _, event in ipairs(physics_get_hit(tag_a, tag_b)) do
      local id_a = physics_get_user_data(event.body_a)
      local id_b = physics_get_user_data(event.body_b)
      local collider_a = self.colliders[id_a]
      local collider_b = self.colliders[id_b]
      if collider_a and collider_b then
        if event.tag_a == tag_a and event.tag_b == tag_b then
          table.insert(result, {
            a = collider_a.parent,
            b = collider_b.parent,
            shape_a = event.shape_a,
            shape_b = event.shape_b,
            point_x = event.point_x,
            point_y = event.point_y,
            normal_x = event.normal_x,
            normal_y = event.normal_y,
            approach_speed = event.approach_speed,
          })
        elseif event.tag_a == tag_b and event.tag_b == tag_a then
          table.insert(result, {
            a = collider_b.parent,
            b = collider_a.parent,
            shape_a = event.shape_b,
            shape_b = event.shape_a,
            point_x = event.point_x,
            point_y = event.point_y,
            normal_x = -event.normal_x,
            normal_y = -event.normal_y,
            approach_speed = event.approach_speed,
          })
        end
      end
    end
    return result
  end

  --[[
    Queries for objects at a point.

    Usage:
      for _, object in ipairs(an:query_point(x, y, 'enemy')) do
        object:highlight()
      end
      for _, object in ipairs(an:query_point(x, y, {'enemy', 'pickup'})) do
        object:highlight()
      end

    Returns array of objects whose colliders contain the point.
  ]]
  function an:query_point(x, y, tags)
    if type(tags) == 'string' then tags = {tags} end
    local result = {}
    for _, body in ipairs(physics_query_point(x, y, tags)) do
      local id = physics_get_user_data(body)
      local collider = self.colliders[id]
      if collider then
        table.insert(result, collider.parent)
      end
    end
    return result
  end

  --[[
    Queries for objects within a circle.

    Usage:
      for _, object in ipairs(an:query_circle(x, y, 50, 'enemy')) do
        object:take_damage(10)
      end

    Returns array of objects whose colliders overlap the circle.
  ]]
  function an:query_circle(x, y, radius, tags)
    if type(tags) == 'string' then tags = {tags} end
    local result = {}
    for _, body in ipairs(physics_query_circle(x, y, radius, tags)) do
      local id = physics_get_user_data(body)
      local collider = self.colliders[id]
      if collider then
        table.insert(result, collider.parent)
      end
    end
    return result
  end

  --[[
    Queries for objects within an axis-aligned bounding box.

    Usage:
      for _, object in ipairs(an:query_aabb(x, y, width, height, 'enemy')) do
        object:alert()
      end

    Returns array of objects whose colliders overlap the AABB.
  ]]
  function an:query_aabb(x, y, width, height, tags)
    if type(tags) == 'string' then tags = {tags} end
    local result = {}
    for _, body in ipairs(physics_query_aabb(x, y, width, height, tags)) do
      local id = physics_get_user_data(body)
      local collider = self.colliders[id]
      if collider then
        table.insert(result, collider.parent)
      end
    end
    return result
  end

  --[[
    Queries for objects within a rotated box.

    Usage:
      for _, object in ipairs(an:query_box(x, y, width, height, angle, 'enemy')) do
        object:alert()
      end

    Returns array of objects whose colliders overlap the box.
  ]]
  function an:query_box(x, y, width, height, angle, tags)
    if type(tags) == 'string' then tags = {tags} end
    local result = {}
    for _, body in ipairs(physics_query_box(x, y, width, height, angle, tags)) do
      local id = physics_get_user_data(body)
      local collider = self.colliders[id]
      if collider then
        table.insert(result, collider.parent)
      end
    end
    return result
  end

  --[[
    Queries for objects within a capsule shape.

    Usage:
      for _, object in ipairs(an:query_capsule(x1, y1, x2, y2, radius, 'enemy')) do
        object:stun()
      end

    Returns array of objects whose colliders overlap the capsule.
  ]]
  function an:query_capsule(x1, y1, x2, y2, radius, tags)
    if type(tags) == 'string' then tags = {tags} end
    local result = {}
    for _, body in ipairs(physics_query_capsule(x1, y1, x2, y2, radius, tags)) do
      local id = physics_get_user_data(body)
      local collider = self.colliders[id]
      if collider then
        table.insert(result, collider.parent)
      end
    end
    return result
  end

  --[[
    Queries for objects within a polygon shape.

    Usage:
      local verts = {0, 0, 100, 0, 50, 100}
      for _, object in ipairs(an:query_polygon(x, y, verts, 'enemy')) do
        object:damage()
      end

    Vertices are a flat array: {x1, y1, x2, y2, ...}
    Returns array of objects whose colliders overlap the polygon.
  ]]
  function an:query_polygon(x, y, vertices, tags)
    if type(tags) == 'string' then tags = {tags} end
    local result = {}
    for _, body in ipairs(physics_query_polygon(x, y, vertices, tags)) do
      local id = physics_get_user_data(body)
      local collider = self.colliders[id]
      if collider then
        table.insert(result, collider.parent)
      end
    end
    return result
  end

  --[[
    Casts a ray and returns the first hit.

    Usage:
      local hit = an:raycast(x1, y1, x2, y2, 'wall')
      if hit then
        spawn_impact(hit.point_x, hit.point_y)
        hit.object:take_damage(10)
      end

    Returns: {object, shape, point_x, point_y, normal_x, normal_y, fraction} or nil
  ]]
  function an:raycast(x1, y1, x2, y2, tags)
    if type(tags) == 'string' then tags = {tags} end
    local hit = physics_raycast(x1, y1, x2, y2, tags)
    if hit then
      local id = physics_get_user_data(hit.body)
      local collider = self.colliders[id]
      if collider then
        return {
          object = collider.parent,
          shape = hit.shape,
          point_x = hit.point_x,
          point_y = hit.point_y,
          normal_x = hit.normal_x,
          normal_y = hit.normal_y,
          fraction = hit.fraction,
        }
      end
    end
    return nil
  end

  --[[
    Casts a ray and returns all hits.

    Usage:
      for _, hit in ipairs(an:raycast_all(x1, y1, x2, y2, 'enemy')) do
        hit.object:take_damage(5)
      end

    Returns array of: {object, shape, point_x, point_y, normal_x, normal_y, fraction}
  ]]
  function an:raycast_all(x1, y1, x2, y2, tags)
    if type(tags) == 'string' then tags = {tags} end
    local result = {}
    for _, hit in ipairs(physics_raycast_all(x1, y1, x2, y2, tags)) do
      local id = physics_get_user_data(hit.body)
      local collider = self.colliders[id]
      if collider then
        table.insert(result, {
          object = collider.parent,
          shape = hit.shape,
          point_x = hit.point_x,
          point_y = hit.point_y,
          normal_x = hit.normal_x,
          normal_y = hit.normal_y,
          fraction = hit.fraction,
        })
      end
    end
    return result
  end

  --[[
    Binds a control to an action.

    Usage:
      an:bind('jump', 'key:space')
      an:bind('jump', 'button:a')
      an:bind('fire', 'mouse:1')
      an:bind('left', 'axis:leftx-')

    Control string formats:
      key:a, key:space, key:lshift, key:up, key:1
      mouse:1 (left), mouse:2 (middle), mouse:3 (right)
      button:a, button:b, button:x, button:y, button:start, button:dpup, etc.
      axis:leftx+, axis:leftx-, axis:lefty+, axis:lefty-, axis:triggerleft, etc.
  ]]
  function an:bind(action, control)
    input_bind(action, control)
  end

  --[[
    Unbinds a specific control from an action.

    Usage:
      an:unbind('jump', 'key:space')
  ]]
  function an:unbind(action, control)
    input_unbind(action, control)
  end

  --[[
    Unbinds all controls from an action.

    Usage:
      an:unbind_all('jump')
  ]]
  function an:unbind_all(action)
    input_unbind_all(action)
  end

  --[[
    Binds default controls for all common keys/buttons.

    Usage:
      an:bind_all()

    Creates actions like 'a', 'space', 'mouse_1', 'button_a', etc.
  ]]
  function an:bind_all()
    input_bind_all()
  end

  --[[
    Creates a chord (all actions must be held simultaneously).

    Usage:
      an:bind_chord('super_jump', {'shift', 'jump'})

    The chord triggers when ALL listed actions are held at once.
  ]]
  function an:bind_chord(name, actions)
    input_bind_chord(name, actions)
  end

  --[[
    Creates a sequence (actions in order within time windows).

    Usage:
      an:bind_sequence('dash', {'right', 0.3, 'right'})
      an:bind_sequence('hadouken', {'down', 0.2, 'forward', 0.2, 'punch'})

    Format: {action1, delay1, action2, delay2, action3, ...}
    The sequence triggers when actions are pressed in order within the delays.
  ]]
  function an:bind_sequence(name, sequence)
    input_bind_sequence(name, sequence)
  end

  --[[
    Creates a hold action (triggers after holding for duration).

    Usage:
      an:bind_hold('charge', 1.0, 'attack')

    The hold triggers after holding source_action for duration seconds.
  ]]
  function an:bind_hold(name, duration, source_action)
    input_bind_hold(name, duration, source_action)
  end

  --[[
    Returns true on the frame an action was pressed.

    Usage:
      if an:is_pressed('jump') then
        player:jump()
      end
  ]]
  function an:is_pressed(action)
    return is_pressed(action)
  end

  --[[
    Returns true while an action is held.

    Usage:
      if an:is_down('fire') then
        player:shoot()
      end
  ]]
  function an:is_down(action)
    return is_down(action)
  end

  --[[
    Returns true on the frame an action was released.

    Usage:
      if an:is_released('charge') then
        player:release_charge()
      end
  ]]
  function an:is_released(action)
    return is_released(action)
  end

  --[[
    Returns -1, 0, or 1 based on two opposing actions.

    Usage:
      local horizontal = an:get_axis('left', 'right')
      player.vx = horizontal * speed
  ]]
  function an:get_axis(negative, positive)
    return input_get_axis(negative, positive)
  end

  --[[
    Returns a normalized 2D direction vector.

    Usage:
      local vx, vy = an:get_vector('left', 'right', 'up', 'down')
      player.vx = vx * speed
      player.vy = vy * speed

    The vector is normalized so diagonal movement isn't faster.
  ]]
  function an:get_vector(left, right, up, down)
    return input_get_vector(left, right, up, down)
  end

  --[[
    Returns how long the source action has been held.

    Usage:
      local held = an:get_hold_duration('charge')
      local power = math.min(held / 1.0, 1.0)

    Use for charge bar UI or variable-power attacks.
  ]]
  function an:get_hold_duration(name)
    return input_get_hold_duration(name)
  end

  --[[
    Returns true if any bound action was pressed this frame.

    Usage:
      if an:any_pressed() then
        start_game()
      end
  ]]
  function an:any_pressed()
    return input_any_pressed()
  end

  --[[
    Returns the name of the action that was just pressed, or nil.

    Usage:
      local action = an:get_pressed_action()
      if action then
        print("Pressed: " .. action)
      end
  ]]
  function an:get_pressed_action()
    return input_get_pressed_action()
  end

  --[[
    Returns true if a keyboard key is held.

    Usage:
      if an:key_is_down('space') then
        -- charging
      end
  ]]
  function an:key_is_down(key)
    return key_is_down(key)
  end

  --[[
    Returns true on the frame a keyboard key was pressed.

    Usage:
      if an:key_is_pressed('escape') then
        toggle_pause()
      end
  ]]
  function an:key_is_pressed(key)
    return key_is_pressed(key)
  end

  --[[
    Returns true on the frame a keyboard key was released.

    Usage:
      if an:key_is_released('space') then
        release_attack()
      end
  ]]
  function an:key_is_released(key)
    return key_is_released(key)
  end

  --[[
    Returns the mouse position in game coordinates.

    Usage:
      local mx, my = an:mouse_position()
  ]]
  function an:mouse_position()
    return mouse_position()
  end

  --[[
    Returns the mouse movement this frame.

    Usage:
      local dx, dy = an:mouse_delta()
  ]]
  function an:mouse_delta()
    return mouse_delta()
  end

  --[[
    Returns the mouse wheel movement this frame.

    Usage:
      local wx, wy = an:mouse_wheel()
      zoom = zoom + wy * 0.1
  ]]
  function an:mouse_wheel()
    return mouse_wheel()
  end

  --[[
    Returns true if a mouse button is held.

    Usage:
      if an:mouse_is_down(1) then  -- left button
        player:aim()
      end

    Buttons: 1=left, 2=middle, 3=right
  ]]
  function an:mouse_is_down(button)
    return mouse_is_down(button)
  end

  --[[
    Returns true on the frame a mouse button was pressed.

    Usage:
      if an:mouse_is_pressed(1) then
        player:shoot()
      end
  ]]
  function an:mouse_is_pressed(button)
    return mouse_is_pressed(button)
  end

  --[[
    Returns true on the frame a mouse button was released.

    Usage:
      if an:mouse_is_released(1) then
        player:release()
      end
  ]]
  function an:mouse_is_released(button)
    return mouse_is_released(button)
  end

  --[[
    Shows or hides the system cursor.

    Usage:
      an:mouse_set_visible(false)  -- hide cursor
  ]]
  function an:mouse_set_visible(visible)
    mouse_set_visible(visible)
  end

  --[[
    Locks the cursor to the window.

    Usage:
      an:mouse_set_grabbed(true)  -- for FPS-style control
  ]]
  function an:mouse_set_grabbed(grabbed)
    mouse_set_grabbed(grabbed)
  end

  --[[
    Returns true if a gamepad is connected.

    Usage:
      if an:gamepad_is_connected() then
        show_gamepad_prompts()
      end
  ]]
  function an:gamepad_is_connected()
    return gamepad_is_connected()
  end

  --[[
    Returns a gamepad axis value (-1 to 1).

    Usage:
      local lx = an:gamepad_get_axis('leftx')
      local ly = an:gamepad_get_axis('lefty')

    Axes: leftx, lefty, rightx, righty, triggerleft, triggerright
  ]]
  function an:gamepad_get_axis(axis)
    return gamepad_get_axis(axis)
  end

  --[[
    Returns the last input type: 'keyboard', 'mouse', or 'gamepad'.

    Usage:
      if an:get_last_input_type() == 'gamepad' then
        show_gamepad_prompts()
      else
        show_keyboard_prompts()
      end
  ]]
  function an:get_last_input_type()
    return input_get_last_type()
  end

  --[[
    Starts capturing the next input for rebinding.

    Usage:
      an:start_capture()
      show_prompt("Press a key...")
  ]]
  function an:start_capture()
    input_start_capture()
  end

  --[[
    Returns the captured input string, or nil if still waiting.

    Usage:
      local captured = an:get_captured()
      if captured then
        an:unbind_all('jump')
        an:bind('jump', captured)
      end
  ]]
  function an:get_captured()
    return input_get_captured()
  end

  --[[
    Stops capture mode without binding.

    Usage:
      an:stop_capture()
  ]]
  function an:stop_capture()
    input_stop_capture()
  end

  --[[
    Sets the gamepad stick deadzone.

    Usage:
      an:set_deadzone(0.2)
  ]]
  function an:set_deadzone(deadzone)
    input_set_deadzone(deadzone)
  end

  --[[
    Global update function called by the C engine each physics tick (144Hz).

    Behavior:
      1. Attaches camera transforms to layers
      2. Collects an + all descendants into a flat array
      3. Runs early phase (_early_update) on all objects
      4. Runs main phase (_update) on all objects
      5. Runs late phase (_late_update) on all objects
      6. Runs cleanup to remove dead objects and finished actions
      7. Detaches camera transforms from layers

    The three phases allow proper ordering:
      - early: input handling, pre-update logic
      - main: game logic, movement, collisions
      - late: drawing, post-update cleanup

    Note: This is called automatically by the C engine. Do not call manually.
  ]]
  function update(dt)
    -- Update engine state: dynamic values
    an.frame = engine_get_frame()
    an.step = engine_get_step()
    an.time = engine_get_time()
    an.window_width, an.window_height = engine_get_window_size()
    an.scale = engine_get_scale()
    an.fullscreen = engine_is_fullscreen()
    an.fps = engine_get_fps()
    an.draw_calls = engine_get_draw_calls()

    -- Attach camera transforms to layers before any updates
    for name, lyr in pairs(an.layers) do
      if lyr.camera then
        lyr.camera:attach(lyr, lyr.parallax_x, lyr.parallax_y)
      end
    end

    -- Update time scale values and sync to engine
    engine_set_time_scale(an.time_scale)
    an.dt = engine_get_dt()
    an.unscaled_dt = engine_get_unscaled_dt()

    local all_objects = {an}
    for _, obj in ipairs(an:all()) do
      table.insert(all_objects, obj)
    end

    -- Early phase (uses per-object dt based on hitstop exclusion)
    for _, obj in ipairs(all_objects) do
      obj:_early_update(an:get_dt_for(obj))
    end
    -- Main phase
    for _, obj in ipairs(all_objects) do
      obj:_update(an:get_dt_for(obj))
    end
    -- Late phase
    for _, obj in ipairs(all_objects) do
      obj:_late_update(an:get_dt_for(obj))
    end
    an:cleanup()

    -- Detach camera transforms after all drawing is done
    for name, lyr in pairs(an.layers) do
      if lyr.camera then
        lyr.camera:detach(lyr)
      end
    end
  end
end
