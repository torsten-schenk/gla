local Builder = import("gla.storage.memory.Builder")
local ColumnSpec = import("gla.storage.ColumnSpec")

local colspec = ColumnSpec({ name = "key", type = "integer"}, [{ name = "ValueA", type = "integer" }, { name = "ValueB", type = "integer" }])
local builder = Builder(colspec)

builder.multikey = true
builder.order = 5
print("ORDER: " + builder.order)
print("MULTIKEY: " + builder.multikey)

