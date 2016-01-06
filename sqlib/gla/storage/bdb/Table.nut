local cbridge = import("gla.storage._cbridge")
local Super = import("gla.storage.Table")

local Base = class extends Super {
	static function parameters(table) {
	}

	constructor(params) {
		local subparams = {}
		local v

		if("order" in params) {
			v = params.order
			if(typeof(v) != "integer")
				throw "Invalid parameter 'order': expected integer"
			else if(v < 3)
				throw "Invalid parameter 'order': value too low"
			else
				subparams.order <- v
		}
		else
			subparams.order <- 10
/*
		Super.constructor.call(this, cbridge.MemoryTableHandler, params, subparams)*/
	}
}

return Base

