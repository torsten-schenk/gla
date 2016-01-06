local SaxParser = import("gla.xml.SaxParser")
local ModelCreator = import("gla.goprr.storage.ModelCreator")
local meta = import("gla.xml.model.meta")
local stringlib = import("squirrel.stringlib")

local Element = class {
	id = 0
	binding = 0
	followingTexts = 0
	firstTexts = 0

	constructor(id) {
		this.id = id
	}
}

return class extends SaxParser {
	stack = null
	creator = null
	graph = null
	document = null

	followingTextElement = null

	//parameters, external RW access (mind type)
	trim = false
	ignoreNS = false

	constructor(targetdb) {
		this.stack = []
		this.creator = ModelCreator(targetdb, meta)
	}

	function parse(entity) {
		document = Element(creator.new(meta.object.Document))
		graph = creator.new(meta.graph.Structure)
		creator.set(document.id, "entity", entity)
		creator.explode(document.id, graph)
		creator.set(0, "document", document.id)
		SaxParser.parse.call(this, entity)
		graph = null
	}

	function begin(ns, name) {
		local element;
		if(stack.len() > 0) {
			element = Element(creator.new(meta.object.Element))
			creator.bind(stack.top().binding, meta.role.Child, 0, element.id, graph)
		}
		else {
			element = document
			creator.bind(0, meta.role.Child, 0, element.id, graph)
		}
		creator.set(element.id, "name", name)
		element.binding = creator.new(meta.relationship.Binding, graph)
		creator.bind(element.binding, meta.role.Parent, 0, element.id, graph)
		stack.push(element)
		followingTextElement = null
	}

	function end(ns, name) {
		followingTextElement = stack.pop()
	}

	function text(text) {
		if(trim)
			text = stringlib.lstrip(stringlib.rstrip(text))
		if(text.len() == 0)
			return
		local parent;
		local metarole;
		local id = creator.new(meta.object.Text)
		creator.set(id, "text", text)
		if(followingTextElement == null) {
			parent = stack.top()
			metarole = meta.role.FirstText
		}
		else {
			parent = followingTextElement
			metarole = meta.role.FollowingText
		}
		creator.bind(parent.binding, metarole, 0, id, graph)
	}

	function attribute(ns, key, value) {
		if(key == null)
			return
		local object = creator.new(meta.object.Attribute)
		creator.set(object, "name", key)
		creator.set(object, "value", value)
		creator.bind(stack.top().binding, meta.role.Attribute, 0, object, graph)
	}
}

