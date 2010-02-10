#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#include "../../../simtypes.h"

class ai_wai_t;
class fabrik_t;
class loadsave_t;

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
	wrap_tpl(const T* p=NULL, ai_wai_t* s=NULL): ptr(p), sp(s)
	{
		if(sp  &&  ptr) {
			sp->register_wrapper(this, ptr);
		}
	}

	~wrap_tpl()
	{
		if(sp  &&  ptr) {
			sp->unregister_wrapper(this);
		}
	}
	void notify()
	{
		ptr = NULL;
	}

	void rdwr(loadsave_t *file, const uint16 version, ai_wai_t *sp)
	{
		rdwr_tpl<T>(ptr, file, version, sp);
		if (file->is_loading()) {
			this->sp = sp;
			if (ptr) {
				sp->register_wrapper(this, ptr);
			}
		}
	}
	bool is_bound() const { return ptr!=NULL; }
	const T* operator->() const { return ptr; }

	bool operator != (const wrap_tpl<T> & k) const { return ptr!=k.ptr; }
	bool operator == (const wrap_tpl<T> & k) const { return ptr==k.ptr; }
private:
	const T *ptr;
	ai_wai_t *sp;
};

// now the usefull stuff:
// .. wrap factory ptr
typedef wrap_tpl<fabrik_t> wfabrik_t;



#endif
