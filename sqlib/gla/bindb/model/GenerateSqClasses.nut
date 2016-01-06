local meta = import("gla.bindb.model.meta")
local Printer = import("gla.textgen.Printer")
local Recursor = import("gla.goprr.Recursor")
local Generator = import("gla.goprr.Generator")
local Walker = import("gla.goprr.Walker")
local walkerdefNS = import("gla.bindb.model._generator_sqclasses_ns")
local SimpleTable = import("gla.storage.SimpleTable")

local primitives = {
	u8 = {
		serialize = @(pr, field) pr.pn("io.write(" + field + ", \"u8\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = 0") : pr.pn(field + " = " + value)
	}
	s16 = {
		serialize = @(pr, field) pr.pn("io.write(" + field + ", \"s16\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = 0") : pr.pn(field + " = " + value)
	}
	u16 = {
		serialize = @(pr, field) pr.pn("io.write(" + field + ", \"u16\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = 0") : pr.pn(field + " = " + value)
	}
	s32 = {
		serialize = @(pr, field) pr.pn("io.write(" + field + ", \"s32\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = 0") : pr.pn(field + " = " + value)
	}
	u32 = {
		serialize = @(pr, field) pr.pn("io.write(" + field + ", \"u32\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = 0") : pr.pn(field + " = " + value)
	}
	f64 = {
		serialize = @(pr, field) pr.pn("io.write(" + field + ", \"f64\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = 0") : pr.pn(field + " = " + value)
	}
	bool = {
		serialize = @(pr, field) pr.pn("io.write(" + field + " ? 1 : 0, \"u8\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = false") : pr.pn(field + " = " + value)
	}
	char = {
		serialize = @(pr, field) pr.pn("io.write(" + field + ", \"u8\")")
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = '\0'") : pr.pn(field + " = '" + value + "'")
	}
	string = {
		serialize = function(pr, field) {
			pr.pni("if(" + field + " == null)")
			pr.pnu("io.write(0, \"u32\")")
			pr.pni("else {")
			pr.pn("io.write(" + field + ".len(), \"u32\")")
			pr.pn("io.write(" + field + ", \"string\")")
			pr.upn("}")
		}
		initialize = @(pr, field, value) value == null ? pr.pn(field + " = \"\"") : pr.pn(field + " = \"" + value + "\"")
	}
}

local initialization = SimpleTable(2, 1)
initialization[[meta.object.Value, meta.object.Primitive]] <- function(pr, fieldnode, typenode) {
	local deflt = fieldnode.property("default")
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	primitives[typename].initialize(pr, fieldname, deflt)
}
initialization[[meta.object.Value, meta.object.Class]] <- function(pr, fieldnode, typenode) {
}
initialization[[meta.object.Value, meta.object.Struct]] <- function(pr, fieldnode, typenode) {
	local fieldname = fieldnode.property("name")
	pr.pn(fieldname + " = Struct" + typenode.object() + "()")
}
initialization[[meta.object.Vector, meta.object.Primitive]] <- function(pr, fieldnode, typenode) {
	local fieldname = fieldnode.property("name")
	pr.pn(fieldname + " = []")
}
initialization[[meta.object.Vector, meta.object.Class]] <- function(pr, fieldnode, typenode) {
	local fieldname = fieldnode.property("name")
	pr.pn(fieldname + " = []")
}
initialization[[meta.object.Vector, meta.object.Struct]] <- function(pr, fieldnode, typenode) {
	local fieldname = fieldnode.property("name")
	pr.pn(fieldname + " = []")
}
initialization[[meta.object.Array, meta.object.Primitive]] <- function(pr, fieldnode, typenode) {
	local deflt = fieldnode.property("default")
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	local size = fieldnode.property("size")
	pr.pn(fieldname + " = ::array(" + size + ")")
	pr.pni("for(local _i = 0; _i < " + size + "; _i++)")
	primitives[typename].initialize(pr, fieldname + "[_i]", deflt)
	pr.unindent()
}
initialization[[meta.object.Array, meta.object.Class]] <- function(pr, fieldnode, typenode) {
}
initialization[[meta.object.Array, meta.object.Struct]] <- function(pr, fieldnode, typenode) {
	local fieldname = fieldnode.property("name")
	local size = fieldnode.property("size")
	pr.pn(fieldname + " = ::array(" + size + ")")
	pr.pni("for(local _i = 0; _i < " + size + "; _i++)")
	pr.pnu(fieldname + "[_i] = Struct" + typenode.object() + "()")
}

