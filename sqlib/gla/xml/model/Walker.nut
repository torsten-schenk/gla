local StackTask = import("gla.util.StackTask")
local meta = import("gla.xml.model.meta")
local strlib = import("squirrel.stringlib")

local Node = class {
	walker = null
	reader = null
	context = null

	constructor(walker) {
		this.walker = walker
		this.reader = walker.reader
		this.context = walker.context
	}

	enter = null
	run = null
	leave = null
}

return class extends StackTask {
	static NULL = 0
	static DOCUMENT = 1
	static CHILDREN = 2
	static SWITCH = 3

	static Document = class extends Node {
	}

	static Element = class extends Node {
		attributes = null
		firsttext = null
		tagname = null

		constructor(walker) {
			Node.constructor.call(this, walker)
			local it
			attributes = {}
			tagname = reader.get(context._object, "name")
			it = reader.objectsin(context._graph, context._relationship, meta.role.Attribute)
			for(; !it.atEnd(); it.next())
				attributes[reader.get(it.object(), "name")] <- reader.get(it.object(), "value")
			it = reader.objectsin(context._graph, context._relationship, meta.role.FirstText)
			for(; !it.atEnd(); it.next()) {
				if(firsttext == null)
					firsttext = reader.get(it.object(), "text")
				else
					firsttext += reader.get(it.object(), "text")
			}
			if(firsttext != null) {
				firsttext = strlib.strip(firsttext)
				if(firsttext.len() == 0)
					firsttext = null
			}
		}

		function tag() {
			return tagname
		}

		function attribute(name) {
			if(name in attributes)
				return attributes[name]
			else
				return null
		}

		function text() {
			return firsttext
		}

		function object() { //return object id of current element
			return context._object
		}
	}

	reader = null
	basecontext = null

	context = null //subclass access: read-only
	depth = 0

	constructor(reader, basecontext = {}) {
		this.reader = reader
		this.basecontext = basecontext
	}

	init = null //function init()
	finish = null //function finish()

	function begin() {
		context = basecontext
		depth = 0
		if(init != null)
			return init()
		else
			return true
	}

	function end() {
		if(finish != null)
			finish()
		context = null
	}

	function descend() {
		local newContext = {}
		newContext.setdelegate(context)
		context = newContext
		depth++
	}

	function ascend() {
		context = context.getdelegate()
		depth--
	}

	function edgeBegin(id) {
		local edge = getclass().getattributes(null).edges[id]
//		print("EDGE: " + edge.source + "->" + edge.target + " " + (edge.type == CHILDREN ? edge.element : ""))
		switch(edge.type) {
			case DOCUMENT:
				context._object <- reader.get(0, "document")
				context._graph <- reader.explosion(context._object)
				context._relationship <- reader.obj2rel(context._object, meta.role.Parent, meta.relationship.Binding, context._graph)[1]
				return true
			case CHILDREN:
				context._it <- reader.objectsin(context._graph, context._relationship, meta.role.Child)
				if("element" in edge)
					while(!context._it.atEnd() && reader.get(context._it.object(), "name") != edge.element)
						context._it.next()
				if(context._it.atEnd())
					return false
				context._object <- context._it.object()
				context._relationship <- reader.obj2rel(context._object, meta.role.Parent, meta.relationship.Binding, context._graph)[1]
				return true
			case SWITCH:
				return context._node.tag() == edge.element
			case NULL:
				return true
			default:
				throw "Invalid edge type for edge " + id
		}
	}

	function edgeNext(id, iteration) {
		local edge = getclass().getattributes(null).edges[id]
		switch(edge.type) {
			case CHILDREN:
				context._it.next()
				if("element" in edge)
					while(!context._it.atEnd() && reader.get(context._it.object(), "name") != edge.element)
						context._it.next()
				if(context._it.atEnd())
					return false
				context._object = context._it.object()
				context._relationship <- reader.obj2rel(context._object, meta.role.Parent, meta.relationship.Binding, context._graph)[1]
				return true
			default:
				return false
		}
	}

	function nodeEnter(id) {
		local node = getclass().getattributes(null).nodes[id]
		if(node != null && (node.classname in getclass()))
			context._node <- this[node.classname](this)
		else
			context._node <- Element(this)
		if(context._node.enter != null)
			return context._node.enter()
		else
			return true
	}

	function nodeRun(id) {
		if(context._node.run != null)
			return context._node.run()
		else
			return true
	}

	function nodeLeave(id) {
		//TODO iterate all children again and check if all children have been iterated in edgeNext/edgeBegin, i.e. there are no elements which are not defined by walker definition; do this just if this functionality is requested
		if(context._node.leave != null)
			return context._node.leave()
		else
			return true
	}
}

