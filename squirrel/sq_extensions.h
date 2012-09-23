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
SQRESULT sq_call_restricted(HSQUIRRELVM v, SQInteger params, SQBool retval, SQBool raiseerror, SQInteger ops = 100000);

/**
 * resumes suspended vm.
 * returns and suspends (again) vm if opcode limit is exceeded.
 */
SQRESULT sq_resumevm(HSQUIRRELVM v, SQBool retval, SQBool raiseerror, SQInteger ops = 100000);

#endif
