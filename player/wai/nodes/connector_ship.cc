#include "connector_ship.h"

#include "builder_road_station.h"
#include "vehikel_builder.h"
#include "builder_way_obj.h"

#include "../bt.h"
#include "../datablock.h"
#include "../vehikel_prototype.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simmesg.h"
#include "../../../simtools.h"
#include "../../../bauer/hausbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/loadsave.h"

connector_ship_t::connector_ship_t( ai_wai_t *sp, const char *name) :
	bt_sequential_t( sp, name )
{
	type = BT_CON_SHIP;
	fab1 = NULL;
	fab2 = NULL;
	prototyper = NULL;
	nr_vehicles = 0;
	phase = 0;
	start = koord3d::invalid;
	harbour_pos = koord3d::invalid;;
}
connector_ship_t::connector_ship_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1_, const fabrik_t *fab2_, simple_prototype_designer_t *d, uint16 nr_veh, const koord3d &harbour_pos_ ) :
	bt_sequential_t( sp, name )
{
	type = BT_CON_SHIP;
	fab1 = fab1_;
	fab2 = fab2_;
	prototyper = d;
	nr_vehicles = nr_veh;
	phase = 0;
	start = fab1->get_pos();
	harbour_pos = harbour_pos_;
}

connector_ship_t::~connector_ship_t()
{
	if (prototyper && phase<=2) delete prototyper;
	prototyper = NULL;
}

void connector_ship_t::rdwr( loadsave_t *file, const uint16 version )
{
	bt_sequential_t::rdwr( file, version );
	file->rdwr_byte(phase, "");
	ai_t::rdwr_fabrik(file, sp->get_welt(), fab1);
	ai_t::rdwr_fabrik(file, sp->get_welt(), fab2);
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
	deppos.rdwr(file);
	harbour_pos.rdwr(file);
}

void connector_ship_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);
	start.rotate90(y_size);
	deppos.rotate90(y_size);
	harbour_pos.rotate90(y_size);
}

return_value_t *connector_ship_t::step()
{
	if( childs.empty() ) {
		return_value_t *rv = new_return_value(RT_DONE_NOTHING);
		switch(phase) {
			case 0: {
				// Our first step: Build a harbour.
				// calculate space
				koord zv(sp->get_welt()->lookup_kartenboden(harbour_pos.get_2d())->get_grund_hang());
				koord test_pos(harbour_pos.get_2d() - zv);
				for(uint8 i = 1; i<8; i++) {
					grund_t *gr = sp->get_welt()->lookup_kartenboden(test_pos);
					// TODO: reicht abfrage?
					if (gr && gr->ist_wasser() && !gr->get_halt().is_bound() && gr->find<gebaeude_t>()==NULL) {
						test_pos -= zv;
					}
					else {
						break;
					}
				}
				uint32 len = koord_distance(harbour_pos, test_pos);

				// get a suitable station
				const haus_besch_t* fh = get_random_harbour(sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE, len);
				bool ok = fh!=NULL;

				// build immediately 1x1 stations
				if (ok) {
					ok = sp->call_general_tool(WKZ_STATION, harbour_pos.get_2d(), fh->get_name());
				}
				// harbour_pos now the Molenende
				if (ok) {
					harbour_pos -= zv * (fh->get_h()-1);
				}
				else {
					sp->get_log().warning( "connector_ship_t::step", "failed to build a harbour at %s (route %s => %s)", harbour_pos.get_str(), fab1->get_name(), fab2->get_name() );
					return new_return_value(RT_TOTAL_FAIL);
				}
				break;
			}

			case 1: {
				vector_tpl<koord> tiles;
				fab1->get_tile_list( tiles );
				ai_t::add_neighbourhood( tiles, 5 );
				koord3d best_tile = koord3d::invalid;
				uint32 best_dist = 0xffffffff;
				// Test which tiles are the best:
				for( uint32 j = 0; j < tiles.get_count(); j++ ) {
					const grund_t* gr = sp->get_welt()->lookup_kartenboden( tiles[j] );
					if(  gr  &&  gr->ist_wasser()  &&  !gr->find< gebaeude_t >()  ) {
						uint32 dist = koord_distance( tiles[j], start );
						if( dist < best_dist ) {
							best_dist = dist;
							best_tile = gr->get_pos();
						}
					}
				}
				deppos = best_tile;
				const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, water_wt, sp->get_welt()->get_timeline_year_month(), 0);
				bool ok = dep!=NULL;
				ok &= sp->call_general_tool(WKZ_DEPOT, deppos.get_2d(), dep->get_name());
				if( !ok ) {
					sp->get_log().warning( "connector_ship::step()","depot building failed");
					return new_return_value(RT_TOTAL_FAIL);
				}
				break;
			}

			case 2: {
				// create line
				schedule_t *fpl = new schifffahrplan_t();

				// full load? or do we have unused capacity?
				const uint8 ladegrad = ( 100*prototyper->proto->get_capacity(prototyper->freight) )/ prototyper->proto->get_capacity(NULL);

				fpl->append(sp->get_welt()->lookup(start), ladegrad, 15);
				const grund_t *gr = sp->get_welt()->lookup(harbour_pos);
				koord3d ziel =  get_ship_target(); // harbour_pos - koord(gr->get_grund_hang());
				fpl->append(sp->get_welt()->lookup(ziel), 0);
				fpl->set_aktuell( 0 );
				fpl->eingabe_abschliessen();

				linehandle_t line=sp->simlinemgmt.create_line(simline_t::shipline, sp, fpl);
				delete fpl;

				append_child( new vehikel_builder_t(sp, "vehikel builder", prototyper, line, deppos, nr_vehicles) );
				
				// tell the player
				char buf[256];
				sprintf(buf, translator::translate("%s\nnow operates\n%i ships between\n%s at (%i,%i)\nand %s at (%i,%i)."), sp->get_name(), nr_vehicles, translator::translate(fab1->get_name()), start.x, start.y, translator::translate(fab2->get_name()), ziel.x, ziel.y);
				sp->get_welt()->get_message()->add_message(buf, start.get_2d(), message_t::ai, PLAYER_FLAG|sp->get_player_nr(), prototyper->proto->besch[0]->get_basis_bild());

				// tell the industrymanager via the industry-connector
				rv->data = new datablock_t();
				rv->data->line = line;

				// reset prototyper, will be deleted in vehikel_builder
				prototyper = NULL;
				break;
			}
		}
		sp->get_log().message( "connector_ship_t::step", "completed phase %d", phase);
		phase ++;
		rv->code = phase>3 ? RT_TOTAL_SUCCESS : RT_PARTIAL_SUCCESS;
		return rv;
	}
	else {
		// Step the childs.
		return bt_sequential_t::step();
	}
}

