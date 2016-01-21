local StackTask = import("gla.util.StackTask")

local Node = class {
	walker = null
	reader = null
	context = null

	constructor(walker) {
		this.walker = walker
		this.reader = walker.reader
		this.context = walker.context
	}

	function object() {
		return context._object
	}

	function role() {
		return context._role
	}

	function relationship() {
		return context._relationship
	}

	function metaobject() {
		return reader.metanp(context._object)
	}

	function metarelationship() {
		return reader.metanp(context._relationship, context._graph)
	}

	enter = null
	run = null
	leave = null
}

return class extends StackTask {
	static NULL = 0
	static OBJ2REL = 1
	static REL2OBJ = 2
	static OBJ2OBJ = 3 //CAUTION! currently also iterates start object itself for each relationship, if no roles are set
	static EXPLODE = 4
	static DECOMPOSE = 5
	static ROOT2OBJ = 6
	static CURRENT_OBJ2OBJ = 7
//	static PROPERTIES = 7

	static ObjectNode = class extends Node {
		function property(name) {
			return reader.get(context._object, name)
		}

		function roleproperty(name) {
			return reader.get(context._role, name, context._graph)
		}
	}

	static RelationshipNode = class extends Node {
	}

	static GraphNode = class extends Node {
	}

	reader = null
	basecontext = null

	context = null //subclass access: read-only
	stackDepth = 0

	constructor(reader, basecontext = {}) {
		this.reader = reader
		this.basecontext = basecontext
	}

	init = null //function init()
	finish = null //function finish()

	function begin() {
		context = basecontext
		stackDepth = 0
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
		stackDepth++
	}

	function ascend() {
		context = context.getdelegate()
		stackDepth--
	}

	function edgeBegin(id) {
		local edge = this.getclass().getattributes(null).edges[id]
		if("select" in edge)
			context._graph <- edge.select(reader, context)
		switch(edge.type) {
			case OBJ2REL:
				context._it <- reader.relationshipsfor(context._graph, context._object, ("role" in edge ? edge.role : null))
				_skiprel(context._it, edge)
				if(context._it.atEnd())
					return false
				context._relationship <- context._it.relationship()
				context._role <- context._it.role()
				return true
			case REL2OBJ:
				context._it <- reader.objectsin(context._graph, context._relationship, ("role" in edge ? edge.role : null))
				_skipobj(context._it, edge)
				if(context._it.atEnd())
					return false
				context._role <- context._it.role()
				context._object <- context._it.object()
				return true
			case OBJ2OBJ:
				context._srcit <- reader.relationshipsfor(context._graph, context._object, ("sourcerole" in edge ? edge.sourcerole : null))
				context._tgtit <- null
				while(context._tgtit == null || context._tgtit.atEnd()) {
					if(context._tgtit != null)
						context._srcit.next()
					_skiprel(context._srcit, edge)
					if(context._srcit.atEnd())
						return false
					context._tgtit = reader.objectsin(context._graph, context._srcit.relationship(), ("targetrole" in edge ? edge.targetrole : null))
					_skipobj(context._tgtit, edge)
				}
				context._relationship <- context._srcit.relationship()
				context._role <- context._tgtit.role()
				context._object <- context._tgtit.object()
				return true
			case EXPLODE:
				context._graph <- reader.explosion(context._object)
				return context._graph != null
			case DECOMPOSE:
				assert(false) //TODO
			case ROOT2OBJ:
				context._it <- reader.objectsin(context._graph, 0, ("role" in edge ? edge.role : null))
				_skipobj(context._it, edge)
				if(context._it.atEnd())
					return false
				context._object <- context._it.object()
				context._role <- context._it.role()
				return true
			case CURRENT_OBJ2OBJ:
				if(("sourcerole" in edge) && !reader.meta.role[context._srcit.metarole()].is(edge.sourcerole))
					return false
				else if(("relationship" in edge) && !reader.metanp(context._srcit.relationship()).is(edge.relationship))
					return false
				else if(("targetrole" in edge) && !reader.meta.role[context._tgtit.metarole()].is(edge.targetrole))
					return false
				else if(("object" in edge) && !reader.metanp(context._tgtit.object()).is(edge.object))
					return false
				else
					return true
			case NULL:
				return true
			default:
				throw "Invalid edge type for edge " + id
		}
	}

	function edgeNext(id, iteration) {
		local edge = this.getclass().getattributes(null).edges[id]
		switch(edge.type) {
			case OBJ2REL:
				context._it.next()
				_skiprel(context._it, edge)
				if(context._it.atEnd())
					return false
				context._relationship = context._it.relationship()
				return true
			case REL2OBJ:
			case ROOT2OBJ:
				context._it.next()
				_skipobj(context._it, edge)
				if(context._it.atEnd())
					return false
				context._object = context._it.object()
				return true
			case OBJ2OBJ:
				context._tgtit.next()
				_skipobj(context._tgtit, edge)
				while(context._tgtit.atEnd()) {
					context._srcit.next()
					_skiprel(context._srcit, edge)
					if(context._srcit.atEnd())
						return false
					context._tgtit = reader.objectsin(context._graph, context._srcit.relationship(), ("targetrole" in edge ? edge.targetrole : null))
					_skipobj(context._tgtit, edge)
				}
				context._relationship = context._srcit.relationship()
				context._object = context._tgtit.object()
				return true
			case EXPLODE:
				return false
			case DECOMPOSE:
				assert(false) //TODO
			case NULL:
				return false
			case CURRENT_OBJ2OBJ:
				return false
			default:
				assert(false) //BUG indicator
		}
	}

	function nodeEnter(id) {
		local node = getclass().getattributes(null).nodes[id]
		if(node.classname in getclass())
			context._node <- this[node.classname](this)
		else
			context._node <- null
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

	function _skiprel(it, edge) {
		if("relationship" in edge) //skip realtionships of wrong type, including root relationship (which is of no type)
			while(!it.atEnd() && (it.relationship() == 0 || !reader.metanp(it.relationship(), context._graph).is(edge.relationship)))
				it.next()
	}

	function _skipobj(it, edge) {
		if("object" in edge) //skip objects of wrong type
			while(!it.atEnd() && !reader.metanp(it.object()).is(edge.object))
				it.next()
	}
}

