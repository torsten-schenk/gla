local meta = import("gla.bindb.model.meta")
local Walker = import("gla.goprr.Walker")

const MAIN_DATABASE = 0
const AUX_DATABASE = 1
const GROUP = 2
const CLASS = 3
const STRUCT = 4
const GENERATE_CONSTANT = 5
const GENERATE_FIELD = 6
const GENERATE_SERIALIZATION = 7
const GENERATE_INITIALIZATION = 8
const SUPER_COMPOSED = 9

return {
	nodes = [
		{ classname = "MainDatabase" }
		{ classname = "AuxDatabase" }
		{ classname = "Group" }
		{ classname = "Class" }
		{ classname = "Struct" }
		{ classname = "GenerateConstant" }
		{ classname = "GenerateField" }
		{ classname = "GenerateSerialization" }
		{ classname = "GenerateInitialization" }
		{ classname = "SuperComposed" }
	]
	edges = [ {
		source = null
		target = MAIN_DATABASE
		select = @(reader, context) reader.get(0, "namespace")
		type = Walker.ROOT2OBJ
	} {
		source = MAIN_DATABASE
		target = AUX_DATABASE
		sourcerole = meta.role.Source
		relationship = meta.relationship.embeds
		targetrole = meta.role.Target
		type = Walker.OBJ2OBJ
	} {
		source = MAIN_DATABASE
		target = GROUP
		object = meta.object.Group
		type = Walker.OBJ2OBJ
	} {
		source = MAIN_DATABASE
		target = CLASS
		object = meta.object.Class
		type = Walker.OBJ2OBJ
	} {
		source = MAIN_DATABASE
		target = STRUCT
		object = meta.object.Struct
		type = Walker.OBJ2OBJ
	} {
		source = AUX_DATABASE
		target = GROUP
		object = meta.object.Group
		type = Walker.OBJ2OBJ
	} {
		source = AUX_DATABASE
		target = CLASS
		object = meta.object.Class
		type = Walker.OBJ2OBJ
	} {
		source = AUX_DATABASE
		target = STRUCT
		object = meta.object.Struct
		type = Walker.OBJ2OBJ
	} {
		source = CLASS
		target = CLASS
		sourcerole = meta.role.Source
		relationship = meta.relationship.nests
		targetrole = meta.role.Target
		object = meta.object.Class
		type = Walker.OBJ2OBJ
	} {
		source = CLASS
		target = STRUCT
		sourcerole = meta.role.Source
		object = meta.object.Struct
		type = Walker.OBJ2OBJ
	} {
		source = CLASS
		target = GROUP
		sourcerole = meta.role.Source
		object = meta.object.Group
		type = Walker.OBJ2OBJ
	} {
		source = GROUP
		target = GENERATE_CONSTANT
		select = @(reader, context) reader.get(0, "data")
		object = meta.object.Constant
		type = Walker.OBJ2OBJ
	} {
		priority = 1
		source = CLASS
		target = GENERATE_FIELD
		select = @(reader, context) reader.get(0, "data")
		object = meta.object.Field
		type = Walker.OBJ2OBJ
	} {
		source = CLASS
		target = SUPER_COMPOSED
		select = @(reader, context) reader.get(0, "inheritance")
		sourcerole = meta.role.Source
		targetrole = meta.role.Target
		type = Walker.OBJ2OBJ
	} {
		priority = 1
		source = STRUCT
		target = GENERATE_FIELD
		select = @(reader, context) reader.get(0, "data")
		object = meta.object.Field
		type = Walker.OBJ2OBJ
	} {
		source = STRUCT
		target = SUPER_COMPOSED
		select = @(reader, context) reader.get(0, "inheritance")
		sourcerole = meta.role.Source
		targetrole = meta.role.Target
		type = Walker.OBJ2OBJ
	} {
		source = SUPER_COMPOSED
		target = SUPER_COMPOSED
		sourcerole = meta.role.Source
		targetrole = meta.role.Target
		type = Walker.OBJ2OBJ
	} {
		priority = 1
		source = SUPER_COMPOSED
		target = GENERATE_FIELD
		select = @(reader, context) reader.get(0, "data")
		object = meta.object.Field
		type = Walker.OBJ2OBJ
	} {
		source = GENERATE_FIELD
		target = GENERATE_SERIALIZATION
		select = @(reader, context) reader.get(0, "types")
		object = meta.object.Type
		type = Walker.OBJ2OBJ
	} {
		source = GENERATE_FIELD
		target = GENERATE_INITIALIZATION
		select = @(reader, context) reader.get(0, "types")
		object = meta.object.Type
		type = Walker.OBJ2OBJ
	} ]
}

