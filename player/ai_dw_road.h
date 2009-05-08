#ifndef _ai_dw_road_
#define _ai_dw_road_

#include "ai_dw_tasks.h"

#include "../linehandle_t.h"
#include "../dataobj/koord.h"
#include "../bauer/vehikelbauer.h"

class ai_dw_t;
class fabrik_t;
class ware_besch_t;
class vehikel_prototype_t;
class weg_besch_t;


/************************************
 * Connect two industries
 *
 */
class ait_road_connectf_t: public ai_task_t, public vehikel_evaluator {

	enum { FIRST, SEARCH_PLACES=FIRST, CALC_PROD, CALC_VEHIKEL, CALC_ROAD, BUILD_ROAD, BUILD_DEPOT,CREATE_LINE,BUILD_VEHICLES, READY, _ERROR};

public:
	ait_road_connectf_t(ai_dw_t* const ai): ai_task_t(ai) {}
	ait_road_connectf_t(long t, ai_dw_t* ai, fabrik_t *_start, fabrik_t * _ziel, const ware_besch_t *_f): 
	  ai_task_t(t, ai), 
		  start(_start), 
		  ziel(_ziel), 
		  freight(_f), 
		  place1(koord::invalid), place2(koord::invalid), 
		  proto(NULL),
		  state(FIRST) {}

	virtual void work(ai_task_queue_t &queue);
	
	// evaluate a convoi suggested by vehicle bauer
	virtual sint64 valuate(const vehikel_prototype_t &proto);

	virtual void rdwr(loadsave_t* file);
	virtual uint16 get_type() { return ROAD_CONNEC_FACT;}
protected:
	const fabrik_t *start, *ziel;
	const ware_besch_t *freight;
	koord place1,place2, size1, size2; // place and size of stations
	koord3d start_pos; // here the convois will start
	vehikel_prototype_t *proto;
	const weg_besch_t *weg;
	uint16 count_road;
	
	linehandle_t line;
private:
	uint8 state;
	bool is_ready() { return (state==READY); }
	void search_places();
	void calc_production();
	void calc_vehicle();
	void calc_road();
	void build_road();
	void build_depot();
	void create_line();
	void build_vehicles();

	void clean_markers();

	sint32 prod;
	uint32 dist;
};


#endif
