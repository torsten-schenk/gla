class Super {
	member = null
}
dump.full([ 4, 5, [ 6, 7 ], { asd = 5, dsa = 4 }, class extends Super { function dsa() {} blubb = null } ])
local arr = [ 1, 2 ]
arr[6] = 5
dump.value(arr)
dump.opstack("asd", 1, [])
