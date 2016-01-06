class TupleMap {
	function _set(idx, val) {
		::print("SET: " + idx + " " + val + "\n");
	}

	function _get(idx) {
		::print("GET: " + idx + "\n");
		throw null;
	}
}

local map = TupleMap()

local a = [ 1 ];
local b = [ 1 ];

map[a] = "A value"

print("IS: " + map[b] + "\n");

