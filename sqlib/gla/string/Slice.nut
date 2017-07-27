return class {
	string = null
	begin = null
	end = null

	constructor(string) {
		this.string = string
		begin = 0
		end = string.len()
	}

	function len() {
		return end - begin
	}

	function consumeall(o) {
		local n = o.len()
		local total = 0
		while(true) {
			if(end - begin < n)
				return total
			for(local i = 0; i < n; i++)
				if(o[i] != string[begin + i])
					return total
			begin += n
			total++
		}
	}

	function consume(o) {
		local n = o.len()
		local off = begin
		if(end - begin < n)
			return false

		for(local i = 0; i < n; i++)
			if(o[i] != string[off + i])
				return false
		begin += n
		return true
	}

	function regexconsume(regex, capidx = null) {
		local substr = tostring()
		local cap = regex.capture(substr) //TODO slicing necessary here?
		if(cap == null)
			return null
		begin += cap[0].end
		if(capidx == null) {
			foreach(i, v in cap)
				cap[i] = substr.slice(v.begin, v.end)
			return cap
		}
		else
			return substr.slice(cap[capidx].begin, cap[capidx].end)
	}

	function ltrim() {
		while(begin < end) {
			local c = string[begin]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				begin++
			else
				return
		}
	}

	function rtrim() {
		while(begin < end) {
			local c = string[end - 1]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				end--
			else
				return
		}
	}

	function trim() {
		while(begin < end) {
			local c = string[begin]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				begin++
			else
				return
		}
		while(begin < end) {
			local c = string[end - 1]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				end--
			else
				return
		}
	}

	function empty() {
		return begin == end
	}

	function _tostring() {
		return string.slice(begin, end)
	}
}

