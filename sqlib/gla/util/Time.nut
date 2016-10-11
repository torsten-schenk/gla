return class {
	usec = 0
	sec = 0
	min = 0
	hour = 0 //0-23
	mday = 0 //1-31
	mon = 0 //0-11
	year = 0 //0-6 [sunday, monday, ... ]
	wday = 0
	yday = 0 //0-365
	isdst = 0
	gmtoff = 0

	//function togmt()
	//function fromgmt(time)
	
	function reset() {
		usec = 0
		sec = 0
		min = 0
		hour = 0
		mday = 0
		mon = 0
		year = 0
		wday = 0
		yday = 0
		isdst = 0
		gmtoff = 0
	}

	function dump() {
		print("usec=" + usec + " sec=" + sec + " min=" + min + " hour=" + hour + " mday=" + mday + " mon=" + mon + " year=" + year + " wday=" + wday + " yday=" + yday + " isdst=" + isdst + " gmtoff=" + gmtoff)
	}
}

