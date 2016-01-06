local asd = 5

local Class = class {
	asd = 6

	function test() {
		print("ASD = " + this.asd)
	}
}

local c = Class()
c.test()

