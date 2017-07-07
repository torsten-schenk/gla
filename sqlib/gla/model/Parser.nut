/* Parser principle: file -> model
 * - the file does not contain model data directly, it must parsed from the file
 * - if the file contains model data directly, use an Importer instead
 */

local Adapter = import("gla.model.Adapter")

return class extends Adapter {
	parse = null //function parse(source, model)
}

