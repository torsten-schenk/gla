local ModelReader = import("gla.goprr.storage.ModelReader")
local ModelCreator = import("gla.goprr.storage.ModelCreator")
local Database = import("gla.storage.memory.Database")
local Recursor = import("gla.goprr.Recursor")

local Object = import("gla.goprr.meta.Object")
local Relationship = import("gla.goprr.meta.Relationship")
local Role = import("gla.goprr.meta.Role")
local Graph = import("gla.goprr.meta.Graph")
local Binding = import("gla.goprr.meta.Binding")
local Model = import("gla.goprr.meta.Model")
local StringProperty = import("gla.goprr.meta.StringProperty")

local meta = Model()
local binding
meta.add(Object("Object"))
meta.object.Object.add(StringProperty("description"))
meta.add(Role("Parent"))
meta.add(Role("Child"))
meta.add(Relationship("Government"))
meta.add(Graph("Tree"))
binding = Binding(meta.relationship.Government)
binding.add(meta.role.Parent, 1, 1, meta.object.Object)
binding.add(meta.role.Child, 0, -1, meta.object.Object)
meta.graph.Tree.add(binding)

local db = Database()
local creator = ModelCreator(db, meta)
local reader = ModelReader(db, meta)

local graph = creator.new(meta.graph.Tree)
local root = creator.new(meta.object.Object)
creator.set(root, "description", "Root")

local relationship
local child
local parent
relationship = creator.new(meta.relationship.Government, graph)
creator.bind(relationship, meta.role.Parent, 0, root, graph)

child = creator.new(meta.object.Object)
creator.set(child, "description", "First")
creator.bind(relationship, meta.role.Child, 0, child, graph)

child = creator.new(meta.object.Object)
creator.set(child, "description", "Second")
creator.bind(relationship, meta.role.Child, 0, child, graph)

relationship = creator.new(meta.relationship.Government, graph)
creator.bind(relationship, meta.role.Parent, 0, child, graph)
child = creator.new(meta.object.Object)
creator.set(child, "description", "Third")
creator.bind(relationship, meta.role.Child, 0, child, graph)

creator.bind(1, meta.role.Child, 0, creator.new(meta.object.Object), graph)

relationship = creator.new(meta.relationship.Government, graph)
creator.bind(relationship, meta.role.Parent, 0, root, graph)

child = creator.new(meta.object.Object)
creator.set(child, "description", "Fourth")
creator.bind(relationship, meta.role.Child, 0, child, graph)

for(local rec = Recursor(reader, graph, root, meta.role.Parent, meta.role.Child, meta.relationship.Government); !rec.atEnd(); rec.next()) {
	if(rec.target() == 4)
		rec.skip()
	print("HAS: " + rec.atEnd() + " " + rec.source() + " " + rec.target() + " " + rec.relationship())
}

