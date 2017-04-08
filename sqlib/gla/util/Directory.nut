local strlib = import("squirrel.stringlib")
local SimpleTable = import("gla.storage.SimpleTable")

local MaxInt = (1 << (_intsize_ * 8 - 2)) - 1

local Entry = class {
	_dir = null
	_id = null
	_parent = null
	_name = null
	_data = null
	//context = null //TODO idea: add an optional context table with delegation to each entry.

	constructor(dir, id, parent, name, data = null) {
		this._dir = dir
		this._id = id
		this._parent = parent
		this._name = name
		this._data = data
	}

	function _get(idx) {
		local result = _dir.descend.tryValue([ _id idx ])
		if(result == null)
			throw null
		else
			return _dir.entries[result]
	}

	function _tostring() {
		local name
		local data
		if(_name == null)
			name = ""
		else if(typeof(_name) == "string")
			name = "['" + _name + "']"
		else if(typeof(_name) == "integer")
			name = "[" + _name + "]"
		if(_data == null)
			data = ""
		else if(name.len() > 0)
			data = " " + _data
		else
			data = _data
		return name + data
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
		if(typeof(it.cell(1)) == "string")
			return it.cell(1)
		else
			return null
	}

	function index() {
		if(typeof(it.cell(1)) == "integer")
			return it.cell(1)
		else
			return null
	}

	function id() {
		return it.cell(2)
	}

	function entry() {
		return dir.entries[it.cell(2)]
	}

	function data() {
		return dir.entries[it.cell(2)]._data
	}

	function isIndex() {
		return typeof(it.cell(1)) == "integer"
	}

	function isName() {
		return typeof(it.cell(1)) == "string"
	}
}

local RecursiveIterator = class {
	dir = null
	stack = null
	lastdepth = null

	constructor(dir, it) {
		this.dir = dir
		stack = [ it ]
		lastdepth = 0
	}

	function atEnd() {
		return stack.len() == 0
	}

	function next() {
		lastdepth = stack.len()
		local it = dir.descend.group(stack.top().cell(2))
		if(it.total() > 0)
			stack.push(it)
		else
			while(stack.len() > 0) {
				stack.top().next()
				if(stack.top().atEnd())
					stack.pop()
				else
					return
			}
	}

	function rstrpath(root, sep = ".", prefix = "", postfix = "") {
		return dir.rstrpath(root, stack.top().cell(2), sep, prefix, postfix)
	}

	function strpath(sep = ".", prefix = "", postfix = "") {
		return dir.strpath(stack.top().cell(2), sep, prefix, postfix)
	}

	function parent() {
		return stack.top().cell(0)
	}

	function name() {
		if(typeof(stack.top().cell(1)) == "string")
			return stack.top().cell(1)
		else
			return null
	}

	function index() {
		if(typeof(stack.top().cell(1)) == "integer")
			return stack.top().cell(1)
		else
			return null
	}

	function id() {
		return stack.top().cell(2)
	}

	function entry() {
		return dir.entries[stack.top().cell(2)]
	}

	function data() {
		return dir.entries[stack.top().cell(2)]._data
	}

	function isIndex() {
		return typeof(stack.top().cell(1)) == "integer"
	}

	function isName() {
		return typeof(stack.top().cell(1)) == "string"
	}

	function depth() {
		return stack.len() - 1
	}

	function ddepth() {
		return stack.len() - lastdepth
	}
}

local splitPath = function(path) {
	if(typeof(path) == "string") {
		enum State {
			AwaitingAny, AwaitingIndex, AwaitingIndexAny, AwaitingIndexEnd, StringFragment, IdentifierFragment, IndexStringFragment, IndexNumberFragment
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
					else if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
						state = State.IdentifierFragment
						start = i
					}
					else if(c == '[') {
						state = State.AwaitingIndexAny
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
						state = State.AwaitingIndexAny
					}
					else
						throw "invalid path: '" + path + "'"
					break
				case State.AwaitingIndexAny:
					if(c == '\'') {
						state = State.IndexStringFragment
						start = i + 1
					}
					else if(c >= '0' && c <= '9')
						state = State.IndexNumberFragment
					break
				case State.AwaitingIndexEnd:
					if(c == ']')
						state = State.AwaitingIndex
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
				case State.IndexStringFragment:
					if(c == '\'') {
						fragments.push(path.slice(start, i))
						state = State.AwaitingIndexEnd
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
						state = State.AwaitingIndexAny
						start = i + 1
					}
					else if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {}
					else
						throw "invalid path: '" + path + "'"
					break
				case State.IndexNumberFragment:
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
			self.entries.push(Entry(this, id, key[0], key[1]))
		}
		else if(lu != null && (id in self.rvlookup_id2idx))
			lu[self.rvlookup_id2idx[id]] = true
		key[0] = id
	}
	return id
}

