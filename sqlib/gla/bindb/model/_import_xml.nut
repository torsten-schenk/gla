local Walker = import("gla.xml.model.Walker")

const ROOT = 0
const PRIMITIVE = 1
const MAIN_DATABASE = 2
const AUX_DATABASE = 3
const SCHEME = 4
const STRUCT = 5
const CLASS = 6
const FIELD = 7
const VALUE = 8
const ARRAY = 9
const VECTOR = 10
const GROUP = 11
const ENUM = 12
const FLAG = 13
const INTEGER = 14

return {
	nodes = [
		{ classname = "Root" }
		{ classname = "Primitive" }
		{ classname = "MainDatabase" }
		{ classname = "AuxDatabase" }
		{ classname = "Scheme" }
		{ classname = "Struct" }
		{ classname = "Class" }
		{ classname = "Field" }
		{ classname = "Value" }
		{ classname = "Array" }
		{ classname = "Vector" }
		{ classname = "Group" }
		{ classname = "Enum" }
		{ classname = "Flag" }
		{ classname = "Integer" }
	]
	edges = [ {
			source = null
			target = ROOT
			type = Walker.DOCUMENT
		} {
			priority = -1
			source = ROOT
			target = PRIMITIVE
			type = Walker.CHILDREN
			element = "primitive"
		} {
			source = ROOT
			target = MAIN_DATABASE
			type = Walker.NULL
		} {
			source = MAIN_DATABASE
			target = SCHEME
			type = Walker.NULL
		} {
			source = SCHEME
			target = STRUCT
			type = Walker.CHILDREN
			element = "struct"
		} {
			source = SCHEME
			target = CLASS
			type = Walker.CHILDREN
			element = "class"
		} {
			source = CLASS
			target = STRUCT
			type = Walker.CHILDREN
			element = "struct"
		} {
			source = CLASS
			target = CLASS
			type = Walker.CHILDREN
			element = "class"
		} {
			source = STRUCT
			target = FIELD
			type = Walker.CHILDREN
		} {
			source = CLASS
			target = FIELD
			type = Walker.CHILDREN
		} {
			source = FIELD
			target = VALUE
			type = Walker.SWITCH
			element = "value"
		} {
			source = FIELD
			target = ARRAY
			type = Walker.SWITCH
			element = "array"
		} {
			source = FIELD
			target = VECTOR
			type = Walker.SWITCH
			element = "vector"
		} {
			source = SCHEME
			target = GROUP
			type = Walker.CHILDREN
			element = "group"
		} {
			source = CLASS
			target = GROUP
			type = Walker.CHILDREN
			element = "group"
		} {
			source = GROUP
			target = ENUM
			type = Walker.CHILDREN
			element = "enum"
		} {
			source = GROUP
			target = FLAG
			type = Walker.CHILDREN
			element = "flag"
		} {
			source = GROUP
			target = INTEGER
			type = Walker.CHILDREN
			element = "integer"
		} {
			priority = 1
			source = MAIN_DATABASE
			target = AUX_DATABASE
			type = Walker.CHILDREN
			element = "aux"
		} {
			source = AUX_DATABASE
			target = SCHEME
			type = Walker.NULL
		} ]
}

