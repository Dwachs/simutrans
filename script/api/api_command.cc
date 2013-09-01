#include "api.h"

/** @file api_command.cc exports ... */

#include "../api_param.h"
#include "../api_class.h"
#include "../api_function.h"
#include "../../simmenu.h"
#include "../../script/script.h"

using namespace script_api;


SQInteger command_constructor(HSQUIRRELVM vm)
{
	// create tool
	uint16 id = param<uint16>::get(vm, 2);
	if (werkzeug_t *wkz = create_tool(id)) {
		attach_instance(vm, 1, wkz);
		return 0;
	}
	return -1;
}

SQInteger command_work(HSQUIRRELVM vm)
{
	werkzeug_t *wkz = param<werkzeug_t*>::get(vm, 1);
	spieler_t *sp   = param<spieler_t*>::get(vm, 2);
	koord3d pos     = param<koord3d>::get(vm, 3);
	bool suspended  = false;

	const char* err = NULL;

	// register this tool call for callback with this id
	uint32 callback_id = suspended_scripts_t::get_unique_key(wkz);

	if (wkz != NULL  &&  sp != NULL) {
		wkz->callback_id = callback_id;
		err = welt->call_work(wkz, sp, pos, suspended);
	}
	else {
		err = "";
	}

	if (!suspended) {
		return param<const char*>::push(vm, err);
	}
	else {
		return sq_suspendvm(vm);
	}
}


void export_commands(HSQUIRRELVM vm)
{
	/**
	 * Proof-of-concept to make tools available to scripts.
	 */
	create_class(vm, "command_x");

	/**
	 * Constructor to obtain a tool.
	 * @param id id of the tool
	 * @typemask command_x(tool_ids)
	 */
	register_function(vm, command_constructor, "constructor", 2, "xi");
	// set type tag to custom getter
	sq_settypetag(vm, -1, (void*)param<werkzeug_t*>::get);

	// TODO need member variable for default_param.

	/**
	 * Does the dirty work.
	 * @note In network games script will be suspended until the command is executed.
	 * @param pl player to pay for the work
	 * @param pos coordinate, where something should happen
	 * @returns null upon success, an error string otherwise
	 * @typemask string(player_x,coord3d)
	 */
	register_function(vm, command_work, "work", 3, "x t|x|y t|x|y");

	end_class(vm);
}
