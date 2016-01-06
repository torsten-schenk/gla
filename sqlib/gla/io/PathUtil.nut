local Path = import("gla.io.Path")
local PackagePath = import("gla.io.PackagePath")

return {
	function canonicalizeEntity(entity, defext) {
		local canonical
		if(entity instanceof Path) {
			if(entity.extisset())
				canonical = entity
			else {
				canonical = clone(entity)
				canonical.extdefault(defext)
			}
		}
		else {
			canonical = PackagePath(entity, true)
			canonical.extdefault(defext)
		}
		return canonical
	}

	function canonicalizePackage(package) {
		local canonical
		if(package instanceof Path)
			return package
		else
			return PackagePath(package, false)
	}
}

