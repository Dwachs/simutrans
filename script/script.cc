#include "script.h"

#include <stdarg.h>
#include <string.h>
#include "../squirrel/sqstdaux.h" // for error handlers
#include "../squirrel/sqstdio.h" // for loadfile
#include "../squirrel/sqstdstring.h" // export for scripts
#include "../squirrel/sq_extensions.h" // for loadfile

#include "../utils/log.h"

#include "../tpl/vector_tpl.h"
// for error popups
#include "../gui/help_frame.h"
#include "../simwin.h"
#include "../utils/cbuffer_t.h"
#include "../utils/plainstring.h"

// log file
static log_t* script_log = NULL;
// list of active scripts (they share the same log-file, error and print-functions)
static vector_tpl<script_vm_t*> all_scripts;

static void printfunc(HSQUIRRELVM, const SQChar *s, ...)
{
	va_list vl;
	va_start(vl, s);
	script_log->vmessage("Script", "Print", s, vl);
	va_end(vl);
}


void script_vm_t::errorfunc(HSQUIRRELVM vm, const SQChar *s_, ...)
{

	char *s = strdup(s_);
	char *s_dup = s;
	// remove these linebreaks
	while (s  &&  *s=='\n') s++;
	while (s  &&  s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;

	va_list vl;
	va_start(vl, s_);
	script_log->vmessage("[Script]", "ERROR", s, vl);
	va_end(vl);

	static cbuffer_t buf;
	if (strcmp(s, "<error>")==0) {
		buf.clear();
		buf.printf("<st>Your script made an error!</st>\n");
	}
	if (strcmp(s, "</error>")==0) {
		help_frame_t *win = new help_frame_t();
		win->set_text(buf);
		win->set_name("Script error occured");
		create_win( win, w_info, magic_none);
		// find failed script
		for(uint32 i=0; i<all_scripts.get_count(); i++) {
			if (all_scripts[i]->get_vm() == vm) {
				all_scripts[i]->set_error(buf);
				break;
			}
		}
	}

	va_start(vl, s_);
	buf.vprintf(s, vl);
	va_end(vl);
	buf.append("<br>");

	free(s_dup);
}


// virtual machine
script_vm_t::script_vm_t()
{
	vm = sq_open(1024);
	sqstd_seterrorhandlers(vm);
	sq_setprintfunc(vm, printfunc, errorfunc);
	if (script_log == NULL) {
		script_log = new log_t("script.log", true, true, true, "script engine started.\n");
	}
	all_scripts.append(this);

	// create thread, and put it into registry-table
	sq_pushregistrytable(vm);
	sq_pushstring(vm, "thread", -1);
	thread = sq_newthread(vm, 100);
	sq_newslot(vm, -3, false);
	// create queue table
	sq_pushstring(vm, "queue", -1);
	sq_newarray(vm, 0);
	sq_newslot(vm, -3, false);
	// pop registry
	sq_pop(vm, 1);

	error_msg = NULL;
	// register libraries
	sq_pushroottable(vm);
	sqstd_register_stringlib(vm);
	sq_pop(vm, 1);
}


script_vm_t::~script_vm_t()
{
	sq_close(vm); // also closes thread
	all_scripts.remove(this);
	if (all_scripts.empty()) {
		delete script_log;
		script_log = NULL;
	}
}

const char* script_vm_t::call_script(const char* filename)
{
	// load script
	if (!SQ_SUCCEEDED(sqstd_loadfile(vm, filename, true))) {
		return "Reading / compiling script failed";
	}
	// call it
	sq_pushroottable(vm);
	if (!SQ_SUCCEEDED(sq_call_restricted(vm, 1, SQFalse, SQTrue))) {
		sq_pop(vm, 1); // pop script
		return "Call script failed";
	}
	if (sq_getvmstate(vm) == SQ_VMSTATE_SUSPENDED) {
		set_error("Calling scriptfile took too long");
		return get_error();
	}
	sq_pop(vm, 1); // pop script
	return NULL;
}


const char* script_vm_t::eval_string(const char* squirrel_string)
{
	// compile string
	if (!SQ_SUCCEEDED(sq_compilebuffer(vm, squirrel_string, strlen(squirrel_string), "userdefinedstringmethod", true))) {
		set_error("Error compiling string buffer");
		return get_error();
	}
	// execute
	sq_pushroottable(vm);
	sq_call_restricted(vm, 1, false, true);
	sq_pop(vm, 1);
	return get_error();
}


bool script_vm_t::is_call_suspended(const char* err)
{
	return (err != NULL)  &&  ( strcmp(err, "suspended") == 0);
}


const char* script_vm_t::intern_prepare_call(call_type_t ct, const char* function)
{
	const char* err = NULL;
	bool suspended = sq_getvmstate(vm) == SQ_VMSTATE_SUSPENDED;

	if (!suspended) {
		job = vm;
	}
	else {
		switch (ct) {
			case FORCE:
				job = thread;
				break;
			case QUEUE:
				job = vm;
				break;
			case TRY:
				return "suspended";
		}
	}
	dbg->message("script_vm_t::intern_prepare_call", "ct=%d function=%s stack=%d", ct, function, sq_gettop(job));
	sq_pushroottable(job);
	sq_pushstring(job, function, -1);
	// fetch function from root
	if (SQ_SUCCEEDED(sq_get(job,-2))) {
		sq_remove(job, -2); // root table
		sq_pushroottable(job);
	}
	else {
		err = "Function not found";
		sq_pop(job, 2); // array, root table
	}
	return err;
}


const char* script_vm_t::intern_finish_call(call_type_t ct, int nparams, bool retvalue)
{
	const char* err = NULL;
	bool suspended = sq_getvmstate(job) == SQ_VMSTATE_SUSPENDED;
	// queue function call?
	if (suspended  &&  ct == QUEUE) {
		intern_queue_call(nparams, retvalue);
		err = "suspended";
	}
	if (suspended) {
		intern_resume_call();
	}
	if (!suspended  ||  ct == FORCE) {
		err = intern_call_function(ct, nparams, retvalue);
	}
	return err;
}

const char* script_vm_t::intern_call_function(call_type_t ct, int nparams, bool retvalue)
{
	dbg->message("script_vm_t::intern_call_function", "start: stack=%d nparams=%d ret=%d", sq_gettop(job), nparams, retvalue);
	const char* err = NULL;
	// call the script
	if (!SQ_SUCCEEDED(sq_call_restricted(job, nparams, retvalue, ct == FORCE))) {
		err = "Call function failed";
		retvalue = false;
	}
	if (sq_getvmstate(job) != SQ_VMSTATE_SUSPENDED) {
		// remove closure
		sq_remove(job, retvalue ? -2 : -1);
	}
	else {
		// call suspended: pop dummy return value
		if (retvalue) {
			sq_poptop(job);
		}
		retvalue = false;
		// save retvalue flag
		sq_pushregistrytable(job);
		script_api::param<bool>::create_slot(job, "retvalue", retvalue);
		script_api::param<sint32>::create_slot(job, "nparams", nparams);
		sq_poptop(job);
		err = "suspended";
	}
	dbg->message("script_vm_t::intern_call_function", "stack=%d err=%s", sq_gettop(job)-retvalue, err);
	return err;
}

void script_vm_t::intern_resume_call()
{
	// get retvalue flag
	sq_pushregistrytable(job);
	sq_pushstring(job, "retvalue", -1);
	sq_get(vm, -2);
	bool retvalue = script_api::param<bool>::get(job, -1);
	sq_poptop(job);
	sq_pushstring(job, "nparams", -1);
	sq_get(vm, -2);
	int nparams = script_api::param<sint32>::get(job, -1);
	sq_pop(job, 2);
	// resume vm
	if (!SQ_SUCCEEDED(sq_resumevm(job, retvalue))) {
		retvalue = false;
	}
	// if finished, clear stack
	if (sq_getvmstate(job) != SQ_VMSTATE_SUSPENDED) {
		// remove closure & args
		for(int i=0; i<nparams+1; i++) {
			sq_remove(job, retvalue ? -2 : -1);
		}
		if (retvalue) {
			sq_poptop(job); // TODO  do something meaningful with return value
		}
		dbg->message("script_vm_t::intern_resume_call", "in between stack=%d", sq_gettop(job));

		if (intern_prepare_queued(nparams, retvalue)) {
			intern_call_function(QUEUE, nparams, retvalue);
		}
	}
	dbg->message("script_vm_t::intern_resume_call", "stack=%d", sq_gettop(job));
}


void script_vm_t::intern_queue_call(int nparams, bool retvalue)
{
	SQInteger res;
	// queue call: put closure and parameters in array
	script_api::param<bool>::push(job, retvalue);
	sq_newarray(job, 0); // to hold parameters for queued calls
	// stack: closure, nparams*objects, retvalue, array
	for(int i=nparams+2; i>0; i--) {
		sq_push(job, -2);
		res = sq_arrayappend(job, -2);
		sq_remove(job, -2);
	}
	// stack: array
	sq_pushregistrytable(job);
	sq_pushstring(job, "queue", -1);
	res = sq_get(job, -2);
	// stack: array, registry, queue
	sq_push(job, -3);
	res = sq_arrayappend(job, -2);
	sq_pop(job, 3);

	dbg->message("script_vm_t::intern_queue_call", "stack=%d", sq_gettop(job));
}


bool script_vm_t::intern_prepare_queued(int &nparams, bool &retvalue)
{
	SQInteger res;
	// queued calls
	sq_pushregistrytable(job);
	sq_pushstring(job, "queue", -1);
	res = sq_get(job, -2);
	sq_pushinteger(job, 0);
	res = sq_get(job, -2);
	if (SQ_SUCCEEDED(res)) {
		// there are suspended jobs
		// stack: registry, queue, array[ retvalue nparams*objects closure]
		nparams = -2;
		while( SQ_SUCCEEDED(sq_arraypop(job, -3-nparams, true))) {
			nparams++;
		}
		retvalue = script_api::param<bool>::get(job, -1);
		sq_poptop(job);

		// stack: registry, queue, array, closure, nparams*objects
		sq_remove(job, -2-nparams);
		sq_arrayremove(job, -2-nparams, 0);
		sq_remove(job, -2-nparams);
		sq_remove(job, -2-nparams);
		// stack: closure, nparams*objects
		dbg->message("script_vm_t::intern_prepare_queued", "stack=%d nparams=%d ret=%d", sq_gettop(job), nparams, retvalue);
		return true;
	}
	return false;
}


