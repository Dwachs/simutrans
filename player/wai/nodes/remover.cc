#include "remover.h"
#include "../../ai_wai.h"
#include "../../../bauer/vehikelbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/route.h"
#include "../../../dataobj/loadsave.h"
#include "../../../vehicle/simvehikel.h"
#include "../../../simdepot.h"
#include "../../../simhalt.h"
#include "../../../simwerkz.h"

// returns
enum {
	CP_FATAL  = 0,
	CP_ERROR  = 1,
	CP_WAIT   = 2,
	CP_REMOVE = 3,
	CP_IGNORE = 4
};
// removes depots!
uint8 remover_t::check_position(koord3d pos)
{
	karte_t *welt = sp->get_welt();
	grund_t *gr = welt->lookup(pos);
	if(gr==NULL  ||  !(gr->hat_weg(wt)  ||  (wt==water_wt  &&  (gr->ist_wasser()  ||  gr->is_halt()))) ){
		return CP_FATAL;
	}
	if (gr->get_depot()) {
		if (gr->get_depot()->ist_entfernbar(sp)!=NULL) {
			// depot not empty .. ignore
			return CP_IGNORE;
		}
		wkz_remover_t bulldozer;
		bulldozer.init(welt, sp);
		while(gr->get_depot()  &&  bulldozer.work(welt, sp, gr->get_pos())==NULL) { /*empty*/ }
		if (gr->get_depot()) {
			// depot does not want to go away .. ignore
			return CP_IGNORE;
		}
	}
	if(wt==water_wt  &&  gr->ist_wasser()) {
		return CP_IGNORE;
	}
	weg_t *weg = gr->get_weg(wt);
	if (weg) {
		if (weg->ist_entfernbar(sp)!=NULL) {
			// ignore foreign ways
			return CP_IGNORE;
		}
		ribi_t::ribi ribi = weg->get_ribi_unmasked();
#ifdef remove_crossing_with_no_traffic
		// crossing with traffic
		if (weg  &&  !ribi_t::is_threeway(ribi)  &&  (weg->get_statistics(WAY_STAT_CONVOIS)==0)) {
			return CP_WAIT;
		}
#else
		// crossing - stop removing here
		if (ribi  &&  !ribi_t::ist_einfach(ribi)) {
			return CP_ERROR;
		}
#endif
		if (weg->get_besitzer() == NULL) {
			// ignore city roads
			return CP_IGNORE;
		}
	}
	else {
		return CP_FATAL;
	}
	return CP_REMOVE;
}

return_value_t* remover_t::step()
{
	uint8 res1 = check_position(start);
	uint8 res2 = check_position(end);
	sp->get_log().warning("remover_t::step", "from %s(%d) to %s,%d(%d)", start.get_str(), res1, end.get_2d().get_str(), end.z, res2);
	// fatal
	if (res1==CP_FATAL  ||  res2==CP_FATAL  || (res1==CP_ERROR  &&  res2==CP_ERROR)) {
		return new_return_value(RT_TOTAL_FAIL);
	}
	// wait
	if (res1==CP_WAIT) {
		return new_return_value(RT_DONE_NOTHING);
	}
	karte_t *welt = sp->get_welt();
	// remove harbours at end
	if (wt==water_wt  &&  first_step) {
		remove_harbour(start);
		remove_harbour(end);
	}
	first_step = false;
	if(  wt==powerline_wt  ) {
		return new_return_value(RT_TOTAL_FAIL);
	}
	// start removing
	route_t verbindung;
	// get a default vehikel
	vehikel_besch_t remover_besch(wt, 500, vehikel_besch_t::diesel );
	vehikel_t* test_driver = vehikelbauer_t::baue(start, sp, NULL, &remover_besch);
	verbindung.calc_route(welt, start, end, test_driver, 0, 1);
	test_driver->set_flag(ding_t::not_on_map);
	delete test_driver;
	if (verbindung.get_count()>1) {
		// use wayremover
		wkz_wayremover_t wkz;
		char defpar[20];
		sprintf(defpar, "%d", wt);
		wkz.set_default_param(defpar);
		sint32 imax = verbindung.get_count()-1;
		// remove from both ends in two phases (step=-1 or +1)
		for (sint8 step = -1; step<=1; step+=2) {
			bool ready = true;
			for(sint32 i = step==-1 ? verbindung.get_count()-2 : 1; (i>=0) && (i<=imax); i+=step) {
				// TODO: what happens with harbours?
				const koord3d pos = verbindung.position_bei(i-step);
				uint8 res = check_position(pos);
				sp->get_log().warning("remover_t::step", "check %s(%d)", pos.get_str(), res);
				// if fatal: maybe we want to go over already deleted bridge
				if (res==CP_IGNORE  ||  res==CP_FATAL) {
					if (start == pos) {
						start = verbindung.position_bei(i);
						imax = i;
						sp->get_log().warning("remover_t::step", "set start to %s", start.get_str());
					}
					else if (end == pos) {
						end = verbindung.position_bei(i);
						imax = i;
						sp->get_log().warning("remover_t::step", "set end to %s", end.get_str());
					}
					ready = false;
					continue;
				}
				bool ok = res >= CP_REMOVE;
				if (ok) {
					wkz.init(welt, sp);
					const char *err1 = wkz.work(welt, sp, verbindung.position_bei(min(i,i-step)));
					const char *err2 = (err1==NULL) ? wkz.work(welt, sp, verbindung.position_bei(max(i,i-step))) : err1;
					sp->get_log().warning("remover_t::step", "from %s to %s, res %d/%s/%s", verbindung.position_bei(min(i,i-step)).get_str(),verbindung.position_bei(max(i,i-step)).get_2d().get_str(), res, err1, err2);
					if (err2!=NULL) {
						// maybe end tile owned by other player, remove way directly on the tile
						err1 = wkz.work(welt, sp, pos);
						err2 = (err1==NULL) ? wkz.work(welt, sp, pos) : err1;
					}
					if (err1!=NULL	||  err2!=NULL) {
							ok = false;
					}
				}
				if (!ok){
					// failed, reset start, end
					if (step==-1) {
						end = pos;
						sp->get_log().warning("remover_t::step", "set end to %s", end.get_str());
					}
					else {
						start = pos;
						sp->get_log().warning("remover_t::step", "set start to %s", start.get_str());
					}
					imax = i-step;
					ready = false;
					break;
				}
			}
			if (ready) {
				if (imax==0) {
					// remove everything on start tile
					remove_way_end(start);
				}
				// removed everything
				return new_return_value(RT_TOTAL_SUCCESS);
			}
		}
	}
	else {
		// delete from start/end until crossing is encountered
		remove_way_end(start);
		remove_way_end(end);
		// nothing more to do: no route found / deleted everything we could
		//return new_return_value(RT_TOTAL_SUCCESS);
	}
	return new_return_value(RT_SUCCESS);
}

