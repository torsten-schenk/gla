local StackExecuter = import("gla.util.StackExecuter")

return class extends import("gla.util.StackTask") {
	static EdgeHandler = class {
		atEnd = null //function atEnd()
		next = null //function next(context)
		begin = null //function begin(context)
		end = null //function end(context)
	}

	static NodeHandler = class {
		enter = null
		leave = null
		run = null
	}

	edgehandlers = null
	nodehandlers = null
	basecontext = null
	init = null //function init()
	finish = null //function finish()

	context = null

	//used names in context: curedge, curnode
	constructor(edgehandlers, nodehandlers) {
		this.edgehandlers = edgehandlers
		this.nodehandlers = nodehandlers
	}

	function execute(basecontext = {}) {
		this.basecontext = basecontext
		StackExecuter(this).execute()
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
		local def = this.getclass().getattributes(null).edges[id]
		local handler
		if(("handler" in def) && def.handler != null) {
			handler = edgehandlers[def.handler]()
			if(handler.begin != null && !handler.begin(context))
				return false
			else if(handler.atEnd != null && handler.atEnd())
				return false
		}
		context.curedge <- handler
		return true
	}

	function edgeNext(id, iteration) {
		if(context.curedge == null)
			return false
		else if(context.curedge.atEnd == null)
			return false
		assert(context.curedge.next != null)
		context.curedge.next(context)
		return !context.curedge.atEnd()
	}

	function edgeEnd(id) {
		if(context.curedge != null && context.curedge.end != null)
			context.curedge.end(context)
	}

	function nodeEnter(id) {
		local def = this.getclass().getattributes(null).nodes[id]
		local handler
		if(def != null && ("handler" in def) && def.handler != null) {
			handler = nodehandlers[def.handler]()
			if(handler.enter = null && !handler.enter(context))
				return false
		}
		context.curnode <- handler
		return true
	}

	function nodeRun(id) {
		if(context.curnode != null && context.curnode.run != null)
			return context.curnode.run(context)
		else
			return true
	}

	function nodeLeave(id) {
		if(context.curnode != null && context.curnode.leave != null)
			return context.curnode.leave(context)
		else
			return true
	}
}