void connector_ship_t::debug( log_t &file, cstring_t prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("conr","%s phase=%d", (const char*)prefix, phase);
	cstring_t next_prefix( prefix + " prototyp");
	if (prototyper && phase<=2) prototyper->debug(file, next_prefix);
}

const haus_besch_t* connector_ship_t::get_random_harbour(const uint16 time, const uint8 enables, uint32 max_len)
{
	weighted_vector_tpl<const haus_besch_t*> stops;

	for(  vector_tpl<const haus_besch_t *>::const_iterator iter = hausbauer_t::station_building.begin(), end = hausbauer_t::station_building.end();  iter != end;  ++iter  ) {
		const haus_besch_t* besch = (*iter);
		if(besch->get_utyp()==haus_besch_t::hafen  &&  besch->get_extra()==water_wt  &&  (enables==0  ||  (besch->get_enabled()&enables)!=0)) {
			// ok, now check timeline
			if(time==0  ||  (besch->get_intro_year_month()<=time  &&  besch->get_retire_year_month()>time)) {
				if( besch->get_b() == 1  &&  besch->get_h() <= max_len  ) {
					stops.append(besch,max(1,16-besch->get_level()),16);
				}
			}
		}
	}
	return stops.empty() ? NULL : stops.at_weight(simrand(stops.get_sum_weight()));
}

// from ai_goods
koord3d connector_ship_t::get_ship_target() 
{
	karte_t *welt = sp->get_welt();
	// sea pos (and not on harbour ... )
	halthandle_t halt = haltestelle_t::get_halt(welt,harbour_pos,sp);
	const grund_t *gr = welt->lookup(harbour_pos);
	gebaeude_t *h = gr->find<gebaeude_t>();	
	koord pos1 = harbour_pos.get_2d();
	koord3d best_pos = koord3d::invalid;
	uint8 radius = 1; // welt->get_einstellungen()->get_station_coverage()
	for(  int y = pos1.y-radius;  y<=pos1.y+radius;  y++  ) {
		for(  int x = pos1.x-radius;  x<=pos1.x+radius;  x++  ) {
			const grund_t *gr = welt->lookup(koord3d(x,y,welt->get_grundwasser()));
			// in water, the water tiles have no halt flag!
			if(gr  &&  gr->ist_wasser() && !gr->get_halt().is_bound()  &&  halt == haltestelle_t::get_halt(welt,gr->get_pos(),sp)  &&  (best_pos==koord3d::invalid || koord_distance(gr->get_pos(),start)<koord_distance(best_pos,start))  ) {
				best_pos = gr->get_pos();
			}
		}
	}
	assert(best_pos != koord3d::invalid);
	// no copy constructor for koord3d available :P
	return koord3d(best_pos.get_2d(), best_pos.z);
}
