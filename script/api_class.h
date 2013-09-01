#ifndef _API_CLASS_H_
#define _API_CLASS_H_

#include "api_param.h"

#include "../squirrel/squirrel.h"
#include "../squirrel/sq_extensions.h"  // sq_call_restricted

#include <string>

/** @file api_class.h  handling classes and instances */

namespace script_api {


	/**
	 * Creates squirrel class on the stack. Inherits from @p baseclass.
	 * Has to be complemented by call to end_class.
	 * @param classname name of squirrel class
	 * @param baseclass name of base class (or NULL)
	 * @return SQ_OK or SQ_ERROR
	 */
	SQInteger create_class(HSQUIRRELVM vm, const char* classname, const char* baseclass = NULL);

	/**
	 * Pushes class on stack.
	 * Has to be complemented by call to end_class.
	 * @param classname name of squirrel class, must exist prior to calling this function
	 * @param baseclasses dummy string containing base classes - to create nice doxygen output
	 * @return SQ_OK or SQ_ERROR
	 */
	SQInteger begin_class(HSQUIRRELVM vm, const char* classname, const char* baseclasses = NULL);

	/**
	 * Pops class from stack.
	 */
	void end_class(HSQUIRRELVM vm);

	/**
	 * Pushes the squirrel class onto the stack.
	 * Has to be complemented by call to end_class.
	 * @param classname name of squirrel class
	 * @return SQ_OK or SQ_ERROR
	 */
	SQInteger push_class(HSQUIRRELVM vm, const char* classname);


	/**
	 * Prepares call to class to instantiate.
	 * Pushes class and dummy object.
	 * @param classname name of squirrel class
	 * @return SQ_OK or SQ_ERROR
	 */
	SQInteger prepare_constructor(HSQUIRRELVM vm, const char* classname);

	/**
	 * Function to create & push instances of squirrel classes.
	 * @param classname name of squirrel class
	 */
	inline SQInteger push_instance(HSQUIRRELVM vm, const char* classname)
	{
		if (!SQ_SUCCEEDED(prepare_constructor(vm, classname)) ) {
			return -1;
		}
		bool ok = SQ_SUCCEEDED(sq_call_restricted(vm, 1, true, false));
		sq_remove(vm, ok ? -2 : -1); // remove closure
		return ok ? 1 : -1;
	}

	template<class A1>
	SQInteger push_instance(HSQUIRRELVM vm, const char* classname, const A1 & a1)
	{
		if (!SQ_SUCCEEDED(prepare_constructor(vm, classname)) ) {
			return -1;
		}
		param<A1>::push(vm, a1);
		bool ok = SQ_SUCCEEDED(sq_call_restricted(vm, 2, true, false));
		sq_remove(vm, ok ? -2 : -1); // remove closure
		return ok ? 1 : -1;
	}

	template<class A1, class A2>
	SQInteger push_instance(HSQUIRRELVM vm, const char* classname, const A1 & a1, const A2 & a2)
	{
		if (!SQ_SUCCEEDED(prepare_constructor(vm, classname)) ) {
			return -1;
		}
		param<A1>::push(vm, a1);
		param<A2>::push(vm, a2);
		bool ok = SQ_SUCCEEDED(sq_call_restricted(vm, 3, true, false));
		sq_remove(vm, ok ? -2 : -1); // remove closure
		return ok ? 1 : -1;
	}

	template<class A1, class A2, class A3>
	SQInteger push_instance(HSQUIRRELVM vm, const char* classname, const A1 & a1, const A2 & a2, const A3 & a3)
	{
		if (!SQ_SUCCEEDED(prepare_constructor(vm, classname)) ) {
			return -1;
		}
		param<A1>::push(vm, a1);
		param<A2>::push(vm, a2);
		param<A3>::push(vm, a3);
		bool ok = SQ_SUCCEEDED(sq_call_restricted(vm, 4, true, false));
		sq_remove(vm, ok ? -2 : -1); // remove closure
		return ok ? 1 : -1;
	}

	template<class A1, class A2, class A3, class A4>
	SQInteger push_instance(HSQUIRRELVM vm, const char* classname, const A1 & a1, const A2 & a2, const A3 & a3, const A4 & a4)
	{
		if (!SQ_SUCCEEDED(prepare_constructor(vm, classname)) ) {
			return -1;
		}
		param<A1>::push(vm, a1);
		param<A2>::push(vm, a2);
		param<A3>::push(vm, a3);
		param<A4>::push(vm, a4);
		bool ok = SQ_SUCCEEDED(sq_call_restricted(vm, 5, true, false));
		sq_remove(vm, ok ? -2 : -1); // remove closure
		return ok ? 1 : -1;
	}

	/**
	 * Releases memory allocate by attach_instance.
	 * Do not call directly!
	 */
	template<class C>
	SQInteger command_release_hook(SQUserPointer up, SQInteger)
	{
		C **p = (C **)up;
		delete *p;
		delete p;
		return 1;
	}

	/**
	 * Stores pointer to c++ class as userpointer of squirrel class instance.
	 * C++ class will be deleted when squirrel class instance gets released.
	 */
	template<class C>
	void attach_instance(HSQUIRRELVM vm, SQInteger index, C* o)
	{
		// store pointer
		C **p = new C*;
		*p = o;
		// set userpointer of class instance
		sq_setinstanceup(vm, index, p );
		sq_setreleasehook(vm, index, command_release_hook<C>);
	}

	/**
	 * Retrieves pointer to stored c++ instance.
	 */
	template<class C>
	C* get_attached_instance(HSQUIRRELVM vm, SQInteger index, SQUserPointer tag)
	{
		SQUserPointer up;
		sq_getinstanceup(vm, index, &up, tag);
		if (up) {
			return *( (C**)up );
		}
		return NULL;
	}

}; // end of namespace
#endif