local toPathId = function(self, root, thang, mk = false, lu = null) {
	if(thang == null)
		return -1
	else if(thang instanceof Entry)
		return thang._id
	else if(typeof(thang) == "integer")
		return thang
	else if(typeof(thang) == "array" || typeof(thang) == "string")
		return findPath(self, root, thang, mk, lu)
	else
		assert(false)
}

local mkReverseLU = function(self, id = null) {
	if(self.rvlookup_idx == null)
		return null
	local result = array(self.rvlookup_idx.len())
	for(local i = 0; i < result.len(); i++)
		result[i] = false
	while(id != null) {
		if(id in self.rvlookup_id2idx)
			result[self.rvlookup_id2idx[id]] = true
		if(id == -1)
			id = null
		else
			id = self.entries[id]._parent
	}
	return result
}

local updateData = function(self, lu, id, data) {
	local old = self.entries[id]._data
	if(lu != null)
		foreach(i, v in lu)
			if(v) {
				if(old != null)
					delete self.rvlookup_idx[i][old]
				if(data != null)
					self.rvlookup_idx[i][data] <- id
			}
	self.entries[id]._data = data
}

local dumpRecursion
dumpRecursion = function(self, parent, indent) {
	local istr = ""
	for(local i = 0; i < indent; i++)
		istr += "  "
	for(local it = self.descend.group(parent); !it.atEnd(); it.next()) {
		local name
		local value
		if(self.entries[it.cell(2)]._data == null)
			value = ""
		else
			value = " -> " + self.entries[it.cell(2)]._data
		print(istr + it.cell(1) + " {" + it.cell(2) + "}" + value)
		dumpRecursion(self, it.cell(2), indent + 1)
	}
}

