local SimpleTable = import("gla.storage.SimpleTable")

local table = SimpleTable(2, 1)

//table.insert([ 1, 2 ], 4)
table[[1, 2]] <- 4
print("GET: " + table[[ 1, 2 ]])

table[[1, 2]] = 8
print("GET: " + table[[ 1, 2 ]])



