#include "connector_road.h"
#include "../bt.h"
#include "../../ai.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simworld.h"
#include "../../../bauer/brueckenbauer.h"
#include "../../../bauer/tunnelbauer.h"
#include "../../../bauer/wegbauer.h"
#include "../../../dataobj/koord.h"

connector_road_t::connector_road_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1_, const fabrik_t *fab2_, const weg_besch_t* road_besch_ ) : bt_sequential_t( sp, name)
{
	type = BT_CON_ROAD;
	fab1 = fab1_;
	fab2 = fab2_;
	road_besch = road_besch_;
}

void connector_road_t::rdwr( loadsave_t *file, const uint16 version )
{
	type = BT_CON_ROAD;
	bt_sequential_t::rdwr( file, version );
	ai_t::rdwr_fabrik(file, sp->get_welt(), fab1);
	ai_t::rdwr_fabrik(file, sp->get_welt(), fab2);
	/*
	 * TODO: road_besch speichern
	 */
}

/*
 * Helper function.
 */
void add_neighbourhood( vector_tpl<koord> &list, const uint16 size)
{
	uint32 old_size = list.get_count();
	koord test;
	for( uint32 i = 0; i < old_size; i++ ) {
		for( test.x = -size; test.x < size+1; test.x++ ) {
			for( test.y = -size; test.y < size+1; test.y++ ) {
				list.append_unique( list[i] + test );
			}
		}
	}
}



return_code connector_road_t::step()
{
	if( childs.empty() ) {
		// Our first step -> calc the route.
		koord start( fab1->get_pos().get_2d() );
		koord ziel( fab2->get_pos().get_2d() );
		vector_tpl<koord3d> tile_list[2];
		const uint8 cov = sp->get_welt()->get_einstellungen()->get_station_coverage();
		koord test;
		for( uint8 i = 0; i < 2; i++ ) {
			const fabrik_t *fab =  i==0 ? fab1 : fab2;
			vector_tpl<koord> fab_tiles;
			fab->get_tile_list( fab_tiles );
			add_neighbourhood( fab_tiles, cov );
			vector_tpl<koord> one_more( fab_tiles );
			add_neighbourhood( one_more, 1 );
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
			add_neighbourhood( connected_halts, 1 );
			add_neighbourhood( other_halts, 1 );
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
					if(  gr  &&  gr->get_grund_hang() == hang_t::flach  &&  !gr->hat_wege()  &&  !gr->get_leitung()  ) {
						tile_list[i].append_unique( gr->get_pos() );
					}
				}
				if( !tile_list[i].empty() ) {
					// Skip, if found tiles beneath halts.
					break;
				}
				next = &fab_tiles;
			}
		}
		bool ok;
		// Test which tiles are the best:
		wegbauer_t bauigel(sp->get_welt(), sp );
		bauigel.route_fuer( wegbauer_t::strasse, road_besch, tunnelbauer_t::find_tunnel(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()), brueckenbauer_t::find_bridge(road_wt,road_besch->get_topspeed(),sp->get_welt()->get_timeline_year_month()) );
		// we won't destroy cities (and save the money)
		bauigel.set_keep_existing_faster_ways(true);
		bauigel.set_keep_city_roads(true);
		bauigel.set_maximum(10000);
		bauigel.calc_route(tile_list[0], tile_list[1]);
		if(bauigel.max_n > 1) {
			// Sometimes reverse route is the best, so we have to change the koords.
			if( tile_list[0].is_contained( bauigel.get_route()[0]) ) {
				start = bauigel.get_route()[0].get_2d();
				ziel = bauigel.get_route()[bauigel.max_n].get_2d();
			} else {
				start = bauigel.get_route()[bauigel.max_n].get_2d();
				ziel = bauigel.get_route()[0].get_2d();
			}
			ok = true;
		}
		if( ok ) {
			/*
			 * Append bau-knoten
			 */
			/* append_child( new builder_road_t( sp, "builder_road_t", start, ziel, road_besch ) );
			append_child( new builder_road_station_t( sp, "builder_road_station_t", start ) );
			append_child( new builder_road_station_t( sp, "builder_road_station_t", ziel ) ); */
			return RT_PARTIAL_SUCCESS;
		}
		else {
			return RT_ERROR;
		}

	}
	else {
		// Step the childs.
		return bt_sequential_t::step();
	}
}

