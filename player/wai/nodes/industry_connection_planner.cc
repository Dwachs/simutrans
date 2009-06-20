#include "industry_connection_planner.h"

#include "../vehikel_prototype.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simworld.h"
#include "../../../bauer/wegbauer.h"
#include "../../../besch/weg_besch.h"
#include "../../../dataobj/loadsave.h"
#include "../../../dings/wayobj.h"

return_code industry_connection_planner_t::step()
{
	// get a way
	const weg_besch_t *wb = wegbauer_t::weg_search(wt, 0, sp->get_welt()->get_timeline_year_month(), weg_t::type_flat);
	if (!wb) {
		sp->get_log().warning("industry_connection_planner_t::step","no way found for waytype %d", wt);
		return RT_ERROR;
	}
	// TODO: check for depots, station, 
	
	// get a vehicle
	simple_prototype_designer_t d(sp);
	d.freight = freight;
	d.include_electric = true;
	d.max_length = 1;
	d.max_weight = 0xffffffff;
	d.min_speed  = 1;
	d.not_obsolete = false;
	d.wt = wt;

	d.update();

	if (d.proto->is_empty()) {
		sp->get_log().warning("industry_connection_planner_t::step","no vehicle found for waytype %d and freight %s", wt, freight->get_name());
		return RT_ERROR;
	}

	// electrification available?
	const way_obj_besch_t *e = NULL;
	if (d.proto->is_electric()) {
		e = wayobj_t::wayobj_search(wt,overheadlines_wt,sp->get_welt()->get_timeline_year_month());
		if (!e) {
			sp->get_log().warning("industry_connection_planner_t::step","no electrification found for waytype %d", wt);
			return RT_ERROR;
		}
	}
	// update way
	wb = wegbauer_t::weg_search(wt, d.proto->max_speed, sp->get_welt()->get_timeline_year_month(), weg_t::type_flat );

	// calculate distance
	const uint32 dist = koord_distance(start->get_pos(), ziel->get_pos());

	uint32 max_speed = min( d.proto->max_speed, wb->get_topspeed() );
	if (e) max_speed = min( max_speed, e->get_topspeed());
	const uint32 tiles_per_month = (kmh_to_speed(max_speed) * sp->get_welt()->ticks_per_tag) >> (8+12); // 12: fahre_basis, 8: 2^8 steps per tile

	// speed bonus calculation see vehikel_t::calc_gewinn
	const sint32 ref_speed = sp->get_welt()->get_average_speed(wt );
	const sint32 speed_base = (100*speed_to_kmh(d.proto->min_top_speed))/ref_speed-100;
	const sint32 freight_price = freight->get_preis() * max( 128, 1000+speed_base*freight->get_speed_bonus());

	// net gain per tile (matching freight)
	const sint64 gain_per_tile = ((sint64)d.proto->get_capacity(freight) * freight_price +1500ll )/3000ll - 2*d.proto->get_maintenance();

	// TODO: estimate production, number of vehicles
	//count_road = min( 254, (2*prod*dist) / (proto->get_capacity(freight)*tiles_per_month)+1 );
	

	// create report
	// TODO: costs for depots, stations
	// TODO: save the prototype-designer somewhere
	report = new report_t();
	report->cost_fix				= dist*( wb->get_preis() + (e!=NULL ? e->get_preis() : 0) );
	report->cost_monthly			= dist*( wb->get_wartung() + (e!=NULL ? e->get_wartung() : 0) );
	report->cost_monthly_per_vehicle = d.proto->get_maintenance();
	report->cost_per_vehicle		= 0;
	report->gain_per_v_m			= gain_per_tile * tiles_per_month ;
	report->nr_vehicles				= 0;

	report->action = new bt_sequential_t(sp, "");

	sp->get_log().message("industry_connection_planner_t::step","report delivered, gain /v /m = %d", report->gain_per_v_m);

	return RT_TOTAL_SUCCESS;
}
void industry_connection_planner_t::rdwr( loadsave_t* file, const uint16 version)
{
	planner_t::rdwr(file, version);
	ai_t::rdwr_fabrik(file, sp->get_welt(), start);
	ai_t::rdwr_fabrik(file, sp->get_welt(), ziel);
	ai_t::rdwr_freight(file, freight);
	
	sint16 iwt = wt;
	file->rdwr_short(iwt,"");
	wt = (waytype_t)iwt;
}