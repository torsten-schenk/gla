local cbridge = import("gla.storage._cbridge")
local Super = import("gla.storage.Builder")

const ORDER_MIN = 3
const ORDER_DEFAULT = 10

return class extends Super {
	constructor(colspec) {
		Super.constructor.call(this, colspec)
	}
}

