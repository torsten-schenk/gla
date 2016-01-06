local Database = import("gla.storage.memory.Database")
local Builder = import("gla.storage.memory.Builder")
local ColumnSpec = import("gla.storage.ColumnSpec")
local Super = import("gla.storage.memory.Table")

local db = Database()
local n = 0

return function(nkey, nvalue) {
	local colspec = ColumnSpec(nkey, nvalue)
	local builder = Builder(colspec)
	local table = db.open("table_" + n, "ct", builder, 0)
	n++
	return table
}

