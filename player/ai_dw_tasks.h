#ifndef _ai_dw_tasks_
#define _ai_dw_tasks_


#include "../tpl/binary_heap_tpl.h"
#include "../dataobj/loadsave.h"

class loadsave_t;
class ai_task_queue_t;
class ai_dw_t;
/************************************************************************
  AI tasks 
*************************************************************************/

class ai_task_t abstract {
public:
	// all tasks
	enum {
		FACT_SEARCH_TREE,		// init search of factory trees 
		FACT_SEARCH_MISS,		// search for missing suppliers
		FACT_COMPLT_TREE,		// complete one factory tree

		ROAD_CONNEC_FACT,		// connect two factories
		MAXTASKS
	};
	ai_task_t(ai_dw_t* const ai): time(0), sp(ai) {}
	ai_task_t(uint32 t, ai_dw_t* ai): time(t), sp(ai) {}

	virtual void work(ai_task_queue_t &queue) {}

	virtual void rotate90( const sint16 y_size ) {}

	virtual void rdwr(loadsave_t* file) { file->rdwr_long(time, " "); }

	virtual uint16 get_type() { return 0;}

	long get_time() const { return time;}
	void set_time(uint32 t) { time=t;}
	
	inline bool operator <= (const ai_task_t t) const { return get_time() <= t.get_time(); }
	// next one only needed for sorted_heap_tpl
	inline bool operator == (const ai_task_t t) const { return get_time() == t.get_time(); }
private:
	uint32 time; // when the task has to be performed
protected:
	ai_dw_t * const sp;
};

/************************************************************************
  Queue of AI tasks 
*************************************************************************/
class ai_task_queue_t {
public:
	ai_task_queue_t(): queue() {};

	//~ai_task_queue_t() {}
	void rotate90( const sint16 y_size );

	void rdwr(loadsave_t* file, ai_dw_t* sp);

	ai_task_t *get_first(uint32 time);

	void append(ai_task_t*);

	bool is_empty() const {return queue.empty(); }
private:
	binary_heap_tpl<ai_task_t*> queue;
};

#endif
