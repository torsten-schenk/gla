local Object = import("gla.goprr.meta.Object")
local Relationship = import("gla.goprr.meta.Relationship")
local Role = import("gla.goprr.meta.Role")
local Graph = import("gla.goprr.meta.Graph")
local Binding = import("gla.goprr.meta.Binding")
local Model = import("gla.goprr.meta.Model")
local ReferenceProperty = import("gla.goprr.meta.ReferenceProperty")
local StringProperty = import("gla.goprr.meta.StringProperty")

local model = Model()
model.version = 1
local binding

model.add(Object("Element"))
model.add(Object("Document", model.object.Element))
model.add(Object("Attribute")) //TODO constraint: Unique, i.e. each two attributes differ in at least one property
model.add(Object("Text")) //TODO constraint: Unique, i.e. each two texts differ in at least one property

model.object.Document.add(StringProperty("entity"))
model.object.Element.add(StringProperty("name"))
model.object.Attribute.add(StringProperty("name"))
model.object.Attribute.add(StringProperty("value"))
model.object.Text.add(StringProperty("text"))

model.add(Role("Parent"))
model.add(Role("Child"))
model.add(Role("Attribute"))
model.add(Role("Text"))
model.add(Role("FirstText", model.role.Text))
model.add(Role("FollowingText", model.role.Text))

model.add(Relationship("Binding"))

model.add(Graph("Structure"))

model.graph.Structure.root.addout(model.role.Child, model.object.Document, 1, 1)

binding = Binding(model.relationship.Binding)
binding.addin(model.role.Parent, model.object.Element, 1, 1)
binding.addout(model.role.Child, model.object.Element, 0, -1)
binding.addout(model.role.Attribute, model.object.Attribute, 0, -1)
binding.addout(model.role.FirstText, model.object.Text, 0, -1)
binding.addout(model.role.FollowingText, model.object.Text, 0, -1)
model.graph.Structure.add(binding)

model.object.Document.explosion = model.graph.Structure

model.add(ReferenceProperty("document", model.object.Document))

model.finish()

return model

