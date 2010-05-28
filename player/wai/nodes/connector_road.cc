#include "connector_road.h"

#include "builder_road_station.h"
#include "builder_way_obj.h"
#include "free_tile_searcher.h"
#include "vehikel_builder.h"

#include "../bt.h"
#include "../datablock.h"
#include "../vehikel_prototype.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simmesg.h"
#include "../../../bauer/brueckenbauer.h"
#include "../../../bauer/hausbauer.h"
#include "../../../bauer/tunnelbauer.h"
#include "../../../bauer/wegbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/loadsave.h"

connector_road_t::connector_road_t( ai_wai_t *sp, const char *name) :
	bt_sequential_t( sp, name), fab1(NULL, sp), fab2(NULL, sp)
{
	type = BT_CON_ROAD;
	road_besch = NULL;
	prototyper = NULL;
	nr_vehicles = 0;
	phase = 0;
	force_through = 0;
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	e = NULL;
	harbour_pos = koord3d::invalid;
}

connector_road_t::connector_road_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1_, const fabrik_t *fab2_, const weg_besch_t *road_besch_, simple_prototype_designer_t *d, uint16 nr_veh, const way_obj_besch_t *e_, const koord3d harbour_pos_ ) :
	bt_sequential_t( sp, name ), fab1(fab1_, sp), fab2(fab2_, sp)
{
	type = BT_CON_ROAD;
	road_besch = road_besch_;
	prototyper = d;
	nr_vehicles = nr_veh;
	phase = 0;
	force_through = 0;
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	e = e_;
	harbour_pos = harbour_pos_;


	if( harbour_pos != koord3d::invalid ) {
		const grund_t *gr = sp->get_welt()->lookup(harbour_pos);
		tile_list[0].append( harbour_pos + koord(gr->get_grund_hang()) + koord3d(0,0,1) );
	}
	else {
		append_child(new free_tile_searcher_t( sp, "free_tile_searcher", fab1->get_pos() ));
	}
	append_child(new free_tile_searcher_t( sp, "free_tile_searcher", fab2->get_pos() ));
}

connector_road_t::~connector_road_t()
{
	if (prototyper && phase<=2) delete prototyper;
	prototyper = NULL;
}

void connector_road_t::rdwr( loadsave_t *file, const uint16 version )
{
	bt_sequential_t::rdwr( file, version );
	file->rdwr_byte(phase, "");
	file->rdwr_byte(force_through, "");
	fab1.rdwr(file, version, sp);
	fab2.rdwr(file, version, sp);
	ai_t::rdwr_weg_besch(file, road_besch);
	file->rdwr_short(nr_vehicles, "");
	if (phase<=2) {
		if (file->is_loading()) {
			prototyper = new simple_prototype_designer_t(sp);
		}
		prototyper->rdwr(file);
	}
	else {
		prototyper = NULL;
	}
	start.rdwr(file);
	ziel.rdwr(file);
	deppos.rdwr(file);
	harbour_pos.rdwr(file);
	tile_list[0].rdwr(file);
	tile_list[1].rdwr(file);
	through_tile_list[0].rdwr(file);
	through_tile_list[1].rdwr(file);
	/*
	 * TODO: e speichern
	 */
}

void connector_road_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);
	start.rotate90(y_size);
	ziel.rotate90(y_size);
	deppos.rotate90(y_size);
	harbour_pos.rotate90(y_size);
	tile_list[0].rotate90(y_size);
	tile_list[1].rotate90(y_size);
	through_tile_list[0].rotate90(y_size);
	through_tile_list[1].rotate90(y_size);
}

