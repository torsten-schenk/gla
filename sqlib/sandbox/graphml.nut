local Loader = import("gla.graphml.Loader")
local ModelParser = import("gla.xml.model.Parser")
local ModelReader = import("gla.goprr.storage.ModelReader")
local Database = import("gla.storage.memory.Database")
local StackExecuter = import("gla.util.StackExecuter")

local db = Database()
local parser = ModelParser(db)
parser.trim = false
parser.parse("<graphml>sandbox.pop")

local NodeParser = class {
	function parsed(node, graph) {
		rt.dump.value(node)
	}
}

local EdgeParser = class {
	function parsed(edge) {
		rt.dump.value(edge)
	}
}

local loader = Loader(ModelReader(db), NodeParser, EdgeParser)
StackExecuter(loader).execute()

