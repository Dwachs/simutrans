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
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	depot_pos = koord3d::invalid;
}

connector_generic_t::connector_generic_t( ai_wai_t *sp, const char *name, koord3d start_, koord3d ziel_, const weg_besch_t *wb_) :
	bt_sequential_t( sp, name )
{
	type = BT_CON_GENERIC;
	weg_besch = wb_;
	phase = 0;
	force_through = 0;
	start = start_;
	ziel = ziel_;
	depot_pos = koord3d::invalid;

	wt = weg_besch ? weg_besch->get_wtyp() : invalid_wt;

	append_child(new free_tile_searcher_t( sp, "free_tile_searcher", start));
	append_child(new free_tile_searcher_t( sp, "free_tile_searcher", ziel ));
}

void connector_generic_t::rdwr( loadsave_t *file, const uint16 version )
{
	bt_sequential_t::rdwr( file, version );
	file->rdwr_byte(phase);
	file->rdwr_byte(force_through);

	ai_t::rdwr_weg_besch(file, weg_besch);
	if (file->is_loading()) {
		wt = weg_besch ? weg_besch->get_wtyp() : invalid_wt;
	}
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
				bool ok = true;

				bauigel.calc_route(tile_list[0], tile_list[1]);
				ok = bauigel.get_count() > 2;
				if( !ok ) {
					sp->get_log().warning( "connector_generic_t::step", "didn't found a route (%s) => (%s)", start.get_str(), ziel.get_2d().get_str());

					for(uint8 i=0; i<2; i++) {
						for(uint32 j=0; j < tile_list[i].get_count(); j++) {
							sp->get_log().message( "connector_generic_t::step", "tile_list[%d][%d] = %s", i,j,tile_list[i][j].get_str());
						}
					} 
					// try to find route to through stations
					if ( force_through < 3) {
						force_through += 1;
						sp->get_log().message( "connector_generic_t::step", "force(%d) to search routes to through halts", force_through );
						for(uint8 i=0; i<2; i++) {
							tile_list[i].clear();
							through_tile_list[i].clear();
						}
						append_child(new free_tile_searcher_t( sp, "free_tile_searcher", start, force_through&1 ));
						append_child(new free_tile_searcher_t( sp, "free_tile_searcher", ziel,  force_through&2 ));
						sp->get_log().message( "connector_generic_t::step", "did not complete phase %d", phase);
						return new_return_value(RT_PARTIAL_SUCCESS);
					}
					else {
						return new_return_value(RT_TOTAL_FAIL);
					}
				}
				// find locations of stations (special search for through stations)
				uint8 found = 3 ^ through;
				if (ok) {
					for(uint8 i=0;  i<2; i++) {
						for(uint8 j=0; j<2; j++) {
							// Sometimes reverse route is the best - try both ends of the routes
							uint32 n = j==0 ? 0 : bauigel.get_count()-1;
							if( tile_list[i].is_contained( bauigel.get_route()[n]) ) { 
								// through station
								if (through & (i+1) ) {
									grund_t * gr = welt->lookup(bauigel.get_route()[n]);
									sp->get_log().message( "connector_generic_t::step", "found route to tile (%s)", gr->get_pos().get_str());
									// now find the right neighbor
									for(uint8 r=0; r<4; r++) {
										grund_t *to;
										// append the through station tile to the bauigel route
										if (gr->get_neighbour(to, wt, koord::nsow[r])) {
											sp->get_log().message( "connector_generic_t::step", "try neighbor (%s)", to->get_pos().get_str());
											if (through_tile_list[i].is_contained(to->get_pos())) {
												bool ribi_ok = !bauigel.get_route().is_contained(to->get_pos());
												if (!ribi_ok) {
													ribi_ok = ribi_t::ist_gerade(bauigel.get_route().get_ribi(n) & to->get_weg_ribi_unmasked(wt));
												}
												if (ribi_ok) {
													if (i==0) {
														start = to->get_pos();
													}
													else {
														ziel = to->get_pos();
													}
													sp->get_log().message( "connector_generic_t::step", "passt (%s)", to->get_pos().get_str());
													found |= i+1;
												}
												else {
													sp->get_log().warning( "connector_generic_t::step", "passt nicht: (%s)", to->get_pos().get_str());
												}
												// TODO: catch the else branch here
											}
										}
									}
								}
								// generic station - can be built on top of the last tile
							 	else {
									if (i==0) {
										start = bauigel.get_route()[n];
									}
									else {
										ziel = bauigel.get_route()[n];
									}
								}
							}
						}
					}
				}
				ok = ok && found == 3;
				if( !ok ) {
					sp->get_log().warning( "connector_generic_t::step", "could not find places for road stations" );
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
				sint64 cost = bauigel.calc_costs();
				if ( !sp->is_cash_available(cost) ) {
					sp->get_log().warning( "connector_generic_t::step", "route (%s) => (%s) too expensive", start.get_str(), ziel.get_2d().get_str());
					// TODO: geldbedarf merken, den connector schlafen lassen, bis es da ist
					// TODO: try later
					return new_return_value(RT_TOTAL_FAIL);
				}
				// now build the route
				bauigel.baue();
				sp->get_log().message( "connector_generic_t::step", "build route (%s) => (%s)", start.get_str(), ziel.get_2d().get_str());
				uint8 completed = 0;

				// build immediately 1x1 stations
				ok = sp->call_general_tool(WKZ_STATION, start.get_2d(), through & 1 ? through_st->get_name() : terminal_st->get_name());
				if (!ok) {
					sp->get_log().warning( "connector_generic_t::step", "failed to built station at (%s)", start.get_str() );
				}
				else {
					completed = 1;
					ok = sp->call_general_tool(WKZ_STATION, ziel.get_2d(), through & 2 ? through_st->get_name() : terminal_st->get_name());
					if (!ok) {
						sp->get_log().warning( "connector_generic_t::step", "failed to built station at (%s)", ziel.get_str() );
					}
				}
				if (!ok) {
					// try undo
					switch (completed) {
						case 1:
							// remove start station
							sp->call_general_tool(WKZ_REMOVER, start.get_2d(), "");
							// fall-through
						case 0:
						default:
							if ( sp->is_cash_available(cost) ) {
								sp->undo();
							}
					}
					sp->get_log().warning( "connector_generic_t::step", "road no 1: (%s) no N-1: (%s)", bauigel.get_route()[1].get_2d().get_str(), bauigel.get_route()[bauigel.get_count()-2].get_str() );
					return new_return_value(RT_TOTAL_FAIL);
				}
				// TODO: station so erweitern, dass Kapazitaet groesser als Kapazitaet eines einzelnen Convois
				/*
				append_child( new builder_road_station_t( sp, "builder_road_station_t", start, ware_besch ) );
				append_child( new builder_road_station_t( sp, "builder_road_station_t", ziel, ware_besch ) );
				*/
				

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
						ok = sp->call_general_tool(WKZ_DEPOT, depot_pos.get_2d(), dep->get_name());
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

void connector_generic_t::debug( log_t &file, string prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("cong","%s phase=%d", prefix.c_str(), phase);
}
