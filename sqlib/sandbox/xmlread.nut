local ModelCreator = import("gla.goprr.storage.ModelCreator")
local ModelReader = import("gla.goprr.storage.ModelReader")
local Database = import("gla.storage.bdb.Database")

local database = Database({ package = "tmp", mode = "r" })
local reader = ModelReader(database)


/*local document = reader.get(0, "document")
print("ROOT: " + document + " " + reader.get(document, "entity"))
print("TYPE: " + reader.meta(document).name)
local graph = reader.explosion(document)
print("GRAPH: " + graph + " " + reader.meta(graph).name)

local root = reader.get(graph, "root")
print("ROOT: " + root)

local binding = reader.obj2rel(root, meta.role.Parent, meta.relationship.Binding, graph)[1]

print("RELATIONSHIP: " + binding + " " + reader.meta(binding, graph).name)

local it = reader.iterateRO(binding, meta.role.Child, graph)
print("HAS: " + it.total() + " CHILDREN")
while(!it.atEnd()) {
	print("  CHILD " + it.at() + ": " + it.role() + " " + it.object())
	it.next()
}

it = reader.iterateRR(root, meta.role.Parent, graph)
print("HAS: " + it.total() + " BINDINGS")
while(!it.atEnd()) {
	print("  BINDING " + it.at() + ": " + it.role() + " " + it.relationship())
	it.next()
}

it = reader.objects()
while(!it.atEnd()) {
	print("  OBJECT " + it.at() + ": " + it.id() + " " + it.meta().name + "   " + it.explosion())
	it.next()
}*/

database = Database({ package = "tmp.bindb", mode = "ct" })

local XmlImporter = import("gla.bindb.model.XmlImporter")
local creator = ModelCreator(database)
local converter = XmlImporter(reader, creator)
local StackExecuter = import("gla.util.StackExecuter")

StackExecuter(converter).execute()

print("END")


