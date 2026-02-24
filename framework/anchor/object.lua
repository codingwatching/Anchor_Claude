require('anchor.class')

object = class:extend()

--[[
  Creates a new object with optional name.

  Usage:
    obj = object()           -- anonymous object
    obj = object('player')   -- named object

  Properties initialized:
    self.name     - string or nil, used for bidirectional links
    self.parent   - reference to parent object, nil if root
    self.children - array of child objects
    self.dead     - boolean, true when killed (removed at end of frame)
    self.tags     - set of tags, used for querying with all(tag) and is()
]]
function object:new(name)
  self.name = name
  self.parent = nil
  self.children = {}
  self.dead = false
  self.tags = {}
end

--[[
  Adds a child to this object's tree.

  Usage:
    self:add(child)
    self:add(object('timer')):add(object('collider'))  -- chainable

  Behavior:
    - Appends child to self.children array
    - Sets child.parent = self
    - If child has a name: creates self[child.name] = child (parent can access child by name)
    - If parent has a name: creates child[self.name] = self (child can access parent by name)
    - If a child with the same name already exists, kills the old child first (replacement)

  Edge cases:
    - Adding same child twice: child appears twice in self.children, both get killed
    - Adding child that has another parent: child ends up in two parents' arrays (avoid this)

  Returns: self (for chaining)
]]
function object:add(child)
  table.insert(self.children, child)
  child.parent = self
  if child.name then
    if self[child.name] then self[child.name]:kill() end
    self[child.name] = child
  end
  if self.name then
    child[self.name] = self
  end
  return self
end

