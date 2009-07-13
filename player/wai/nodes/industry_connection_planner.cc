#include "industry_connection_planner.h"

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

return_value_t *industry_connection_planner_t::step()
{
	if(sp->get_industry_manager()->is_connection<unplanable>(start, ziel, freight)) {		
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
		sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	// check for depots, station
	const haus_besch_t* st  = hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::generic_station );
	const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, wt, sp->get_welt()->get_timeline_year_month(), 0);
	if (st==NULL || dep ==NULL) {
		sp->get_log().warning("industry_connection_planner_t::step", "no %s%s available for waytype=%d", (st==NULL?"station ":""), (dep==NULL?"depot":""), wt);
		sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	// cost for stations
	sint64 cost_buildings = 2*calc_building_cost(st) + calc_building_cost(dep);
	// TODO: maintenance of these buildings

	// estimate production
	sint32 prod = calc_production();
	if (prod<0) {
		sp->get_log().warning("industry_connection_planner_t::step","production = %d", prod);
		sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}


	// get a vehicle
	simple_prototype_designer_t* d = new simple_prototype_designer_t(sp);
	d->freight = freight;
	d->include_electric = wt != road_wt;
	d->max_length = 1;
	d->max_weight = 0xffffffff;
	d->min_speed  = 1;
	d->not_obsolete = false;
	d->wt = wt;

	d->update();

	if (d->proto->is_empty()) {
		sp->get_log().warning("industry_connection_planner_t::step","no vehicle found for waytype %d and freight %s", wt, freight->get_name());
		sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}

	// electrification available?
	const way_obj_besch_t *e = NULL;
	if (d->proto->is_electric()) {
		e = wayobj_t::wayobj_search(wt,overheadlines_wt,sp->get_welt()->get_timeline_year_month());
		if (!e) {
			sp->get_log().warning("industry_connection_planner_t::step","no electrification found for waytype %d", wt);
			sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
			return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
		}
	}
	// update way
	wb = wegbauer_t::weg_search(wt, d->proto->max_speed, sp->get_welt()->get_timeline_year_month(), weg_t::type_flat );

	bool include_ships = false;
	koord3d harbour_pos = koord3d::invalid;
	simple_prototype_designer_t* d2 = new simple_prototype_designer_t(sp);
	uint16 nr_ships = 0;

	if(start->get_besch()->get_platzierung()==fabrik_besch_t::Wasser || ziel->get_besch()->get_platzierung()==fabrik_besch_t::Wasser) {
		if(start->get_besch()->get_platzierung()!=fabrik_besch_t::Wasser || ziel->get_besch()->get_platzierung()==fabrik_besch_t::Wasser) {
			sp->get_log().warning("industry_connection_planner_t::step", "only oil rigs supported yet");
			sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
			return new_return_value(RT_TOTAL_FAIL);
		}
		sp->get_log().warning("industry_connection_planner_t::step", "start factory at water side spotted");
		include_ships = true;

		// check for ship depots, dock
		const haus_besch_t* st  = hausbauer_t::get_random_station(haus_besch_t::generic_stop, water_wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::generic_station );
		const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, water_wt, sp->get_welt()->get_timeline_year_month(), 0);
		if (st==NULL || dep ==NULL) {
			sp->get_log().warning("industry_connection_planner_t::step", "no %s%s available for waytype=%d", (st==NULL?"station ":""), (dep==NULL?"depot":""), water_wt);
			sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
			return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
		}
		// cost for stations
		cost_buildings += 2*calc_building_cost(st) + calc_building_cost(dep);

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

		if( bauigel.max_n > 1 ) {
			bool wasser = sp->get_welt()->lookup(bauigel.get_route()[0])->ist_wasser();
			for( sint32 i = 1; i <= bauigel.max_n; i++ ) {
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
		else {
			sp->get_log().warning("industry_connection_planner_t::step", "Keine Amphibienroute");
			sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
			return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
		}

		d2->freight = freight;
		d2->include_electric = false;
		d2->max_length = 1;
		d2->max_weight = 0xffffffff;
		d2->min_speed  = 1;
		d2->not_obsolete = false;
		d2->wt = water_wt;

		d2->update();

		if (d2->proto->is_empty()) {
			sp->get_log().warning("industry_connection_planner_t::step","no vehicle found for waytype %d and freight %s", water_wt, freight->get_name());
			sp->get_industry_manager()->set_connection<forbidden>(start, ziel, freight);
			return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
		}
		const uint32 dist = koord_distance(harbour_pos, ziel->get_pos());

		uint32 max_speed = d2->proto->max_speed;
		const uint32 tiles_per_month = (kmh_to_speed(max_speed) * sp->get_welt()->ticks_per_tag) >> (8+12); // 12: fahre_basis, 8: 2^8 steps per tile

		// number of vehicles
		nr_ships = min( min(254,dist), (2*prod*dist) / (d2->proto->get_capacity(freight)*tiles_per_month)+1 );
	}

	// calculate distance
	const uint32 dist = koord_distance( include_ships ? harbour_pos : start->get_pos(), ziel->get_pos());

	uint32 max_speed = min( d->proto->max_speed, wb->get_topspeed() );
	if (e) max_speed = min( max_speed, e->get_topspeed());
	const uint32 tiles_per_month = (kmh_to_speed(max_speed) * sp->get_welt()->ticks_per_tag) >> (8+12); // 12: fahre_basis, 8: 2^8 steps per tile

	// speed bonus calculation see vehikel_t::calc_gewinn
	const sint32 ref_speed = sp->get_welt()->get_average_speed(wt );
	const sint32 speed_base = (100*speed_to_kmh(d->proto->min_top_speed))/ref_speed-100;
	const sint32 freight_price = freight->get_preis() * max( 128, 1000+speed_base*freight->get_speed_bonus());

	// net gain per tile (matching freight)
	const sint64 gain_per_tile = ((sint64)d->proto->get_capacity(freight) * freight_price +1500ll )/3000ll - 2*d->proto->get_maintenance();

	// number of vehicles
	const uint16 nr_vehicles = min( min(254,dist), (2*prod*dist) / (d->proto->get_capacity(freight)*tiles_per_month)+1 );

	// create report
	// TODO: maintenance for depots, stations
	// TODO: save the prototype-designer somewhere
	report = new report_t();
	report->cost_fix                 = dist*( wb->get_preis() + (e!=NULL ? e->get_preis() : 0) ) + cost_buildings;
	report->cost_monthly             = dist*( wb->get_wartung() + (e!=NULL ? e->get_wartung() : 0) );
	report->cost_monthly_per_vehicle = 0;
	report->cost_per_vehicle         = 0;
	report->gain_per_v_m             = gain_per_tile * tiles_per_month ;
	report->nr_vehicles              = nr_vehicles;
	report->nr_ships                 = nr_ships;

	if( include_ships ) {
		bt_sequential_t *action = new bt_sequential_t( sp, "bt_sequential mit road+ship" );
		action->append_child( new connector_road_t(sp, "connector_road_t", start, ziel, wb, d, nr_vehicles, NULL, harbour_pos) );
		// TODO: was passiert, wenn der road-connector seine Route nicht bauen kann?
		action->append_child( new connector_ship_t(sp, "connector_ship_t", start, ziel, d2, nr_ships, harbour_pos) );
		report->action = action;
	}
	else {
		report->action = new connector_road_t(sp, "connector_road_t", start, ziel, wb, d, nr_vehicles, NULL);
	}

	sp->get_log().message("industry_connection_planner_t::step","report delivered, gain /v /m = %d", report->gain_per_v_m);

	sp->get_industry_manager()->set_connection<planned>(start, ziel, freight);

	return new_return_value(RT_TOTAL_SUCCESS);
}


/* @from ai_goods */
sint32 industry_connection_planner_t::calc_production()
{
	karte_t *welt = sp->get_welt();
	// calculate distance
	koord zv = start->get_pos().get_2d() - ziel->get_pos().get_2d();
	uint32 dist = koord_distance(zv, koord(0,0));

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


sint64 industry_connection_planner_t::calc_building_cost(const haus_besch_t* st)
{
	switch(st->get_utyp()) {
		case haus_besch_t::generic_stop: {
			sint64 cost = - st->get_level()*st->get_b()*st->get_h();
			switch(wt) {
				case road_wt:
					cost *= sp->get_welt()->get_einstellungen()->cst_multiply_roadstop;
					break;
				case track_wt:
				case monorail_wt:
				case maglev_wt:
				case narrowgauge_wt:
				case tram_wt:
					cost *= sp->get_welt()->get_einstellungen()->cst_multiply_station;
					break;
				case water_wt:
					cost *= sp->get_welt()->get_einstellungen()->cst_multiply_dock;
					break;
				case air_wt:
					cost *= sp->get_welt()->get_einstellungen()->cst_multiply_airterminal;
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
					return -sp->get_welt()->get_einstellungen()->cst_depot_road;
				case track_wt:
				case monorail_wt:
				case tram_wt:
				case maglev_wt:
				case narrowgauge_wt:
					return -sp->get_welt()->get_einstellungen()->cst_depot_rail;
				case water_wt:
					return -sp->get_welt()->get_einstellungen()->cst_depot_ship;
				case air_wt:
					return -sp->get_welt()->get_einstellungen()->cst_depot_air;
				default:
					assert(0);
					return 0;
			}
		default:
			assert(0);
			return 0;
	}
}


void industry_connection_planner_t::rdwr( loadsave_t* file, const uint16 version)
{
	planner_t::rdwr(file, version);
	ai_t::rdwr_fabrik(file, sp->get_welt(), start);
	ai_t::rdwr_fabrik(file, sp->get_welt(), ziel);
	ai_t::rdwr_ware_besch(file, freight);

	sint16 iwt = wt;
	file->rdwr_short(iwt,"");
	wt = (waytype_t)iwt;
}
