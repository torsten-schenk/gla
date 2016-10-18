local cbridge = import("gla.storage._cbridge")
local ColumnSpec = import("gla.storage.ColumnSpec")

return class {
	_refcount = 0
	_db = null
	_name = null
	_c = null
	_colspec = null

	function close() {
		assert(_refcount > 0)
		_refcount--
		if(_refcount == 0)
			_db.close(name)
	}

	function len() {
		return _c.sz()
	}

	function cell(key, col) {
		local result
		local dim
		col = _colspec.find(col)
		
		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		if(!_c.ldrl(dim)) {
			_c.clr()
			throw "No such row"
		}
		result = _c.gtc(col)
		_c.clr()
		return result
	}

	function cellAt(row, col) {
		local result
		col = _colspec.find(col)
		
		dim = _putcells(0, _colspec.kcols, key)
		_c.ldri(row)
		result = _c.gtc(col)
		_c.clr()
		return result
	}

	/* @param col: index of value column to put */
	function putCellAt(row, col, value) {
		if(col < 0 || col >= _colspec.vcols)
			throw "Error putting cell: invalid column index " + col
		if(!_colspec.isValidFor(col + _colspec.kcols, value))
			throw "Error putting cell: invalid value type given"
		_c.ldri(row)
		_c.ptc(col + _colspec.kcols, value)
		_c.str()
	}

	function putCell(key, col, value) {
		local dim

		if(col < 0 || col >= _colspec.vcols)
			throw "Error putting cell: invalid column index " + col
		if(!_colspec.isValidFor(col + _colspec.kcols, value))
			throw "Error putting cell: invalid value type given"
		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		if(!_c.ldrl(dim)) {
			_c.clr()
			throw "No such row"
		}
		_c.ptc(col + _colspec.kcols, value)
		_c.str()
	}

	/* Put multiple cells into given row.
	 * @param row Index of the row to be modified.
	 * @param cells An array containing all cells to be put into
	 *   the given row. Size limits: 0 <= len < number of value columns - offset
	 * @param offset Column number of the first cell in @@param{cells}.
	 */
	function putAt(row, cells, offset = 0) {
		::assert(_colspec.kcols == 0)
		local dim
		local exists
		_c.ldri(row)
		_putcells(offset, _colspec.vcols - offset, cells)
		_c.str()
	}

	function put(key, value = null) {
		local dim
		local exists

		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		exists = _c.ldrl(dim)
		if(value != null)
			_putcells(_colspec.kcols, _colspec.vcols, value)
		if(exists)
			_c.str()
		else {
			_putcells(0, _colspec.kcols, key)
			_c.mkrk()
		}
		_c.clr()
		return exists
	}

	function keyCellAt(row, col) {
		local value

		assert(col >= 0)
		assert(col < _colspec.kcols)
		_c.ldri(row)
		value = _c.gtc(col)
		_c.clr()
		return value
	}

	function valueCellAt(row, col) {
		local value

		assert(col >= 0)
		assert(col < _colspec.vcols)
		_c.ldri(row)
		value = _c.gtc(_colspec.kcols + col)
		_c.clr()
		return value
	}

	function keyAt(row) {
		local result

		if(_colspec.kcols == 0)
			return null;
		_c.ldri(row)
		if(_colspec.kcols == 1)
			result = _c.gtc(0)
		else {
			result = ::array(_colspec.kcols)
			for(local i = 0; i < _colspec.kcols; i++)
				result[i] = _c.gtc(i)
		}
		_c.clr()
		return result
	}

	function valueAt(row) {
		local result

		if(_colspec.vcols == 0)
			return null;
		_c.ldri(row)
		if(_colspec.vcols == 1)
			result = _c.gtc(_colspec.kcols);
		else {
			result = ::array(_colspec.vcols)
			for(local i = 0; i < _colspec.vcols; i++)
				result[i] = _c.gtc(_colspec.kcols + i)
		}
		_c.clr()
		return result
	}

	function rowAt(row) {
		local result

		_c.ldri(row)
		if(_colspec.kcols + _colspec.vcols == 1)
			result = _c.gtc(0)
		else {
			result = ::array(_colspec.kcols + _colspec.vcols)
			for(local i = 0; i < _colspec.kcols + _colspec.vcols; i++)
				result[i] = _c.gtc(i)
		}
		_c.clr()
		return result
	}

	function value(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		if(!_c.ldrl(dim)) {
			_c.clr()
			throw "No such row"
		}
		if(_colspec.vcols == 1)
			result = _c.gtc(_colspec.kcols)
		else {
			result = ::array(_colspec.vcols)
			for(local i = 0; i < _colspec.vcols; i++)
				result[i] = _c.gtc(i + _colspec.kcols)
		}
		_c.clr()
		return result
	}

	function tryValue(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		if(!_c.ldrl(dim)) {
			_c.clr()
			return null
		}
		if(_colspec.vcols == 1)
			result = _c.gtc(_colspec.kcols)
		else {
			result = ::array(_colspec.vcols)
			for(local i = 0; i < _colspec.vcols; i++)
				result[i] = _c.gtc(i + _colspec.kcols)
		}
		_c.clr()
		return result
	}

	function exists(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		result = _c.ldrl(dim)
		_c.clr()
		return result
	}

	function append(value = null) {
		::assert(_colspec.kcols == 0)
		if(value != null)
			_putcells(0, _colspec.vcols, value)
		_c.mkri(_c.sz())
		_c.clr()
	}

	function insertAt(row, value = null) {
		::assert(_colspec.kcols == 0)
		if(row < 0 || row > _c.sz())
			throw "Error inserting row: invalid row index '" + row + "'"
		if(value != null)
			_putcells(0, _colspec.vcols, value)
		_c.mkri(row)
		_c.clr()
	}

	function insert(key, value = null) {
		local rowidx;
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		if(value != null)
			_putcells(_colspec.kcols, _colspec.vcols, value)
		_c.mkrk()
		_c.clr()
	}

	function clear() {
		return _c.rmr(0, _c.sz())
	}

	function removeAt(row, n = 1) {
		return _c.rmr(index, n)
	}

	function remove(key) {
		local index
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		if(!_c.ldrl(dim)) {
			_c.clr
			throw "No such row"
		}
		index = _c.idx()
		_c.clr()
		return _c.rmr(index, 1)
	}

	function removeAll(key) {
		local begin
		local end
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		_c.ldrl(dim)
		begin = _c.idx()
		_c.clr()

		dim = _putcells(0, _colspec.kcols, key)
		_c.ldru(dim)
		end = _c.idx()
		_c.clr()

		if(end > begin)
			return _c.rmr(begin, end - begin)
		else
			return 0
	}

	function find(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw "Invalid key given"
		if(!_c.ldrl(dim)) {
			_c.clr()
			return null
		}
		result = _c.it()
		_c.clr()
		result._c = _c
		result._colspec = _colspec
		result.begin = result.index
		result.end = result.index + 1
		return result
	}

	function group(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		_c.ldrl(dim)
		result = _c.it()
		_c.clr()
		result._c = _c
		result._colspec = _colspec
		result.begin = result.index

		dim = _putcells(0, _colspec.kcols, key)
		_c.ldru(dim)
		result.end = _c.idx()
		if(result.end == null)
			result.end = _c.sz()
		_c.clr()
		return result
	}

	function lower(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		_c.ldrl(dim)
		result = _c.it()
		_c.clr()
		result._c = _c
		result._colspec = _colspec
		result.begin = 0
		result.end = _c.sz()
		return result
	}

	function upper(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		_c.ldru(dim)
		result = _c.it()
		_c.clr()
		result._c = _c
		result._colspec = _colspec;
		result.begin = 0
		result.end = _c.sz()
		return result
	}

	function at(row) {
		local result

		_c.ldri(row)
		result = _c.it()
		_c.clr()
		result._c = _c
		result._colspec = _colspec
		result.begin = 0
		result.end = _c.sz()
		return result
	}

	function begin() {
		local result

		_c.ldrb()
		result = _c.it()
		_c.clr()
		result._c = _c
		result._colspec = _colspec
		result.begin = result.index
		result.end = _c.sz()
		return result
	}

	function range(beginit, endit) {
		local result

		result = _c.itdup(beginit)
		result._c = _c
		result._colspec = _colspec
		result.end = endit.index
		result.begin = beginit.index
		return result
	}

	function end() {
		local result

		_c.ldre()
		result = _c.it()
		_c.clr()
		result._c = _c
		result._colspec = _colspec;
		result.begin = 0
		result.end = result.index
		return result
	}

	function _putcells(offset, nmax, cells) {
		local n

		if(typeof(cells) == "array") {
			n = cells.len()
			if(n > nmax)
				throw "Too many cells given"
			for(local i = 0; i < n; i++) {
				if(!_colspec.isValidFor(offset + i, cells[i])) {
					_c.clr()
					throw "Error putting cell: invalid value type given for column " + (offset + i)
				}
				_c.ptc(offset + i, cells[i])
			}
		}
		else if(typeof(cells) == "table") {
			assert(false) /* TODO use column names to put cells; check column for range [ offset, offset + nmax [ */
		}
		else if(nmax >= 1) {
			if(!_colspec.isValidFor(offset, cells)) {
				_c.clr()
				throw "Error putting cell: invalid value type given"
			}
			n = 1
			_c.ptc(offset, cells)
		}
		else
			throw "Invalid cells given"
		return n
	}

	function _newslot(key, value) {
		insert(key, value)
	}

	function _get(key) {
		local value = tryValue(key)
		if(value == null)
			throw null
		else
			return value
	}

	function _set(key, value) {
		local rowidx;
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		if(dim != _colspec.kcols)
			throw null
		if(!_c.ldrl(dim))
			throw null
		if(value != null)
			_putcells(_colspec.kcols, _colspec.vcols, value)
		_c.str()
	}
}

