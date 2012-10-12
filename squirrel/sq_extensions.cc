#include "sq_extensions.h"

#include "squirrel/sqpcheader.h" // for declarations...
#include "squirrel/sqvm.h"       // for Raise_Error_vl
#include <stdarg.h>

void sq_raise_error(HSQUIRRELVM vm, const SQChar *s, ...)
{
	va_list vl;
	va_start(vl, s);
	vm->Raise_Error_vl(s, vl);
	va_end(vl);

	vm->CallErrorHandler(vm->_lasterror);
}

SQRESULT sq_call_restricted(HSQUIRRELVM v, SQInteger params, SQBool retval, SQBool throw_if_no_ops, SQInteger ops)
{
	if(v->_ops_remaining < 4*ops) {
		v->_ops_remaining += ops;
	}
	v->_throw_if_no_ops = throw_if_no_ops;
	v->_check_ops = true;

	SQRESULT ret = sq_call(v, params, retval, true /*raise_error*/);
	v->_check_ops = false;
	return ret;
}

SQRESULT sq_resumevm(HSQUIRRELVM v, SQBool retval, SQInteger ops)
{
	if(v->_ops_remaining < 4*ops) {
		v->_ops_remaining += ops;
	}
	v->_throw_if_no_ops = false;
	v->_check_ops = true;

	SQRESULT ret = sq_wakeupvm(v, false, retval, true /*raise_error*/, false);
	v->_check_ops = false;
	return ret;
}
