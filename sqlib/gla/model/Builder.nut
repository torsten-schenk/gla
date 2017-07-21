local BaseMeta = import("gla.model.Meta")

return class {
	curmeta = null
	curmodel = null
	curpackage = null

	constructor(verbose = null) {
	}

	function reset() {
		curmeta = null
		curmodel = null
		curpackage = null
	}

	function create(package, source, type = "default", options = null) {
		local meta = BaseMeta.get(package)
		local creator = meta.creator(type, options)
		if(creator == null)
			throw "no creator '" + type + "' found for package '" + package + "'"
		local model = meta.new()
		creator.create(source, model)
		curmeta = meta
		curmodel = model
		curpackage = package
		return this
	}

	function parse(package, source, type = "default", options = null) {
		local meta = BaseMeta.get(package)
		local parser = meta.parser(type, options)
		if(parser == null)
			throw "no parser '" + type + "' found for package '" + package + "'"
		local model = meta.new()
		parser.parse(source, model)
		curmeta = meta
		curmodel = model
		curpackage = package
		return this
	}

	function convert(package, options = null) {
		local tgtmeta = BaseMeta.get(package)
		local converter = curmeta.converterto(package, options)
		if(converter == null)
			throw "no converter from package '" + curpackage + "' to package '" + package + "' found"
		local model = tgtmeta.new()
		converter.convert(curmodel, model)
		curmeta = tgtmeta
		curmodel = model
		curpackage = package
		return this
	}

	function generate(target, type = "default", options = null) {
		if(curmeta == null)
			throw "cannot generate: must load a model first"
		local generator = curmeta.generator(type, options)
		if(generator == null)
			throw "no generator '" + type + "' found for package '" + curpackage + "'"
		generator.generate(curmodel, target)
		return this
	}
}

