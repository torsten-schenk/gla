local BdbTable = import("gla.storage.bdb.Table")
local BdbDatabase = import("gla.storage.bdb.Database")

local database = BdbDatabase("/tmp/db")
local table = database.createTable({ name = "mytable", kcols = { name = "Key", type = "integer", sort = false }, vcols = [{ name = "ValueA", type = "integer" }, { name = "ValueB", type = "integer" }], multikey = true })
//local table = BdbTable({ kcols = 1, vcols = 2, multikey = true })

print("SIZE: " + table.len())
table.insert(2, [2, 3])
table.insert(3, [2, 3])
table.insert(1, [2, 3])
table.insert(1, [20, 30])
print("SIZE: " + table.len())

return

dump.value(table.value(1))
table.putCellAt(0, 0, 4)
table.putCell(2, 1, 5)
dump.value(table.rowAt(0))
dump.value(table.keyAt(0))
dump.value(table.valueAt(0))

local it = table.lower(0)
dump.value(it.value())
it.next()
dump.value(it.value())
it.next()
dump.value(it.value())
it.next()
it.prev()
dump.value(it.value())
it.prev()
dump.value(it.value())
it.prev()
dump.value(it.value())


dump.value(table.at(2).value())

print("FORWARD ITERATION:")
for(local it = table.begin(); it < table.end(); it.next()) {
	print("AT: " + it.index)
	dump.value(it.value())
	dump.value(it.key())
}

print("BACKWARD ITERATION:")
for(local it = table.end(); it > table.begin();) {
	it.prev()
	print("AT: " + it.index)
	dump.value(it.value())
	dump.value(it.key())
}

table.remove(1)
print("SIZE NOW: " + table.len())

print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++")

local table = BdbTable(database, { kcols = [{ name = "KeyA", type = "integer", sort = false }, { name = "KeyB", type = "integer", sort = false }], vcols = { name = "Value", type = "integer" }, multikey = true })

table.insert([1, 1], 0)
table.insert([1, 2], 1)
table.insert([2, 1], 2)
table.insert([2, 2], 3)
table.insert([2, 3], 4)
table.insert([3, 1], 5)

print("FORWARD ITERATION:")
for(local it = table.begin(); it < table.end(); it.next()) {
	print("AT: " + it.index)
	dump.value(it.key())
	dump.value(it.value())
}

it = table.lower(2)
print("LOWER: " + it.index)
it = table.upper(2)
print("UPPER: " + it.index)

