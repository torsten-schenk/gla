local StackTask = import("gla.util.StackTask")
local meta = import("gla.bindb.model.meta")

local Node = class {
}

local ComposedNode = class extends Node {
}

local FieldNode = class extends Node {
	function fields() { //only for Arrays
	}
}

return class extends StackTask {
	static MAIN_DATABASE_SCHEME = 1
	static ANCHORED_CLASSES = 2
	static ANCHORED_STRUCTS = 3
	static EMBEDDED_CLASSES = 4
	static EMBEDDED_STRUCTS = 5
	static FIELDS = 6
	static TYPE_TO_ASSIGNEES = 7
	static FIELD_TO_OWNER = 8
}

