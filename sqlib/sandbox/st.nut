local Database = import("gla.storage.memory.Database")
local Builder = import("gla.storage.memory.Builder")
local ColumnSpec = import("gla.storage.ColumnSpec")

local database = Database()

local colspec = ColumnSpec({ name = "key", type = "integer"}, [{ name = "ValueA", type = "integer" }, { name = "ValueB", type = "integer" }])
local builder = Builder(colspec)

local table = database.open("mytable", "c", builder)
print("HERE")
table.insert(2, [ 5, 9 ])
print(table.len() + " " + table.value(2)[0] + " " + table.value(2)[1])

print("END")

