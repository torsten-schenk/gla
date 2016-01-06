local cbridge = import("gla.io.cbridge")

//Note: the purpose of this class is to instantiate it (i.e. a CRC32 instance) and to use the instanceup for callback functions.
//This is due to the reason that it does not make much sense to implement a hash function in squirrel.
return cbridge.HashFunction(class {
	//static CRC32 = <defined by cbridge>
})

