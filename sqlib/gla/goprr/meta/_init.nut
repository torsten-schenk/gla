local Property = class {
	name = null

	constructor(name) {
		this.name = name
	}
}

local NonProperty = class {
	name = null
	super = null
	property = null

	constructor(name, super = null) {
		this.name = name
		this.super = super
		this.property = {}
		assert(super == null || super.getclass() == this.getclass())
	}

	function add(prop) {
		assert(!(prop.name in property))
		property[prop.name] <- prop
	}

	function is(other) {
		if(other == null)
			return true
		local cur = this
		while(cur != null)
			if(cur == other)
				return true
			else
				cur = cur.super
		return false
	}
}

local Binding = class {
	relationship = null
	incoming = null
	outgoing = null

	constructor(relationship = null) {
		this.relationship = relationship
		this.incoming = {}
		this.outgoing = {}
	}

	//'objects': may be instanceof Object or an array of instanceof Object or null for any object type
	function addin(role, objects, min, max) {
		assert(!(role in this.incoming) && !(role in this.outgoing))
		this.incoming[role] <- [ objects, min, max ]
	}

	function addout(role, objects, min, max) {
		assert(!(role in this.incoming) && !(role in this.outgoing))
		this.outgoing[role] <- [ objects, min, max ]
	}
}

local StringProperty = class extends Property {
	constructor(name, defaultValue = null) {
		Property.constructor.call(this, name)
	}
}

local IntegerProperty = class extends Property {
	constructor(name, defaultValue = null) {
		Property.constructor.call(this, name)
	}
}

local BoolProperty = class extends Property {
	constructor(name, defaultValue = null) {
		Property.constructor.call(this, name)
	}
}

local ReferenceProperty = class extends Property {
	nonprop = null

	constructor(name, nonprop) {
		Property.constructor.call(this, name)
		this.nonprop = nonprop
	}
}

local Object = class extends NonProperty {
	explosion = null
}

local Role = class extends NonProperty {
}

local Relationship = class extends NonProperty {
}

local Graph = class extends NonProperty {
	binding = null
	root = null

	constructor(name, super = null) {
		NonProperty.constructor.call(this, name, super)
		binding = {}
		root = Binding()
	}

	function add(thang) {
		if(thang instanceof Property)
			NonProperty.add.call(this, thang)
		else if(thang instanceof Binding) {
			assert(!(thang.relationship in binding))
			binding[thang.relationship] <- thang
		}
		else
			throw "Unknown or invalid thang"
	}
}

local Model = class {
	_entityof = null

	version = 0
	object = null
	role = null
	relationship = null
	graph = null
	property = null

	constructor() {
		object = {}
		role = {}
		relationship = {}
		graph = {}
		property = {}
	}

	function add(thang) {
		if(thang instanceof Object) {
			assert(!(thang.name in object))
			object[thang.name] <- thang
		}
		else if(thang instanceof Role) {
			assert(!(thang.name in role))
			role[thang.name] <- thang
		}
		else if(thang instanceof Relationship) {
			assert(!(thang.name in relationship))
			relationship[thang.name] <- thang
		}
		else if(thang instanceof Graph) {
			assert(!(thang.name in graph))
			graph[thang.name] <- thang
		}
		else if(thang instanceof Property) {
			assert(!(thang.name in property))
			property[thang.name] <- thang
		}
		else
			throw "Unknown or invalid thang"
	}

	function finish() {
		//TODO type order: establish a type order, so that lower() and upper() can be used to determine the range of all instances, INCLUDING those of subclasses
		//in addition, add another argument 'metaobject' to ModelReader.anchors, so that the specified objects of the requested type can be iterated
	}
}

rt.mount.register("gla.goprr.meta.Property", Property)
rt.mount.register("gla.goprr.meta.NonProperty", NonProperty)
rt.mount.register("gla.goprr.meta.Binding", Binding)
rt.mount.register("gla.goprr.meta.Graph", Graph)
rt.mount.register("gla.goprr.meta.Object", Object)
rt.mount.register("gla.goprr.meta.Relationship", Relationship)
rt.mount.register("gla.goprr.meta.Role", Role)
rt.mount.register("gla.goprr.meta.Model", Model)
rt.mount.register("gla.goprr.meta.StringProperty", StringProperty)
rt.mount.register("gla.goprr.meta.IntegerProperty", IntegerProperty)
rt.mount.register("gla.goprr.meta.BoolProperty", BoolProperty)
rt.mount.register("gla.goprr.meta.ReferenceProperty", ReferenceProperty)

