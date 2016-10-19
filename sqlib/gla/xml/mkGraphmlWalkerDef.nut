local GraphmlLoader = import("gla.graphml.Loader")
local Printer = import("gla.textgen.Printer")
local IOSink = import("gla.textgen.IOSink")

if(rt.arg.len() != 2)
	throw "Invalid arguments: expected <graphml entity> <output entity>"

local nodes = []
local edges = []
local root = null

local byid = {}
local byname = {}

local Node = class {
	id = null
	index = null
	name = null

	constructor(data) {
		name = data.nameLabel
		id = data.id
		byid[id] <- this
		if(name != null) {
			if(name in byname)
				throw "multiple nodes named '" + name + "'"
			byname[name] <- this
		}
		nodes.push(this)
	}

	function generate(pr) {
		if(name != null)
			pr.pn("{ classname = \"" + name + "\" }")
		else
			pr.pn("/* null */")
	}
}

local ElementNode = class extends Node {
}

local TextNode = class extends Node {
}

local Edge = class {
	source = null
	target = null
	constraints = null

	constructor(data) {
		if(GraphmlLoader.canonicalizeEdge(data, "none", "standard")) {
			if(data.source.id == root)
				source = null
			else
				source = byid[data.source.id]
			if(data.target.id == root)
				throw "looping back to root node not allowed"
			else
				target = byid[data.target.id]
			if(target instanceof ElementNode) {
				if(data.nameLabel != null) //check, if expression at edge is valid squirrel code
					eval("return { " + data.nameLabel + " }")
				constraints = data.nameLabel
			}
			else if(target instanceof TextNode) {
			}
			else
				assert(false)
		}
		else
			throw "unsupported edge with arrows '" + data.source.type + "' and '" + data.target.type + "' encountered"
		edges.push(this)
	}

	function generate(pr) {
		pr.pni("{")
		if(source == null)
			pr.pn("source = null")
		else
			pr.pn("source = " + source.index)
		pr.pn("target = " + target.index)
		if(target instanceof ElementNode)
			pr.pn("type = Walker.Element")
		else if(target instanceof TextNode)
			pr.pn("type = Walker.Text")
		else
			assert(false)
		if(constraints != null)
			pr.pt(constraints, "  ")
		pr.upn("}")
	}
}

local NodeParser = class {
	function parsed(data, graph) {
		local node
		switch(data.type) {
			case "shape:hexagon":
				node = ElementNode(data)
				break
			case "shape:rectangle":
				node = TextNode(data)
				break
			case "shape:ellipse":
				if(root != null)
					throw "multiple root nodes encountered"
				root = data.id
				break
			default:
				throw "unexpected node type '" + data.type + "'"
		}
	}
}

local EdgeParser = class {
	function parsed(data) {
		local edge = Edge(data)
	}
}

GraphmlLoader.simpleLoad(rt.arg[0], NodeParser, EdgeParser, null, false)

if(root == null)
	throw "missing root node"

local pr = Printer()

//null named nodes to back
local cmp = function(a, b) {
	if(a.name == null && b.name == null)
		return 0
	else if(a.name == null)
		return 1
	else if(b.name == null)
		return -1
	else
		return a.name <=> b.name
}
nodes.sort(cmp)
for(local i = 0; i < nodes.len(); i++)
	nodes[i].index = i

pr.pn(@"local Walker = import(""gla.xml.DocumentWalker"")")
pr.pni(@"return {")
pr.pn("Super = Walker")
pr.pni("nodes = [")
foreach(n in nodes)
	n.generate(pr)
pr.upn("]")
pr.pni("edges = [")
foreach(e in edges)
	e.generate(pr)
pr.upn("]")
pr.upn("}")
pr.commit(IOSink(rt.arg[1]))

