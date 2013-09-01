#ifndef _SQ_EXTENSIONS_
#define _SQ_EXTENSIONS_

#include "squirrel.h"

/**
 * Extensions to the squirrel engine
 * for simutrans
 */
void sq_raise_error(HSQUIRRELVM vm, const SQChar *s, ...);

/**
 * calls a function with limited number of opcodes.
 * returns and suspends vm if opcode limit is exceeded
 */
SQRESULT sq_call_restricted(HSQUIRRELVM v, SQInteger params, SQBool retval, SQBool throw_if_no_ops, SQInteger ops = 1000);

/**
 * resumes suspended vm.
 * returns and suspends (again) vm if opcode limit is exceeded.
 */
SQRESULT sq_resumevm(HSQUIRRELVM v, SQBool retval, SQInteger ops = 1000);

/**
 * pops return value from stack and sets it for the suspended script.
 */
void sq_setwakeupretvalue(HSQUIRRELVM v);

/**
 * checks whether vm can be resumed. in particular if script waits for return value on wakeup.
 */
bool sq_canresumevm(HSQUIRRELVM v);

#endif
