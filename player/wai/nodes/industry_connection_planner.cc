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

bool cmp_reports(report_t *r1, report_t *r2) {
	return r1 ? (r2 ? r1->gain_per_m > r2->gain_per_m : true) : (r2==NULL);
}

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
	if(ziel->get_besch()->get_platzierung()==fabrik_besch_t::Wasser) {
		sp->get_log().warning("industry_connection_planner_t::step", "only oil rigs supported yet");
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL);
	}
	// check if we already have a report
	if( report ) {
		// This shouldn't happen. We are deleted, if we delivered a report.
		assert( false );
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	// estimate production
	sint32 prod = calc_production();
	if (prod<0) {
		sp->get_log().warning("industry_connection_planner_t::step","production = %d", prod);
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	// plan road and road/water connections
	vector_tpl<report_t*> reports(3);
	report_t *report0 = plan_simple_connection(road_wt, prod);
	if (report0) {
		reports.insert_ordered(report0, cmp_reports);
	}
	report0 = plan_amph_connection(road_wt, prod, true);
	if (report0) {
		reports.insert_ordered(report0, cmp_reports);
	}
	report0 = plan_amph_connection(road_wt, prod, false);
	if (report0) {
		reports.insert_ordered(report0, cmp_reports);
	}

	if (reports.empty()) {
		sp->get_log().warning("industry_connection_planner_t::step","no report");
		sp->get_industry_manager()->set_connection(forbidden, *start, *ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	for(uint32 i=0; i<reports.get_count(); i++) {
		sp->get_log().warning("industry_connection_planner_t::step","report[%d] g/m %d", i, reports[i]->gain_per_m);
	}
	// concatenate reports
	report = reports[0];
	for(uint32 i=1; i<reports.get_count(); i++) {
		reports[i-1]->action->append_report(reports[i]);
	}

	sp->get_log().message("industry_connection_planner_t::step","report delivered, gain /m = %lld", report->gain_per_m/100);

	sp->get_industry_manager()->set_connection(planned, *start, *ziel, freight);

	return new_return_value(RT_TOTAL_SUCCESS);
}



report_t* industry_connection_planner_t::plan_simple_connection(waytype_t wt, sint32 prod, koord3d start_pos, koord3d ziel_pos, bool create_industry_connector)
{
	// TODO: check if route exists
	// check for ways/stations/depots
	if (!is_infrastructure_available(wt, sp->get_welt(), true)) {
		sp->get_log().warning("industry_connection_planner_t::plan_simple_connection","no ways/stations/depots found for waytype %d", wt);
		return NULL;
	}
	// distance
	koord3d p1 = start_pos!=koord3d::invalid ? start_pos : start->get_pos();
	koord3d p2 =  ziel_pos!=koord3d::invalid ?  ziel_pos : ziel->get_pos();
	const uint32 dist = koord_distance(p1, p2);
	const uint32 dist_paid = max(koord_distance(ziel->get_pos(), p1) - koord_distance(ziel->get_pos(), p2), 0);
	
	if (dist==0) {
		return new report_t();
	}

	sp->get_log().warning("industry_connection_planner_t::plan_simple_connection","%s %s %d %d\n", p1.get_str(), p2.get_2d().get_str(), dist, dist_paid);
	// wt planner
	connection_plan_data_t *cpd = calc_plan_data(wt, prod, dist, dist_paid);
	if (!cpd->is_ok()) {
		delete cpd;
		return NULL;
	}
	// get report
	report_t *report = cpd->report;
	cpd->report = NULL;

	// create action node
	bt_node_t *action = NULL;
	switch(wt) {
		case road_wt:
			action = new connector_road_t(sp, "connector_road_t", *start, *ziel, p1, p2, cpd->wb, cpd->d, report->nr_vehicles);
			break;
		case water_wt:
			// p1, p2 contain positions of harbour
			action = new connector_ship_t(sp, "connector_ship_t", *start, *ziel, p1, p2, cpd->d, report->nr_vehicles);
			break;
		default:
			sp->get_log().warning("industry_connection_planner_t::plan_simple_connection","unhandled waytype %d", wt);
	}
	if (action  &&  create_industry_connector) {
		industry_connector_t *connector = new industry_connector_t( sp, "industry_connector", *start, *ziel, freight );
		connector->append_child(action);
		report->action = connector;
	}
	else {
		report->action = action;
	}

	cpd->d = NULL;
	delete cpd;

	return report;
}


/**
 * plans connection from start to ziel
 * @param reverse: if true the ships will drive from harbour to ziel (default: from start to harbour)
 */
report_t* industry_connection_planner_t::plan_amph_connection(waytype_t wt, sint32 prod, bool reverse)
{
	// find position for harbour
	koord3d start_harbour;
	koord3d target_harbour = !reverse ? get_harbour_pos(*start, *ziel, start_harbour) : get_harbour_pos(*ziel, *start, start_harbour);
	if (target_harbour == koord3d::invalid  ||  target_harbour == start_harbour) {
		sp->get_log().warning("industry_connection_planner_t::plan_amph_connection", "no marine route");
		return NULL;
	}
	// now:
	// start_harbour  = !reverse ? load there : unload there
	// target_harbour = !reverse ? unload there : load there

	// find position for station on land
	const grund_t *gr = sp->get_welt()->lookup(target_harbour);
	koord3d land_pos = target_harbour + koord(gr->get_grund_hang()) + koord3d(0,0,1);

	report_t *report1 = plan_simple_connection(wt, prod, !reverse ? land_pos : koord3d::invalid, !reverse ? koord3d::invalid : land_pos);
	if (report1) {
		report_t *report2 = plan_simple_connection(water_wt, prod, !reverse ? start_harbour : target_harbour, !reverse ? target_harbour : start_harbour, false /*no ind_connector*/);
		if (report2) {
			report1->merge_report(report2);
		}
	}
	return report1;
}

connection_plan_data_t* industry_connection_planner_t::calc_plan_data(waytype_t wt, sint32 prod, uint32 dist, uint32 dist_paid)
{
	karte_t *welt = sp->get_welt();
	// check for depots, station
	const haus_besch_t* st  = hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, welt->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::generic_station );
	const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, wt, welt->get_timeline_year_month(), 0);

	// get a vehicle
	connection_plan_data_t* cpd = new connection_plan_data_t();
	cpd->d  = new simple_prototype_designer_t(sp);
	simple_prototype_designer_t* d = cpd->d;
	d->freight = freight;
	d->production = prod;
	d->include_electric = false; // wt != road_wt;
	d->max_length = wt != water_wt ? 1 : 4;
	d->max_weight = 0xffffffff;
	d->min_speed  = 1;
	d->not_obsolete = true;
	d->wt = wt;
	d->min_trans= 0;
	d->distance = dist;
	d->update();
	const vehikel_prototype_t *proto = d->proto;

	if (proto->is_empty()) {
		sp->get_log().warning("industry_connection_planner_t::calc_plan_data","no vehicle found for waytype %d and freight %s", wt, translator::translate(freight->get_name()));
		return cpd;
	}
	// we checked everything, now the plan can be developed

	// cost for stations
	const sint64 cost_buildings = 2*calc_building_cost(st) + calc_building_cost(dep);
	const sint64 main_buildings = 2*calc_building_maint(st) + calc_building_maint(dep);

	// init report
	cpd->report = new report_t();
	cpd->report->gain_per_m = 0x8000000000000000;

	// speed bonus calculation see vehikel_t::calc_gewinn
	const sint32 ref_speed = welt->get_average_speed(wt );
	const sint32 speed_base = (100*speed_to_kmh(proto->min_top_speed))/ref_speed-100;
	const sint32 freight_price = freight->get_preis() * max( 128, 1000+speed_base*freight->get_speed_bonus());

	// net gain per tile (matching freight)
	const sint64 gain_per_tile = ((sint64)proto->get_capacity(freight) * freight_price +1500ll )/3000ll - 2*proto->get_maintenance();

	// find the best way
	vector_tpl<const weg_besch_t *> *ways;
	if (wt!=water_wt) {
		ways = wegbauer_t::get_way_list(wt, welt);
	}
	else {
		ways = new vector_tpl<const weg_besch_t *>(1);
		ways->append(NULL);
	}
	const uint32 capacity = proto->get_capacity(freight);
	// loop over all ways and find the best
	for(uint32 i=0; i<ways->get_count(); i++) {
		const weg_besch_t *wb = ways->operator [](i);

		// no builder -> player cannot build, ai should not build
		if (wb  &&  wb->get_builder()==NULL) continue;

		sint32 max_speed = proto->max_speed;
		if (wb && wb->get_topspeed()< max_speed) max_speed=wb->get_topspeed();

		const uint32 tiles_per_month = welt->speed_to_tiles_per_month(kmh_to_speed(max_speed));

		// number of vehicles
		const uint16 nr_vehicles = min( max(dist/8,3), (2*prod*dist) / (capacity*tiles_per_month)+1 );

		const uint32 real_tiles_per_month = (2*prod*dist) / (capacity*nr_vehicles)+1;
		// now check
		const sint64 cost_monthly = (main_buildings + (wb ? dist*wb->get_wartung(): 0)) << (welt->ticks_per_world_month_shift-18);
		const sint64 gain_per_v_m = (gain_per_tile * real_tiles_per_month * dist_paid) / dist;
		const sint64 gain_per_m   = gain_per_v_m * nr_vehicles - cost_monthly;
		if (gain_per_m > cpd->report->gain_per_m) {
			cpd->report->cost_fix                 = cost_buildings + (wb ? dist*wb->get_preis()  : 0);
			cpd->report->cost_monthly             = cost_monthly;
			cpd->report->gain_per_v_m             = gain_per_v_m;
			cpd->report->nr_vehicles              = nr_vehicles;
			cpd->report->gain_per_m               = gain_per_m;
			cpd->wb                               = wb;
		}
		sp->get_log().message("industry_connection_planner_t::plan_connection", "way: %20s(%d) gain/tile=%lld tiles/m=%d cost/m=%lld nrv=%d gain/v*m=%lld gain/m=%lld", wb ? wb->get_name() : "open water", wb ? wb->get_topspeed() : 0, gain_per_tile, tiles_per_month, cost_monthly, nr_vehicles, gain_per_v_m, gain_per_m);
	}
	delete ways;


	sp->get_log().message("industry_connection_planner_t::plan_connection","wt=%d:%20s(%d)  gain/m=%lld  vehicles=%d  cost/m=%lld", wt, cpd->wb ? cpd->wb->get_name() : "open water", cpd->wb ? cpd->wb->get_topspeed() : 0, cpd->report->gain_per_m, cpd->report->nr_vehicles, cpd->report->cost_monthly);
	return cpd;
}

/* @from ai_goods */
sint32 industry_connection_planner_t::calc_production()
{
	karte_t *welt = sp->get_welt();

	// properly calculate production & consumption
	const array_tpl<ware_production_t>& ausgang = start->get_ausgang();
	uint start_ware=0;
	while(  start_ware<ausgang.get_count()  &&  ausgang[start_ware].get_typ()!=freight  ) {
		start_ware++;
	}
	const array_tpl<ware_production_t>& eingang = ziel->get_eingang();
	uint ziel_ware=0;
	while(  ziel_ware<eingang.get_count()  &&  eingang[ziel_ware].get_typ()!=freight  ) {
		ziel_ware++;
	}
	const uint8  shift = 8 - welt->ticks_per_world_month_shift +10 +8; // >>(simfab) * welt::monatslaenge /PRODUCTION_DELTA_T +dummy
	const sint32 prod_z = (ziel->get_base_production()  *  ziel->get_besch()->get_lieferant(ziel_ware)->get_verbrauch() )>>shift;
	const sint32 prod_s = ((start->get_base_production() * start->get_besch()->get_produkt(start_ware)->get_faktor())>> shift) - ausgang[start_ware].get_stat(1, FAB_GOODS_DELIVERED);
	const sint32 prod = min(  prod_z,prod_s);

	sp->get_log().message("industry_connection_planner_t::calc_production", "prod: %d consum: %d", prod_z, prod_s);

	return prod;
}

/**
 * tries to find amphibean route from start to end:
 * ships will drive from start to the harbour
 * trucks will drive from the harbour to the end
 * @returns position of harbour (or koord3d::invalid if no such route was found)
 */
koord3d industry_connection_planner_t::get_harbour_pos(const fabrik_t* fstart, const fabrik_t* fend, koord3d &start_harbour) const
{
	// TODO: make sure that both harbours have enough distance
	start_harbour = koord3d::invalid;
	karte_t *welt = sp->get_welt();
	const uint16 station_coverage = welt->get_einstellungen()->get_station_coverage();
	// find the harbour position
	vector_tpl<koord> startplatz;
	fstart->get_tile_list( startplatz );
	ai_t::add_neighbourhood( startplatz, station_coverage);
	vector_tpl<koord3d> startplatz2;
	if(fstart->get_besch()->get_platzierung()==fabrik_besch_t::Wasser) {
		// factory in water -> we do not need to build harbour
		for( uint32 i = 0; i < startplatz.get_count(); i++ ) {
			grund_t *gr = welt->lookup_kartenboden(startplatz[i]);
			if (gr->ist_wasser()) {
				startplatz2.append( gr->get_pos() );
			}
		}
	}
	else {
		// look for suitable place of start_harbour
		for( uint32 i = 0; i < startplatz.get_count(); i++ ) {
			grund_t *gr = welt->lookup_kartenboden(startplatz[i]);
			// find a dry place for the harbour
			if (gr  &&  !gr->ist_wasser()  &&  gr->get_hoehe() == welt->get_grundwasser()  &&  gr->ist_natur()) {
				// now look for water in front of it
				grund_t *grw = welt->lookup_kartenboden(startplatz[i] - koord(gr->get_grund_hang()));
				if (grw  &&  grw->ist_wasser()) {
					startplatz2.append( grw->get_pos() );
				}
				/* maybe one day we can try this...
				for(uint8 r=0; r<4; r++) {
					grund_t *grw = welt->lookup_kartenboden(startplatz[i] + koord::nsow[r]);
					if (grw  &&  grw->ist_wasser()) {
						startplatz2.append( grw->get_pos() );
						break;
					}
				}*/
			}
		}
	}

	vector_tpl<koord> zielplatz;
	fend->get_tile_list( zielplatz );
	ai_t::add_neighbourhood( zielplatz, station_coverage );
	vector_tpl<koord3d> zielplatz2;
	for( uint32 i = 0; i < zielplatz.get_count(); i++ ) {
		grund_t *gr = welt->lookup_kartenboden(zielplatz[i]);
		if (gr  &&  !gr->ist_wasser()) {
			zielplatz2.append( gr->get_pos() );
		}
	}

	if (startplatz2.empty()  ||  zielplatz2.empty()) {
		return koord3d::invalid;
	}

	amphi_searcher_t bauigel(sp->get_welt(), sp );
	const weg_besch_t *weg_besch = wegbauer_t::weg_search(wt, 0, welt->get_timeline_year_month(), weg_t::type_flat);
	bauigel.route_fuer( (wegbauer_t::bautyp_t)wt, weg_besch, NULL, NULL );
	// we won't destroy cities (and save the money)
	bauigel.set_keep_existing_faster_ways(true);
	bauigel.set_keep_city_roads(true);
	bauigel.set_maximum(10000);
	bauigel.calc_route(startplatz2, zielplatz2);

	koord3d harbour_pos = koord3d::invalid;
	if( bauigel.get_count() > 2 ) {
		bool wasser = welt->lookup(bauigel.get_route()[0])->ist_wasser();
		for( uint32 i = 1; i < bauigel.get_count(); i++ ) {
			bool next_is_wasser = welt->lookup(bauigel.get_route()[i])->ist_wasser();
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

		// where do we have to build the start harbour
		if(fstart->get_besch()->get_platzierung()!=fabrik_besch_t::Wasser) {
			koord water_pos = (wasser ? bauigel.get_route()[0] : bauigel.get_route().back()).get_2d();
			// find the dry place
			for(uint8 r=0; r<4; r++) {
				grund_t *gr = welt->lookup_kartenboden(water_pos + koord::nsow[r]);
				if (gr  &&  gr->get_hoehe()==welt->get_grundwasser()  &&  gr->ist_natur()  &&  koord(gr->get_grund_hang())==koord::nsow[r]  &&  startplatz.is_contained(gr->get_pos().get_2d())) {
					// found!
					start_harbour = gr->get_pos();
					break;
				}
			}
		}
	}
	return harbour_pos;
}

bool industry_connection_planner_t::is_infrastructure_available(waytype_t wt, karte_t *welt, bool check_for_ways)
{
	uint16 time = welt->get_timeline_year_month();
	if (wt != water_wt) {
		// check for depots, stations, ways
		return hausbauer_t::get_random_station(haus_besch_t::depot, wt, time, 0) != NULL
			&& hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, time, haltestelle_t::WARE, hausbauer_t::generic_station )!=NULL
			&& (!check_for_ways  ||  wegbauer_t::weg_search(wt, 0, time, weg_t::type_flat) != NULL);
	}
	else {
		return hausbauer_t::get_random_station(haus_besch_t::depot, wt, time, 0) != NULL
			&& (!check_for_ways  ||  wegbauer_t::weg_search(wt, 0, time, weg_t::type_flat) != NULL)
			&& (hausbauer_t::get_random_station(haus_besch_t::hafen, wt, time, haltestelle_t::WARE, hausbauer_t::generic_station )!=NULL
				|| (check_for_ways  &&  hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, time, haltestelle_t::WARE, hausbauer_t::generic_station )!=NULL) );
	}
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
	file->rdwr_short(iwt);
	wt = (waytype_t)iwt;
}
