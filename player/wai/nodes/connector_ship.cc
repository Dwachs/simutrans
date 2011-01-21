#include "connector_ship.h"

#include "builder_road_station.h"
#include "vehikel_builder.h"
#include "builder_way_obj.h"

#include "../bt.h"
#include "../datablock.h"
#include "../vehikel_prototype.h"
#include "../utils/water_digger.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simmenu.h"
#include "../../../simmesg.h"
#include "../../../simtools.h"
#include "../../../bauer/hausbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../besch/weg_besch.h"
#include "../../../dataobj/loadsave.h"
#include "../../../ifc/fahrer.h"


class shipyard_searcher_t : public fahrer_t
{
private:
	karte_t *welt;
	spieler_t *sp;

public:
	shipyard_searcher_t(karte_t *w_, spieler_t *s_) : welt(w_), sp(s_), find_depot(false) {}

	// force to find existing depot
	bool find_depot;

	bool ist_befahrbar(const grund_t *gr) const { return gr->get_weg_ribi(water_wt)!=ribi_t::keine; }
	ribi_t::ribi get_ribi(const grund_t *gr) const { return gr->get_weg_ribi(water_wt); }
	waytype_t get_waytype() const { return water_wt; }

	int get_kosten(const grund_t *gr,const sint32, koord /*from_pos*/) const // ignored in find_route
	{
		// our depot?
		depot_t *dep = gr->get_depot();
		return (dep  &&  dep->get_besitzer() == sp)  ?  1  : 10;
	}

	// returns true for the way search to an unknown target.
	// first is current ground, second is starting ground
	virtual bool ist_ziel(const grund_t *gr, const grund_t *from) const
	{
		if (gr  &&  (gr->ist_wasser()  ||  ribi_t::ist_einfach(gr->get_weg_ribi_unmasked(water_wt)))) {
			// our depot?
			depot_t *dep = gr->get_depot();
			if (dep) {
				if (dep->get_besitzer() != sp) {
					return false;
				}
				if (!gr->ist_wasser()) {
					// check whether we can enter depot from the previous ground
					return ribi_typ(gr->get_pos(), from->get_pos()) == gr->get_weg_ribi_unmasked(water_wt);
				}
				// depot found
				return true;
			}
			if (find_depot) {
				return false;
			}
			// no halt
			const planquadrat_t *plan = welt->lookup(gr->get_pos().get_2d());
			if (plan->get_haltlist_count() > 0) {
				return false;
			}
			// can we build on the canal?
			if (!gr->ist_wasser()) {
				weg_t *canal = gr->get_weg(water_wt);
				assert(canal);
				if (canal->ist_entfernbar(sp)!=NULL) {
					return false;
				}
			}
			return true;
		}
		return false;
	}
};


connector_ship_t::connector_ship_t( ai_wai_t *sp, const char *name) :
	bt_sequential_t( sp, name ), fab1(NULL, sp), fab2(NULL, sp)
{
	type = BT_CON_SHIP;
	prototyper = NULL;
	nr_vehicles = 0;
	phase = 0;
	harbour_pos = koord3d::invalid;
	ourdepot = false;
}
connector_ship_t::connector_ship_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1, const fabrik_t *fab2, koord3d start_, koord3d ziel_, simple_prototype_designer_t *d, uint16 nr_veh) :
	bt_sequential_t( sp, name ), fab1(fab1, sp), fab2(fab2, sp)
{
	type = BT_CON_SHIP;
	prototyper = d;
	nr_vehicles = nr_veh;
	phase = 0;
	start_harbour_pos = start_;
	harbour_pos = ziel_;
	ourdepot = false;
}

connector_ship_t::~connector_ship_t()
{
	if (prototyper && phase<=2) delete prototyper;
	prototyper = NULL;
}

