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

model.add(Object("NonProperty"))
model.add(Object("Global", model.object.NonProperty))
model.add(Object("Local", model.object.NonProperty))
model.add(Object("Object", model.object.Global))
model.add(Object("Graph", model.object.Global))
model.add(Object("Role", model.object.Local))
model.add(Object("Relationship", model.object.Local))

model.add(Graph("Globals")) //one per model
model.add(Graph("Locals")) //one per graph

model.object.Graph.explosion = model.graph.Locals

model.finish()

return model

