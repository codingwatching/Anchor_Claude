-- Class system based on rxi/classic.
-- https://github.com/rxi/classic
--
-- Create a class:
--   animal = class:extend()
--   function animal:new(name) self.name = name end
--   a = animal('dog')
--
-- Inheritance:
--   dog = animal:extend()
--   function dog:new(name)
--     animal.new(self, name)
--     self.type = 'dog'
--   end

local next_id = 0

class = {}
class.__index = class

function class:new()
end

function class:extend()
  local cls = {}
  for k, v in pairs(self) do
    if k:find("__") == 1 then
      cls[k] = v
    end
  end
  cls.__index = cls
  cls.super = self
  setmetatable(cls, self)
  return cls
end

function class:implement(...)
  for _, cls in pairs({...}) do
    for k, v in pairs(cls) do
      if self[k] == nil and type(v) == "function" then
        self[k] = v
      end
    end
  end
end

function class:is_a(T)
  local mt = getmetatable(self)
  while mt do
    if mt == T then
      return true
    end
    mt = getmetatable(mt)
  end
  return false
end

function class:__call(...)
  local obj = setmetatable({}, self)
  next_id = next_id + 1
  obj.id = next_id
  obj:new(...)
  return obj
end
