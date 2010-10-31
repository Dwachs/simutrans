#include "free_tile_searcher.h"

#include "../datablock.h"
#include "../return_value.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../dataobj/loadsave.h"
#include "../../../dings/gebaeude.h"

free_tile_searcher_t::free_tile_searcher_t( ai_wai_t *sp, const char* name ) :
	bt_node_t( sp, name ), pos(koord3d::invalid)
{
	type = BT_FREE_TILE;
}

free_tile_searcher_t::free_tile_searcher_t( ai_wai_t *sp, const char* name, koord3d pos_, bool through ) :
	bt_node_t( sp, name ), pos(pos_)
{
	this->through = through;
	type = BT_FREE_TILE;
}

void free_tile_searcher_t::rdwr( loadsave_t *file, const uint16 )
{
	pos.rdwr(file);
	file->rdwr_bool(through);
}

return_value_t *free_tile_searcher_t::step() 
{
	karte_t *welt = sp->get_welt();
	grund_t *gr = welt->lookup(pos);
	if(gr==NULL) {	
		sp->get_log().warning("free_tile_searcher_t::step", "invalid position (%s)", pos.get_str());
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	datablock_t *data = new datablock_t();

	const uint16 cov = sp->get_welt()->get_einstellungen()->get_station_coverage();

	// generate two list of candidate positions
	vector_tpl<koord3d> list1, list2;

	// is there a factory?
	gebaeude_t *gb = gr->find<gebaeude_t>();
	if (gb  &&  gb->get_fabrik()) {
		fabrik_t *fab = gb->get_fabrik();
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
				bool halt_connected = halt->get_fab_list().is_contained( fab );
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
		// halt list tiles go into list1
		for(uint32 i=0; i<halts.get_count(); i++) {
			grund_t *bd = welt->lookup_kartenboden(halts[i]);
			if (bd) {
				list1.append(bd->get_pos());
			}
		}
		// fab tiles to list 2
		for(uint32 i=0; i<fab_tiles.get_count(); i++) {
			grund_t *bd = welt->lookup_kartenboden(fab_tiles[i]);
			if (bd) {
				list2.append(bd->get_pos());
			}
		}
	}
	else if (gr->is_halt()) {
	}
	else {
		list1.append(pos);
	}

	if(!through) {
		vector_tpl<koord3d> *next = &list1;
		for( uint8 k = 0; k < 2; k++ ) {
			// On which tiles we can start?
			for( uint32 j = 0; j < next->get_count(); j++ ) {
				const grund_t* gr = sp->get_welt()->lookup( (*next)[j] );
				if(  gr  &&  gr->get_grund_hang() == hang_t::flach  &&  !gr->hat_wege()  &&  !gr->get_leitung()  && !gr->find<gebaeude_t>() ) {
					data->pos1.append_unique( gr->get_pos() );
				}
			}
			if( !data->pos1.empty() ) {
				// Skip, if found tiles beneath halts.
				break;
			}
			next = &list2;
		}
	}
	// try to find a place for a durchgangshalt - append the neighbors of possible positions to tilelist
	// remember the candidates for the stations in a separate list
	if(through || data->pos1.empty() ) {
		vector_tpl<koord3d> *next = &list1;
		for( uint8 k = 0; k < 2; k++ ) {
			for( uint32 j = 0; j < next->get_count(); j++ ) {
				const grund_t* gr = sp->get_welt()->lookup( (*next)[j] );
				// TODO: reicht diese Abfrage aus?? Es muss noch getestet werden, ob einem die Strasse gehört.
				// Und was ist, wenn der Wegbauer aus dem geraden Stück eine T-Kreuzung macht?
				if(  gr  &&  gr->get_grund_hang() == hang_t::flach  &&  gr->hat_weg(road_wt) &&  !gr->ist_uebergang() && !gr->get_leitung() &&  !gr->find<gebaeude_t>() && !gr->is_halt()  ) {
					weg_t *w = gr->get_weg(road_wt);
					const ribi_t::ribi ribi = w->get_ribi_unmasked();
					if (w->ist_entfernbar(sp)==NULL && ribi_t::ist_gerade(ribi)) {
						grund_t *to; 
						bool found = false;
						if (gr->get_neighbour(to, road_wt, koord((ribi_t::ribi)(ribi & ribi_t::dir_suedost)) )) {
							data->pos1.append_unique( to->get_pos() );
							found = true;
						}
						if (gr->get_neighbour(to, road_wt, koord((ribi_t::ribi)(ribi & ribi_t::dir_nordwest)) )) {
							data->pos1.append_unique( to->get_pos() );
							found = true;
						}
						if (found) {
							data->pos2.append_unique(gr->get_pos());
						}
					}
				}
			}
			next = &list2;
		}
	}
	if( !data->pos2.empty() ) {
		for(uint32 j=0; j < data->pos1.get_count(); j++) {
			sp->get_log().message( "free_tile_searcher::step", "data->pos1[%d] = %s", j,data->pos1[j].get_str());
		}
		for(uint32 j=0; j < data->pos2.get_count(); j++) {
			sp->get_log().message( "free_tile_searcher::step", "data->pos2[%d] = %s", j,data->pos2[j].get_str());
		}
	}


	return_value_t *rv = new return_value_t( RT_TOTAL_SUCCESS, type );
	rv->data = data;
	return rv;
}
