local strlib = import("squirrel.stringlib")
local Identifier = import("gla.string.Identifier")

local Node = import("gla.model.Node")

return class {
	static Node = Node
	node = null

	constructor() {
		node = {}
	}

	function newnode(pathname, slots = null, staticslots = null) {
		local name 
		local err
		local path = strlib.split(pathname, ".")
		local cur = node
		local cursuper = Node
		for(local i = 0; i < path.len() - 1; i++) {
			name = Identifier(path[i], Identifier.LowerUnderscore)
			err = name.validate()
			if(err != null)
				throw "invalid node class name '" + name + "': " + err
			local supername = name.convert(Identifier.CapitalCamel)

			local super

			if(supername.tostring() in cur)
				cursuper = cur[supername.tostring()]
			else {
				cursuper = class extends cursuper {}
				cur[supername.tostring()] <- cursuper
			}

			if(!(name.tostring() in cur))
				cur[name.tostring()] <- {}
			cur = cur[name.tostring()]
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
		cur[name.tostring()] <- clazz
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

