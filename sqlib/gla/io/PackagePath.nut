local cbridge = import("gla.io.cbridge")
local Path = import("gla.io.Path")
local strlib = import("squirrel.stringlib")

const TYPE_PACKAGE = 0x00
const TYPE_ENTITY = 0x01

const STATE_TYPE = 0x01
const STATE_IT = 0x02
const STATE_IT_END = 0x04

local regexFilename = strlib.regexp(@"([^\.]*)\.(.*)")

local EntityName = class {
	name = null
	extension = null

	constructor(name = null, extension = null) {
		this.name = name
		this.extension = extension
	}
}

local PackageIterator = cbridge.PackagePath_PackageIterator(class {
	_prev = null
	_depth = null
	name = null

	constructor(path) {
		_prev = path._it
		_depth = path._size
		_init(path)
	}
})

local EntityIterator = cbridge.PackagePath_EntityIterator(class {
	_prev = null
	name = null
	extension = null

	constructor(path) {
		_prev = path._it
		_init(path)
	}
})

local Base
Base = cbridge.PackagePath(class extends Path {
	//static MAX_DEPTH = <defined by cbridge>

	_it = null
	_package = null
	_entity = null
	_extension = null
	_size = 0
	_state = 0

	constructor(path = null, isentity = null) {
		_package = ::array(MAX_DEPTH)
		if(path instanceof Base) {
			if(!path.isValid())
				throw "Invalid path given"
			_size = path._size
			_entity = path._entity
			_extension = path._extension
			_state = path._state & STATE_TYPE
			for(local i = 0; i < _size; i++)
				_package[i] = path._package[i]
			if(isentity != null) {
				if(isentity && (_state & STATE_TYPE) != TYPE_ENTITY)
					throw "Entity expected but package found"
				else if(!isentity && (_state & STATE_TYPE) != TYPE_PACKAGE)
					throw "Package expected but entity found"
			}
			return
		}
		if(isentity == null)
			isentity = false
		if(typeof(path) == "string")
			parse(path, isentity)
		else if(path != null)
			throw("Invalid path given")
		if(isentity)
			_state = TYPE_ENTITY
		else
			_state = TYPE_PACKAGE
	}

	//function parse(string, isfolder = false) = <implemented by cbridge>
	//function touch() = <implemented by cbridge>
	//function rename() = <implemented by cbridge>
	//function exists() = <implemented by cbridge>
	//function tofilepath() = <implemented by cbridge>

	function createable() {
	}

	//function open(mode = "r") = <implemented by cbridge>

	function extension() {
		return _extension
	}

	function entity() {
		return _entity
	}

	function len() {
		return _size
	}

	//iterator end: it.name == null
	function packageit() {
		return PackageIterator(this)
	}

	//iterator end: it.name == null
	function entityit() {
		return EntityIterator(this)
	}

	function packages() {
		local list = []
		for(local it = PackageIterator(this); it.name != null; it.next())
			list.push(it.name)
		return list
	}

	function entities() {
		local list = []
		for(local it = EntityIterator(this); it.name != null; it.next())
			list.push(EntityName(it.name, it.extension))
		return list
	}

	function folders() {
		return packages()
	}

	function leaves() {
		return entities()
	}

	function down(fragment = null) {
		assert(isPackage())
		assert(_size < MAX_DEPTH - 1)
		_state = TYPE_PACKAGE

		if(fragment == null) {
			_it = PackageIterator(this)
			_state = _state | STATE_IT
			if(_it.name == null)
				_state = _state | STATE_IT_END
			else
				_package[_it._depth] = _it.name
		}
		else
			_package[_size] = fragment
		_size++
		return this
	}

	function up() {
		assert((_state & STATE_TYPE) == TYPE_PACKAGE)
		if(_size > 0) {
			if((_state & STATE_IT) != 0)
				_it = _it._prev
			_size--
			_state = TYPE_PACKAGE
			if(_it != null && _size == _it._depth + 1)
				_state = _state | STATE_IT
			_package[_size] = null
		}
		return this
	}

	function next() {
		assert(isIterating())
		assert(isValid()) //i.e. not at end

		_it.next()
		if(_it.name == null)
			_state = _state | STATE_IT_END
		else if((_state & STATE_TYPE) == TYPE_PACKAGE)
			_package[_it._depth] = _it.name
		else {
			_entity = _it.name
			_extension = _it.extension
		}
		return this
	}

	function selectfile(filename) {
		local cap = regexFilename.capture(filename)
		if(cap == null)
			select(filename)
		else
			select(filename.slice(cap[1].begin, cap[1].end), filename.slice(cap[2].begin, cap[2].end))
	}

	function select(leaf = null, extension = null) {
		_state = TYPE_ENTITY
		if(leaf == null) {
			_it = EntityIterator(this)
			_state = _state | STATE_IT
			if(_it.name == null)
				_state = _state | STATE_IT_END
			else {
				_entity = _it.name
				_extension = _it.extension
			}
		}
		else {
			_extension = extension
			_entity = leaf
		}
		return this
	}

	function extset(extension) {
		_extension = extension
	}

	function extisset() {
		return _extension != null
	}

	function extclear() {
		_extension = null
	}

	function extdefault(def) {
		if(_extension == null) {
			_extension = def
			return true
		}
		else
			return false
	}

	function unselect() {
		assert((_state & STATE_TYPE) == TYPE_ENTITY)
		if((_state & STATE_IT) != 0)
			_it = _it._prev
		_state = TYPE_PACKAGE
		if(_it != null && _size == _it._depth + 1)
			_state = _state | STATE_IT
		_extension = null
		_entity = null
		return this
	}

	function isFolder() {
		return isPackage()
	}

	function isLeaf() {
		return isEntity()
	}

	function isPackage() {
		return (_state & STATE_TYPE) == TYPE_PACKAGE && (_state & STATE_IT_END) == 0
	}

	function isEntity() {
		return (_state & STATE_TYPE) == TYPE_ENTITY && (_state & STATE_IT_END) == 0
	}

	function isValid() {
		return (_state & STATE_IT_END) == 0
	}

	function isIterating() {
		return (_state & STATE_IT) != 0
	}

	function fragment(index) {
		if(index < 0 || index >= _size)
			return null
		else
			return _package[index]
	}

	function _tostring() { //TODO quote fragments which require quoting (i.e. those containing a dot)
		local result
		if(_extension != null)
			result = "<" + _extension + ">"
		else
			result = ""
		for(local i = 0; i < _size; i++)
			result += (i == 0 ? "" : ".") + _package[i]
		if(_entity != null) {
			if(_size > 0)
				result += "."
			result += _entity
		}
		return result
	}

	//don't clone iterators; if original path is invalid (i.e. iterator at end), this method will fail!
	function _cloned(org) {
		assert(org.isValid())
		_package = array(MAX_DEPTH)
		for(local i = 0; i < _size; i++)
			_package[i] = org._package[i]
		_it = null
		_state = org._state & STATE_TYPE
	}

	static function EntityDescendingName(a, b) {
		return a.name <=> b.name
	}

	static function EntityAscendingName(a, b) {
		return b.name <=> a.name
	}

	static function CanonicalizeEntity(entity) {
		if(typeof(entity) == "string")
			return Base(entity, true)
		else if(entity instanceof Base) {
			if(entity.isEntity())
				return entity
			else
				throw "expected entity"
		}
		else
			throw "invalid entity given"
	}

	static function CanonicalizePackage(package) {
		if(typeof(package) == "string")
			return Base(package)
		else if(package instanceof Base) {
			if(package.isPackage())
				return package
			else
				throw "expected package"
		}
		else
			throw "invalid package given"
	}
})

return Base

