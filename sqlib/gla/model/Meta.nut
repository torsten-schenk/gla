local strlib = import("squirrel.stringlib")
local Directory = import("gla.util.Directory")
local Identifier = import("gla.string.Identifier")

local Node = import("gla.model.Node")

return class {
	dir = null
	node = null

	constructor() {
		node = {}
		dir = Directory()
	}

	function newnode(pathname, slots, staticslots = null) {
		local name 
		local err
		local path = Directory.split(pathname)
		local curid = -1
		local cursuper = Node
		for(local i = 0; i < path.len() - 1; i++) {
			name = Identifier(path[i], Identifier.LowerUnderscore)
			err = name.validate()
			if(err != null)
				throw "invalid node class name '" + name + "': " + err
			local supername = name.convert(Identifier.CapitalCamel)
			local superid = dir.rentry(curid, supername.tostring())
			if(superid == null) {
				cursuper = class extends cursuper {}
				superid = dir.rinsert(curid, supername.tostring(), cursuper)
			}
			else
				cursuper = dir.data(superid)
			local childid = dir.rfind(curid, name.tostring())
			if(childid == null)
				curid = dir.rinsert(curid, name.tostring())
			else
				curid = childid
		}

		name = Identifier(path[path.len() - 1], Identifier.CapitalCamel)
		err = name.validate()
		if(err != null)
			throw "invalid node class name '" + name + "': " + err
		local clazz = class extends cursuper {}
		if(slots != null)
			foreach(i, v in slots)
				clazz.newmember(i, v)
		if(staticslots != null)
			foreach(i, v in staticslots)
				clazz.newmember(i, v, null, true)
		dir.rinsert(curid, name.tostring(), clazz)
		return


		local super = Node
		local id = dir.insert(name)

		local parentid = dir.parent(id)
		if(parentid != null) {
			local supername = Identifier(dir.name(parentid), Identifier.LowerUnderscore)
			supername.convert(Identifier.CapitalCamel)
			parentid = dir.parent(parentid)
			local superid = dir.rentry(parentid, supername.tostring())
		}
		local path = strlib.split(name, ".")
		local err = Identifier(name, Identifier.CapitalCamel).validate()
		if(err != null)
			throw "invalid class name '" + name + "': " + err
	}

	function toexport() {
		local result = {}
		return result
	}

	function finish() {
	}
}

