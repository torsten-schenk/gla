local StackExecuter = import("gla.util.StackExecuter")

local WalkerEdge = class {
	constructor(model, context) {
	}


}

return class extends import("gla.util.StackTask") {
	model = null
	nodedef = null
	basecontext = null
	c = null
	depth = 0

	init = null //function init()
	finish = null //function finish()

	constructor(options = null) {}

	function walk(basectx, model, nodedef, basecontext = {}) {
		this.model = model
		this.nodedef = nodedef
		this.basecontext = basecontext
		StackExecuter(this).execute()
	}

	function begin() {
		c = {
			edge = null
			node = null
		}
		c.setdelegate(basecontext)

		if(init != null)
			return init()
		else
			return true
	}

	function end() {
		if(finish != null)
			finish()
		c = null
	}

	function descend() {
		local context = {
			node = null
			edge = null
		}
		context.setdelegate(c)
		c = context
		depth++
	}

	function ascend() {
		c = c.getdelegate()
		depth--
	}

	function edgeBegin(id) {
		local Edge = getclass().getattributes(null).edges[id]
		if(depth == 0)
			c.edge = Edge(model, null, c)
		else
			c.edge = Edge(model, c.getdelegate().node, c)
		c.node = c.edge.reset()
		return c.node != null
	}

	function edgeNext(id, iteration) {
		c.node = c.edge.next(iteration)
		return c.node != null
	}

	function edgeEnd(id) {
	}

	function nodeEnter(id) {
		local name = getclass().getattributes(null).nodes[id]
		if(name in nodedef) {
			local def = nodedef[name]
			switch(typeof(def)) {
				case "function":
					return true
				case "table":
					if("enter" in def)
						return def.enter(model, c.node, c)
					else
						return true
				default:
					throw "invalid node definition type for node '" + name + "': '" + typeof(def) + "'"
			}
		}
	}

	function nodeRun(id) {
		local name = getclass().getattributes(null).nodes[id]
		if(name in nodedef) {
			local def = nodedef[name]
			switch(typeof(def)) {
				case "function":
					return def(model, c.node, c)
				case "table":
					if("run" in def)
						return def.run(model, c.node, c)
					else
						return true
				default:
					assert(false)
			}
		}
	}

	function nodeLeave(id) {
		local name = getclass().getattributes(null).nodes[id]
		if(name in nodedef) {
			local def = nodedef[name]
			switch(typeof(def)) {
				case "function":
					return true
				case "table":
					if("leave" in def)
						return def.leave(model, c.node, c)
					else
						return true
				default:
					assert(false)
			}
		}
	}
}

