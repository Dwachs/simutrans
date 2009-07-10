#include "connector_road.h"

#include "builder_road_station.h"
#include "vehikel_builder.h"
#include "builder_way_obj.h"

#include "../bt.h"
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
	bt_sequential_t( sp, name)
{
	type = BT_CON_ROAD;
	fab1 = NULL;
	fab2 = NULL;
	road_besch = NULL;
	prototyper = NULL;
	nr_vehicles = 0;
	phase = 0;
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	e = NULL;
	harbour_pos = koord3d::invalid;
}

connector_road_t::connector_road_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1_, const fabrik_t *fab2_, const weg_besch_t *road_besch_, simple_prototype_designer_t *d, uint16 nr_veh, const way_obj_besch_t *e_, const koord3d harbour_pos_ ) :
	bt_sequential_t( sp, name )
{
	type = BT_CON_ROAD;
	fab1 = fab1_;
	fab2 = fab2_;
	road_besch = road_besch_;
	prototyper = d;
	nr_vehicles = nr_veh;
	phase = 0;
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	e = e_;
	harbour_pos = harbour_pos_;
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
	ai_t::rdwr_fabrik(file, sp->get_welt(), fab1);
	ai_t::rdwr_fabrik(file, sp->get_welt(), fab2);
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
	/*
	 * TODO: road_besch, e speichern
	 */
}

void connector_road_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);
	start.rotate90(y_size);
	ziel.rotate90(y_size);
	deppos.rotate90(y_size);
	harbour_pos.rotate90(y_size);
}

