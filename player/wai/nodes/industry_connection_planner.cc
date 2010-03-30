#include "industry_connection_planner.h"

#include "industry_connector.h"
#include "industry_manager.h"
#include "connector_road.h"
#include "connector_ship.h"
#include "../vehikel_prototype.h"
#include "../utils/amphi_searcher.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simworld.h"
#include "../../../bauer/wegbauer.h"
#include "../../../besch/haus_besch.h"
#include "../../../besch/weg_besch.h"
#include "../../../dataobj/loadsave.h"
#include "../../../dings/wayobj.h"

// container class with designer and filled report
class connection_plan_data_t {
public:
	connection_plan_data_t() : wb(0), d(0), report(0) {}
	~connection_plan_data_t() 
	{
		if (d) { delete d; d=NULL; }
		if (report) { delete report; report=NULL; }
	}
	const weg_besch_t *wb;
	simple_prototype_designer_t* d;
	report_t *report;
	// TODO: wayobj

	bool is_ok() { return (d!=NULL) && (report!=NULL) && (!d->proto->is_empty()); }
};

return_value_t *industry_connection_planner_t::step()
{
	if(!start.is_bound()  ||  !ziel.is_bound()) {	
		sp->get_log().warning("industry_connection_planner_t::step", "%s %s disappeared", start.is_bound() ? "" : "start", ziel.is_bound() ? "" : "ziel");
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	if(sp->get_industry_manager()->is_connection(unplanable, *start, *ziel, freight)) {		
		sp->get_log().warning("industry_connection_planner_t::step", "connection already planned/built/forbidden");
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	// check if we already have a report
	if( report ) {
		// This shouldn't happen. We are deleted, if we delivered a report.
		assert( false );
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	// get a way
	const weg_besch_t *wb = wegbauer_t::weg_search(wt, 0, sp->get_welt()->get_timeline_year_month(), weg_t::type_flat);
	if (!wb) {
		sp->get_log().warning("industry_connection_planner_t::step","no way found for waytype %d", wt);
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	// check for depots, station
	const haus_besch_t* st  = hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::generic_station );
	const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, wt, sp->get_welt()->get_timeline_year_month(), 0);
	if (st==NULL || dep ==NULL) {
		sp->get_log().warning("industry_connection_planner_t::step", "no %s%s available for waytype=%d", (st==NULL?"station ":""), (dep==NULL?"depot":""), wt);
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	// estimate production
	sint32 prod = calc_production();
	if (prod<0) {
		sp->get_log().warning("industry_connection_planner_t::step","production = %d", prod);
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	bool include_ships = false;
	koord3d harbour_pos = koord3d::invalid;
	// need ships too?
	if(start->get_besch()->get_platzierung()==fabrik_besch_t::Wasser || ziel->get_besch()->get_platzierung()==fabrik_besch_t::Wasser) {
		if(start->get_besch()->get_platzierung()!=fabrik_besch_t::Wasser || ziel->get_besch()->get_platzierung()==fabrik_besch_t::Wasser) {
			sp->get_log().warning("industry_connection_planner_t::step", "only oil rigs supported yet");
			sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
			return new_return_value(RT_TOTAL_FAIL);
		}
		sp->get_log().message("industry_connection_planner_t::step", "start factory at water side spotted");
		include_ships = true;

		// check for ship depots, dock
		const haus_besch_t* st  = hausbauer_t::get_random_station(haus_besch_t::generic_stop, water_wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::generic_station );
		const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, water_wt, sp->get_welt()->get_timeline_year_month(), 0);
		if (st==NULL || dep ==NULL) {
			sp->get_log().warning("industry_connection_planner_t::step", "no %s%s available for waytype=%d", (st==NULL?"station ":""), (dep==NULL?"depot":""), water_wt);
			sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
			return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
		}

		harbour_pos = get_harbour_pos();
		if (harbour_pos == koord3d::invalid) {
			sp->get_log().warning("industry_connection_planner_t::step", "Keine Amphibienroute");
			sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
			return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
		}
	}
	// wt planner
	const uint32 dist1 = koord_distance(include_ships ? harbour_pos : start->get_pos(), ziel->get_pos());
	connection_plan_data_t *cpd_road = plan_connection(wt, prod, dist1);
	if (!cpd_road->is_ok()) {
		delete cpd_road;
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	// ship planner
	const uint32 dist2 = koord_distance(harbour_pos, start->get_pos());
	connection_plan_data_t *cpd_ship = include_ships ? plan_connection(water_wt, prod, dist2) : NULL;
	if (include_ships && !cpd_ship->is_ok()) {
		delete cpd_road;
		delete cpd_ship;
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	// merge the reports and create new report
	// TODO: save the prototype-designer somewhere
	report = cpd_road->report; 
	if (include_ships) {
		report->merge_report(cpd_ship->report);
	}

	// create the action nodes
	if( include_ships ) {
		bt_sequential_t *action = new industry_connector_t( sp, "industry_connector with road+ship", *start, *ziel, freight );
		action->append_child( new connector_road_t(sp, "connector_road_t", *start, *ziel, wb, cpd_road->d, cpd_road->report->nr_vehicles, NULL, harbour_pos) );
		// TODO: was passiert, wenn der road-connector seine Route nicht bauen kann?
		action->append_child( new connector_ship_t(sp, "connector_ship_t", *start, *ziel, cpd_ship->d, cpd_ship->report->nr_vehicles, harbour_pos) );
		report->action = action;
	}
	else {
		bt_sequential_t *action = new industry_connector_t( sp, "industry_connector with road", *start, *ziel, freight );
		action->append_child( new connector_road_t(sp, "connector_road_t", *start, *ziel, wb, cpd_road->d, cpd_road->report->nr_vehicles, NULL) );
		report->action = action;
	}
	// free memory
	cpd_road->report = NULL;
	cpd_road->d = NULL; delete cpd_road;
	if (include_ships) {
		cpd_ship->d = NULL; delete cpd_ship;
	}

	sp->get_log().message("industry_connection_planner_t::step","report delivered, gain /m = %lld", report->gain_per_m/100);

	sp->get_industry_manager()->set_connection(planned, *start, *ziel, freight);

	return new_return_value(RT_TOTAL_SUCCESS);
}

connection_plan_data_t* industry_connection_planner_t::plan_connection(waytype_t wt, sint32 prod, uint32 dist)
{
	connection_plan_data_t* cpd = new connection_plan_data_t();
	// get a vehicle
	cpd->d = new simple_prototype_designer_t(sp);
	cpd->d->freight = freight;
	cpd->d->production = prod;
	cpd->d->include_electric = false; // wt != road_wt;
	cpd->d->max_length = 1;
	cpd->d->max_weight = 0xffffffff;
	cpd->d->min_speed  = 1;
	cpd->d->not_obsolete = true;
	cpd->d->wt = wt;
	cpd->d->min_trans= 0;
	cpd->d->distance = dist;
	cpd->d->update();
	const vehikel_prototype_t *proto = cpd->d->proto;

	if (proto->is_empty()) {
		sp->get_log().warning("industry_connection_planner_t::step","no vehicle found for waytype %d and freight %s", wt, freight->get_name());
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return cpd;
	}
	// check for ship depots, dock
	const haus_besch_t* st  = hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::generic_station );
	const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, wt, sp->get_welt()->get_timeline_year_month(), 0);

	// cost for stations
	const sint64 cost_buildings = 2*calc_building_cost(st) + calc_building_cost(dep);
	const sint64 main_buildings = 2*calc_building_maint(st) + calc_building_maint(dep);
	
	// init report
	cpd->report = new report_t();
	cpd->report->gain_per_m = 0x8000000000000000;

	// speed bonus calculation see vehikel_t::calc_gewinn
	const sint32 ref_speed = sp->get_welt()->get_average_speed(wt );
	const sint32 speed_base = (100*speed_to_kmh(proto->min_top_speed))/ref_speed-100;
	const sint32 freight_price = freight->get_preis() * max( 128, 1000+speed_base*freight->get_speed_bonus());

	// net gain per tile (matching freight)
	const sint64 gain_per_tile = ((sint64)proto->get_capacity(freight) * freight_price +1500ll )/3000ll - 2*proto->get_maintenance();

	// find the best way
	vector_tpl<const weg_besch_t *> *ways;
	if (wt!=water_wt) {
		ways = wegbauer_t::get_way_list(wt, sp->get_welt());
	}
	else {
		ways = new vector_tpl<const weg_besch_t *>(1);
		ways->append(NULL);
	}
	// loop over all ways and find the best
	for(uint32 i=0; i<ways->get_count(); i++) {
		const weg_besch_t *wb = ways->operator [](i);

		uint32 max_speed = proto->max_speed;
		if (wb && wb->get_topspeed()< max_speed) max_speed=wb->get_topspeed();

		const uint32 tiles_per_month = (kmh_to_speed(max_speed) * sp->get_welt()->ticks_per_tag) >> (8+12); // 12: fahre_basis, 8: 2^8 steps per tile

		// number of vehicles
		const uint16 nr_vehicles = min( max(dist/8,3), (2*prod*dist) / (proto->get_capacity(freight)*tiles_per_month)+1 );

		// now check
		const sint64 cost_monthly = (main_buildings + (wb ? dist*wb->get_wartung(): 0)) << (sp->get_welt()->ticks_bits_per_tag-18);
		const sint64 gain_per_v_m = gain_per_tile * tiles_per_month;
		const sint64 gain_per_m   = gain_per_v_m * nr_vehicles - cost_monthly;
		if (gain_per_m > cpd->report->gain_per_m) {
			cpd->report->cost_fix                 = cost_buildings + (wb ? dist*wb->get_preis()  : 0);
			cpd->report->cost_monthly             = cost_monthly;
			cpd->report->gain_per_v_m             = gain_per_v_m;
			cpd->report->nr_vehicles              = nr_vehicles;
			cpd->report->gain_per_m               = gain_per_m;
			cpd->wb                               = wb;
		}
	}
	delete ways;


	sp->get_log().message("industry_connection_planner_t::plan_connection","wt=%d  gain/vm=%lld  vehicles=%d  cost/m=%lld", wt, cpd->report->gain_per_v_m, cpd->report->nr_vehicles, cpd->report->cost_monthly);
	return cpd;
}

/* @from ai_goods */
sint32 industry_connection_planner_t::calc_production()
{
	karte_t *welt = sp->get_welt();

	// properly calculate production & consumption
	const vector_tpl<ware_production_t>& ausgang = start->get_ausgang();
	uint start_ware=0;
	while(  start_ware<ausgang.get_count()  &&  ausgang[start_ware].get_typ()!=freight  ) {
		start_ware++;
	}
	const vector_tpl<ware_production_t>& eingang = ziel->get_eingang();
	uint ziel_ware=0;
	while(  ziel_ware<eingang.get_count()  &&  eingang[ziel_ware].get_typ()!=freight  ) {
		ziel_ware++;
	}
	const uint8  shift = 8 - welt->ticks_bits_per_tag +10 +8; // >>(simfab) * welt::monatslaenge /PRODUCTION_DELTA_T +dummy
	const sint32 prod_z = (ziel->get_base_production()  *  ziel->get_besch()->get_lieferant(ziel_ware)->get_verbrauch() )>>shift;
	const sint32 prod_s = ((start->get_base_production() * start->get_besch()->get_produkt(start_ware)->get_faktor())>> shift) - start->get_abgabe_letzt(start_ware);
	const sint32 prod = min(  prod_z,prod_s);

	return prod;
}

koord3d industry_connection_planner_t::get_harbour_pos() 
{
	// find the harbour position
	vector_tpl<koord> startplatz;
	start->get_tile_list( startplatz );
	ai_t::add_neighbourhood( startplatz, sp->get_welt()->get_einstellungen()->get_station_coverage() );
	vector_tpl<koord3d> startplatz2;
	for( uint32 i = 0; i < startplatz.get_count(); i++ ) {
		startplatz2.append( sp->get_welt()->lookup_kartenboden(startplatz[i])->get_pos() );
	}

	vector_tpl<koord> zielplatz;
	ziel->get_tile_list( zielplatz );
	ai_t::add_neighbourhood( zielplatz, sp->get_welt()->get_einstellungen()->get_station_coverage() );
	vector_tpl<koord3d> zielplatz2;
	for( uint32 i = 0; i < zielplatz.get_count(); i++ ) {
		zielplatz2.append( sp->get_welt()->lookup_kartenboden(zielplatz[i])->get_pos() );
	}

	amphi_searcher_t bauigel(sp->get_welt(), sp );
	const weg_besch_t *weg_besch = wegbauer_t::weg_search(wt, 0, sp->get_welt()->get_timeline_year_month(), weg_t::type_flat);
	bauigel.route_fuer( (wegbauer_t::bautyp_t)wt, weg_besch, NULL, NULL );
	// we won't destroy cities (and save the money)
	bauigel.set_keep_existing_faster_ways(true);
	bauigel.set_keep_city_roads(true);
	bauigel.set_maximum(10000);
	bauigel.calc_route(startplatz2, zielplatz2);

	koord3d harbour_pos = koord3d::invalid;
	if( bauigel.get_count() > 2 ) {
		bool wasser = sp->get_welt()->lookup(bauigel.get_route()[0])->ist_wasser();
		for( uint32 i = 1; i < bauigel.get_count(); i++ ) {
			bool next_is_wasser = sp->get_welt()->lookup(bauigel.get_route()[i])->ist_wasser();
			if( wasser != next_is_wasser ) {
				if( next_is_wasser ) {
					harbour_pos = bauigel.get_route()[i-1];
				}
				else {
					harbour_pos = bauigel.get_route()[i];
				}
				break;
			}
		}
	}
	return harbour_pos;
}


sint64 industry_connection_planner_t::calc_building_cost(const haus_besch_t* st)
{
	return calc_building_cost(st, wt, sp->get_welt());
}

sint64 industry_connection_planner_t::calc_building_cost(const haus_besch_t* st, waytype_t wt, karte_t *welt)
{
	switch(st->get_utyp()) {
		case haus_besch_t::generic_stop: {
			sint64 cost = - st->get_level()*st->get_b()*st->get_h();
			switch(wt) {
				case road_wt:
					cost *= welt->get_einstellungen()->cst_multiply_roadstop;
					break;
				case track_wt:
				case monorail_wt:
				case maglev_wt:
				case narrowgauge_wt:
				case tram_wt:
					cost *= welt->get_einstellungen()->cst_multiply_station;
					break;
				case water_wt:
					cost *= welt->get_einstellungen()->cst_multiply_dock;
					break;
				case air_wt:
					cost *= welt->get_einstellungen()->cst_multiply_airterminal;
					break;
				default:
					assert(0);
					return 0;
			}
			return cost;
		}
		case haus_besch_t::depot:
			switch(wt) {
				case road_wt:
					return -welt->get_einstellungen()->cst_depot_road;
				case track_wt:
				case monorail_wt:
				case tram_wt:
				case maglev_wt:
				case narrowgauge_wt:
					return -welt->get_einstellungen()->cst_depot_rail;
				case water_wt:
					return -welt->get_einstellungen()->cst_depot_ship;
				case air_wt:
					return -welt->get_einstellungen()->cst_depot_air;
				default:
					assert(0);
					return 0;
			}
		default:
			assert(0);
			return 0;
	}
}


sint64 industry_connection_planner_t::calc_building_maint(const haus_besch_t* besch, karte_t *welt)
{
	return welt->get_einstellungen()->maint_building*besch->get_level()*besch->get_groesse().x*besch->get_groesse().y;
}

sint64 industry_connection_planner_t::calc_building_maint(const haus_besch_t* st)
{
	return calc_building_maint(st, sp->get_welt());
}

void industry_connection_planner_t::rdwr( loadsave_t* file, const uint16 version)
{
	planner_t::rdwr(file, version);
	start.rdwr(file, version, sp);
	ziel.rdwr(file, version, sp);
	ai_t::rdwr_ware_besch(file, freight);

	sint16 iwt = wt;
	file->rdwr_short(iwt,"");
	wt = (waytype_t)iwt;
}