--[[
  Returns all descendants of this object, optionally filtered by tag.

  Usage:
    all_objects = self:all()           -- all descendants
    enemies = self:all('enemy')        -- only descendants with 'enemy' tag

  Behavior:
    - Uses iterative DFS (depth-first search), left-to-right order
    - Does NOT include self, only descendants
    - Returns ALL descendants including dead ones (dead check is caller's responsibility)
    - When tag provided, only returns objects where obj.tags[tag] is truthy

  Returns: array of objects
]]
function object:all(tag)
  local nodes = {}
  local stack = {}
  for i = #self.children, 1, -1 do
    table.insert(stack, self.children[i])
  end
  while #stack > 0 do
    local node = table.remove(stack)
    if tag then
      if node.tags[tag] then
        table.insert(nodes, node)
      end
    else
      table.insert(nodes, node)
    end
    for i = #node.children, 1, -1 do
      table.insert(stack, node.children[i])
    end
  end
  return nodes
end

--[[
  Marks this object (and descendants) as dead. Actual removal happens at end of frame.

  Usage:
    self:kill()                -- kill self and all descendants
    self:kill('enemy')         -- kill all descendants with 'enemy' tag (and their children)

  Behavior (no tag):
    - Sets self.dead = true on self
    - Sets dead = true on ALL descendants (children never outlive parents)

  Behavior (with tag):
    - If self has the tag: calls self:kill() (kills self and all descendants)
    - Otherwise: finds all descendants with that tag and calls kill() on each
      (which kills them and their descendants, even if descendants don't have the tag)

  Edge cases:
    - Killing already dead object: no-op, safe to call multiple times
    - Dead objects are removed from tree at end of frame by cleanup()

  Returns: self (for chaining)
]]
function object:kill(tag)
  if tag then
    if self.tags[tag] then
      self:kill()
    else
      for _, obj in ipairs(self:all(tag)) do
        obj:kill()
      end
    end
  else
    self.dead = true
    if self.linked_from then
      for _, link in ipairs(self.linked_from) do
        if not link.source.dead then
          if link.callback then
            link.callback(link.source)
          else
            link.source:kill()
          end
        end
      end
    end
    for _, obj in ipairs(self:all()) do
      obj.dead = true
    end
  end
  return self
end

--[[
  Adds one or more tags to this object.

  Usage:
    self:tag('enemy')                    -- single tag
    self:tag('enemy', 'flying', 'boss')  -- multiple tags

  Behavior:
    - Tags are stored as self.tags[tagname] = true (set semantics)
    - Used for querying with all(tag) and checking with is()

  Returns: self (for chaining)
]]
function object:tag(...)
  for _, t in ipairs({...}) do
    self.tags[t] = true
  end
  return self
end

--[[
  Checks if object matches a name or has a tag.

  Usage:
    if self:is('player')    -- checks name OR tag
    if self:is('enemy')     -- true if self.name == 'enemy' OR self.tags['enemy']

  Returns: truthy if match, nil/false otherwise
]]
function object:is(name_or_tag)
  return self.name == name_or_tag or self.tags[name_or_tag]
end

--[[
  Creates a horizontal link to another object for death notification.

  Usage:
    self:link(target)                                    -- kill self when target dies
    self:link(target, function(s) s.homing = false end)  -- run callback when target dies

  Behavior:
    - When target dies (kill() is called), callback runs with self as argument
    - If no callback provided, self is killed when target dies
    - Links are bidirectional internally: self.links stores outgoing, target.linked_from stores incoming
    - Both are cleaned up when either object is removed from tree

  Returns: self (for chaining)
]]
function object:link(target, callback)
  if not self.links then self.links = {} end
  table.insert(self.links, {target = target, callback = callback})
  if not target.linked_from then target.linked_from = {} end
  table.insert(target.linked_from, {source = self, callback = callback})
  return self
end

--[[
  Assigns properties from a table to this object.

  Usage:
    self:set({x = 100, y = 200, hp = 50})

  Behavior:
    - Iterates over key-value pairs in the table
    - Assigns each key-value pair to self

  Returns: self (for chaining)
]]
function object:set(properties)
  for key, value in pairs(properties) do
    self[key] = value
  end
  return self
end

--[[
  Runs a build function with this object as the argument.

  Usage:
    self:build(function(s)
      s.x = 100
      s.y = 200
      s.hp = s.x + s.y
    end)

  Behavior:
    - Calls the function with self as argument
    - Useful for complex initialization that can't be done with a simple table

  Returns: self (for chaining)
]]
function object:build(build_function)
  build_function(self)
  return self
end

--[[
  Adds this object to a parent (reverse of add).

  Usage:
    player:flow_to(arena)    -- equivalent to: arena:add(player)

  Behavior:
    - Calls parent:add(self) internally
    - Useful for fluent chaining when creating objects inline

  Returns: self (for chaining)
]]
function object:flow_to(parent)
  parent:add(self)
  return self
end

--[[
  Adds an action to run during the early phase (before main update).

  Usage:
    self:early_action(function() print('runs every frame') end)           -- anonymous
    self:early_action('input', function(s) s:handle_input() end)          -- named
    self:early_action(function() return true end)                         -- one-shot (returns true to remove)

  Behavior:
    - Anonymous: function stored in self.early_actions array
    - Named: function also accessible as self[name], replaces existing action with same name
    - Actions receive (self, dt) as arguments
    - If action returns true, it's removed at end of frame

  Early phase runs before main phase, useful for input handling.

  Returns: self (for chaining)
]]
function object:early_action(name_or_fn, fn)
  if not self.early_actions then self.early_actions = {} end
  if not self.early_action_names then self.early_action_names = {} end
  if type(name_or_fn) == 'string' then
    local name = name_or_fn
    for i, n in ipairs(self.early_action_names) do
      if n == name then
        self.early_actions[i] = fn
        self[name] = fn
        return self
      end
    end
    table.insert(self.early_actions, fn)
    table.insert(self.early_action_names, name)
    self[name] = fn
  else
    table.insert(self.early_actions, name_or_fn)
    table.insert(self.early_action_names, false)
  end
  return self
end

--[[
  Adds an action to run during the main phase.

  Usage:
    self:action(function() print('runs every frame') end)                 -- anonymous
    self:action('move', function(s, dt) s.x = s.x + s.speed * dt end)    -- named
    self:action(function(s, dt) s.lifetime = s.lifetime - dt; return s.lifetime <= 0 end)  -- one-shot when lifetime expires

  Behavior:
    - Anonymous: function stored in self.actions array
    - Named: function also accessible as self[name], replaces existing action with same name
    - Actions receive (self, dt) as arguments
    - If action returns true, it's removed at end of frame

  Main phase is the standard update phase, runs after early and before late.

  Returns: self (for chaining)
]]
function object:action(name_or_fn, fn)
  if not self.actions then self.actions = {} end
  if not self.action_names then self.action_names = {} end
  if type(name_or_fn) == 'string' then
    local name = name_or_fn
    for i, n in ipairs(self.action_names) do
      if n == name then
        self.actions[i] = fn
        self[name] = fn
        return self
      end
    end
    table.insert(self.actions, fn)
    table.insert(self.action_names, name)
    self[name] = fn
  else
    table.insert(self.actions, name_or_fn)
    table.insert(self.action_names, false)
  end
  return self
end

--[[
  Adds an action to run during the late phase (after main update).

  Usage:
    self:late_action(function(s) draw_sprite(s.x, s.y) end)              -- anonymous
    self:late_action('draw', function(s) s:render() end)                  -- named

  Behavior:
    - Anonymous: function stored in self.late_actions array
    - Named: function also accessible as self[name], replaces existing action with same name
    - Actions receive (self, dt) as arguments
    - If action returns true, it's removed at end of frame

  Late phase runs after main phase, useful for drawing and post-update logic.

  Returns: self (for chaining)
]]
function object:late_action(name_or_fn, fn)
  if not self.late_actions then self.late_actions = {} end
  if not self.late_action_names then self.late_action_names = {} end
  if type(name_or_fn) == 'string' then
    local name = name_or_fn
    for i, n in ipairs(self.late_action_names) do
      if n == name then
        self.late_actions[i] = fn
        self[name] = fn
        return self
      end
    end
    table.insert(self.late_actions, fn)
    table.insert(self.late_action_names, name)
    self[name] = fn
  else
    table.insert(self.late_actions, name_or_fn)
    table.insert(self.late_action_names, false)
  end
  return self
end

--[[
  Internal: runs early phase for this object.
  Called by init.lua's update loop, not meant to be called directly.

  Behavior:
    - Returns immediately if self.dead
    - Calls self:early_update(dt) if object has an early_update method
    - Runs all early_actions, marking those that return true for removal
]]
function object:_early_update(dt)
  if self.dead then return end
  if self.early_update then self:early_update(dt) end
  if self.early_actions then
    for i, fn in ipairs(self.early_actions) do
      if fn(self, dt) == true then
        if not self.early_actions_to_remove then self.early_actions_to_remove = {} end
        table.insert(self.early_actions_to_remove, i)
      end
    end
  end
end

--[[
  Internal: runs main phase for this object.
  Called by init.lua's update loop, not meant to be called directly.

  Behavior:
    - Returns immediately if self.dead
    - Calls self:update(dt) if object has an update method
    - Runs all actions, marking those that return true for removal
]]
function object:_update(dt)
  if self.dead then return end
  if self.update then self:update(dt) end
  if self.actions then
    for i, fn in ipairs(self.actions) do
      if fn(self, dt) == true then
        if not self.actions_to_remove then self.actions_to_remove = {} end
        table.insert(self.actions_to_remove, i)
      end
    end
  end
end

--[[
  Internal: runs late phase for this object.
  Called by init.lua's update loop, not meant to be called directly.

  Behavior:
    - Returns immediately if self.dead
    - Calls self:late_update(dt) if object has a late_update method
    - Runs all late_actions, marking those that return true for removal
]]
function object:_late_update(dt)
  if self.dead then return end
  if self.late_update then self:late_update(dt) end
  if self.late_actions then
    for i, fn in ipairs(self.late_actions) do
      if fn(self, dt) == true then
        if not self.late_actions_to_remove then self.late_actions_to_remove = {} end
        table.insert(self.late_actions_to_remove, i)
      end
    end
  end
end

--[[
  End-of-frame cleanup: removes dead actions and dead objects from tree.
  Called by init.lua at end of each frame on the root object (an).

  Behavior:
    1. Collects self + all descendants
    2. For each object, removes actions marked for removal (from all three phases)
       - Removes from actions array and action_names array
       - Clears self[name] reference for named actions
    3. Removes dead children from tree (iterates in reverse for children-first destroy order)
       - Calls child:destroy() if child has a destroy method (for resource cleanup)
       - Clears bidirectional named links
       - Removes from parent.children array

  Edge cases:
    - Dead objects' descendants are also dead (kill propagates), so they get cleaned recursively
    - Destroy is called children-first (deepest nodes first) for proper resource cleanup order
]]
function object:cleanup()
  local objects = {self}
  for _, obj in ipairs(self:all()) do
    table.insert(objects, obj)
  end

  -- Remove marked actions
  for _, obj in ipairs(objects) do
    if obj.early_actions_to_remove then
      for i = #obj.early_actions_to_remove, 1, -1 do
        local idx = obj.early_actions_to_remove[i]
        local name = obj.early_action_names[idx]
        if name then obj[name] = nil end
        table.remove(obj.early_actions, idx)
        table.remove(obj.early_action_names, idx)
      end
      obj.early_actions_to_remove = nil
    end
    if obj.actions_to_remove then
      for i = #obj.actions_to_remove, 1, -1 do
        local idx = obj.actions_to_remove[i]
        local name = obj.action_names[idx]
        if name then obj[name] = nil end
        table.remove(obj.actions, idx)
        table.remove(obj.action_names, idx)
      end
      obj.actions_to_remove = nil
    end
    if obj.late_actions_to_remove then
      for i = #obj.late_actions_to_remove, 1, -1 do
        local idx = obj.late_actions_to_remove[i]
        local name = obj.late_action_names[idx]
        if name then obj[name] = nil end
        table.remove(obj.late_actions, idx)
        table.remove(obj.late_action_names, idx)
      end
      obj.late_actions_to_remove = nil
    end
  end

  -- Remove dead children
  for i = #objects, 1, -1 do
    local parent = objects[i]
    local j = 1
    while j <= #parent.children do
      local child = parent.children[j]
      if child.dead then
        -- Clean up child's outgoing links (remove from targets' linked_from)
        if child.links then
          for _, link in ipairs(child.links) do
            if link.target.linked_from then
              for k = #link.target.linked_from, 1, -1 do
                if link.target.linked_from[k].source == child then
                  table.remove(link.target.linked_from, k)
                end
              end
            end
          end
        end
        -- Clean up child's incoming links (remove from sources' links)
        if child.linked_from then
          for _, entry in ipairs(child.linked_from) do
            if entry.source.links then
              for k = #entry.source.links, 1, -1 do
                if entry.source.links[k].target == child then
                  table.remove(entry.source.links, k)
                end
              end
            end
          end
        end
        if child.destroy then child:destroy() end
        if child.name then parent[child.name] = nil end
        if parent.name then child[parent.name] = nil end
        child.parent = nil
        table.remove(parent.children, j)
      else
        j = j + 1
      end
    end
  end
end
