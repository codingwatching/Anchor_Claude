--[[
  Module that implements UI behavior.
  Each UI behavior is independent and there is no central system or idea that connects them.
  A button has its own attributes and methods and ways of working, while a scrollbar, or a frame, or anything else, will have other properties.
  For now this is this way because I just want something that works, and I can try to generalize it into something better later.
--]]
ui = class:class_new()

--[[
  Creates a grid centered on position x, y, with size w, h, and with i, j columns and rows, respectively.
  The size of each cell in the grid is calculated automatically, and the function returns a grid object (see grid.lua file).
  Inside each cell, there's a rectangle object with the properties .x, .y (rectangle center), .x1, .y1 (rectangle left-top), .x2, .y2 (rectangle right-bottom) and .w, .h (rectangle size).
  Example:
    TODO
--]]
function ui:ui_grid(x, y, w, h, i, j)
  local grid = object():grid(i, j)
  local x1, y1 = x - w/2, y - h/2
  local x2, y2 = x + w/2, y + h/2
  local cell_w, cell_h = w/i, h/j
  grid:grid_set_dimensions(x, y, cell_w, cell_h)
  for k, l, v in grid:grid_pairs() do
    v = object()
    v.x1 = x1 + (k-1)*cell_w
    v.y1 = y1 + (l-1)*cell_h
    v.x2 = x1 + k*cell_w
    v.y2 = y1 + l*cell_h
    v.x = x1 + (k-1)*cell_w + cell_w/2
    v.y = y1 + (l-1)*cell_h + cell_h/2
    v.w = cell_w
    v.h = cell_h
    grid:grid_set(k, l, v)
  end
  return grid
end

--[[
  Same as ui_grid except x, y are the top-left positions of the grid instead of its center.
--]]
function ui:ui_grid_lt(x, y, w, h, i, j)
  local grid = object():grid(i, j)
  local x1, y1 = x, y
  local x2, y2 = x + w, y + h
  local cell_w, cell_h = w/i, h/j
  grid:grid_set_dimensions(x1 + w/2, y1 + h/2, cell_w, cell_h)
  for k, l, v in grid:grid_pairs() do
    v = object()
    v.x1 = x1 + (k-1)*cell_w
    v.y1 = y1 + (l-1)*cell_h
    v.x2 = x1 + k*cell_w
    v.y2 = y1 + l*cell_h
    v.x = x1 + (k-1)*cell_w + cell_w/2
    v.y = y1 + (l-1)*cell_h + cell_h/2
    v.w = cell_w
    v.h = cell_h
    grid:grid_set(k, l, v)
  end
  return grid
end

--[[
  Creates a text button centered on position x, y, with size w, h, that when clicked will call the given click action.
  The button is returned as a normal engine object with properties .x, .y (button's center), .x1, .y1 (button's left-top), .x2, .y2 (button's right-bottom) and .w, .h (button's size).
  The user is responsible for taking the returned button object and attaching an action to it so that it is drawn.
--]]
function ui:ui_button(x, y, w, h, click_action)
  return object():build(function(self)
    self.x, self.y = x, y
    self.w, self.h = w, h
    self.x1, self.y1 = self.x - self.w/2, self.y - self.h/2
    self.x2, self.y2 = self.x + self.w/2, self.y + self.h/2
    self:mouse_hover()
    self.click_action = click_action
  end):action(function(self, dt)
    if self.mouse_active and an:is_pressed('1') then
      self:click_action()
    end
  end)
end

--[[
  Same as ui_button except x, y are the top-left positions of the grid instead of its center.
--]]
function ui:ui_button_lt(x, y, w, h, click_action)
  return object():build(function(self)
    self.x, self.y = x + w/2, y + h/2
    self.w, self.h = w, h
    self.x1, self.y1 = self.x, self.y
    self.x2, self.y2 = self.x + self.w, self.y + self.h
    self:mouse_hover()
    self.click_action = click_action
  end):action(function(self, dt)
    if self.mouse_active and an:is_pressed('1') then
      self:click_action()
    end
  end)
end
