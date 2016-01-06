/* Node class methods:
 * - ctor(context [, params]); params depends on whether parameters is specified
 * - enter() : bool (whether to continue)/void (equal to true)
 * - run() : bool (whether to continue)/void (equal to true)
 * - leave() : void
 * - catch(exception) : bool(whether exception has been catched)/void (equal to false)
 */
return class {
	context = null
	enter = null
	run = null
	leave = null
	catsh = null
}