void connector_ship_t::rdwr( loadsave_t *file, const uint16 version )
{
	bt_sequential_t::rdwr( file, version );
	file->rdwr_byte(phase);
	fab1.rdwr(file, version, sp);
	fab2.rdwr(file, version, sp);
	file->rdwr_short(nr_vehicles);
	if (phase<=3) {
		if (file->is_loading()) {
			prototyper = new simple_prototype_designer_t(sp);
		}
		prototyper->rdwr(file);
	}
	else {
		prototyper = NULL;
	}
	deppos.rdwr(file);
	harbour_pos.rdwr(file);
	start_harbour_pos.rdwr(file);
	file->rdwr_bool(ourdepot);
}

void connector_ship_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);
	deppos.rotate90(y_size);
	harbour_pos.rotate90(y_size);
	start_harbour_pos.rotate90(y_size);
}

return_value_t *connector_ship_t::step()
{
	if(!fab1.is_bound()  ||  !fab2.is_bound()) {	
		sp->get_log().warning("connector_ship_t::step", "%s %s disappeared", fab1.is_bound() ? "" : "start", fab2.is_bound() ? "" : "ziel");
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	if( childs.empty() ) {
		karte_t *welt = sp->get_welt();
		return_value_t *rv = new_return_value(RT_DONE_NOTHING);
		switch(phase) {
			case 0: {
				// Our first step: Build a harbour.
				bool ok = true;
				if (start_harbour_pos != fab1->get_pos()) {
					ok = build_harbour(start_harbour_pos);
				}
				if (ok  &&  harbour_pos != fab2->get_pos()) {
					ok = build_harbour(harbour_pos);
				}
				if (!ok) {
					sp->get_log().warning( "connector_ship_t::step", "failed to build a harbour (route %s => %s)", fab1->get_name(), fab2->get_name() );
					return new_return_value(RT_TOTAL_FAIL);
				}
				break;
			}
			case 1: {
				// test the digger
				water_digger_t baubiber(welt, sp);
				
				baubiber.route_fuer((wegbauer_t::bautyp_t)water_wt, wegbauer_t::weg_search(water_wt, 1, 0, weg_t::type_flat));
				// start / end in water
				koord3d s = get_water_tile(start_harbour_pos);
				koord3d z = get_water_tile(harbour_pos);
				baubiber.calc_route(s, z);
				const sint64 cost = baubiber.calc_costs();
				if (baubiber.get_route().get_count()>2  &&  sp->is_cash_available(cost)) {
					sp->get_log().message( "connector_ship_t::step", "digging %s => %s", start_harbour_pos.get_2d().get_str(), harbour_pos.get_str());
					baubiber.terraform();
				}
				// no else branch as a route should exist without digging anyway
				break;
			}

			case 2: {
				// build depot
				route_t route;
				shipyard_searcher_t *werfti = new shipyard_searcher_t(welt, sp);
				koord3d start = get_water_tile(start_harbour_pos);
				// first try to find own depot
				werfti->find_depot = true;
				bool ok = route.find_route(welt, start, werfti, 1000, ribi_t::alle, 10);
				if (!ok) {
					werfti->find_depot = false;
					ok = route.find_route(welt, start, werfti, 1000, ribi_t::alle, 100);
				}
				if (ok) {
					deppos = route.back();
				}
				const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, water_wt, welt->get_timeline_year_month(), 0);
				ok = ok  &&  dep!=NULL;
				if (ok && welt->lookup_kartenboden(deppos.get_2d())->get_depot()==NULL) {
					ok = sp->call_general_tool(WKZ_DEPOT, deppos.get_2d(), dep->get_name());
					ourdepot = true;
				}
				if( !ok ) {
					sp->get_log().warning( "connector_ship::step()","depot building failed at (%s)", deppos.get_str());
					cleanup();
					return new_return_value(RT_TOTAL_FAIL);
				}
				break;
			}

			case 3: {
				// create line
				schedule_t *fpl = new schifffahrplan_t();

				// full load? or do we have unused capacity?
				const uint8 ladegrad = ( 100*prototyper->proto->get_capacity(prototyper->freight) )/ prototyper->proto->get_capacity(NULL);

				koord3d start = get_ship_target(start_harbour_pos, harbour_pos);
				koord3d ziel = get_ship_target(harbour_pos, start_harbour_pos);
				if (start == koord3d::invalid  ||  ziel == koord3d::invalid) {
					sp->get_log().warning( "connector_ship::step()","no starting position found");
					cleanup();
					return new_return_value(RT_TOTAL_FAIL);
				}
				fpl->append(welt->lookup(start), ladegrad);
				fpl->append(welt->lookup(ziel), 0);
				fpl->set_aktuell( 0 );
				fpl->eingabe_abschliessen();

				linehandle_t line=sp->simlinemgmt.create_line(simline_t::shipline, sp, fpl);
				delete fpl;

				append_child( new vehikel_builder_t(sp, "vehikel builder", prototyper, line, deppos, min(nr_vehicles,3)) );
				
				// tell the player
				char buf[256];
				sprintf(buf, translator::translate("%s\nnow operates\n%i ships between\n%s at (%i,%i)\nand %s at (%i,%i)."), sp->get_name(), nr_vehicles, translator::translate(fab1->get_name()), start.x, start.y, translator::translate(fab2->get_name()), ziel.x, ziel.y);
				welt->get_message()->add_message(buf, start.get_2d(), message_t::ai, PLAYER_FLAG|sp->get_player_nr(), prototyper->proto->besch[0]->get_basis_bild());

				// tell the industrymanager via the industry-connector
				rv->data = new datablock_t();
				rv->data->line = line;

				// reset prototyper, will be deleted in vehikel_builder
				prototyper = NULL;
				break;
			}
			default: 
				break;
		}
		sp->get_log().message( "connector_ship_t::step", "completed phase %d", phase);
		phase ++;
		rv->code = phase>4 ? RT_TOTAL_SUCCESS : RT_PARTIAL_SUCCESS;
		return rv;
	}
	else {
		// Step the childs.
		return bt_sequential_t::step();
	}
}