return_value_t *connector_road_t::step()
{
	if(!fab1.is_bound()  ||  !fab2.is_bound()) {	
		sp->get_log().warning("connector_road_t::step", "%s %s disappeared", fab1.is_bound() ? "" : "start", fab2.is_bound() ? "" : "ziel");
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	if( childs.empty() ) {
		datablock_t *data = NULL;
		sp->get_log().message("connector_road_t::step", "phase %d of build route %s => %s", phase, fab1->get_name(), fab2->get_name() );
		switch(phase) {
			case 0: {
				// need through station?
				uint through = 0;
				if( !through_tile_list[0].empty() ) {
					through |= 1;
				}
				if( !through_tile_list[1].empty() ) {
					through |= 2;
				}
				// Our first step -> calc the route.

				// Test which tiles are the best:
				wegbauer_t bauigel(sp->get_welt(), sp );
				bauigel.route_fuer( wegbauer_t::strasse, road_besch, tunnelbauer_t::find_tunnel(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()), brueckenbauer_t::find_bridge(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()) );
				// we won't destroy cities (and save the money)
				bauigel.set_keep_existing_faster_ways(true);
				bauigel.set_keep_city_roads(true);
				bauigel.set_maximum(10000);
				bool ok = true;

				bauigel.calc_route(tile_list[0], tile_list[1]);
				ok = bauigel.get_count() > 2;
				if( !ok ) {
					sp->get_log().warning( "connector_road_t::step", "didn't found a route %s => %s", fab1->get_name(), fab2->get_name() );
					sp->get_log().message( "connector_road_t::step", "harbour pos = (%s)", harbour_pos.get_str() );

					for(uint8 i=0; i<2; i++) {
						for(uint32 j=0; j < tile_list[i].get_count(); j++) {
							sp->get_log().message( "connector_road_t::step", "tile_list[%d][%d] = %s", i,j,tile_list[i][j].get_str());
						}
					} 
					// try to find route to through stations
					if ( force_through + (harbour_pos != koord3d::invalid ? 1 :0) < 3) {
						force_through += harbour_pos!=koord3d::invalid ? 2 : 1;
						sp->get_log().message( "connector_road_t::step", "force(%d) to search routes to through halts", force_through );
						for(uint8 i=0; i<2; i++) {
							tile_list[i].clear();
							through_tile_list[i].clear();
						}
						if( harbour_pos != koord3d::invalid ) {
							const grund_t *gr = sp->get_welt()->lookup(harbour_pos);
							tile_list[0].append( harbour_pos + koord(gr->get_grund_hang()) + koord3d(0,0,1) );
						}
						else {
							append_child(new free_tile_searcher_t( sp, "free_tile_searcher", fab1->get_pos(), force_through&1 ));
						}
						append_child(new free_tile_searcher_t( sp, "free_tile_searcher", fab2->get_pos(), force_through&2 ));
						sp->get_log().message( "connector_road_t::step", "did not complete phase %d", phase);
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
									grund_t * gr = sp->get_welt()->lookup(bauigel.get_route()[n]);
									sp->get_log().message( "connector_road_t::step", "found route to tile (%s)", gr->get_pos().get_str());
									// now find the right neighbor
									for(uint8 r=0; r<4; r++) {
										grund_t *to;
										// append the through station tile to the bauigel route
										if (gr->get_neighbour(to, road_wt, koord::nsow[r])) {
											sp->get_log().message( "connector_road_t::step", "try neighbor (%s)", to->get_pos().get_str());
											if (through_tile_list[i].is_contained(to->get_pos())) {
												bool ribi_ok = !bauigel.get_route().is_contained(to->get_pos());
												if (!ribi_ok) {
													ribi_ok = ribi_t::ist_gerade(bauigel.get_route().get_ribi(n) & to->get_weg(road_wt)->get_ribi_unmasked());
												}
												if (ribi_ok) {
													if (i==0) {
														start = to->get_pos();
													}
													else {
														ziel = to->get_pos();
													}
													sp->get_log().message( "connector_road_t::step", "passt (%s)", to->get_pos().get_str());
													found |= i+1;
												}
												else {
													sp->get_log().warning( "connector_road_t::step", "passt nicht: (%s)", to->get_pos().get_str());
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
					sp->get_log().warning( "connector_road_t::step", "could not find places for road stations" );
					return new_return_value(RT_TOTAL_FAIL);
				}
				// get a suitable station
				const haus_besch_t* terminal_st = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::terminal_station );
				const haus_besch_t* through_st = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, hausbauer_t::through_station );
				if (terminal_st==NULL) {
					terminal_st = through_st;
				}
				ok = (through_st!=NULL) || (through==0);
				if (!ok) {
					sp->get_log().warning( "connector_road_t::step", "no suitable station found" );
					return new_return_value(RT_TOTAL_FAIL);
				}

				// kontostand checken
				sint64 cost = bauigel.calc_costs();
				if ( !sp->is_cash_available(cost) ) {
					sp->get_log().warning( "connector_road_t::step", "route %s => %s too expensive", fab1->get_name(), fab2->get_name() );
					// TODO: geldbedarf merken, den connector schlafen lassen, bis es da ist
					// TODO: try later
					return new_return_value(RT_TOTAL_FAIL);
				}
				// now build the route
				bauigel.baue();
				sp->get_log().message( "connector_road_t::step", "build route %s => %s", fab1->get_name(), fab2->get_name() );
				uint8 completed = 0;

				// build immediately 1x1 stations
				ok = sp->call_general_tool(WKZ_STATION, start.get_2d(), through & 1 ? through_st->get_name() : terminal_st->get_name());
				if (!ok) {
					sp->get_log().warning( "connector_road_t::step", "failed to built road station at (%s)", start.get_str() );
				}
				else {
					completed = 1;
					ok = sp->call_general_tool(WKZ_STATION, ziel.get_2d(), through & 2 ? through_st->get_name() : terminal_st->get_name());
					if (!ok) {
						sp->get_log().warning( "connector_road_t::step", "failed to built road station at (%s)", ziel.get_str() );
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
					sp->get_log().warning( "connector_road_t::step", "road no 1: (%s) no N-1: (%s)", bauigel.get_route()[1].get_2d().get_str(), bauigel.get_route()[bauigel.get_count()-2].get_str() );
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
				// TODO: do something smarter here
				vector_tpl<koord> tiles;
				tiles.append(start.get_2d());
				ai_t::add_neighbourhood( tiles, 5 );
				vector_tpl<koord3d> dep_ziele, dep_exist;
				for( uint32 j = 0; j < tiles.get_count(); j++ ) {
					const grund_t* gr = sp->get_welt()->lookup_kartenboden( tiles[j] );
					if(  gr  &&  gr->hat_weg(road_wt) && gr->get_weg(road_wt)->get_besitzer()==sp && gr->get_depot()) {
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
				wegbauer_t bauigel(sp->get_welt(), sp );
				bauigel.route_fuer( wegbauer_t::strasse, road_besch, tunnelbauer_t::find_tunnel(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()), brueckenbauer_t::find_bridge(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()) );
				// we won't destroy cities (and save the money)
				bauigel.set_keep_existing_faster_ways(true);
				bauigel.set_keep_city_roads(true);
				bauigel.set_maximum(10000);
				bauigel.calc_route(dep_start, !dep_exist.empty() ? dep_exist : dep_ziele);
				if(bauigel.get_count() >= 2) {
					// Sometimes reverse route is the best, so we have to change the koords.
					deppos =  ( start == bauigel.get_route()[0]) ? bauigel.get_route()[bauigel.get_count()-1] : bauigel.get_route()[0];	
					ok = true;
				}
				const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, road_wt, sp->get_welt()->get_timeline_year_month(), 0);
				ok = ok && (dep!=NULL || sp->get_welt()->lookup(deppos)->get_depot() );
				if (ok) {
					bauigel.baue();		
					// built depot
					if ( sp->get_welt()->lookup(deppos)->get_depot()==NULL ) {
						ok = sp->call_general_tool(WKZ_DEPOT, deppos.get_2d(), dep->get_name());
					}
				}
				else {
					sp->get_log().warning( "connector_road::step()","depot building failed");
					return new_return_value(RT_TOTAL_FAIL);
				}
				break;
			}
			case 2: {
				// create line
				schedule_t *fpl=new autofahrplan_t();

				// full load? or do we have unused capacity?
				const uint8 ladegrad = ( 100*prototyper->proto->get_capacity(prototyper->freight) )/ prototyper->proto->get_capacity(NULL);

				fpl->append(sp->get_welt()->lookup(start), ladegrad);
				fpl->append(sp->get_welt()->lookup(ziel), 0);
				fpl->set_aktuell( 0 );
				fpl->eingabe_abschliessen();

				linehandle_t line=sp->simlinemgmt.create_line(simline_t::truckline, sp, fpl);
				delete fpl;

				append_child( new vehikel_builder_t(sp, "vehikel builder", prototyper, line, deppos, min(nr_vehicles,3) ) );
				
				// tell the player
				char buf[256];
				sprintf(buf, translator::translate("%s\nnow operates\n%i trucks between\n%s at (%i,%i)\nand %s at (%i,%i)."), sp->get_name(), nr_vehicles, translator::translate(fab1->get_name()), start.x, start.y, translator::translate(fab2->get_name()), ziel.x, ziel.y);
				sp->get_welt()->get_message()->add_message(buf, start.get_2d(), message_t::ai, PLAYER_FLAG|sp->get_player_nr(), prototyper->proto->besch[0]->get_basis_bild());

				// tell the industrymanager
				data = new datablock_t();
				data->line = line;

				// reset prototyper, will be deleted in vehikel_builder
				prototyper = NULL;
				break;
			}
		}
		sp->get_log().message( "connector_road_t::step", "completed phase %d", phase);
		return_value_t *rv = new_return_value(phase>2 ? RT_TOTAL_SUCCESS : RT_PARTIAL_SUCCESS);
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

void connector_road_t::debug( log_t &file, cstring_t prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("conr","%s phase=%d", (const char*)prefix, phase);
	cstring_t next_prefix( prefix + " prototyp");
	if (prototyper && phase<=2) prototyper->debug(file, next_prefix);
}
