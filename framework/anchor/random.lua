require('anchor.class')

--[[
  Random child object for seeded random number generation.

  Usage:
    self:add(random())           -- unseeded (uses os.time)
    self:add(random(12345))      -- seeded for deterministic replays

  Random is added as a child object. When the parent dies, the random dies automatically.
  Multiple random instances can exist (gameplay RNG vs cosmetic RNG).

  Random methods:
    float        - Random float in range
    int          - Random integer in range
    angle        - Random angle 0 to 2pi
    sign         - Random -1 or 1
    bool         - Random true/false
    normal       - Gaussian distribution
    choice       - Pick random element from array
    choices      - Pick n unique elements from array
    weighted     - Weighted index selection
    get_seed     - Get current seed
    set_seed     - Reset with new seed
]]
random = object:extend()

--[[
  Creates a new random number generator.

  Usage:
    self:add(random())           -- uses os.time as seed
    self:add(random(12345))      -- deterministic seed

  Parameters:
    seed - (optional) integer seed for deterministic sequences

  The random is automatically named 'random' and accessible as self.random on the parent.
]]
function random:new(seed)
  object.new(self, 'random')
  self.rng = random_create(seed or os.time())
end

--[[
  Returns a random float.

  Usage:
    self.random:float()          -- 0 to 1
    self.random:float(10)        -- 0 to 10
    self.random:float(5, 10)     -- 5 to 10

  Parameters:
    min - (optional) minimum value, or maximum if max not provided
    max - (optional) maximum value

  Returns: random float in the specified range
]]
function random:float(min, max)
  if max == nil then
    if min == nil then
      return random_float_01(self.rng)
    else
      return random_float(0, min, self.rng)
    end
  else
    return random_float(min, max, self.rng)
  end
end

--[[
  Returns a random integer (inclusive).

  Usage:
    self.random:int(10)          -- 1 to 10
    self.random:int(5, 10)       -- 5 to 10

  Parameters:
    min - minimum value, or maximum (with min=1) if max not provided
    max - (optional) maximum value

  Returns: random integer in [min, max]
]]
function random:int(min, max)
  if max == nil then
    return random_int(1, min, self.rng)
  else
    return random_int(min, max, self.rng)
  end
end

--[[
  Returns a random angle in radians.

  Usage:
    self.random:angle()

  Returns: random float in [0, 2pi]
]]
function random:angle()
  return random_angle(self.rng)
end

--[[
  Returns -1 or 1 randomly.

  Usage:
    self.random:sign()           -- 50% chance each
    self.random:sign(75)         -- 75% chance of 1

  Parameters:
    chance - (optional) percentage chance of returning 1 (default 50)

  Returns: -1 or 1
]]
function random:sign(chance)
  return random_sign(chance, self.rng)
end

--[[
  Returns true or false randomly.

  Usage:
    self.random:bool()           -- 50% chance true
    self.random:bool(10)         -- 10% chance true

  Parameters:
    chance - (optional) percentage chance of returning true (default 50)

  Returns: true or false
]]
function random:bool(chance)
  return random_bool(chance, self.rng)
end

--[[
  Returns a normally distributed random number.

  Usage:
    self.random:normal()             -- mean 0, stddev 1
    self.random:normal(100, 15)      -- mean 100, stddev 15

  Parameters:
    mean   - (optional) center of distribution (default 0)
    stddev - (optional) standard deviation (default 1)

  Returns: random float from gaussian distribution
]]
function random:normal(mean, stddev)
  return random_normal(mean, stddev, self.rng)
end

--[[
  Returns a random element from an array.

  Usage:
    enemy = self.random:choice(enemies)
    color = self.random:choice({'red', 'green', 'blue'})

  Parameters:
    array - array to pick from

  Returns: random element from the array
]]
function random:choice(array)
  return random_choice(array, self.rng)
end

--[[
  Returns n unique random elements from an array.

  Usage:
    items = self.random:choices(loot_table, 3)

  Parameters:
    array - array to pick from
    n     - number of elements to pick

  Returns: array of n unique elements
]]
function random:choices(array, n)
  return random_choices(array, n, self.rng)
end

--[[
  Returns an index based on weighted probabilities.

  Usage:
    weights = {1, 2, 7}  -- 10%, 20%, 70%
    index = self.random:weighted(weights)

  Parameters:
    weights - array of numeric weights

  Returns: 1-based index into the weights array
]]
function random:weighted(weights)
  return random_weighted(weights, self.rng)
end

--[[
  Returns the current seed.

  Usage:
    seed = self.random:get_seed()

  Returns: the seed used to initialize this RNG
]]
function random:get_seed()
  return random_get_seed(self.rng)
end

--[[
  Resets the RNG with a new seed.

  Usage:
    self.random:set_seed(12345)

  Parameters:
    seed - new seed value
]]
function random:set_seed(seed)
  random_seed(seed, self.rng)
end
