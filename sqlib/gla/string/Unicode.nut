local cbridge = import("gla.string._cbridge")

return class {
	static utf8toutf32 = cbridge.unicode.utf8toutf32
	static utf8strlen = cbridge.unicode.utf8strlen
}

