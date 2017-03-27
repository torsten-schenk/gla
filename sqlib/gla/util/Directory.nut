local strlib = import("squirrel.stringlib")
local SimpleTable = import("gla.storage.SimpleTable")

local regexFragment = strlib.regexp(@"([a-zA-Z_][a-zA-Z_0-9]*)?(\[[0-9]+\])?")
local regexStringFragment = strlib.regexp(@"'([^']*)'(\[[0-9]+\])?")
local strDelim = "'"[0]

local MaxInt = (1 << (_intsize_ * 8 - 2)) - 1

local Entry = class {
	parent = null
	name = null
	index = null
	data = null

	constructor(parent, name, data = null) {
		this.parent = parent
		this.name = name
		this.data = data
	}
}

local FlatIterator = class {
	dir = null
	it = null

	constructor(dir, it) {
		this.dir = dir
		this.it = it
	}

	function atEnd() {
		return it.atEnd()
	}

	function next() {
		return it.next()
	}

	function parent() {
		return it.cell(0)
	}

	function name() {
		return it.cell(1)
	}

	function id() {
		return it.cell(2)
	}

	function entry() {
		return dir.entries[it.cell(2)]
	}

	function data() {
		return dir.entries[it.cell(2)].data
	}
}

local splitPath = function(path) {
	if(typeof(path) == "string") {
		enum State {
			AwaitingAny, AwaitingIndex, StringFragment, IdentifierFragment, IndexFragment
		}
		local state = State.AwaitingAny
		local start
		local fragments = []
		for(local i = 0; i <= path.len(); i++) {
			local c
			if(i == path.len())
				c = 0
			else
				c = path[i]
			switch(state) {
				case State.AwaitingAny:
					if(c == '\'') {
						state = State.StringFragment
						start = i + 1
					}
					else if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
						state = State.IdentifierFragment
						start = i
					}
					else if(c == '[') {
						state = State.IndexFragment
						start = i + 1
					}
					else if(c == 0) {}
					else
						throw "invalid path: '" + path + "'"
					break
				case State.AwaitingIndex:
					if(c == '.' || c == 0)
						state = State.AwaitingAny
					else if(c == '[') {
						start = i + 1
						state = State.IndexFragment
					}
					else
						throw "invalid path: '" + path + "'"
					break
				case State.StringFragment:
					if(c == '\'') {
						fragments.push(path.slice(start, i))
						state = State.AwaitingIndex
					}
					else if(c == 0)
						throw "invalid path: '" + path + "'"
					break
				case State.IdentifierFragment:
					if(c == '.' || c == 0) {
						fragments.push(path.slice(start, i))
						state = State.AwaitingAny
					}
					else if(c == '[') {
						fragments.push(path.slice(start, i))
						state = State.IndexFragment
						start = i + 1
					}
					else if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {}
					else
						throw "invalid path: '" + path + "'"
					break
				case State.IndexFragment:
					if(c == ']') {
						if(start == i)
							throw "invalid path: '" + path + "'"
						fragments.push(path.slice(start, i).tointeger())
						state = State.AwaitingIndex
					}
					else if(c >= '0' && c <= '9') {}
					else
						throw "invalid path: '" + path + "'"
			}
		}
		return fragments
	}
	else if(typeof(path) == "array")
		return path
	else
		assert(false)
}

local findPath = function(self, root, pathname, mk = false, lu = null) {
	local path = splitPath(pathname)
	local key = ::array(2)
	local cap
	local id
	key[0] = root
	foreach(v in path) {
		key[1] = v
		id = self.descend.tryValue(key)
		if(id == null) {
			if(!mk)
				return null
			id = self.entries.len()
			self.descend.insert(key, id)
			self.entries.push(Entry(key[0], key[1]))
		}
		else if(lu != null && (id in self.rvlookup_id2idx))
			lu[self.rvlookup_id2idx[id]] = true
		key[0] = id
	}
	return id
}

local mkReverseLU = function(self) {
	if(self.rvlookup_idx == null)
		return null
	local result = array(self.rvlookup_idx.len())
	for(local i = 0; i < result.len(); i++)
		result[i] = false
	return result
}

local updateData = function(self, lu, id, data) {
	local old = self.entries[id].data
	if(lu != null)
		foreach(i, v in lu)
			if(v) {
				if(old != null)
					delete self.rvlookup[i][old]
				if(data != null)
					self.rvlookup_idx[i][data] <- id
			}
	self.entries[id].data = data
}

local dumpRecursion
dumpRecursion = function(self, parent, indent) {
	local istr = ""
	for(local i = 0; i < indent; i++)
		istr += "  "
	for(local it = self.descend.group(parent); !it.atEnd(); it.next()) {
		local name
		local value
		if(self.entries[it.cell(2)].data == null)
			value = ""
		else
			value = " -> " + self.entries[it.cell(2)].data
		print(istr + it.cell(1) + " {" + it.cell(2) + "}" + value)
		dumpRecursion(self, it.cell(2), indent + 1)
	}
}

return class {
	descend = null
	entries = null
	rvlookup_idx = null
	rvlookup_id2idx = null

	constructor(params = null) {
		descend = SimpleTable(2, 1)
		entries = []
		if(params != null && ("rvlookup" in params) && params.rvlookup.len() > 0) { //create all reverse lookup roots
			rvlookup_id2idx = {}
			rvlookup_idx = array(params.rvlookup.len())
			foreach(i, v in params.rvlookup) {
				local id = findPath(this, -1, v, true)
				rvlookup_id2idx[id] <- i
				rvlookup_idx[i] = {}
			}
		}
	}

	function insert(pathname, data) {
		local lu = mkReverseLU(this)
		local id = findPath(this, -1, pathname, true, lu)
		updateData(this, lu, id, data)
	}

	function put(pathname, data) {
		local lu = mkReverseLU(this)
		local id = findPath(this, -1, pathname, true, lu)
		updateData(this, lu, id, data)
	}

	function append(pathname, data) {
		local lu = mkReverseLU(this)
		local parent = findPath(this, -1, pathname, true, lu)
		local it = descend.upper([ parent MaxInt ])
		local index = 0
		if(!it.atBegin()) {
			it.prev()
			if(it.cell(0) == parent && typeof(it.cell(1)) == "integer")
				index = it.cell(1) + 1
		}
		local id = findPath(this, parent, [ index ], true)
		updateData(this, lu, id, data)
	}

	function find(pathname) {
		return findPath(this, -1, pathname)
	}

	function rvfind(luroot, data) {
		local root = findPath(this, -1, luroot)
		if(root == null || !(root in rvlookup_id2idx))
			throw "no such lookup-root: " + luroot
		local rvlookup = rvlookup_idx[rvlookup_id2idx[root]]
		if(!(data in rvlookup))
			return null
		else
			return rvlookup[data]
	}

	function iterate(root, recursive = false) {
		assert(!recursive) //TODO implement
		local id = findPath(this, -1, root)
		if(id == null)
			return null
		return FlatIterator(this, descend.group(id))
	}

	function parent(id) {
		return entries[id].parent
	}

	function get(pathname) {
		local id = findPath(this, -1, pathname)
		if(id == null)
			return null
		else
			return entries[id].data
	}

	function name(id) {
		return entries[id].name
	}

	function data(id) {
		return entries[id].data
	}

	function entry(id) {
		return entries[id]
	}

	function dump() {
		dumpRecursion(this, -1, 0)
	}
}

