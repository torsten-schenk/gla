local cbridge = import("gla.yaml._cbridge")

return class {
	function parse(entity) {
		cbridge.cbparser.parse(this, entity)
	}

	//function streamStart()
	//function streamEnd()
	//function versionDirective(major, minor)
	//function tagDirective(handle, prefix)
	//function documentStart()
	//function documentEnd()
	//function blockSequenceStart()
	//function blockMappingStart()
	//function blockEnd()
	//function flowSequenceStart()
	//function flowSequenceEnd()
	//function flowMappingStart()
	//function flowMappingEnd()
	//function blockEntry()
	//function flowEntry()
	//function key()
	//function value()
	//function alias()
	//function anchor()
	//function tag()
	//function scalar()
}