local serialization = SimpleTable(2, 1)
serialization[[meta.object.Value, meta.object.Primitive]] <- function(pr, fieldnode, typenode) {
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	primitives[typename].serialize(pr, fieldname)
}
serialization[[meta.object.Value, meta.object.Class]] <- function(pr, fieldnode, typenode) {
	local fieldname = fieldnode.property("name")
	pr.pni("if(" + fieldname + " == null)")
	pr.pnu("io.write(0, \"u32\")")
	pr.pni("else {")
	pr.pn("io.write(" + fieldname + "._typeid, \"u32\")")
	pr.pn("io.write(" + fieldname + ")")
	pr.upn("}")
}
serialization[[meta.object.Value, meta.object.Struct]] <- function(pr, fieldnode, typenode) {
	local fieldname = fieldnode.property("name")
	pr.pn("io.write(" + fieldname + ")")
}
serialization[[meta.object.Vector, meta.object.Primitive]] <- function(pr, fieldnode, typenode) {
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	pr.pn("io.write(" + fieldname + ".len(), \"u32\")")
	pr.pni("for(local _i = 0; _i < " + fieldname + ".len(); _i++)")
	primitives[typename].serialize(pr, fieldname + "[_i]")
	pr.unindent()
}
serialization[[meta.object.Vector, meta.object.Class]] <- function(pr, fieldnode, typenode) {
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	pr.pn("io.write(" + fieldname + ".len(), \"u32\")")
	pr.pni("for(local _i = 0; _i < " + fieldname + ".len(); _i++)")
	pr.pni("if(" + fieldname + "[_i] == null)")
	pr.pnu("io.write(0, \"u32\")")
	pr.pni("else {")
	pr.pn("io.write(" + fieldname + "[_i]._typeid, \"u32\")")
	pr.pn("io.write(" + fieldname + "[_i])")
	pr.upn("}")
	pr.unindent()
}
serialization[[meta.object.Vector, meta.object.Struct]] <- function(pr, fieldnode, typenode) {
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	pr.pn("io.write(" + fieldname + ".len(), \"u32\")")
	pr.pni("for(local _i = 0; _i < " + fieldname + ".len(); _i++)")
	pr.pnu("io.write(" + fieldname + "[_i])")
}
serialization[[meta.object.Array, meta.object.Primitive]] <- function(pr, fieldnode, typenode) {
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	local size = fieldnode.property("size")
	pr.pni("for(local _i = 0; _i < " + size + "; _i++)")
	primitives[typename].serialize(pr, fieldname + "[_i]")
	pr.unindent()
}
serialization[[meta.object.Array, meta.object.Class]] <- function(pr, fieldnode, typenode) {
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	local size = fieldnode.property("size")
	pr.pni("for(local _i = 0; _i < " + size + "; _i++)")
	pr.pni("if(" + fieldname + "[_i] == null)")
	pr.pnu("io.write(0, \"u32\")")
	pr.pni("else {")
	pr.pn("io.write(" + fieldname + "[_i]._typeid, \"u32\")")
	pr.pn("io.write(" + fielname + "[_i])")
	pr.upn("}")
	pr.unindent()
}
serialization[[meta.object.Array, meta.object.Struct]] <- function(pr, fieldnode, typenode) {
	local typename = typenode.property("name")
	local fieldname = fieldnode.property("name")
	local size = fieldnode.property("size")
	pr.pni("for(local _i = 0; _i < " + size + "; _i++)")
	pr.pnu("io.write(" + fieldname + "[_i])")
}

