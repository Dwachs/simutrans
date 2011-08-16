#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#include "../../ai_wai.h"
#include "../../../simdebug.h"
#include "../../../simtypes.h"
#include "../../../dataobj/loadsave.h"

class fabrik_t;

/*
 * abstract class to contain the notify routine
 * notify is called when ptr is deleted
 */
class wrapper_t {
public:
	virtual void notify()=0;
};

/*
 * function template for rdwr the wrapped pointer
 */
template<class T> void rdwr_tpl(const T* &p, loadsave_t *file, const uint16, ai_wai_t *sp);

/*
 * basic template to hold a pointer that
 * gets auto-NULLed if the object it points to gets deleted
 * and the player is notified
 *
 * TODO: all kind of operators (=, copy constructor etc)
 */
template <class T> class wrap_tpl : public wrapper_t {
public:
	wrap_tpl(const T* p, ai_wai_t* s): ptr(p), sp(s)
	{
		registerw();
	}
	~wrap_tpl()
	{
		unregisterw();
	}
	void notify()
	{
		ptr = NULL;
	}

	void rdwr(loadsave_t *file, const uint16 version, ai_wai_t *sp)
	{
		rdwr_tpl<T>(ptr, file, version, sp);
		if (file->is_loading()) {
			unregisterw();
			this->sp = sp;
			registerw();
		}
	}
	bool is_bound() const { return ptr!=NULL; }

	const T* operator->() const { return ptr; }
	const T* operator*() const { return ptr; }

	bool operator != (const wrap_tpl<T> & k) const { return ptr!=k.ptr; }
	bool operator == (const wrap_tpl<T> & k) const { return ptr==k.ptr; }
	bool operator == (const T* p) const { return ptr==p; }

	void set(const T* p)
	{
		unregisterw();
		ptr=p;
		registerw();
	}

	void registerw()
	{
		assert(sp);
		if(sp  &&  ptr) {
			sp->register_wrapper(this, ptr);
		}
	}
	void unregisterw()
	{
		assert(sp);
		if(sp) {
			sp->unregister_wrapper(this);
		}
	}

private:
	const T *ptr;
	ai_wai_t *sp;
};

// now the usefull stuff:
// .. wrap factory ptr
typedef wrap_tpl<fabrik_t> wfabrik_t;



#endif
