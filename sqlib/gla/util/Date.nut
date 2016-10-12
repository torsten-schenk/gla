return class {
	year = 0
	month = 0
	day = 0

	//function fromepoch(offset) 'offset': days since 0000-01-01
	//function toepoch(): days since 0000-01-01

	function reset() {
		year = 0
		month = 0
		day = 0
	}

	function dump() {
		print("year=" + year + " month=" + month + " day=" + day)
	}
}

