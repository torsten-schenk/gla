local Database = import("gla.bdb.Database")
local Table = import("gla.bdb.Table")
local List = import("gla.bdb.List")

local db = Database.create("tmp.db")

//local table = Table.create(db, "mytable", 2, 2)
local list = List.create(db, "mylist", 2)

list.append(0x11002200, "A long String")
/*list.append(0x66778899, "A long String")*/
local value = list.get(0)
dump.value(value)
print("VALUE IS: " + 0x11002200)
//list.flush()

list.close()

/*table.close()

db.close()*/

print("DONE")