/**
 * build harbour at pos
 * pos is changed to be at the end of the newly build harbour mole
 * return if operation was succesfull
 */
bool connector_ship_t::build_harbour(koord3d &pos) const
{
	karte_t *welt = sp->get_welt();
	// calculate space
	koord zv(welt->lookup_kartenboden(pos.get_2d())->get_grund_hang());
	koord test_pos(pos.get_2d() - zv);
	for(uint8 i = 1; i<8; i++) {
		grund_t *gr = welt->lookup_kartenboden(test_pos);
		// TODO: reicht abfrage?
		if (gr && gr->ist_wasser() && !gr->get_halt().is_bound() && gr->find<gebaeude_t>()==NULL  &&  gr->get_depot()==NULL  &&  gr->kann_alle_obj_entfernen(sp)==NULL) {
			test_pos -= zv;
		}
		else {
			break;
		}
	}
	uint32 len = koord_distance(pos, test_pos);

	// get a suitable station
	const haus_besch_t* fh = get_random_harbour(welt->get_timeline_year_month(), haltestelle_t::WARE, len);
	bool ok = fh!=NULL;

	// build harbour immediately
	if (ok) {
		ok = sp->call_general_tool(WKZ_STATION, pos.get_2d(), fh->get_name());
	}
	// harbour_pos now the Molenende
	if (ok) {
		pos -= zv * (fh->get_h()-1);
	}
	else {
		sp->get_log().warning( "connector_ship_t::build_harbour", "failed to build a harbour at %s (route %s => %s)", pos.get_str(), fab1->get_name(), fab2->get_name() );
	}
	return ok;
}