return_value_t *connector_road_t::step()
{
	if( childs.empty() ) {
		switch(phase) {
			case 0: {
				// need through station?
				uint through = 0;
				// Our first step -> calc the route.
				vector_tpl<koord3d> tile_list[2];
				vector_tpl<koord3d> through_tile_list[2];
				const uint8 cov = sp->get_welt()->get_einstellungen()->get_station_coverage();
				koord test;
				for( uint8 i = 0; i < 2; i++ ) {
					if( i == 0  &&  harbour_pos != koord3d::invalid ) {
						const grund_t *gr = sp->get_welt()->lookup(harbour_pos);
						tile_list[0].append( harbour_pos + koord(gr->get_grund_hang()) + koord3d(0,0,1) );
						continue;
					}
					const fabrik_t *fab =  i==0 ? fab1 : fab2;
					vector_tpl<koord> fab_tiles;
					fab->get_tile_list( fab_tiles );
					ai_t::add_neighbourhood( fab_tiles, cov );
					vector_tpl<koord> one_more( fab_tiles );
					ai_t::add_neighbourhood( one_more, 1 );
					// Any halts here?
					vector_tpl<koord> connected_halts, other_halts;
					for( uint32 j = 0; j < one_more.get_count(); j++ ) {
						halthandle_t halt = haltestelle_t::get_halt( sp->get_welt(), one_more[j], sp );
						if( halt.is_bound() && !( other_halts.is_contained(halt->get_basis_pos()) || connected_halts.is_contained(halt->get_basis_pos()) ) ) {
							bool halt_connected = halt->get_fab_list().is_contained( (fabrik_t*)fab );
							for( slist_tpl< haltestelle_t::tile_t >::const_iterator iter = halt->get_tiles().begin(); iter != halt->get_tiles().end(); ++iter ) {
								koord pos = iter->grund->get_pos().get_2d();
								if( halt_connected ) {
									connected_halts.append_unique( pos );
								}
								else {
									other_halts.append_unique( pos );
								}
							}
						}
					}
					ai_t::add_neighbourhood( connected_halts, 1 );
					ai_t::add_neighbourhood( other_halts, 1 );
					vector_tpl<koord> halts( connected_halts );
					for( uint32 j = 0; j < other_halts.get_count(); j++ ) {
						if( fab_tiles.is_contained( other_halts[j] ) ) {
								halts.append_unique( other_halts[j] );
						}
					}
					vector_tpl<koord> *next = &halts;
					for( uint8 k = 0; k < 2; k++ ) {
						// On which tiles we can start?
						for( uint32 j = 0; j < next->get_count(); j++ ) {
							const grund_t* gr = sp->get_welt()->lookup_kartenboden( next->operator[](j) );
							if(  gr  &&  gr->get_grund_hang() == hang_t::flach  &&  !gr->hat_wege()  &&  !gr->get_leitung()  && !gr->find<gebaeude_t>() ) {
								tile_list[i].append_unique( gr->get_pos() );
							}
						}
						if( !tile_list[i].empty() ) {
							// Skip, if found tiles beneath halts.
							break;
						}
						next = &fab_tiles;
					}
					// try to find a place for a durchgangshalt - append the neighbors of possible positions to tilelist
					// remember the candidates for the stations in a separate list
					if( tile_list[i].empty() ) {
						for( uint32 j = 0; j < next->get_count(); j++ ) {
							const grund_t* gr = sp->get_welt()->lookup_kartenboden( next->operator[](j) );
							// TODO: reicht diese Abfrage aus??
							if(  gr  &&  gr->get_grund_hang() == hang_t::flach  &&  gr->hat_weg(road_wt) &&  !gr->has_two_ways() && !gr->get_leitung() &&  !gr->find<gebaeude_t>() && !gr->is_halt()  ) {
								const weg_t *w = gr->get_weg(road_wt);
								const ribi_t::ribi ribi = w->get_ribi_unmasked();
								if (spieler_t::check_owner(sp, w->get_besitzer()) && ribi_t::ist_gerade(ribi)) {
									grund_t *to; 
									bool found = false;
									if (gr->get_neighbour(to, road_wt, koord((ribi_t::ribi)(ribi & ribi_t::dir_suedost)) )) {
										tile_list[i].append_unique( to->get_pos() );
										found = true;
									}
									if (gr->get_neighbour(to, road_wt, koord((ribi_t::ribi)(ribi & ribi_t::dir_nordwest)) )) {
										tile_list[i].append_unique( to->get_pos() );
										found = true;
									}
									if (found) {
										through_tile_list[i].append_unique(gr->get_pos());
										through |= i+1;
									}
								}
							}
						}
					}
					if (through & (i+1)) {
						for(uint32 j=0; j < tile_list[i].get_count(); j++) {
							sp->get_log().message( "connector_road_t::step", "tile_list[%d][%d] = %s", i,j,tile_list[i][j].get_str());
						}
						for(uint32 j=0; j < through_tile_list[i].get_count(); j++) {
							sp->get_log().message( "connector_road_t::step", "through_tile_list[%d][%d] = %s", i,j,through_tile_list[i][j].get_str());
						}
					}
				}
				// Test which tiles are the best:
				wegbauer_t bauigel(sp->get_welt(), sp );
				bauigel.route_fuer( wegbauer_t::strasse, road_besch, tunnelbauer_t::find_tunnel(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()), brueckenbauer_t::find_bridge(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()) );
				// we won't destroy cities (and save the money)
				bauigel.set_keep_existing_faster_ways(true);
				bauigel.set_keep_city_roads(true);
				bauigel.set_maximum(10000);
				bool ok = true;

				bauigel.calc_route(tile_list[0], tile_list[1]);
				ok = bauigel.max_n > 1;
				// find locations of stations (special search for through stations)
				uint8 found = 3 ^ through;
				if (ok) {
					for(uint8 i=0;  i<2; i++) {
						for(uint8 j=0; j<2; j++) {
							// Sometimes reverse route is the best - try both ends of the routes
							uint32 n = j==0 ? 0 : bauigel.max_n;
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
				if(ok) {
					// get a suitable station
					const haus_besch_t* fh = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, through!=0 ? hausbauer_t::through_station : hausbauer_t::generic_station );
					ok = fh!=NULL;

					// TODO: kontostand checken!
					if (ok) {
						//sint64 cost = bauigel.calc_costs();
						//sint64 cash = sp->get_finance_history_year(0, COST_CASH);
						//if (10*cost < 9*cash) {
							bauigel.baue();
							sp->get_log().message( "connector_road_t::step", "found a route %s => %s", fab1->get_name(), fab2->get_name() );
						//}
						//else {
						//	ok = false;
						//	sp->get_log().message( "connector_road_t::step", "not enough money (cost: %ld, cash: %ld)", cost, cash );
						//}
					}

					// build immediately 1x1 stations
					if (ok) {
						ok = sp->call_general_tool(WKZ_STATION, start.get_2d(), fh->get_name());
						ok = ok && sp->call_general_tool(WKZ_STATION, ziel.get_2d(), fh->get_name());
					}
					else {
						sp->get_log().message( "connector_road_t::step", "failed to built road station at (%s) or (%s)", start.get_2d().get_str(), ziel.get_str() );
						sp->get_log().message( "connector_road_t::step", "road no 1: (%s) no N-1: (%s)", bauigel.get_route()[1].get_2d().get_str(), bauigel.get_route()[bauigel.max_n-1].get_str() );
					}

					// TODO: station so erweitern, dass Kapazitaet groesser als Kapazitaet eines einzelnen Convois
					/*
					append_child( new builder_road_station_t( sp, "builder_road_station_t", start, ware_besch ) );
					append_child( new builder_road_station_t( sp, "builder_road_station_t", ziel, ware_besch ) );
					*/
				}

				if( !ok ) {
					sp->get_log().message( "connector_road_t::step", "didn't found a route %s => %s", fab1->get_name(), fab2->get_name() );
					return new_return_value(RT_TOTAL_SUCCESS);
				}
				break;
			}
			case 1: {
				// TODO: do something smarter here
				vector_tpl<koord> tiles;
				tiles.append(start.get_2d());
				ai_t::add_neighbourhood( tiles, 5 );
				vector_tpl<koord3d> dep_ziele;
				for( uint32 j = 0; j < tiles.get_count(); j++ ) {
					const grund_t* gr = sp->get_welt()->lookup_kartenboden( tiles[j] );
					if(  gr  &&  gr->get_grund_hang() == hang_t::flach  &&  !gr->hat_wege()  &&  !gr->get_leitung()  ) {
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
				bauigel.calc_route(dep_start, dep_ziele);
				if(bauigel.max_n >= 1) {
					// Sometimes reverse route is the best, so we have to change the koords.
					deppos =  ( start == bauigel.get_route()[0]) ? bauigel.get_route()[bauigel.max_n] : bauigel.get_route()[0];	
					ok = true;
				}
				const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, road_wt, sp->get_welt()->get_timeline_year_month(), 0);
				ok = ok && dep!=NULL;
				if (ok) {
					bauigel.baue();		
					// built depot
					ok = sp->call_general_tool(WKZ_DEPOT, deppos.get_2d(), dep->get_name());
				}
				else {
					sp->get_log().message( "connector_road::step()","depot building failed");
				}
				break;
			}
			case 2: {
				// create line
				schedule_t *fpl=new autofahrplan_t();

				// full load? or do we have unused capacity?
				const uint8 ladegrad = ( 100*prototyper->proto->get_capacity(prototyper->freight) )/ prototyper->proto->get_capacity(NULL);

				fpl->append(sp->get_welt()->lookup(start), ladegrad, 15);
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
				industry_connection_t& ic = sp->get_industry_manager()->get_connection(fab1, fab2, prototyper->freight);
				ic.set<own>();
				ic.unset<planned>();
				ic.set_line(line);

				// reset prototyper, will be deleted in vehikel_builder
				prototyper = NULL;
				break;
			}
		}
		sp->get_log().message( "connector_road_t::step", "completed phase %d", phase);
		phase ++;
		return new_return_value(phase>3 ? RT_TOTAL_SUCCESS : RT_PARTIAL_SUCCESS);
	}
	else {
		// Step the childs.
		return bt_sequential_t::step();
	}
}

void connector_road_t::debug( log_t &file, cstring_t prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("conr","%s phase=%d", (const char*)prefix, phase);
	cstring_t next_prefix( prefix + " prototyp");
	if (prototyper && phase<=2) prototyper->debug(file, next_prefix);
}
