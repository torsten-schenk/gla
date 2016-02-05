local Table = import("gla.storage.SimpleTable")
local Iterator = import("gla.storage.Iterator")
local Parser = import("gla.xml.DocumentParser")

local MyIterator = class {
	_it = null

	constructor(it) {
		_it = it
	}

	function atEnd() {
		return _it.atEnd()
	}

	function atBegin() {
		return _it.atBegin()
	}

	function next() {
		_it.next()
	}

	function prev() {
		_it.prev()
	}
}

local StructureIterator = class extends MyIterator {
	function parent() {
		return _it.cell(0)
	}

	function namespace() {
		return _it.cell(1)
	}

	function name() {
		return _it.cell(2)
	}

	function text() {
		return _it.cell(3)
	}

	function child() {
		return _it.cell(3)
	}

	function isText() {
		return _it.cell(2) == null
	}

	function isElement() {
		return _it.cell(2) != null
	}
}

local AttributeIterator = class extends MyIterator {
	function parent() {
		return _it.cell(0)
	}

	function name() {
		return _it.cell(1)
	}

	function namespace() {
		return _it.cell(2)
	}

	function value() {
		return _it.cell(3)
	}
}

return class {
	_structure = null
	_attributes = null

	constructor(entity = null) {
		_structure = Table(1, 3, true) //parent id, namespace, tagname/null, child id/text
		_attributes = Table(3, 1) //id, key/null, namespace, value/tagname
		if(entity != null) {
			local parser = Parser(this)
			parser.parse(entity)
		}
	}

	//iterate texts and elements; order (text/node) of file is maintained
	function children(parent = null) {
		return StructureIterator(_structure.range(_structure.lower(parent), _structure.upper(parent)))
	}

	function attributes(parent) {
		//range: start at first attribute after null since null is the element name
		return AttributeIterator(_attributes.range(_attributes.upper([ parent, null, null ]), _attributes.upper(parent)))
	}

	function attribute(element, name = null, namespace = null) {
		return _attributes.tryValue([ element, name, namespace ])
	}

	function dump(root = null) {
		for(local it = children(root); !it.atEnd(); it.next()) {
			if(it.isText() == null)
				print("T '" + it.text() + "'")
			else
				_dumpElement(0, it.child())
		}
	}

	function _dumpElement(indent, element) {
		local dummy = ""
		local end
		for(local i = 0; i < indent; i++)
			dummy += "  "
		print(dummy + "E <" + attribute(element) + ">")
		for(local it = attributes(element); !it.atEnd(); it.next())
			print(dummy + "  A " + it.name() + " = '" + it.value() + "'")
		for(local it = children(element); !it.atEnd(); it.next())
			if(it.isText())
				print(dummy + "  T '" + it.text() + "'")
			else
				_dumpElement(indent + 1, it.child())
	}
}

