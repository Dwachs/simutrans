#ifndef _ai_dw_fac_
#define _ai_dw_fac_

#include "ai_dw_tasks.h"
#include "ai_dw_road.h"

#include "../dataobj/koord.h"

class ai_dw_t;
class fabrik_t;
class ware_besch_t;
/************************************************************************
  Factory tasks 
*************************************************************************/


/************************************
  Find the 'best' factory tree
*************************************/
class ait_fct_init_t: public ai_task_t {
public:
	ait_fct_init_t(ai_dw_t* const ai): ai_task_t(ai) {}
	ait_fct_init_t(long t, ai_dw_t* ai): ai_task_t(t, ai) {}
	virtual void work(ai_task_queue_t &queue);
	virtual uint16 get_type() { return FACT_SEARCH_TREE;}
private:
	int get_factory_tree_missing_count( fabrik_t *fab );
};
/************************************
  Find factory that needs suppliers 
   in a given factory tree
*************************************/
class ait_fct_find_t: public ai_task_t {
public:
	ait_fct_find_t(ai_dw_t* const ai): ai_task_t(ai) {}
	ait_fct_find_t(long t, ai_dw_t* ai, fabrik_t *fab): ai_task_t(t, ai), root(fab) {}
	virtual void work(ai_task_queue_t &queue);
	virtual void rdwr(loadsave_t* file);
	virtual uint16 get_type() { return FACT_SEARCH_MISS;}
protected:
	fabrik_t *root, *start, *ziel;
	const ware_besch_t *freight;
private:
	bool get_factory_tree_lowest_missing( fabrik_t *fab );
};

/************************************
  Completes a factory tree
*************************************/
class ait_fct_complete_tree_t: public ait_fct_find_t {
public:
	ait_fct_complete_tree_t(ai_dw_t* const ai): ait_fct_find_t(ai) {}
	ait_fct_complete_tree_t(long t, ai_dw_t* ai, fabrik_t *_root): ait_fct_find_t(t, ai,_root) {}
	virtual void work(ai_task_queue_t &queue);
	virtual uint16 get_type() { return FACT_COMPLT_TREE;}

	//virtual void rotate90( const sint16 y_size );
	//virtual void rdwr(loadsave_t* file);
};

#endif
