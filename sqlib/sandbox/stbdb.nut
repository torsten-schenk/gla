local Database = import("gla.storage.bdb.Database")
local Builder = import("gla.storage.bdb.Builder")
local ColumnSpec = import("gla.storage.ColumnSpec")

local create = false
local database = null
local table = null

if(create)
	database = Database({ package = "tmp", mode = "ct" })
else
	database = Database({ package = "tmp", mode = "" })

local exists = database.exists("mytable")
print("exists: " + exists)

//local colspec = ColumnSpec("i'key'|i'ValueA'i'ValueB'")
//local colspec = ColumnSpec("i|ii")
local colspec = ColumnSpec({ name = "key", type = "integer"}, [{ name = "ValueA", type = "integer" }, { name = "ValueB", type = "integer" }])
local builder = Builder(colspec)

if(create) {
	table = database.open("mytable", "ct", builder, 0)
	table.insert(3, [ 5, 9 ])
	table.insert(2, [ 5, 9 ])
}
else
	table = database.open("mytable", "", null, 0)
print(table.len() + " " + table.value(2)[0] + " " + table.value(2)[1])

print("END")

