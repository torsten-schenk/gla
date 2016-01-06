class Super {
	constructor() {
		print("ctor from super")
		dump.opstack(this)
	}

	function aFunction() {
		print("from superclass")
	}
}

class Base {
	constructor() {
		print("ctor from base")
		dump.opstack(this)
		Super.constructor.call(this)
	}
	function aFunction() {
		Super.aFunction.call(this)
		print("from baseclass")
	}
}

local b = Base()
b.aFunction()

