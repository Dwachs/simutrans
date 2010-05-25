#ifndef _IND_CONN_PLAN_
#define _IND_CONN_PLAN_

#include "../planner.h"
#include "../utils/wrapper.h"

class fabrik_t;
class karte_t;
class koord3d;
class haus_besch_t;
class ware_besch_t;
class connection_plan_data_t;

class industry_connection_planner_t : public planner_t {
public:
	industry_connection_planner_t(ai_wai_t *sp_, const char* name_, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f, waytype_t _wt)
		: planner_t(sp_,name_), start(s,sp_), ziel(z,sp_), freight(f), wt(_wt) { type = BT_IND_CONN_PLN;}
	industry_connection_planner_t(ai_wai_t *sp_, const char* name_)
		: planner_t(sp_,name_), start(0,sp_), ziel(0,sp_), freight(0), wt(invalid_wt) { type = BT_IND_CONN_PLN;}

	virtual return_value_t *step();
	virtual void rdwr( loadsave_t* file, const uint16 version);

	static bool is_infrastructure_available(waytype_t wt, karte_t *welt, bool check_for_ways=true);
	static sint64 calc_building_cost(const haus_besch_t* st, waytype_t wt, karte_t *welt);
	static sint64 calc_building_maint(const haus_besch_t* st, karte_t *welt);
private:
	/**
	 * calculates convoi prototype, gain / cost
	 */
	connection_plan_data_t* calc_plan_data(waytype_t wt, sint32 prod, uint32 dist);

	/**
	 * plans a connection
	 * @param start_pos: start coordinate, if invalid then position of start-factory is taken
	 * @param ziel_pos: ziel coordinate, if invalid then position of ziel-factory is taken
	 *			if wt==water_wt then ziel_pos can be harbour position
	 * @param create_industry_connector if true then report->action is of type industry_connector_t*
	 * @returns complete report inclusive action nodes
	 */
	report_t* plan_simple_connection(waytype_t wt, sint32 prod, koord3d start_pos=koord3d::invalid, koord3d end_pos=koord3d::invalid, bool create_industry_connector=true);

	/**
	 * plans combined land+sea connection
	 * @param wt non-water waytype
	 */
	report_t* plan_amph_connection(waytype_t wt, sint32 prod);


	koord3d get_harbour_pos();
	sint32 calc_production();
	sint64 calc_building_cost(const haus_besch_t* st);
	sint64 calc_building_maint(const haus_besch_t* st);

	wfabrik_t start;
	wfabrik_t ziel;
	const ware_besch_t *freight;
	waytype_t wt;
};


#endif