return class {
	root = null
	descend = null
	entries = null
	rvlookup_idx = null
	rvlookup_id2idx = null

	constructor(params = null) {
		descend = SimpleTable(2, 1)
		entries = []
		root = Entry(this, -1, null, null, null)
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

	function exists(pathname) {
		return findPath(this, -1, pathname) != null
	}

	function rexists(root, pathname) {
		local rootid = toPathId(this, -1, root)
		if(rootid == null)
			return false
		else
			return findPath(this, rootid, pathname) != null
	}

	function insert(pathname, data = null) {
		local lu = mkReverseLU(this)
		local id = findPath(this, -1, pathname, true, lu)
		updateData(this, lu, id, data)
		return id
	}

	function rinsert(root, pathname, data = null) {
		local rootid = toPathId(this, -1, root)
		if(rootid == null)
			return null
		local lu = mkReverseLU(this, rootid)
		local id = findPath(this, rootid, pathname, true, lu)
		updateData(this, lu, id, data)
		return id
	}

	function put(pathname, data = null) {
		local lu
		local id
		if(typeof(pathname) == "integer") {
			id = pathname
			lu = mkReverseLU(this, id)
		}
		else {
			lu = mkReverseLU(this)
			id = findPath(this, -1, pathname, true, lu)
		}
		updateData(this, lu, id, data)
		return id
	}

	function append(pathname, data = null) {
		local parent = toPathId(this, -1, pathname, true)
		local lu = mkReverseLU(this, parent)
		local it = descend.upper([ parent MaxInt ])
		local index = 0
		if(!it.atBegin()) {
			it.prev()
			if(it.cell(0) == parent && typeof(it.cell(1)) == "integer")
				index = it.cell(1) + 1
		}
		local id = findPath(this, parent, [ index ], true)
		updateData(this, lu, id, data)
		return id
	}

	function count(pathname, recursive = false) { //TODO implement recursion
		local id = findPath(this, -1, pathname, false)
		return descend.group(id).total()
	}

	function find(pathname) {
		return findPath(this, -1, pathname)
	}

	function rfind(root, pathname) {
		local rootid = toPathId(this, -1, root)
		if(rootid == null)
			return null
		return findPath(this, rootid, pathname)
	}

	function intree(root, child) {
		while(true) {
			if(child == root)
				return true
			else if(child == null)
				return false
			child = entries[child]._parent
		}
	}

	function findrv(luroot, data) {
		local root = toPathId(this, -1, luroot)
		if(root == null || !(root in rvlookup_id2idx))
			throw "no such lookup-root: " + luroot
		local rvlookup = rvlookup_idx[rvlookup_id2idx[root]]
		if(!(data in rvlookup))
			return null
		else
			return rvlookup[data]
	}

	function findns(rootname, pivotname, pathname) { //namespace lookup: 'rootname' is the root namespace; 'pivotname' is the current namespace (starting at 'rootname'); 'pathname' is the namespace to find. works similar to c++ namespace resolution. 'pathname' may be prefixed with '::' in order to start resolving at 'rootname' instead of 'pivotname'
		local root = toPathId(this, -1, rootname)
		local pivot
		local path
		if(root == null)
			return null
		if(pathname.len() >= 2 && pathname[0] == ':' && pathname[1] == ':') {
			pathname = pathname.slice(2)
			pivot = root
		}
		else if(pivotname == null)
			pivot = root
		else {
			pivot = toPathId(this, root, pivotname)
			if(pivot == null || !intree(root, pivot))
				pivot = root
		}
		while(true) {
			path = findPath(this, pivot, pathname)
			if(path != null)
				return path
			else if(pivot == root)
				return null
			pivot = entries[pivot]._parent
		}
	}

	function iterate(rootname, recursive = false) {
		local id = toPathId(this, -1, rootname)
		if(id == null)
			return null
		if(recursive)
			return RecursiveIterator(this, descend.group(id))
		else
			return FlatIterator(this, descend.group(id))
	}

	function mk(root, pathname, data = null) {
		local lu = mkReverseLU(this, root)
		local id = findPath(this, root, pathname, true, lu)
		updateData(this, lu, id, data)
		return id
	}

	function mkappend(root, data = null) {
		local lu = mkReverseLU(this, root)
		local it = descend.upper([ root MaxInt ])
		local index = 0
		if(!it.atBegin()) {
			it.prev()
			if(it.cell(0) == root && typeof(it.cell(1)) == "integer")
				index = it.cell(1) + 1
		}
		local id = findPath(this, root, [ index ], true)
		updateData(this, lu, id, data)
		return id
	}

	function parent(path) {
		local id = toPathId(this, -1, path)
		if(id == null)
			return null
		return entries[id]._parent
	}

	function get(pathname) {
		local id = findPath(this, -1, pathname)
		if(id == null)
			return null
		else
			return entries[id]._data
	}

	function name(path) {
		local id = toPathId(this, -1, path)
		if(id == null)
			return null
		return entries[id]._name
	}

	function data(path) {
		local id = toPathId(this, -1, path)
		if(id == null)
			return null
		return entries[id]._data
	}

	function rentry(root, path = null) {
		local rootid = toPathId(this, -1, root)
		if(rootid == null)
			return null
		local id = toPathId(this, rootid, path)
		if(id == null)
			return null
		else if(id == -1)
			return this.root
		else
			return entries[id]
	}

	function entry(path = null) {
		local id = toPathId(this, -1, path)
		if(id == null)
			return null
		else if(id == -1)
			return root
		else
			return entries[id]
	}

	function rstrpath(root, path, sep = ".", prefix = "", postfix = "") {
		local rootid = toPathId(this, -1, root)
		if(rootid == null)
			return null
		local pathid = toPathId(this, rootid, path)
		if(pathid == null)
			return null
		local strpath = null
		while(pathid != rootid) {
			if(strpath == null)
				strpath = prefix + entries[pathid]._name + postfix
			else
				strpath = prefix + entries[pathid]._name + postfix + sep + strpath
			pathid = entries[pathid]._parent
		}
		return strpath
	}

	function strpath(path, sep = ".", prefix = "", postfix = "") {
		local pathid = toPathId(this, -1, path)
		if(pathid == null)
			return null
		local strpath = null
		while(pathid != -1) {
			if(strpath == null)
				strpath = prefix + entries[pathid]._name + postfix
			else
				strpath = prefix + entries[pathid]._name + postfix + sep + strpath
			pathid = entries[pathid]._parent
		}
		return strpath
	}

	function dump() {
		dumpRecursion(this, -1, 0)
	}
}