void remover_t::remove_way_end(koord3d &pos)
{
	karte_t *welt = sp->get_welt();
	wkz_wayremover_t bulldozer;
	char buf[10];
	sprintf(buf, "%d", wt);
	bulldozer.set_default_param(buf);

	while(check_position(pos)==CP_REMOVE) {
		grund_t *gr = welt->lookup(pos);
		assert(gr);
		if(gr->hat_weg(wt)) {
			weg_t *weg = gr->get_weg(wt);
			ribi_t::ribi ribi = weg->get_ribi_unmasked();
			if (ribi_t::ist_einfach(ribi)  ||  ribi == ribi_t::keine) {
				grund_t *to = NULL;
				gr->get_neighbour(to, wt, koord(ribi));
				// 'double click' to remove way here
				bulldozer.init(welt, sp);
				const char *msg = bulldozer.work(welt, sp, pos);
				if (msg==NULL) {
					msg = bulldozer.work(welt, sp, pos);
				}
				if (msg) {
					sp->get_log().warning("remover_t::remove_way_end", "bulldozing at (%s) failed: %s", pos.get_str(),msg);
				}
				if (to) {
					pos = to->get_pos();
				}
				else {
					break;
				}
			}
		}
	}
}

void remover_t::remove_harbour(koord3d pos) const
{
	karte_t *welt = sp->get_welt();
	// find harbour
	grund_t *harbour = NULL;
	grund_t *gr = welt->lookup(pos);
	halthandle_t halt = haltestelle_t::get_halt(welt, pos, sp);
	if (gr  &&  !gr->is_halt()  &&  halt.is_bound()  &&  halt->get_besitzer()==sp) {
		for (uint8 r=0; r<8; r++) {
			grund_t *test=welt->lookup_kartenboden(pos.get_2d() + koord::neighbours[r]);
			if (test  &&  test->is_halt()  &&  test->get_halt() == halt
				&&  (!test->hat_wege()  ||  test->hat_weg(water_wt))) {
				harbour = test;
				break;
			}
		}
	}
	if (harbour) {
		wkz_remover_t bulldozer;
		const char *msg;
		do {
			msg = bulldozer.work(welt, sp, harbour->get_pos());
		} while (msg==NULL  ||  harbour->is_halt());
		sp->get_log().warning("remover_t::step", "harbour at (%s) deleted, msg = %s", harbour->get_pos().get_str(), msg);
	}
}

void remover_t::rdwr(loadsave_t* file, const uint16 version)
{
	bt_node_t::rdwr(file,version);
	start.rdwr(file);
	end.rdwr(file);
	uint8 iwt = wt;
	file->rdwr_byte(iwt);
	wt = (waytype_t) iwt;
	file->rdwr_bool(first_step);
}
void remover_t::rotate90( const sint16 y_size)
{
	bt_node_t::rotate90(y_size);
	start.rotate90(y_size);
	end.rotate90(y_size);
}
void remover_t::debug( log_t &file, string prefix )
{
	file.message("remo","%s from %s to %s,%d", prefix.c_str(), start.get_str(), end.get_2d().get_str(), end.z);
}
