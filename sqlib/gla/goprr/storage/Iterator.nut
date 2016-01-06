return class {
	begin = null
	end = null
	it = null

	current = null
	hasCurrent = false

	function total() {
		return end - begin
	}

	function at() {
		return it.index - begin
	}

	function to(pos) {
		if(pos < 0)
			throw "Invalid index: must not be negative"
		else if(pos > end - begin)
			throw "Invalid index: too big"
		else if(pos > it.index - begin) {
			it.skip(pos - it.index + begin)
			hasCurrent = false
		}
		else if(pos < it.index - begin) {
			it.rewind(it.index - begin - pos)
			hasCurrent = false
		}
		assert(it.index = begin + pos)
	}

	function toBegin() {
		if(it.index != begin) {
			it.rewind(it.index - begin)
			hasCurrent = false
		}
		assert(it.index == begin)
	}

	function toEnd() {
		if(it.index != end) {
			it.skip(end - it.index)
			hasCurrent = false
		}
		assert(it.index == end)
	}

	function next() {
		if(it.index == end)
			throw "Iterator already at end"
		it.next()
		hasCurrent = false
	}

	function prev() {
		if(it.index == begin)
			throw "Iterator already at begin"
		it.prev()
		hasCurrent = false
	}

	function atEnd() {
		return it.index == end
	}

	function atBegin() {
		return it.index == begin
	}

	function _getcurrent() {
		if(!hasCurrent) {
			_updatecurrent()
			hasCurrent = true
		}
		return current
	}
}

