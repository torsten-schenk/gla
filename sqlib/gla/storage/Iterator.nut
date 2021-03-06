/* NOTE on comparison between two Iterator instances:
 * operators <, <=, >, >= work, BUT == and != will check for sameness!!! */
return class {
	_c = null
	_colspec = null
	_groupkey = null

	/* May be used read-only by external code */
	groupoff = 0
	grouplen = 0
	groupbegin = null
	groupend = null
	begin = null //index of iteration begin
	end = null //index of iteration end
	index = null

	function _cmp(o) {
		if(index < o.index)
			return -1;
		else if(index > o.index)
			return 1;
		else if(index == o.index)
			return 0;
	}

	function subgroups(nrkey) {
		assert(nrkey + grouplen <= _colspec.kcols)
		assert(nrkey >= 0)
		local dim
		local sub = _c.itdup(this)
		sub._c = _c
		sub._colspec = _colspec
		sub.groupoff = grouplen
		sub.grouplen = nrkey + grouplen
		sub.groupbegin = begin
		sub.groupend = end
		sub._groupkey = array(sub.grouplen)
		for(local i = 0; i < grouplen; i++)
			sub._groupkey[i] = _groupkey[i]

		_c.itldr(this)
		for(local i = grouplen; i < sub.grouplen; i++)
			sub._groupkey[i] = _c.gtc(i)
		_c.clr()
		dim = _putcells(0, sub.grouplen, sub._groupkey)
		_c.ldrl(dim)
		sub.end = _c.idx() //will become sub.begin() after groupNext()
		_c.clr()
		sub.groupNext()
		return sub
/*
		for(local i = grouplen; i < sub.grouplen; i++)
			sub._groupkey[i] = cur[i]
		dim = _putcells(0, sub.grouplen, sub._groupkey)
		_c.ldrl(dim)
		sub.begin = _c.idx()
		_c.clr()
		dim = _putcells(0, sub.grouplen, sub._groupkey)
		_c.ldru(dim)
		sub.end = _c.idx()
		_c.clr()*/
	}

	function groupNext() {
		local dim
		begin = end
		index = begin
		if(begin == groupend)
			return
		_c.ldri(index)
		_c.itupd(this)
		for(local i = groupoff; i < grouplen; i++)
			_groupkey[i] = _c.gtc(i)
		_c.clr()
		dim = _putcells(0, grouplen, _groupkey)
		_c.ldru(dim)
		end = _c.idx()
	}

	function groupAtEnd() {
		return begin == groupend
	}

	function total() {
		if(begin == null || end == null)
			return null
		else
			return end - begin
	}

	function toUpper(key) {
		local result
		local dim

		dim = _putcells(0, _colspec.kcols, key)
		_c.ldru(dim)
		_c.itupd(this)
		_c.clr()
	}

	function next() {
		_c.itmv(this, 1)
	}

	function prev() {
		_c.itmv(this, -1)
	}

	function atEnd() {
		return index == end
	}

	function atBegin() {
		return index == begin
	}

	function skip(amount) {
		assert(typeof(amount) == "integer")
		_c.itmv(this, amount)
	}

	function rewind(amount) {
		assert(typeof(amount) == "integer")
		_c.itmv(this, -amount)
	}

	//note: invalidates iterator! (at least for now)
	function remove() {
		_c.rmr(index, 1)
		//the following code should keep the iterator valid (after next() has been called), but it is untested yet
		//most probably the internal iterator needs to be updated to
		index--
		end--
	}

	function value() {
		local result

		_c.itldr(this)
		if(_colspec.vcols == 1)
			result = _c.gtc(_colspec.kcols)
		else {
			result = array(_colspec.vcols)
			for(local i = 0; i < _colspec.vcols; i++)
				result[i] = _c.gtc(i + _colspec.kcols)
		}
		_c.clr()
		return result
	}

	function key() {
		local result

		_c.itldr(this)
		if(_colspec.kcols == 1)
			result = _c.gtc(0)
		else {
			result = array(_colspec.kcols)
			for(local i = 0; i < _colspec.kcols; i++)
				result[i] = _c.gtc(i)
		}
		_c.clr()
		return result
	}

	function cell(index) {
		local result
		index = _colspec.find(index)

		_c.itldr(this)
		result = _c.gtc(index)
		_c.clr()
		return result
	}

	function putCell(index, value) {
		local result
		index = _colspec.find(index)
		if(index < _colspec.kcols)
			throw "Error setting key column: not allowed"

		_c.itldr(this)
		_c.ptc(index, value)
		_c.str()
	}

	function _putcells(offset, nmax, cells) { //TODO same as in Table (create a common function)
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

}

