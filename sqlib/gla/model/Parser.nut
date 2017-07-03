/* Parser principle: file -> model
 * - the file does not contain model data directly, it must parsed from the file
 * - if the file contains model data directly, use an Importer instead
 */

return class {
	entity = null

	constructor(entity) {
		this.entity = entity
	}

	parse = null //function parse(options = null)

}

