local Object = import("gla.goprr.meta.Object")
local Relationship = import("gla.goprr.meta.Relationship")
local Role = import("gla.goprr.meta.Role")
local Graph = import("gla.goprr.meta.Graph")
local Binding = import("gla.goprr.meta.Binding")
local Model = import("gla.goprr.meta.Model")
local ReferenceProperty = import("gla.goprr.meta.ReferenceProperty")
local StringProperty = import("gla.goprr.meta.StringProperty")
local IntegerProperty = import("gla.goprr.meta.IntegerProperty")

local model = Model()
local binding

model.add(Object("Database"))
model.add(Object("MainDatabase", model.object.Database))
model.add(Object("AuxDatabase", model.object.Database))
model.add(Object("Type"))
model.add(Object("Primitive", model.object.Type))
model.add(Object("Composed", model.object.Type))
model.add(Object("Struct", model.object.Composed))
model.add(Object("Class", model.object.Composed))
model.add(Object("Field")) //constraint: Unique, i.e. each two fields differ in at least one property
model.add(Object("Value", model.object.Field))
model.add(Object("Array", model.object.Field))
model.add(Object("Vector", model.object.Field))
model.add(Object("Group"))
model.add(Object("Constant"))
model.add(Object("Enum", model.object.Constant))
model.add(Object("Flag", model.object.Constant))
model.add(Object("Integer", model.object.Constant))

model.object.AuxDatabase.add(StringProperty("name"))
model.object.Type.add(StringProperty("name"))
model.object.Class.add(IntegerProperty("typeid"))
model.object.Field.add(StringProperty("name"))
model.object.Field.add(StringProperty("default"))
model.object.Array.add(IntegerProperty("size"))
model.object.Group.add(StringProperty("name"))
model.object.Constant.add(StringProperty("name"))
model.object.Flag.add(IntegerProperty("bit"))
model.object.Enum.add(IntegerProperty("value"))
model.object.Integer.add(IntegerProperty("value"))

model.add(Role("Source"))
model.add(Role("Target"))

model.add(Relationship("embeds"))
model.add(Relationship("nests"))
model.add(Relationship("inherits"))
model.add(Relationship("owns"))
model.add(Relationship("consists"))
model.add(Relationship("defines"))
model.add(Relationship("assigns"))

model.add(Graph("Namespace")) //one per model
model.add(Graph("Inheritance")) //one per model
model.add(Graph("Types")) //one per model
model.add(Graph("Data")) //one per model

model.graph.Namespace.root.addout(model.role.Target, model.object.MainDatabase, 0, -1)

binding = Binding(model.relationship.embeds)
binding.addin(model.role.Source, model.object.MainDatabase, 1, 1)
binding.addout(model.role.Target, model.object.AuxDatabase, 1, -1)
model.graph.Namespace.add(binding)

binding = Binding(model.relationship.owns)
binding.addin(model.role.Source, model.object.Database, 1, 1)
binding.addout(model.role.Target, [ model.object.Composed, model.object.Group ], 1, -1)
model.graph.Namespace.add(binding)

binding = Binding(model.relationship.nests)
binding.addin(model.role.Source, model.object.Class, 1, 1)
binding.addout(model.role.Target, [ model.object.Composed, model.object.Group ], 1, -1)
model.graph.Namespace.add(binding)

binding = Binding(model.relationship.inherits)
binding.addin(model.role.Source, model.object.Composed, 1, 1)
binding.addout(model.role.Target, model.object.Composed, 1, 1)
model.graph.Inheritance.add(binding)

binding = Binding(model.relationship.consists)
binding.addin(model.role.Source, model.object.Composed, 1, 1)
binding.addout(model.role.Target, model.object.Field, 1, 1)
model.graph.Data.add(binding)

binding = Binding(model.relationship.defines)
binding.addin(model.role.Source, model.object.Group, 1, 1)
binding.addout(model.role.Target, model.object.Constant, 1, 1)
model.graph.Data.add(binding)

binding = Binding(model.relationship.assigns)
binding.addin(model.role.Source, model.object.Field, 1, 1)
binding.addout(model.role.Target, model.object.Type, 1, 1)
model.graph.Types.add(binding)

model.add(ReferenceProperty("namespace", model.graph.Namespace))
model.add(ReferenceProperty("data", model.graph.Data))
model.add(ReferenceProperty("types", model.graph.Types))
model.add(ReferenceProperty("inheritance", model.graph.Inheritance))

model.finish()

return model

