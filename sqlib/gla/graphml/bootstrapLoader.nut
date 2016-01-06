local TgfParser = import("gla.tgf.CallbackParser")
local Printer = import("gla.textgen.Printer")
local Sink = import("gla.textgen.IOSink")
local strlib = import("squirrel.stringlib")

local regexPriority = strlib.regexp(@"\s*\[([+-]?[1-9][0-9]*)\]")
local regexElement = strlib.regexp(@"\s*([^\s:]+)\s*:\s*Walker")
local regexRoot = strlib.regexp(@"\s*/\s*")
local regexNode = strlib.regexp(@"\s*([^\s]+)\s*")

enum State {
	None, Node, Edge
}

local MyParser = class extends TgfParser {
	p = null
	state = null
	nodemap = null /* tgf node id -> sq node id (array index) */
	root = null
	curid = null

	constructor(printer) {
		p = printer
		state = State.None
		nodemap = {}
		curid = 0
	}

	function edge(source, target, label) {
		local cap
		local priority = 0
		local child = null

		if(!(source in nodemap))
			throw "Invalid edge: no such source node " + source
		else if(!(target in nodemap))
			throw "Invalid edge: no such target node " + target
		else if(nodemap[target] == null)
			throw "Invalid edge: must not have root node as target"

		if(state == State.Node) {
			p.upn(@"]")
			p.pni(@"edges = [")
			state = State.Edge
		}
		assert(state == State.Edge)
		cap = regexPriority.capture(label)
		if(cap != null) {
			priority = label.slice(cap[1].begin, cap[1].end).tointeger()
			label = label.slice(cap[0].end)
		}
		cap = regexElement.capture(label)
		if(cap != null)
			child = label.slice(cap[1].begin, cap[1].end)

		p.pni(@"{")
		if(nodemap[source] == null)
			p.pn(@"source = null")
		else
			p.pn(@"source = " + nodemap[source])
		p.pn(@"target = " + nodemap[target])
		if(priority != 0)
			p.pn(@"priority = " + priority)
		if(nodemap[source] == null)
			p.pn(@"type = Walker.DOCUMENT")
		else if(child == null)
			p.pn(@"type = Walker.NULL")
		else {
			p.pn(@"type = Walker.CHILDREN")
			p.pn(@"element = """ + child + @"""")
		}
		p.upn(@"}")
	}

	function node(tgfid, label) {
		local cap
		local name
		local sqid

		if(state == State.None) {
			p.pn(@"local Walker = import(""gla.xml.model.Walker"")")
			p.pn()
			p.pni(@"return {")
			p.pni(@"nodes = [")
			state = State.Node
		}
		assert(state == State.Node)

		if(tgfid in nodemap)
			throw "Duplicate node " + tgfid + " (label: '" + label + "'"
		if(regexRoot.match(label)) {
			if(root != null)
				throw "Duplicate root node: " + tgfid
			root = tgfid
			nodemap[tgfid] <- null
		}
		else {
			nodemap[tgfid] <- curid++
			cap = regexNode.capture(label)
			if(cap != null) {
				name = label.slice(cap[1].begin, cap[1].end)
				p.pn(@"{ classname = """ + name + @""" }")
			}
			else
				p.pn(@"null")
		}
	}

	function finish() {
		if(state != State.None) {
			p.upn(@"]")
			p.upn(@"}")
		}
	}
}

local printer = Printer()
local parser = MyParser(printer)
parser.parse("gla.graphml.Loader")
parser.finish()
printer.commit(Sink("<nut>tmp.asd"))

