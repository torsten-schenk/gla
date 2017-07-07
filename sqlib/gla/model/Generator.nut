local Adapter = import("gla.model.Adapter")

return class extends Adapter {
	options = null

	constructor(options = null) {
		this.options = options	
	}

	generate = null //function(model, target : entity)
}

