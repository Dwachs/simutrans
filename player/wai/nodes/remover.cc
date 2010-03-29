#include "remover.h"
#include "../../ai_wai.h"
#include "../../../bauer/vehikelbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/route.h"
#include "../../../dataobj/loadsave.h"
#include "../../../vehicle/simvehikel.h"
#include "../../../simhalt.h"
#include "../../../simwerkz.h"

// returns
// 0 .. error, abort removing
// 1 .. cannot remove now, wait
// 2 .. ignore this tile
// 3 .. remove way here
// removes depots!
uint8 remover_t::check_position(koord3d pos) 
{
	karte_t *welt = sp->get_welt();
	grund_t *gr = welt->lookup(pos);
	if(gr==NULL  ||  !gr->hat_weg(wt)) {
		return 0;
	}
	if(wt==water_wt  &&  gr->ist_wasser()) {
		return 2;
	}
	weg_t *weg = gr->get_weg(wt);
	if (weg->ist_entfernbar(sp)!=NULL) {
		// ignore foreign ways
		return 2;
	}
	if (gr->get_depot()) {
		if (gr->get_depot()->ist_entfernbar(sp)!=NULL) {
			// depot not empty .. ignore
			return 2;
		}
		wkz_remover_t bulldozer;
		bulldozer.init(welt, sp);
		while(gr->get_depot()  &&  bulldozer.work(welt, sp, gr->get_pos())==NULL);
		if (gr->get_depot()) {
			// depot does not want to go away .. ignore
			return 2;
		}
	}
	// crossing with traffic
	if (ribi_t::is_threeway(weg->get_ribi_unmasked())  &&  (weg->get_statistics(WAY_STAT_CONVOIS)==0)) {
		return 1;
	}
	return 3;
}

return_value_t* remover_t::step()
{
	uint8 res1 = check_position(start);
	uint8 res2 = check_position(end);
	sp->get_log().warning("remover_t::step", "from %s(%d) to %s,%d(%d)", start.get_str(), res1, end.get_2d().get_str(), end.z, res2);
	// fatal
	if (res1==0  ||  res2==0) {
		return new_return_value(RT_TOTAL_FAIL);
	}
	// wait
	if (res1==1) {
		return new_return_value(RT_DONE_NOTHING);
	}
	// start removing
	route_t verbindung;
	// get a default vehikel
	fahrer_t* test_driver;
	if(  wt!=powerline_wt  ) {
		vehikel_besch_t remover_besch(wt, 500, vehikel_besch_t::diesel );
		test_driver = vehikelbauer_t::baue(start, sp, NULL, &remover_besch);
	}
	else {
		return new_return_value(RT_TOTAL_FAIL);
	}
	verbindung.calc_route(sp->get_welt(), start, end, test_driver, 0);
	delete test_driver;
	karte_t *welt = sp->get_welt();
	if (verbindung.get_count()>1) {
		// use wayremover
		wkz_wayremover_t wkz;
		char defpar[20]; 
		sprintf(defpar, "%d", wt);
		wkz.set_default_param(defpar);		
		sint32 imax = verbindung.get_count()-1;
		// remove from both ends in two phases (step=-1 or +1)
		for (sint8 step = -1; step<=1; step+=2) {
			for(sint32 i = step==-1 ? verbindung.get_count()-2 : 1; (i>=0) && (i<=imax); i+=step) {
				// TODO: what happens with harbours and shipyards?
				uint8 res = check_position(verbindung.position_bei(i-step));
				sp->get_log().warning("remover_t::step", "check %s(%d)", verbindung.position_bei(i-step).get_str(), res);
				if (res==2) continue;
				bool ok = res > 1;
				if (ok) {
					wkz.init(welt, sp);
					const char *err1 = wkz.work(welt, sp, verbindung.position_bei(i-step));
					const char *err2 = (err1==NULL) ? wkz.work(welt, sp, verbindung.position_bei(i)) : err1;
					sp->get_log().warning("remover_t::step", "from %s to %s, res %d/%s/%s", verbindung.position_bei(i-step).get_str(),verbindung.position_bei(i).get_2d().get_str(), res, err1, err2);
					if (err1!=NULL	||  err2!=NULL) {
							ok = false;
					}
				}
				if (!ok){
					// failed, reset start, end
					if (step==-1) {
						end = verbindung.position_bei(i-step);
					}
					else {
						start = verbindung.position_bei(i-step);
					}
					imax = i-step;
					break;
				}
			}
			if (imax == verbindung.get_count()-1) {
				// removed everything
				return new_return_value(RT_TOTAL_SUCCESS);
			}
			if (!twosided) {
				break;
			}
		}
	}
	return new_return_value(RT_SUCCESS);
}

void remover_t::rdwr(loadsave_t* file, const uint16 version)
{
	bt_node_t::rdwr(file,version);
	start.rdwr(file);
	end.rdwr(file);
	uint8 iwt = wt;
	file->rdwr_byte(iwt, "");
	wt = (waytype_t) iwt;
}
void remover_t::rotate90( const sint16 y_size)
{
	bt_node_t::rotate90(y_size);
	start.rotate90(y_size);
	end.rotate90(y_size);
}
void remover_t::debug( log_t &file, cstring_t prefix )
{
	file.message("remo","from %s to %s,%d", start.get_str(), end.get_2d().get_str(), end.z);
}
