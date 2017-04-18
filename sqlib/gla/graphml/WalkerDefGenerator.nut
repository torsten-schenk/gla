local Printer = import("gla.textgen.Printer")
local Loader = import("gla.graphml.Loader")
local strlib = import("squirrel.stringlib")

local regexEdge = strlib.regexp(@"^\s*(\[\s*[+-]?[1-9][0-9]*\s*\])?\s*([^\s]*)\s*$")
local regexNode = strlib.regexp(@"^\s*([^\s]*)\s*$")

local NodeParser = class {
	generator = null
	nodes = null
	byid = null

	constructor(context) {
		generator = context.generator
		nodes = context.nodes
		byid = context.byid
	}

	function parsed(data, graph) {
		switch(data.type) {
			case "shape:roundrectangle":
			case "shape:rectangle":
				local node = generator.newNode(data)
				node.id = nodes.len()
				byid[data.id] <- node.id
				nodes.push(node)
				break
			case "shape:ellipse":
				byid[data.id] <- -1
				break
			default:
				throw "unexpected node type: '" + data.type + "'"
		}
	}
}

local EdgeParser = class {
	generator = null
	nodes = null
	edges = null
	byid = null

	constructor(context) {
		generator = context.generator
		nodes = context.nodes
		edges = context.edges
		byid = context.byid
	}

	function parsed(data) {
		if(Loader.canonicalizeEdge(data, "none", "standard")) {
			local srcid = byid[data.source.id]
			local tgtid = byid[data.target.id]
			local source
			local target
			if(srcid == -1)
				source = null
			else
				source = nodes[srcid]
			if(tgtid == -1)
				throw "cannot link back to root node"
			else
				target = nodes[tgtid]
			local edge = generator.newEdge(source, target, data)
			edges.push(edge)
		}
		else
			throw "unexpected edge arrows: '" + data.source.type + "' -> '" + data.target.type + "'"
	}
}

local Generator = class {
	static Edge = class {
	}

	static Node = class {
		id = null
	}

	p = null

	function generate(entity) {
		p = {
			main = Printer()
			import = Printer()
			nodes = Printer()
			edges = Printer()
		}
		local context = {
			p = p
			generator = this
			nodes = []
			edges = []
			byid = {}
		}
		p.main.embed(p.import)
		p.main.pn()
		p.main.pni("return {")
		p.main.pni("nodes = [")
		p.main.embed(p.nodes)
		p.main.upn("]")
		p.main.pni("edges = [")
		p.main.embed(p.edges)
		p.main.upn("]")
		p.main.upn("}")
		Loader.simpleLoad(entity, NodeParser, EdgeParser, context)
		foreach(v in context.nodes)
			v.generate(p.nodes)
		foreach(v in context.edges)
			v.generate(p.edges)
	}
}

local DefaultNode = class extends Generator.Node {
	handler = null

	constructor(data) {
		if(data.nameLabel != null) {
			local cap = regexNode.capture(data.nameLabel)
			if(cap == null)
				throw "invalid node label: '" + data.nameLabel + "'"
			if(cap[1].begin < cap[1].end)
				handler = data.nameLabel.slice(cap[1].begin, cap[1].end)
		}
	}

	function generate(p) {
		if(handler == null)
			p.pn("null")
		else
			p.pn("{ handler = \"" + handler + "\" }")
	}
}

local DefaultEdge = class extends Generator.Edge {
	source = null
	target = null
	handler = null
	priority = 0

	constructor(source, target, data) {
		this.source = source
		this.target = target
		if(data.nameLabel != null) {
			local cap = regexEdge.capture(data.nameLabel)
			if(cap == null)
				throw "invalid edge label: '" + data.nameLabel + "'"
			if(cap[1].begin < cap[1].end)
				priority = strlib.strip(data.nameLabel.slice(cap[1].begin + 1, cap[1].end - 1)).tointeger()
			if(cap[2].begin < cap[2].end)
				handler = data.nameLabel.slice(cap[2].begin, cap[2].end)
		}
	}

	function generate(p) {
		p.pni("{")
		if(source == null)
			p.pn("source = null")
		else
			p.pn("source = " + source.id)
		p.pn("target = " + target.id)
		if(handler != null)
			p.pn("handler = \"" + handler + "\"")
		if(priority != 0)
			p.pn("priority = " + priority)
		p.upn("}")
	}
}

Generator.newNode <- function(data) {
	return DefaultNode(data)
}

Generator.newEdge <- function(source, target, data) {
	return DefaultEdge(source, target, data)
}

return Generator

