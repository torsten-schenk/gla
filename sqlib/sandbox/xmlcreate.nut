local ModelParser = import("gla.xml.ModelParser")
local Database = import("gla.storage.bdb.Database")

local database = Database({ package = "tmp", mode = "ct" })
local parser = ModelParser(database)
parser.trim = true
//parser.parse("<graphml>sandbox.pop")
//parser.parse("sandbox.Document")
parser.parse("net.eyeeg.ngsu.DataScheme")
//parser.parse("sandbox.DataScheme")

//return

local table = database.open("global", "r")
local it = table.begin()
local end = table.end()

print(table.valueAt(1))
print("ROOT: " + table.value([ 1, 1, "entity"]))

print("LEN: " + table.len() + " " + end.index)

/*for(local i = 0; i < table.len(); i++)
	print("IS: " + table.rowAt(i)[0] + " " + table.rowAt(i)[1] + " " + table.rowAt(i)[2] + " -> " + table.rowAt(i)[3])
return*/

while(it.index < end.index) {
	try {
		print("IS: " + it.key()[0] + " " + it.key()[1] + " " + it.key()[2] + " -> " + it.value())
		it.next()
	}
	catch(e) {
		print("Error: " + e + "  " + it.index)
		return
	}
}

print("")

table = database.open("local_2", "r")
it = table.begin()
end = table.end()
print("LEN: " + table.len())
while(it.index < end.index) {
	print("IS: " + it.key()[0] + " " + it.key()[1] + " " + it.key()[2] + " -> " + it.value())
	it.next()
}

table = database.open("bindo_2", "r")
it = table.begin()
end = table.end()
print("LEN: " + table.len())
while(it.index < end.index) {
	print("IS: " + it.key()[0] + " " + it.key()[1] + " -> " + it.value()[0] + " " + it.value()[1])
	it.next()
}

table = database.open("bindr_2", "r")
it = table.begin()
end = table.end()
print("LEN: " + table.len())
while(it.index < end.index) {
	print("IS: " + it.key()[0] + " " + it.key()[1] + " -> " + it.value()[0] + " " + it.value()[1])
	it.next()
}

print("END")

