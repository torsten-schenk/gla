local CsvParser = import("gla.csv.CallbackParser")
local PathUtil = import("gla.io.PathUtil")
local strlib = import("squirrel.stringlib")

enum Mode {
	Edge,
	Node
}

local regexEdge = strlib.regexp(@"^([0-9]+)\s+([0-9]+)(\s+.*)?$")
local regexEmpty = strlib.regexp(@"^\s*$")
local regexSeparator = strlib.regexp(@"^\s*#\s*$")
local regexNode = strlib.regexp(@"^([0-9]+)(\s+.*)?$")

local MyParser = class extends CsvParser {
	self = null
	mode = null

	constructor(self) {
		this.self = self
		delim = '\0'
		quote = '\0'
		mode = Mode.Node
	}

	function entry(index, string) {
		local cap
		local source
		local target
		local node
		local label
		switch(mode) {
			case Mode.Edge:
				cap = regexEdge.capture(string)
				if(cap == null) {
					if(regexEmpty.match(string))
						return true
					else
						throw "Invalid line while parsing edges: '" + string + "'"
				}
				source = string.slice(cap[1].begin, cap[1].end).tointeger()
				target = string.slice(cap[2].begin, cap[2].end).tointeger()
				label = strlib.strip(string.slice(cap[3].begin, cap[3].end))
				return self.edge(source, target, label)
			case Mode.Node:
				cap = regexNode.capture(string)
				if(cap == null) {
					if(regexSeparator.match(string)) {
						mode = Mode.Edge
						return true
					}
					else if(regexEmpty.match(string))
						return true
					else
						throw "Invalid line while parsing nodes: '" + string + "'"
				}
				node = string.slice(cap[1].begin, cap[1].end).tointeger()
				label = strlib.strip(string.slice(cap[2].begin, cap[2].end))
				return self.node(node, label)
		}
	}
}

return class {
	function parse(entity) {
		local parser = MyParser(this)
		parser.parse(PathUtil.canonicalizeEntity(entity, "tgf"))
	}

	function edge(source, target, label) {
		return true
	}

	function node(id, label) {
		return true
	}
}

