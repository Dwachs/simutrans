#include "connector_generic.h"


#include "connector_road.h"

#include "free_tile_searcher.h"

#include "../bt.h"
#include "../datablock.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simmenu.h"
#include "../../../simmesg.h"
#include "../../../bauer/brueckenbauer.h"
#include "../../../bauer/hausbauer.h"
#include "../../../bauer/tunnelbauer.h"
#include "../../../bauer/wegbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/loadsave.h"

connector_generic_t::connector_generic_t( ai_wai_t *sp, const char *name) :
	bt_sequential_t( sp, name)
{
	type = BT_CON_GENERIC;
	weg_besch = NULL;
	phase = 0;
	force_through = 0;
	target_start = koord3d::invalid;
	target_ziel = koord3d::invalid;
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	depot_pos = koord3d::invalid;
	station_length = 1;
}

connector_generic_t::connector_generic_t( ai_wai_t *sp, const char *name, koord3d start_, koord3d ziel_, const weg_besch_t *wb_, uint16 station_length_) :
	bt_sequential_t( sp, name )
{
	type = BT_CON_GENERIC;
	weg_besch = wb_;
	phase = 0;
	force_through = 0;
	target_start = start_;
	target_ziel = ziel_;
	start = target_start;
	ziel = target_ziel;
	depot_pos = koord3d::invalid;
	station_length = station_length_;

	wt = weg_besch ? weg_besch->get_wtyp() : invalid_wt;

	append_child(new free_tile_searcher_t( sp, "free_tile_searcher", wt, target_start, false, station_length));
	append_child(new free_tile_searcher_t( sp, "free_tile_searcher", wt, target_ziel , false, station_length));
}

void connector_generic_t::rdwr( loadsave_t *file, const uint16 version )
{
	bt_sequential_t::rdwr( file, version );
	file->rdwr_byte(phase);
	file->rdwr_byte(force_through);
	file->rdwr_short(station_length);

	ai_t::rdwr_weg_besch(file, weg_besch);
	if (file->is_loading()) {
		wt = weg_besch ? weg_besch->get_wtyp() : invalid_wt;
	}
	target_start.rdwr(file);
	target_ziel.rdwr(file);
	start.rdwr(file);
	ziel.rdwr(file);
	depot_pos.rdwr(file);
	tile_list[0].rdwr(file);
	tile_list[1].rdwr(file);
	through_tile_list[0].rdwr(file);
	through_tile_list[1].rdwr(file);
}

void connector_generic_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);
	start.rotate90(y_size);
	target_ziel.rotate90(y_size);
	target_start.rotate90(y_size);
	ziel.rotate90(y_size);
	depot_pos.rotate90(y_size);
	tile_list[0].rotate90(y_size);
	tile_list[1].rotate90(y_size);
	through_tile_list[0].rotate90(y_size);
	through_tile_list[1].rotate90(y_size);
}

