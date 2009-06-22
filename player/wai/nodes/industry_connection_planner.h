#ifndef _IND_CONN_PLAN_
#define _IND_CONN_PLAN_

#include "../planner.h"

class fabrik_t;
class karte_t;
class ware_besch_t;

class industry_connection_planner_t : public planner_t {
public:
	industry_connection_planner_t(ai_wai_t *sp_, const char* name_, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f, waytype_t _wt)
		: planner_t(sp_,name_), start(s), ziel(z), freight(f), wt(_wt) { type = BT_IND_CONN_PLN;}
	industry_connection_planner_t(ai_wai_t *sp_, const char* name_)
		: planner_t(sp_,name_), start(0), ziel(0), freight(0), wt(invalid_wt) { type = BT_IND_CONN_PLN;}
	
	virtual return_code step();
	virtual void rdwr( loadsave_t* file, const uint16 version);
private:
	sint32 calc_production();
	
	const fabrik_t *start;
	const fabrik_t *ziel;
	const ware_besch_t *freight;
	waytype_t wt;
};


#endif
