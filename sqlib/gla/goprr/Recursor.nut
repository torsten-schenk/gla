local StackTask = import("gla.util.StackTask")
local StackExecuter = import("gla.util.StackExecuter")

enum Action {
	Relationships,
	Objects
}

local edges = [ {
		source = null,
		target = 0,
		action = Action.Relationships
	}, {
		source = 0,
		target = 1,
		action = Action.Objects
	}, {
		source = 1,
		target = 0,
		action = Action.Relationships
	}
]

local Task = class extends StackTask </ edges = edges /> {
	executer = null
	context = null
	skip = false

	reader = null
	graph = null
	startobject = null
	sourcemetarole = null
	metarelationship = null
	targetmetarole = null

	constructor(reader, graph, startobject, sourcemetarole, targetmetarole, metarelationship) {
		this.reader = reader
		this.graph = graph
		this.startobject = startobject
		this.sourcemetarole = sourcemetarole
		this.metarelationship = metarelationship
		this.targetmetarole = targetmetarole
		executer = StackExecuter(this)
	}

	function begin() {
		context = {
			object = startobject
		}
	}

	function end() {
		context = null
	}

	function descend() {
		if(skip) {
			skip = false
			return false
		}
		local old = context
		context = {}
		context.setdelegate(old)
	}

	function ascend() {
		context = context.getdelegate()
	}

	function edgeBegin(edge) {
		switch(edges[edge].action) {
			case Action.Relationships:
				context.relit <- reader.relationshipsfor(graph, context.object, sourcemetarole)
				while(!context.relit.atEnd() && !reader.metanp(context.relit.relationship(), graph).is(metarelationship))
					context.relit.next()
				return !context.relit.atEnd()
			case Action.Objects:
				context.objit <- reader.objectsin(graph, context.relit.relationship(), targetmetarole)
				if(context.objit.atEnd())
					return false
				context.object <- context.objit.object()
				executer.suspend()
				return true
		}
	}

	function edgeNext(edge, iteration) {
		switch(edges[edge].action) {
			case Action.Relationships:
				context.relit.next()
				return !context.relit.atEnd()
			case Action.Objects:
				context.objit.next()
				if(context.objit.atEnd())
					return false
				context.object <- context.objit.object()
				executer.suspend()
				return true
		}
	}
}

return class {
	_task = null

	constructor(reader, graph, startobject, sourcemetarole, targetmetarole, metarelationship = null) {
		_task = Task(reader, graph, startobject, sourcemetarole, targetmetarole, metarelationship)
		_task.executer.execute()
	}

	function atEnd() {
		return _task.context == null
	}

	function next() {
		_task.executer.execute()
	}

	function target() {
		return _task.context.object
	}

	function source() {
		return _task.context.getdelegate().object
	}


	function sourcerole() {
		return _task.context.relit.role()
	}

	function targetrole() {
		return _task.context.objit.role()
	}

	function relationship() {
		return _task.context.relit.relationship()
	}

	//do not descend to target object
	function skip() {
		_task.skip = true
	}
}

