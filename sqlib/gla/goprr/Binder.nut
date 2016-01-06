local Base
Base = class {
	static function OnRequestNoRole(creator, graph, source, srcmetarole, metarelationship, tgtmetarole) {
		local result = Base()
		result.creator = creator
		result.graph = graph
		result.source = source
		result.srcmetarole = srcmetarole
		result.metarelationship = metarelationship
		result.tgtmetarole = tgtmetarole
		return result
	}

	static function ImmediateNoRole(creator, graph, source, srcmetarole, metarelationship, tgtmetarole, target) {
		local relationship = creator.new(metarelationship, graph)
		creator.bind(relationship, srcmetarole, 0, source, graph)
		creator.bind(relationship, tgtmetarole, 0, target, graph)
	}

	creator = null
	graph = null
	source = null
	srcmetarole = null
	metarelationship = null
	tgtmetarole = null
	relationship = null

	function bind(target) {
		if(relationship == null) {
			relationship = creator.new(metarelationship, graph)
			creator.bind(relationship, srcmetarole, 0, source, graph)
		}
		creator.bind(relationship, tgtmetarole, 0, target, graph)
	}
}

return Base

