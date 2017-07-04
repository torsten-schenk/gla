local strlib = import("squirrel.stringlib")
local SimpleTable = import("gla.storage.SimpleTable")
local BaseParser = import("gla.model.Parser")
local BaseMeta = import("gla.model.Meta")

local cvbyfrom = SimpleTable(2, 1) //[ fromPackage toPackage ] -> [ converter ]
local cvbyto = SimpleTable(2, 1) //[ toPackage fromPackage ] -> [ converter ]
local parsers = SimpleTable(2, 1) //[ package name ] -> [ parser ]
local importers = SimpleTable(2, 1) //[ package name ] -> [ importer ]
local generators = SimpleTable(2, 1) //[ package name ] -> [ generator ]
local exporters = SimpleTable(2, 1) //[ package name ] -> [ exporter ]

local regexParse = strlib.regexp(@"parse:(.*)")
local regexImport = strlib.regexp(@"import:([a-zA-Z_0-9\.]+)->(a-zA-Z0-9\.]+)")

local configured = {} //package -> Meta instance

local configure = function(package) {
	if(!(package in configured)) {
		local meta = import(package + ".meta")
		assert(meta instanceof BaseMeta)
		configured[package] <- meta
	}
/*		return
	local config = import(package + "._model")	
	if("parser" in config)
		foreach(i, v in config.parser)
			parsers.insert([ package i ], package + "." + v)
	if("importer" in config)
		foreach(i, v in config.parser)
			importers.insert([ package i ], package + "." + v)
	if("generator" in config)
		foreach(i, v in config.generator)
			generators.insert([ package i ], package + "." + v)
	if("exporter" in config)
		foreach(i, v in config.parser)
			exporters.insert([ package i ], package + "." + v)
	configured[package] <- import(package + ".meta")*/
}

return class {
	curmodel = null
	curpackage = null

	constructor() {
	}

/*	function parser(package, options = null, type = "default") {
		configure(package)
		local entity = parsers.tryValue([ package type ])
		if(entity == null)
			return null
		local Parser = import(entity)
		local parser = Parser(options)
		assert(parser instanceof BaseParser)
		return parser
	}*/

	function parse(package, source, options = null, type = "default") {
		local parser = BaseMeta.parser(package, options, type)
		if(parser == null)
			throw "no parser '" + type + "' found for package '" + package + "'"
		local meta = BaseMeta.get(package)
		local model = meta.new()
		parser.parse(source, model)
		curmodel = model
		curpackage = package
		return this
	}

	function convert(package, options = null) {
		return this
	}

	function generate(package, target, options = null, type = "default") {
		return this
	}

	static function addparser(package, name, entity) {
	}
}