return_value_t *connector_generic_t::step()
{
	//	sp->get_log().warning("connector_generic_t::step", "%s %s disappeared", fab1.is_bound() ? "" : "start", fab2.is_bound() ? "" : "ziel");
	//	return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	karte_t *welt = sp->get_welt();
	if( childs.empty() ) {
		datablock_t *data = NULL;
		sp->get_log().message("connector_generic_t::step", "phase %d of build route (%s) => (%s)", phase, start.get_str(), ziel.get_2d().get_str());
		switch(phase) {
			case 0: {
				koord3d start_station_end, ziel_station_end;
				koord3d start_station_front, ziel_station_front;
				// need through station?
				uint8 through = 0;
				if( !through_tile_list[0].empty() ) {
					through |= 1;
				}
				if( !through_tile_list[1].empty() ) {
					through |= 2;
				}
				// Our first step -> calc the route.

				// Test which tiles are the best:
				wegbauer_t bauigel(welt, sp );
				bauigel.route_fuer((wegbauer_t::bautyp_t)wt, weg_besch,
					tunnelbauer_t::find_tunnel(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()),
					brueckenbauer_t::find_bridge(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()) );
				// we won't destroy cities (and save the money)
				bauigel.set_keep_existing_faster_ways(true);
				bauigel.set_keep_city_roads(true);
				bauigel.set_maximum(10000);

				bauigel.calc_route(tile_list[0], tile_list[1]);

				// now try route with terraforming
				wegbauer_t baumaulwurf(welt, sp);
				baumaulwurf.route_fuer(wegbauer_t::terraform_flag|(wegbauer_t::bautyp_t)wt, weg_besch,
					tunnelbauer_t::find_tunnel(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()),
					brueckenbauer_t::find_bridge(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()) );
				baumaulwurf.set_keep_existing_faster_ways(true);
				baumaulwurf.set_keep_city_roads(true);
				baumaulwurf.set_maximum(10000);
				baumaulwurf.calc_route(tile_list[0], tile_list[1]);

				// build with terraforming if shorter and enough money is available
				bool with_tf = (baumaulwurf.get_count() > 2)  &&  (10*baumaulwurf.get_count() < 9*bauigel.get_count()  ||  bauigel.get_count() <= 2);
				if (with_tf) {
					with_tf = sp->is_cash_available( baumaulwurf.calc_costs() );
				}
				wegbauer_t &bautier = with_tf ? baumaulwurf : bauigel;
				// TODO: check whether both lists overlap!
				bool ok = bautier.get_count() > 2;
				if( !ok ) {
					sp->get_log().warning( "connector_generic_t::step", "didn't found a route (%s) => (%s)", start.get_str(), ziel.get_2d().get_str());

					// try to find route to through stations
					if ( force_through < 3) {
						force_through += 1;
						sp->get_log().message( "connector_generic_t::step", "force(%d) to search routes to through halts", force_through );
						for(uint8 i=0; i<2; i++) {
							tile_list[i].clear();
							through_tile_list[i].clear();
						}
						append_child(new free_tile_searcher_t( sp, "free_tile_searcher", wt, start, force_through&1 ));
						append_child(new free_tile_searcher_t( sp, "free_tile_searcher", wt, ziel,  force_through&2 ));
						sp->get_log().message( "connector_generic_t::step", "did not complete phase %d", phase);
						return new_return_value(RT_PARTIAL_SUCCESS);
					}
					else {
						return new_return_value(RT_TOTAL_FAIL);
					}
				}
				// find locations of stations (special search for through stations)
				uint8 found = 3 ^ through;

				for(uint8 i=0;  i<2; i++) {
					for(uint8 j=0; j<2; j++) {
						// Sometimes reverse route is the best - try both ends of the routes
						uint32 n = j==0 ? 0 : bautier.get_count()-1;
						const koord3d &route_pos = bautier.get_route()[n];
						for(uint32 k=0; k<tile_list[i].get_count(); k++) {
							if (tile_list[i][k] != route_pos) {
								continue;
							}
							// through station
							if (through & (i+1) ) {
								// through station begins here
								const koord3d &through_pos = through_tile_list[i][k];
								// .. and goes into this direction
								const koord dir = koord(ribi_typ( through_pos-route_pos ));
								sp->get_log().message( "connector_generic_t::step", "found route to tile (%s), station tile at (%s)", route_pos.get_str(), through_pos.get_2d().get_str());

								bool ribi_ok = !bautier.get_route().is_contained(through_pos);

								if (station_length == 1) {
									if (!ribi_ok) {
										grund_t* to = welt->lookup( through_pos );
										ribi_ok = ribi_t::ist_gerade(bautier.get_route().get_ribi(n) & to->get_weg_ribi_unmasked(wt));
									}
								}
								else {
									// track backwards
									koord3d pos = through_pos;
									for(uint16 l=1; l<station_length  &&  ribi_ok; l++) {
										pos += dir;
										ribi_ok = !bautier.get_route().is_contained(pos);
									}
									if (ribi_ok) {
										if (i==0) {
											start_station_end = pos;
											start_station_front = route_pos;
										}
										else {
											ziel_station_end = pos;
											ziel_station_front = route_pos;
										}
									}
								}
								if (ribi_ok) {
									if (i==0) {
										start = through_pos;
									}
									else {
										ziel = through_pos;
									}
									sp->get_log().message( "connector_generic_t::step", "passt (%s)", through_pos.get_str());
									found |= i+1;
								}
								else {
									sp->get_log().warning( "connector_generic_t::step", "passt nicht: (%s)", through_pos.get_str());
								}
							}
							// generic station - can be built on top of the last tile
						 	else {
								if (i==0) {
									start = route_pos;
								}
								else {
									ziel = route_pos;
								}
								// tile is unique
								break;
							}
						}
					}
				}

				ok = found == 3;
				if( !ok ) {
					// TODO: if route is found but no station position, remove start/end tiles from tile lists
					//       and restart route finding
					sp->get_log().warning( "connector_generic_t::step", "could not find places for stations (wt=%d)", wt );
					return new_return_value(RT_TOTAL_FAIL);
				}
				// get a suitable station
				const haus_besch_t* terminal_st = hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, welt->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::terminal_station );
				const haus_besch_t* through_st = hausbauer_t::get_random_station(haus_besch_t::generic_stop, wt, welt->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::through_station );
				if (terminal_st==NULL) {
					terminal_st = through_st;
				}
				ok = (through_st!=NULL) || (through==0);
				if (!ok) {
					sp->get_log().warning( "connector_generic_t::step", "no suitable station found" );
					return new_return_value(RT_TOTAL_FAIL);
				}

				// kontostand checken
				sint64 cost = bautier.calc_costs();
				if ( !sp->is_cash_available(cost) ) {
					sp->get_log().warning( "connector_generic_t::step", "route (%s) => (%s) too expensive", start.get_str(), ziel.get_2d().get_str());
					// TODO: geldbedarf merken, den connector schlafen lassen, bis es da ist
					// TODO: try later
					return new_return_value(RT_TOTAL_FAIL);
				}
				// now build the route
				bautier.baue();
				sp->get_log().message( "connector_generic_t::step", "build route (%s) => (%s)", start.get_str(), ziel.get_2d().get_str());
				uint8 completed = 0;
				// save route to be able to undo it
				vector_tpl<koord3d> undo_route, undo_way_start, undo_way_ziel;
				waytype_t dummy_wt = wt;
				sp->swap_undo(dummy_wt, undo_route);

				// initialize for station_length == 1
				if (station_length == 1) {
					start_station_front = koord3d::invalid;
					start_station_end = start;
					ziel_station_front = koord3d::invalid;
					ziel_station_end = ziel;
				}

				ok = build_station(target_start, start_station_front, start_station_end, through & 1 ? through_st : terminal_st, undo_way_start);
				if (ok) {
					completed++;
					ok = build_station(target_ziel, ziel_station_front, ziel_station_end, through & 2 ? through_st : terminal_st, undo_way_ziel);
				}
				if (!ok) {
					// try undo
					switch (completed) {
						case 1:
							// remove start station
							remove_station(undo_way_start);
							// fall-through
						case 0:
						default:
							if ( sp->is_cash_available(cost) ) {
								sp->swap_undo(dummy_wt, undo_route);
								sp->undo();
							}
					}
					sp->get_log().warning( "connector_generic_t::step", "road no 1: (%s) no N-1: (%s)", bautier.get_route()[1].get_2d().get_str(), bautier.get_route()[bautier.get_count()-2].get_str() );
					return new_return_value(RT_TOTAL_FAIL);
				}
				// TODO: station so erweitern, dass Kapazitaet groesser als Kapazitaet eines einzelnen Convois


				break;
			}
			case 1: {
				// build depot
				// TODO: do something smarter here
				vector_tpl<koord> tiles;
				tiles.append(start.get_2d());
				ai_t::add_neighbourhood( tiles, 5 );
				vector_tpl<koord3d> dep_ziele, dep_exist;
				for( uint32 j = 0; j < tiles.get_count(); j++ ) {
					const grund_t* gr = welt->lookup_kartenboden( tiles[j] );
					// FiX this for wt=water
					if(  gr  &&  gr->hat_weg(wt) && gr->get_weg(wt)->get_besitzer()==sp && gr->get_depot()) {
						dep_exist.append_unique( gr->get_pos() );
					}
					if(  gr  &&  gr->get_grund_hang() == hang_t::flach  &&  !gr->get_leitung()  &&  !gr->hat_wege()){
						dep_ziele.append_unique( gr->get_pos() );
					}
				}
				vector_tpl<koord3d> dep_start;
				dep_start.append(start);
				bool ok = false;
				// Test which tiles are the best:
				wegbauer_t bauigel(welt, sp );
				bauigel.route_fuer( (wegbauer_t::bautyp_t)wt, weg_besch,
					tunnelbauer_t::find_tunnel(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()),
					brueckenbauer_t::find_bridge(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()) );
				// we won't destroy cities (and save the money)
				bauigel.set_keep_existing_faster_ways(true);
				bauigel.set_keep_city_roads(true);
				bauigel.set_maximum(10000);
				bauigel.calc_route(dep_start, !dep_exist.empty() ? dep_exist : dep_ziele);
				if(bauigel.get_count() >= 2) {
					// Sometimes reverse route is the best, so we have to change the koords.
					depot_pos =  ( start == bauigel.get_route()[0]) ? bauigel.get_route()[bauigel.get_count()-1] : bauigel.get_route()[0];
					ok = true;
				}
				const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, wt, welt->get_timeline_year_month(), 0);
				ok = ok && (dep!=NULL || welt->lookup(depot_pos)->get_depot() );
				if (ok) {
					bauigel.baue();
					// built depot
					if ( welt->lookup(depot_pos)->get_depot()==NULL ) {
						ok = sp->call_general_tool(WKZ_DEPOT, depot_pos, dep->get_name());
					}
				}
				else {
					sp->get_log().warning( "connector_road::step()","depot building failed");
					return new_return_value(RT_TOTAL_FAIL);
				}
				// deliver positions of stations and depots
				data = new datablock_t();
				data->pos1.append(start);
				data->pos1.append(ziel);
				data->pos1.append(depot_pos);
				break;
			}
		}
		sp->get_log().message( "connector_generic_t::step", "completed phase %d", phase);
		return_value_t *rv = new_return_value(phase>=1 ? RT_TOTAL_SUCCESS : RT_PARTIAL_SUCCESS);
		rv->data = data;

		phase ++;
		return rv;
	}
	else {
		// Step the childs.
		return_value_t *rv = bt_sequential_t::step();
		if( rv->type == BT_FREE_TILE ) {
			uint8 i = 1;
			if( tile_list[0].empty() && through_tile_list[0].empty() ) {
				i = 0;
			}
			swap<koord3d>( tile_list[i], rv->data->pos1 );
			swap<koord3d>( through_tile_list[i], rv->data->pos2 );
			delete rv;
			return new_return_value( RT_PARTIAL_SUCCESS );
		}
		else {
			return rv;
		}
	}
}