void connector_ship_t::cleanup()
{
	switch (phase) {
		case 3: { // remove depot
			if (ourdepot) {
				sp->call_general_tool(WKZ_REMOVER, deppos.get_2d(), "");
			}
		} // fall through
		case 2:
		case 1: { // remove harbours
			if (start_harbour_pos != fab1->get_pos()) {
				sp->call_general_tool(WKZ_REMOVER, start_harbour_pos.get_2d(), "");
			}
			if (harbour_pos != fab2->get_pos()) {
				sp->call_general_tool(WKZ_REMOVER, harbour_pos.get_2d(), "");
			}
		} // fall through
		default: ;
	}
}

void connector_ship_t::debug( log_t &file, string prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("conr","%s phase=%d", prefix.c_str(), phase);
	string next_prefix( prefix + " prototyp");
	if (prototyper && phase<=2) prototyper->debug(file, next_prefix);
}

const haus_besch_t* connector_ship_t::get_random_harbour(const uint16 time, const uint8 enables, uint32 max_len) const
{
	weighted_vector_tpl<const haus_besch_t*> stops;

	for(  vector_tpl<const haus_besch_t *>::const_iterator iter = hausbauer_t::station_building.begin(), end = hausbauer_t::station_building.end();  iter != end;  ++iter  ) {
		const haus_besch_t* besch = (*iter);
		if(besch->get_utyp()==haus_besch_t::hafen  &&  besch->get_extra()==water_wt  &&  (enables==0  ||  (besch->get_enabled()&enables)!=0)) {
			// ok, now check timeline
			if(time==0  ||  (besch->get_intro_year_month()<=time  &&  besch->get_retire_year_month()>time)) {
				if( besch->get_b() == 1  &&  (uint32)besch->get_h() <= max_len  ) {
					stops.append(besch,max(1,16-besch->get_level()),16);
				}
			}
		}
	}
	return stops.empty() ? NULL : stops.at_weight(simrand(stops.get_sum_weight()));
}

/**
 * from ai_goods
 * find ship target position near harbout at pos, 
 * which is as close as possible to target (in Euclidean distance, no route search)
 */
koord3d connector_ship_t::get_ship_target(koord3d pos, koord3d target) const
{
	karte_t *welt = sp->get_welt();
	// sea pos (and not on harbour ... )
	halthandle_t halt = haltestelle_t::get_halt(welt,pos,sp);
	koord pos1 = pos.get_2d();
	koord3d best_pos = koord3d::invalid;
	for(uint8 radius = 1; radius<=2 /* welt->get_einstellungen()->get_station_coverage() */; radius++) {
		for(  int y = pos1.y-radius;  y<=pos1.y+radius;  y++  ) {
			for(  int x = pos1.x-radius;  x<=pos1.x+radius;  x++  ) {
				const grund_t *gr = welt->lookup_kartenboden(koord(x,y));
				// in water, the water tiles have no halt flag!
				if(gr  &&  gr->ist_wasser()  &&  !gr->get_depot()  &&  !gr->get_halt().is_bound()  &&  halt == haltestelle_t::get_halt(welt,gr->get_pos(),sp)
					   &&  (best_pos==koord3d::invalid || koord_distance(gr->get_pos(),target) < koord_distance(best_pos,target))  ) {
					best_pos = gr->get_pos();
				}
			}
		}
		if (best_pos != koord3d::invalid) {
			break;
		}
	}
	if (best_pos == koord3d::invalid) {
		sp->get_log().warning( "connector_ship::get_ship_target()","no starting position found near (%s)", pos.get_str());
	}
	// no copy constructor for koord3d available :P
	return koord3d(best_pos.get_2d(), best_pos.z);
}

/**
 * if pos is water returns pos otherwise the water tile in front of it
 */
koord3d connector_ship_t::get_water_tile(koord3d pos) const
{
	grund_t *gr = sp->get_welt()->lookup(pos);
	return gr->ist_wasser() ? pos : pos - koord(gr->get_grund_hang());
}
