return {
	function consumeall(a, b, sliceb) {
		local n = a.len()
		local total = 0
		while(true) {
			if(sliceb.end - sliceb.begin < n)
				return total
			for(local i = 0; i < n; i++)
				if(a[i] != b[sliceb.begin + i])
					return total
			sliceb.begin += n
			total++
		}
	}

	function consume(a, b, sliceb) {
		local n = a.len()
		local off = sliceb.begin
		if(sliceb.end - sliceb.begin < n)
			return false

		for(local i = 0; i < n; i++)
			if(a[i] != b[off + i])
				return false
		sliceb.begin += n
		return true
	}

	function regexconsume(regex, b, sliceb, capidx = null) {
		local substr = b.slice(sliceb.begin, sliceb.end)
		local cap = regex.capture(substr) //TODO slicing necessary here?
		if(cap == null)
			return null
		sliceb.begin += cap[0].end
		if(capidx == null) {
			foreach(i, v in cap)
				cap[i] = substr.slice(v.begin, v.end)
			return cap
		}
		else
			return substr.slice(cap[capidx].begin, cap[capidx].end)
	}

	function ltrim(a, slicea) {
		while(slicea.begin < slicea.end) {
			local c = a[slicea.begin]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				slicea.begin++
			else
				return
		}
	}

	function rtrim(a, slicea) {
		while(slicea.begin < slicea.end) {
			local c = a[slicea.end - 1]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				slicea.end--
			else
				return
		}
	}

	function trim(a, slicea) {
		while(slicea.begin < slicea.end) {
			local c = a[slicea.begin]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				slicea.begin++
			else
				return
		}
		while(slicea.begin < slicea.end) {
			local c = a[slicea.end - 1]
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				slicea.end--
			else
				return
		}
	}

	function slice(a, slicea, emptyisnull = false) {
		if(slicea.begin == slicea.end && emptyisnull)
			return null
		else
			return a.slice(slicea.begin, slicea.end)
	}
}

