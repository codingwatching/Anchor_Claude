tiled_map = class:class_new()
function tiled_map:_tiled_map(map_path) -- this is named _tiled_map as otherwise it collies with the an:tiled_map function.
  self.tags.tiled_map = true
  self.map_data = require(map_path)
  self.w, self.h = self.map_data.width, self.map_data.height
  self.tile_w, self.tile_h = self.map_data.tilewidth, self.map_data.tileheight
  return self
end

--[[
  Creates and returns vertices based on the map's given layer. x, y are offset values.
  If such a layer isn't defined then nothing will happen.
--]]
function tiled_map:tiled_map_get_layer_vertices(layer_name, x, y)
  local objects = {}
  local layer_objects = self:tiled_map_get_objects_from_layer(layer_name)
  for _, object in ipairs(layer_objects) do
    if object.shape == 'rectangle' then
      local vertices = math.to_rectangle_vertices(object.x, object.y, object.x + object.width, object.y + object.height)
      for i = 1, #vertices, 2 do
        vertices[i] = vertices[i] + (x or 0)
        vertices[i+1] = vertices[i+1] + (y or 0)
      end
      table.insert(objects, vertices)
    elseif object.shape == 'polygon' then
      local vertices = {}
      for i = 1, #object.polygon do
        table.insert(vertices, object.x + object.polygon[i].x + (x or 0))
        table.insert(vertices, object.y + object.polygon[i].y + (y or 0))
      end
      table.insert(objects, vertices)
    end
  end
  return objects
end

--[[
  Returns a table of objects from an object layer with the given name.
  If the layer isn't an object layer then the table returned will be empty.
  If type_string is defined, then it will only return objects that have their .type attribute as that string. x, y are offset values.
  Objects returned have the properties exactly as they are exported from Tiled. To generate the .lua exported file for a given map, choose "export as" and then ".lua".
--]]
function tiled_map:tiled_map_get_objects_from_layer(layer_name, type_string, x, y)
  local objects = {}
  for _, layer in ipairs(self.map_data.layers) do
    if layer.type == 'objectgroup' and layer.name == layer_name then
      for _, object in ipairs(layer.objects) do
        if not type_string or (type_string and object.type == type_string) then
          for k, v in pairs(object.properties) do object[k] = v end
          object.x = object.x + (x or 0)
          object.y = object.y + (y or 0)
          table.insert(objects, object)
        end
      end
    end
  end
  return objects
end