return class extends Walker </ nodes = walkerdefNS.nodes, edges = walkerdefNS.edges /> {
	constructor(reader) {
		Walker.constructor.call(this, reader)
	}

	//generate a single main database with given printer and quirks
	function single(printer, quirksentity = null) {
		basecontext.printer <- printer
		basecontext.quirksentity <- quirksentity
		if(quirksentity != null)
			basecontext.quirks <- import(quirksentity)
		else
			basecontext.quirks <- null
		basecontext.done <- false
	}

	MainDatabase = class extends Walker.ObjectNode {
		function enter() {
			if(context.done)
				throw "Multiple main databases in model but prepared for just a single one"
			else if(context.printer == null)
				throw "Missing configuration, initialize generator before usage"
			context.pproto <- Printer()
			context.pgroup <- Printer()
			context.pstruct <- Printer()
			context.pclass <- Printer()
			context.pregister <- Printer()
			context.pnamespace <- Printer()
			if(context.quirksentity != null)
				context.printer.pn("local _quirks = import(\"" + context.quirksentity + "\")")
			context.printer.embed(context.pproto)
			context.printer.embed(context.pgroup)
			context.printer.embed(context.pstruct)
			context.printer.embed(context.pclass)
			context.printer.pni("return {")
			context.printer.pni("register = {")
			context.printer.embed(context.pregister)
			context.printer.upn("}")
			context.printer.pni("namespace = {")
			context.printer.embed(context.pnamespace)
			context.printer.upn("}")
			context.printer.upn("}")
			context.namespace <- ":"
		}
		function leave() {
			context.done = true
		}
	}

	AuxDatabase = class extends Walker.ObjectNode {
		function enter() {
			local pnamespace = Printer()
			context.namespace <- property("name") + ":"
			context.pnamespace.pni("\"" + property("name") + "\" : {")
			context.pnamespace.embed(pnamespace)
			context.pnamespace.upn("}")
			context.pnamespace <- pnamespace
		}
		function leave() {
			delete context.pnamespace
		}
	}

	Group = class extends Walker.ObjectNode {
		function enter() {
			switch(metarelationship()) {
				case meta.relationship.owns:
					context.namespace <- context.namespace + property("name")
					context.pnamespace.pn("\"" + property("name") + "\" : Group" + object())
					break
				case meta.relationship.nests:
					context.namespace <- context.namespace + "." + property("name")
					break
				default:
					assert(false) //model inconsistent
			}
			context.pconstant <- Printer()
			context.pregister.pn("\"" + context.namespace + "\" : Group" + object())
			context.pgroup.pni("local Group" + object() + " = { // " + context.namespace)
			context.pgroup.embed(context.pconstant)
			context.pgroup.upn("}")
		}
		function leave() {
			delete context.namespace
		}
	}

	Class = class extends Walker.ObjectNode {
		function enter() {
			switch(metarelationship()) {
				case meta.relationship.owns:
					context.namespace <- context.namespace + property("name")
					context.pnamespace.pn("\"" + property("name") + "\" : Class" + object())
					break
				case meta.relationship.nests:
					context.pnesting.pn("Class" + context.composed.object() + ".newmember(\"" + property("name") + "\", Class" + object() + ", null, true)")
					context.namespace <- context.namespace + "." + property("name")
					break
				default:
					assert(false) //model inconsistent
			}
			context.composed <- this
			context.pfield <- Printer()
			context.pinit <- Printer()
			context.pserialize <- Printer()
			context.pnesting <- Printer()
			context.pregister.pn("\"" + context.namespace + "\" : Class" + object())
			if(context.quirks != null && ("super" in context.quirks) && (context.namespace in context.quirks.super))
				context.pproto.pn("local Class" + object() + " = class extends _quirks.super[\"" + context.namespace + "\"] {}")
			else
				context.pproto.pn("local Class" + object() + " = class {}")
			if(!property("abstract"))
				context.pclass.pn("Class" + object() + ".newmember(\"_typeid\", " + property("typeid") + ", null, true)")
			context.pclass.embed(context.pnesting)
			context.pclass.embed(context.pfield)
			context.pclass.pni("Class" + object() + ".newmember(\"constructor\", function() {")
			context.pclass.embed(context.pinit)
			if(context.quirks != null && ("super" in context.quirks) && (context.namespace in context.quirks.super) && ("constructor" in context.quirks.super[context.namespace]))
				context.pclass.pn("_quirks.super[\"" + context.namespace + "\"].constructor.call(this)")
			context.pclass.upn("})")
			context.pclass.pni("Class" + object() + ".newmember(\"_serialize\", function(io) {")
			context.pclass.embed(context.pserialize)
			context.pclass.upn("})")
		}
		function leave() {
			delete context.composed
			delete context.namespace
		}
	}

	Struct = class extends Walker.ObjectNode {
		function enter() {
			switch(metarelationship()) {
				case meta.relationship.owns:
					context.namespace <- context.namespace + property("name")
					context.pnamespace.pn("\"" + property("name") + "\" : Struct" + object())
					break
				case meta.relationship.nests:
					context.pnesting.pn("Class" + context.composed.object() + ".newmember(\"" + property("name") + "\", Struct" + object() + ", null, true)")
					context.namespace <- context.namespace + "." + property("name")
					break
				default:
					assert(false) //model inconsistent
			}
			context.composed <- this
			context.pfield <- Printer()
			context.pinit <- Printer()
			context.pserialize <- Printer()
			context.pregister.pn("\"" + context.namespace + "\" : Struct" + object())
			if(context.quirks != null && ("super" in context.quirks) && (context.namespace in context.quirks.super))
				context.pproto.pn("local Struct" + object() + " = class extends _quirks.super[\"" + context.namespace + "\"] {}")
			else
				context.pproto.pn("local Struct" + object() + " = class {}")
			context.pstruct.embed(context.pfield)
			context.pclass.pni("Struct" + object() + ".newmember(\"constructor\", function() {")
			context.pclass.embed(context.pinit)
			if(context.quirks != null && ("super" in context.quirks) && (context.namespace in context.quirks.super) && ("constructor" in context.quirks.super[context.namespace]))
				context.pclass.pn("_quirks.super[\"" + context.namespace + "\"].constructor.call(this)")
			context.pclass.upn("})")
			context.pclass.pni("Struct" + object() + ".newmember(\"_serialize\", function(io) {")
			context.pclass.embed(context.pserialize)
			context.pclass.upn("})")
		}
		function leave() {
			delete context.namespace
			delete context.composed
		}
	}

	GenerateConstant = class extends Walker.ObjectNode {
		function run() {
			switch(metaobject()) {
				case meta.object.Integer:
				case meta.object.Enum:
					context.pconstant.pn("\"" + property("name") + "\" : " + property("value"))
					break
				case meta.object.Flag:
					context.pconstant.pn("\"" + property("name") + "\" : " + (1 << property("bit")))
					break
			}
		}
	}

	GenerateField = class extends Walker.ObjectNode {
		function run() {
			local name = property("name")
			context.pfield.pn(context.composed.metaobject().name + context.composed.object() + ".newmember(\"" + name + "\", null)")
			context.field <- this
		}
	}

	GenerateSerialization = class extends Walker.ObjectNode {
		function run() {
			serialization[[context.field.metaobject(), metaobject()]](context.pserialize, context.field, this)
		}
	}

	GenerateInitialization = class extends Walker.ObjectNode {
		function run() {
			initialization[[context.field.metaobject(), metaobject()]](context.pinit, context.field, this)
		}
	}
}

