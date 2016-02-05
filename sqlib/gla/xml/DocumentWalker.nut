enum Type {
	ELEMENT = 0x01
	TEXT = 0x02
	ATTRIBUTE = 0x04
}

return class extends import("gla.util.StackTask") {
	static Element = Type.ELEMENT
	static Text = Type.TEXT
	static Attribute = Type.ATTRIBUTE

	static Node = class {
		context = null

		constructor(walker) {
			context = walker.context
		}

		enter = null
		run = null
		leave = null

		function document() {
			return context._document
		}

		function name() {
			if(context.rawin("_attrit"))
				return context._attrit.name()
			else if(context._it.isElement())
				return context._it.name()
			else
				return null
		}

		function value() {
			if(context.rawin("_attrit"))
				return context._attrit.value()
			else
				return null
		}

		function text() {
			if(context.rawin("_it") && context._it.isText())
				return context._it.text()
			else
				return null
		}

		function id() {
			if(context.rawin("_it") && context._it.isElement())
				return context._it.child()
			else
				return null
		}
		
		function namespace() {
			if(context.rawin("_attrit"))
				return context._attrit.namespace()
			else if(context.rawin("_it") && context._it.isElement())
				return context._it.namespace()
			else
				return null

		}

		function attribute(name) {
			if(!isElement())
				return null
			return context._document.attribute(context._it.child(), name)
		}

		function attributes() {
			if(!isElement())
				return null
			return context._document.attributes(context._it.child())
		}

		function isAttribute() {
			return context.rawin("_attrit")
		}

		function isText() {
			return context.rawin("_it") && context._it.isText()
		}

		function isElement() {
			return context.rawin("_it") && context._it.isElement()
		}
	}

	basecontext = null
	document = null
	context = null //subclass access: read-only

	init = null //function init()
	finish = null //function finish()

	constructor(document) {
		this.document = document
		basecontext = {
			_document = document
		}
	}

	function begin() {
		context = basecontext
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
	}

	function ascend() {
		context = context.getdelegate()
	}

	function edgeBegin(id) {
		local edge = this.getclass().getattributes(null).edges[id]
		local parent = null
		local nameFilter = null
		local attributeFilter = null
		if("element" in edge)
			nameFilter = edge.element
		if("attributes" in edge)
			attributeFilter = edge.attributes
		if("_it" in context.getdelegate())
			parent = context.getdelegate()._it.child()

		context._iteration <- 0
		if((edge.type & Type.ATTRIBUTE) != 0) {
			context._attrit <- document.attributes(parent)
			if(context._attrit.atEnd())
				delete context._attrit
			else
				return true
		}

		if((edge.type & (Type.TEXT | Type.ELEMENT)) != 0) {
			context._it <- document.children(parent)
			_skip(context._it, edge.type, nameFilter, attributeFilter)
			if(context._it.atEnd())
				return false
			return true
		}
		return false
	}

	function edgeNext(id, iteration) {
		local edge = this.getclass().getattributes(null).edges[id]
		local nameFilter = null
		local attributeFilter = null
		if("element" in edge)
			nameFilter = edge.element
		if("attributes" in edge)
			attributeFilter = edge.attributes

		context._iteration = iteration
		if(context.rawin("_attrit")) {
			context._attrit.next()
			if(context._attrit.atEnd())
				delete context._attrit
			else
				return true
		}

		if(context.rawin("_it"))
			context._it.next()
		else {
			local parent = null
			if("_it" in context.getdelegate())
				parent = context.getdelegate()._it.child()
			context._it <- document.children(parent)
		}
		_skip(context._it, edge.type, nameFilter, attributeFilter)
		if(context._it.atEnd())
			return false
		return true
	}

	function nodeEnter(id) {
		local nodes = getclass().getattributes(null).nodes
		context._node <- null
		if(id < nodes.len()) {
			local node = getclass().getattributes(null).nodes[id]
			if(node.classname in getclass())
				context._node <- this[node.classname](this)
		}
		if(context._node != null && context._node.enter != null)
			return context._node.enter()
		else
			return true
	}

	function nodeRun(id) {
		if(context._node != null && context._node.run != null)
			return context._node.run()
		else
			return true
	}

	function nodeLeave(id) {
		//TODO iterate all children again and check if all children have been iterated in edgeNext/edgeBegin, i.e. there are no elements which are not defined by walker definition; do this just if this functionality is requested
		if(context._node != null && context._node.leave != null)
			return context._node.leave()
		else
			return true
	}

	function _skip(it, type, nameFilter, attributeFilter) {
		local skipped = true
		while(skipped && !it.atEnd()) {
			skipped = false
			if(it.isText() && (type & Type.TEXT) == 0) {
				it.next()
				skipped = true
			}
			else if(it.isElement()) {
				if((type & Type.ELEMENT) == 0) {
					it.next()
					skipped = true
				}
				else if(nameFilter != null && it.isElement() && it.name() != nameFilter) {
					it.next()
					skipped = true
				}
				else if(attributeFilter != null) {
					foreach(i, v in attributeFilter) {
						if(document.attribute(it.child(), i) != v) {
							it.next()
							skipped = true
							break
						}
					}
				}
			}
		}
	}
}