bool connector_generic_t::build_station(koord3d target, koord3d front, koord3d last, const haus_besch_t* station, vector_tpl<koord3d>& undo_route) const
{
	sp->get_log().warning( "connector_generic_t::build_station", "build station front (%s) last (%s)", front.get_str(), last.get_2d().get_str());
	sp->get_log().warning( "connector_generic_t::build_station", "target (%s)", target.get_str());
	karte_t *welt = sp->get_welt();
	undo_route.clear();
	if (station_length > 1) {
		// first build way underneath
		wegbauer_t bauigel(welt, sp );
		bauigel.route_fuer((wegbauer_t::bautyp_t)wt, weg_besch,
				tunnelbauer_t::find_tunnel(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()),
				brueckenbauer_t::find_bridge(wt,weg_besch->get_topspeed(),welt->get_timeline_year_month()) );
		// build first station
		bauigel.calc_straight_route(front, last);
		bauigel.baue();
		waytype_t dummy_wt = wt;
		sp->swap_undo(dummy_wt, undo_route);
	}
	koord dir = koord( ribi_typ( last-front ) );
	uint8 start_index = 0;
	vector_tpl<koord3d> station_tiles;
	if (station_length > 1) {
		koord3d pos = front + dir;
		// now figure out, which tile to build first,
		// to be connected to a factory / halt
		grund_t *gr = welt->lookup(target);
		// is there a factory?
		gebaeude_t *gb = gr ? gr->find<gebaeude_t>() : NULL;
		fabrik_t *fab = gb ? gb->get_fabrik() : NULL;
		if (fab == NULL) {
			sp->get_log().warning( "connector_generic_t::build_station", "no factory");
			goto ready;
		}
		for (uint8 l = 0; l < station_length; l++, pos+=dir) {
			// test all neighbors
			for(uint8 r=0; r<8; r++) {
				halthandle_t halt = haltestelle_t::get_halt( welt, pos.get_2d() + koord::neighbours[r], sp );
				// now test if it is halt connected to factory
				if (halt.is_bound()) {
					sp->get_log().warning( "connector_generic_t::build_station", "found halt %s near (%s)", halt->get_name(), pos.get_str());
				}
				if (halt.is_bound()  &&  halt->get_fab_list().is_contained( fab )) {
					start_index = l;
					goto ready;
				}
			}

		}
ready:;
	}
	sp->get_log().warning( "connector_generic_t::build_station", "start index %d", start_index);

	koord3d first = station_length>1  ?  front + dir : last;
	bool ok = true;
	for(sint16 l = start_index; l < station_length  &&  ok; l++) {
		const koord3d pos = first + dir*l;
		ok = sp->call_general_tool(WKZ_STATION, pos, station->get_name());
		if (ok) {
			station_tiles.append( pos );
		}
		sp->get_log().warning( "connector_generic_t::build_station", "ok %d %d (%s)", ok, start_index, pos.get_str());
	}
	for(sint16 l = start_index-1; l >=0  &&  ok; l--) {
		const koord3d pos = first + dir*l;
		ok = sp->call_general_tool(WKZ_STATION, pos, station->get_name());
		if (ok) {
			station_tiles.append( pos );
		}
		sp->get_log().warning( "connector_generic_t::build_station", "ok %d %d (%s)", ok, start_index, pos.get_str());
	}
	// in case of fail remove station tiles & ways
	if (!ok) {
		for(uint32 i=0; i<station_tiles.get_count(); i++) {
			grund_t* gr = welt->lookup_kartenboden( station_tiles[i].get_2d() );
			while (gr->get_halt().is_bound()  &&  sp->call_general_tool(WKZ_REMOVER, station_tiles[i], ""));
		}
		waytype_t dummy_wt = wt;
		sp->swap_undo(dummy_wt, undo_route);
		sp->undo();
	}
	return ok;
}


void connector_generic_t::remove_station(vector_tpl<koord3d>& undo_route) const
{
	karte_t *welt = sp->get_welt();
	// remove station tiles
	for(uint32 i=0; i<undo_route.get_count(); i++) {
		const koord3d pos = undo_route[i];
		grund_t* gr = welt->lookup( pos );
		while (gr->get_halt().is_bound()  &&  sp->call_general_tool(WKZ_REMOVER, pos, ""));
	}
	// remove the way
	waytype_t dummy_wt = wt;
	sp->swap_undo(dummy_wt, undo_route);
	sp->undo();
	// swap undo history back
	sp->swap_undo(dummy_wt, undo_route);
}


void connector_generic_t::debug( log_t &file, string prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("cong","%s phase=%d", prefix.c_str(), phase);
}
