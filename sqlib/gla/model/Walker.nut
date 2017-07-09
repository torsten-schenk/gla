local StackExecuter = import("gla.util.StackExecuter")

local BaseEdge = import("gla.model.Edge")
local BaseNode = import("gla.model.Node")

return class extends import("gla.util.StackTask") {
	model = null
	nodedef = null
	basecontext = null
	c = null
	depth = 0

	init = null //function init()
	finish = null //function finish()

	constructor(options = null) {}

	function walk(model, basecontext, nodedef) {
		this.model = model
		this.nodedef = nodedef
		this.basecontext = basecontext
		StackExecuter(this).execute()
	}

	function begin() {
		c = {
			walkeredge = null
			walkernode = null
			edge = null
			node = null
		}
		if(basecontext != null)
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
			walkeredge = null
			walkernode = null
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
			c.walkeredge = Edge(model, null, c)
		else
			c.walkeredge = Edge(model, c.getdelegate().node, c)
		local cur = c.walkeredge.reset()
		switch(typeof(cur)) {
			case "null":
				return false
			case "instance":
				if(cur instanceof BaseEdge) {
					c.edge = cur
					c.node = null
					return true
				}
				else if(cur instanceof BaseNode) {
					c.edge = null
					c.node = cur
					return true
				}
				else
					assert(false)
			case "array":
				c.edge = cur[0]
				c.node = cur[1]
				assert(c.edge instanceof BaseEdge)
				assert(c.node instanceof BaseNode)
				return true
			default:
				assert(false)
		}
		if(cur == null)
			return false
		return c.node != null
	}

	function edgeNext(id, iteration) {
		local cur = c.walkeredge.next(iteration)
		switch(typeof(cur)) {
			case "null":
				return false
			case "instance":
				if(cur instanceof BaseEdge) {
					c.edge = cur
					c.node = null
					return true
				}
				else if(cur instanceof BaseNode) {
					c.edge = null
					c.node = cur
					return true
				}
				else
					assert(false)
			case "array":
				c.edge = cur[0]
				c.node = cur[1]
				assert(c.edge instanceof BaseEdge)
				assert(c.node instanceof BaseNode)
				return true
			default:
				assert(false)
		}
	}

	function edgeEnd(id) {
	}

	function nodeEnter(id) {
		local name = getclass().getattributes(null).nodes[id]
		if(name in nodedef) {
			local def = nodedef[name]
			switch(typeof(def)) {
				case "function":
					c.walkernode = null
					return true
				case "table":
					c.walkernode = null
					if("enter" in def)
						return def.enter(model, c.node, c)
					else
						return true
				case "class":
					c.walkernode = def(model, c.node, c)
					return c.walkernode.enter()
				default:
					throw "invalid node definition type for node '" + name + "': '" + typeof(def) + "'"
			}
		}
	}

	function nodeRun(id) {
		if(c.walkernode != null)
			return c.walkernode.run()

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
		if(c.walkernode != null)
			return c.walkernode.leave()

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

