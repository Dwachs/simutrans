#include "connections_manager.h"
#include "vehikel_builder.h"
#include "industry_connection_planner.h"
#include "../../ai_wai.h"
#include "../../../simdepot.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simline.h"
#include "../../../bauer/vehikelbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/fahrplan.h"
#include "../../../dataobj/loadsave.h"
#include "../../../dataobj/route.h"
#include "../../../vehicle/simvehikel.h"
#include "remover.h"

void connection_t::rdwr_connection(loadsave_t* file, const uint16 version, ai_wai_t *sp, connection_t* &c)
{
	uint8 t;
	if (file->is_saving()) {
		t = c ? (uint8) c->get_type() :  (uint8)CONN_NULL;
	}
	file->rdwr_byte(t);
	if (file->is_loading()) {
		c = alloc_connection((connection_types)t, sp);
	}
	if (c) {
		c->rdwr(file,version,sp);
	}
}

connection_t* connection_t::alloc_connection(connection_types t, ai_wai_t *sp)
{
	switch(t) {
		case CONN_NULL:
			return NULL;
		case CONN_COMBINED:
			return new combined_connection_t();
		case CONN_SERIAL:
			return new serial_connection_t();
		case CONN_PARALLEL:
			return new parallel_connection_t();
		case CONN_FREIGHT:
			return new freight_connection_t(sp);
		case CONN_WITHDRAWN:
			return new withdrawn_connection_t();
		case CONN_SIMPLE:
		default:
			assert(0);
			return NULL;
	}
}

void connection_t::rdwr(loadsave_t* file, const uint16, ai_wai_t *)
{
	simline_t::rdwr_linehandle_t(file, line);
	file->rdwr_byte(state);
}

void connection_t::debug( log_t &file, string prefix )
{
	if (line.is_bound()) {
		file.message("conn", "%s line(%d) (state %d) %s", prefix.c_str(), line.get_id(), state, line->get_name() );
	}
}


combined_connection_t::~combined_connection_t()
{
	for(uint32 i=0; i<connections.get_count(); i++) {
		delete connections[i];
		connections[i]=NULL;
	}
}


report_t* combined_connection_t::get_final_report(ai_wai_t *sp)
{
	report_t *r = new report_t();
	for(uint32 i=0; i<connections.get_count(); i++) {
		report_t *ri = connections[i]->get_final_report(sp);
		r->merge_report(ri);
		delete ri;
	}
	return r;
}

void combined_connection_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp)
{
	file->rdwr_long(next_to_report);

	uint32 count = connections.get_count();
	file->rdwr_long(count);
	for(uint32 i=0; i<count; i++) {
		if (file->is_loading()) {
			connections.append(NULL);
		}
		connection_t::rdwr_connection(file, version, sp, connections[i]);
	}
}
void combined_connection_t::debug( log_t &file, string prefix )
{
	for(uint32 i=0; i<connections.get_count(); i++) {
		char buf[40];
		sprintf(buf, "[%d] ", i);
		connections[i]->debug(file, prefix + buf);
	}
}


report_t* serial_connection_t::get_report(ai_wai_t *sp)
{
	if (connections.empty()) {
		state |= broken;
		return NULL;
	}
	if (next_to_report >= connections.get_count()) {
		next_to_report = 0;
	}
	report_t *report = connections[next_to_report]->get_report(sp);
	if (connections[next_to_report]->is_broken()) {
		state |= broken;
	}
	next_to_report++;
	return report;
}

report_t* parallel_connection_t::get_report(ai_wai_t *sp)
{
	if (connections.empty()) {
		state |= broken;
		return NULL;
	}
	if (next_to_report >= connections.get_count()) {
		next_to_report = 0;
	}
	report_t *report = connections[next_to_report]->get_report(sp);
	if (connections[next_to_report]->is_broken()) {
		if (report) delete report;
		// get final report of broken connection
		report = connections[next_to_report]->get_final_report(sp);
		// .. and delete the connection (infrastructure will be deleted by report)
		connection_t* conn = connections[next_to_report];
		connections.remove_at(next_to_report);
		delete conn;
	}
	next_to_report++;
	return report;
}

report_t* freight_connection_t::get_report(ai_wai_t *sp)
{
	if (!line.is_bound()  ||  line->count_convoys()==0  ||  !ziel.is_bound()) {
		// remove the line
		state |= broken;
		return NULL;
	}
	karte_t* welt = sp->get_welt();
	convoihandle_t cnv0 = line->get_convoy(0);
	// check for modernization of fleet ...
	if (last_upgrade_check != welt->get_timeline_year_month() ) {
		report_t *report = get_upgrade_report(sp, cnv0, false, last_upgrade_check);
		last_upgrade_check = welt->get_timeline_year_month();
		if (report) {
			status &= ~fcst_no_bigger_convois;
			// force upgrade
			report->gain_per_m = max(1000, report->gain_per_m);
			return report;
		}
	}

	// calculate capacities
	uint32 cap_cnv = 0;
	for(uint8 i=0; i<cnv0->get_vehikel_anzahl(); i++) {
		const vehikel_besch_t* besch = cnv0->get_vehikel(i)->get_besch();
		if (besch) {
			// compatible freights?
			if(besch->get_ware()->get_catg_index()==freight->get_catg_index()) {
				cap_cnv += besch->get_zuladung();
			}
		}
	}
	waytype_t wt = cnv0->get_vehikel(0)->get_waytype();
	// we iterate through schedule
	schedule_t* fpl = line->get_schedule();
	// check wether freight is available //WAS: at loading stations (with ladegrad >0)
	// sum of halts with freight for us
	uint8 freight_available = 0;
	bool foreign_freight = false;
	uint32 loading_station = fpl->get_count();
	for(uint32 i=0; i<fpl->get_count(); i++) {
		if (fpl->eintrag[i].ladegrad>0) {
			loading_station = i;
		}
		grund_t* gr = welt->lookup(fpl->eintrag[i].pos);
		if (!gr) {
			// TODO: correct schedule
			state |= broken;
			sp->get_log().warning( "freight_connection_t::get_report()","illegal entry[%d] in schedule of line '%s'", i, line->get_name() );
			return NULL;
		}
		halthandle_t h = wt != water_wt ? gr->get_halt() : haltestelle_t::get_halt(welt, gr->get_pos().get_2d(), sp);
		if (!h.is_bound()) {
			// TODO: correct schedule
			state |= broken;
			sp->get_log().warning( "freight_connection_t::get_report()","illegal entry[%d] in schedule of line '%s'", i, line->get_name() );
			return NULL;
		}
		uint32 cap_halt = h->get_capacity(2);
		// freight to the next our
		koord3d next_pos =  (i+1 < fpl->get_count() ? fpl->eintrag[i+1] : fpl->eintrag[0]).pos;
		halthandle_t next_halt = haltestelle_t::get_halt(welt, next_pos.get_2d(), sp);
		uint32 goods_halt = h->get_ware_fuer_zwischenziel(freight, next_halt);
		// freight available ?
		// either start is 2/3 full or more good available as one cnv can transport
		bool has_freight = (3*goods_halt > 2*cap_halt) || (goods_halt > (fpl->eintrag[i].ladegrad*cap_cnv)/100 );
		freight_available += has_freight;
		// unexpected freight at non-loading stations
		if (has_freight  &&  fpl->eintrag[i].ladegrad==0) {
			foreign_freight = true;
		}
		sp->get_log().message( "freight_connection_t::get_report()","line '%s' at '%s' freight_available=%d (cap_cnv: %d, cap_halt: %d, goods_halt: %d)", line->get_name(), h->get_name(), has_freight, cap_cnv, cap_halt, goods_halt);
	}
	// do we need to correct max-waiting time?
	// convention: ladegrad > 0 at loading station, only adjust waiting time
	if (loading_station < fpl->get_count()) {
		const bool adjust_schedule = foreign_freight ^ (fpl->eintrag[loading_station].waiting_time_shift>0);
		if (adjust_schedule) {
			schedule_t *new_fpl = fpl->copy();
			new_fpl->eintrag[loading_station].waiting_time_shift = foreign_freight ? 4 /*  ??  */ : 0 /* inf wait time */;
			line->set_schedule(new_fpl);
			sp->simlinemgmt.update_line(line);
		}
	}

	// count status of convois
	vector_tpl<convoihandle_t> stopped, empty, loss;
	uint32 newc=0, no_route=0;
	for(uint32 i=0; i<line->count_convoys(); i++) {
		convoihandle_t cnv = line->get_convoy(i);
		if (cnv->get_state()==convoi_t::SELF_DESTRUCT) {
			continue;
		}
		if (cnv->get_loading_level()==0) {
			// nothing transported in last 2 months and not older than 1 month -> probably new
			if ( (cnv->get_finance_history(0, CONVOI_TRANSPORTED_GOODS)+cnv->get_finance_history(1, CONVOI_TRANSPORTED_GOODS))<=0
				&& welt->get_current_month()-cnv->get_vehikel(0)->get_insta_zeit()<=1) {
					newc++;
			}
			else {
				empty.append(cnv);
			}
		}

		sint64 gewinn = 0;
		for( int j=0;  j<12;  j++  ) {
			gewinn += cnv->get_finance_history( j, CONVOI_PROFIT );
		}
		if (welt->get_current_month()-cnv->get_vehikel(0)->get_insta_zeit()>2  &&  gewinn<=0  ) {
			loss.append(cnv);
		}
		switch (cnv->get_state()){
			case convoi_t::NO_ROUTE:
				no_route++;
				/* fall through */
			case convoi_t::INITIAL:
			case convoi_t::LOADING:
			case convoi_t::WAITING_FOR_CLEARANCE:
			case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:
			case convoi_t::CAN_START:
			case convoi_t::CAN_START_ONE_MONTH:
			case convoi_t::WAITING_FOR_CLEARANCE_TWO_MONTHS:
			case convoi_t::CAN_START_TWO_MONTHS:
				stopped.append(cnv);
				break;
			default:
				break;
		}
	}
	sp->get_log().message( "freight_connection_t::get_report()","line '%s' cnv=%d empty=%d loss=%d stopped=%d new=%d", line->get_name(), line->count_convoys(), empty.get_count(), loss.get_count(), stopped.get_count(), newc);
	// now decide something
	if (no_route > 1  ||  no_route==line->count_convoys()) {
		// do not expect to much here: at least the stations will be removed
		state |= broken;
		sp->get_log().message( "freight_connection_t::get_report()","%d convois without route", no_route);
		return NULL;
	}
	if (freight_available) {
		if (newc==0  &&  stopped.get_count()> (uint32)max(line->count_convoys()/2, 2) ) {
			// traffic jam ..
			if (!bigger_convois_impossible()) {
				// try to find the bigger convois
				report_t *report = get_upgrade_report(sp, cnv0, true);
				if (report == NULL) {
					sp->get_log().message( "freight_connection_t::get_report()","no bigger vehicles available for line '%s'", line->get_name());
					status |= fcst_no_bigger_convois;
				}
				else {
					// force upgrade
					report->gain_per_m = max(1000, report->gain_per_m);
					return report;
				}
			}
			if (bigger_convois_impossible()) {
				sp->get_log().message( "freight_connection_t::get_report()","line '%s' sells %d convois due to traffic jam", line->get_name(), 1);
				// sell one empty & stopped convois
				for(uint32 i=0; i<stopped.get_count(); i++) {
					if (stopped[i]->get_loading_level()==0) {
						stopped[i]->self_destruct();
						break;
					}
				}
			}
		}
		else if (newc==0 && stopped.get_count()<=2) {
			// add one vehicle
			sp->get_log().message( "freight_connection_t::get_report()","line '%s' want to buy %d convois", line->get_name(), 1);
			simple_prototype_designer_t *d = new simple_prototype_designer_t(cnv0, freight);
			vehikel_builder_t *v = new vehikel_builder_t(sp, cnv0->get_name(), d, line, cnv0->get_home_depot(), 1);
			report_t *report = new report_t();
			report->action = v;
			report->gain_per_m = calc_gain_p_m();
			report->nr_vehicles = 1;
			return report;
		}
	}
	else {
		// TODO: increase_productivity somewhere
		if (empty.get_count() > 0) {
			sp->get_log().message( "freight_connection_t::get_report()","line '%s' sells %d convois", line->get_name(), (empty.get_count()+1)/3);
			// sell one third of the empty convois, keep minmum one convoi in the line
			const uint32 i0 = empty.get_count() < line->count_convoys() ? 0 : 1;
			for(uint32 i=i0; i<empty.get_count(); i+=3) {
				empty[i]->self_destruct();
			}
		}
	}
	return NULL;
}


report_t* freight_connection_t::get_upgrade_report(ai_wai_t *sp, convoihandle_t cnv, bool better_capacity, uint16 min_intro_date) const
{
	simple_prototype_designer_t *d = new simple_prototype_designer_t(cnv, freight);
	d->proto->calc_data(freight);
	d->max_length = 1;
	d->production = 0;
	if (better_capacity) {
		d->min_trans  = d->proto->max_speed * d->proto->get_capacity(freight)+1;
	}
	d->update();

	if (d->proto->is_empty()) {
		sp->get_log().message( "freight_connection_t::get_upgrade_report()","no vehicles available for line '%s'", line->get_name());
		delete d;
		return NULL;
	}

	// check if identical with convoi (or if now modern vehicles are chosen)
	bool different = cnv->get_vehikel_anzahl() != d->proto->besch.get_count();
	bool modern = false;
	for(uint8 i = 0; i < d->proto->besch.get_count(); i++) {
		if (i < cnv->get_vehikel_anzahl()) {
			different &= cnv->get_vehikel(i)->get_besch() != d->proto->besch[i];
		}
		modern |= d->proto->besch[i]->get_intro_year_month() > min_intro_date;
	}
	if (!different  ||  !modern) {
		if (!different) {
			sp->get_log().message( "freight_connection_t::get_upgrade_report()","best vehicle already in use for line '%s'", line->get_name());
		}
		else {
			sp->get_log().message( "freight_connection_t::get_upgrade_report()","no modern vehicle found for line '%s'", line->get_name());
		}
		delete d;
		return NULL;
	}
	// now we found something
	sp->get_log().message( "freight_connection_t::get_report()","line '%s' get %d new and bigger vehicles", line->get_name(), 3);
	// build three new vehicles
	vehikel_builder_t *v = new vehikel_builder_t(sp, d->proto->besch[0]->get_name(), d, line, cnv->get_home_depot(), 3);
	v->set_withdraw(true);
	report_t *report = new report_t();
	report->action = v;
	report->gain_per_m = calc_gain_p_m();
	report->nr_vehicles = 3;
	return report;
}

sint64 freight_connection_t::calc_gain_p_m() const
{
	sint64 sum = 0;
	for(uint8 i=0; i < MAX_MONTHS; i++) {
		sum += line->get_finance_history(i, LINE_PROFIT);
	}
	return sum / 12;
}

report_t* freight_connection_t::get_final_report(ai_wai_t *sp)
{
	// nothing to do ?
	if (!line.is_bound()) return NULL;

	report_t *report = new report_t();
	// find depot and stations
	koord3d depot(koord3d::invalid), start(koord3d::invalid), end(koord3d::invalid);
	karte_t *const welt = sp->get_welt();
	if (line->count_convoys()>0) {
		convoihandle_t cnv0 = line->get_convoy(0);
		waytype_t wt = cnv0->get_vehikel(0)->get_waytype();
		// find depot (this should be empty and not home depot of another line)
		koord3d try_depot = cnv0->get_home_depot();
		grund_t *gr = welt->lookup(try_depot);
		if (gr) {
			bool ok = false;
			depot_t *dep = gr->get_depot();
			if (dep  &&  dep->get_besitzer()==sp
				&&  dep->get_wegtyp()==cnv0->get_vehikel(0)->get_waytype()
				&&  dep->convoi_count()==0) {
					// search all other lines
					ok = true;
					vector_tpl<linehandle_t> lines;
					sp->simlinemgmt.get_lines( dep->get_line_type(), &lines );
					for(uint32 i=0; i<lines.get_count(); i++) {
						linehandle_t line2 = lines[i];
						if (line!=line2  &&  line2->count_convoys()>0  &&  line2->get_convoy(0)->get_home_depot()==try_depot) {
							ok = false;
							break;
						}
					}
			}
			// TODO: what if a new route is build in the meanwhile that needs this depot?
			if (ok) {
				depot = try_depot;
				report->cost_fix   += -welt->get_settings().cst_multiply_remove_haus;
				report->gain_per_m += industry_connection_planner_t::calc_building_maint(dep->get_tile()->get_besch(), welt);

			}
		}
		// now find the stations
		schedule_t *fpl = cnv0->get_schedule();
		if (fpl  &&   fpl->get_count()>1) {
			for(uint8 i=0; i<fpl->get_count();i++) {
				// TODO: do something smarter here
				// TODO: find harbours
				if (start==koord3d::invalid  &&  fpl->eintrag[i].ladegrad > 0) {
					start = fpl->eintrag[i].pos;
				}
				if (end==koord3d::invalid  &&  fpl->eintrag[i].ladegrad == 0) {
					end = fpl->eintrag[i].pos;
				}
			}
		}
		bt_sequential_t *root = new bt_sequential_t(sp, "remove routes");
		report->action = root;
		// find route start->end, start->depot
		route_t verbindung_e, verbindung_d;
		// get a default vehikel
		vehikel_besch_t remover_besch(wt, 500, vehikel_besch_t::diesel );
		vehikel_t *test_driver = vehikelbauer_t::baue(start, sp, NULL, &remover_besch);
		// .. first start->end
		sp->get_log().warning("freight_connection_t::get_final_report", "start %s", start.get_str());
		sp->get_log().warning("freight_connection_t::get_final_report", "end %s", end.get_str());
		sp->get_log().warning("freight_connection_t::get_final_report", "depot %s", depot.get_str());
		if (start!=koord3d::invalid  &&  end!=koord3d::invalid) {
			verbindung_e.calc_route(sp->get_welt(), start, end, test_driver, 0, 1);
			// evaluate the route
			for(uint32 i=0; i<verbindung_e.get_count(); i++) {
				grund_t *gr = welt->lookup(verbindung_e.position_bei(i));
				if (gr->hat_weg(wt)  &&  gr->get_weg(wt)->get_besch()  &&  gr->get_weg(wt)->ist_entfernbar(sp)==NULL) {
					report->cost_fix   += gr->get_weg(wt)->get_besch()->get_preis();
					report->gain_per_m += gr->get_weg(wt)->get_besch()->get_wartung() << (welt->ticks_per_world_month_shift-18);
				}
				if ( (i==0  ||  i==verbindung_e.get_count()-1)  &&  gr->is_halt()) {
					// TODO estimate costs for bridges/tunnels/wayobjs/signals etc
					report->cost_fix   += -welt->get_settings().cst_multiply_remove_haus;
					if (gr->find<gebaeude_t>()) {
						report->gain_per_m += industry_connection_planner_t::calc_building_maint(gr->find<gebaeude_t>()->get_tile()->get_besch(), welt);
					}
				}
			}
			root->append_child( new remover_t(sp, "remove start->end", wt, start, end));
		}
		// ..  then start->depot
		if (start!=koord3d::invalid  &&  depot!=koord3d::invalid) {
			verbindung_d.calc_route(sp->get_welt(), start, depot, test_driver, 0, 1);
			koord3d depot_end=start;
			if (verbindung_d.get_count()>1) {
				uint32 j;
				if (verbindung_e.get_count()>1) {
					// loop until the first tile that is not in both routes
					for(j=0; (j<verbindung_d.get_count()) && (j<verbindung_e.get_count())  &&  (verbindung_d.position_bei(j)==verbindung_e.position_bei(j)); j++) ;
				}
				else {
					for(j=0; j<verbindung_d.get_count(); j++) {
						grund_t *gr = welt->lookup(verbindung_d.position_bei(j));
						if (!gr  ||  ribi_t::is_threeway(gr->get_weg_ribi_unmasked(wt))) {
							j++;
							break;
						}
					}
				}
				if (j<verbindung_d.get_count()) {
					sp->get_log().warning("freight_connection_t::get_final_report", "entry %s", verbindung_d.position_bei(j>0 ? j-1 : 0).get_str());
					// evaluate the route
					for(uint32 i=j; i<verbindung_e.get_count(); i++) {
						grund_t *gr = welt->lookup(verbindung_e.position_bei(i));
						if (gr->hat_weg(wt)  &&  gr->get_weg(wt)->get_besch()  &&  gr->get_weg(wt)->ist_entfernbar(sp)==NULL) {
							report->cost_fix   += gr->get_weg(wt)->get_besch()->get_preis();
							report->gain_per_m += gr->get_weg(wt)->get_besch()->get_wartung() << (welt->ticks_per_world_month_shift-18);
						}
					}
					depot_end = verbindung_d.position_bei(j>0 ? j-1 : 0);
				}
			}
			root->append_child( new remover_t(sp, "remove depot->start", wt, depot, depot_end));
		}
		test_driver->set_flag(ding_t::not_on_map);
		delete test_driver;
	}
	// immediately sell all convois
	for(sint32 i=line->count_convoys()-1; i>=0; i--) {
		line->get_convoy(i)->self_destruct();
	}
	return report;
}

void freight_connection_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp)
{
	connection_t::rdwr(file, version, sp);

	ziel.rdwr(file, version, sp);
	ai_t::rdwr_ware_besch(file, freight);
	file->rdwr_byte(status);
}

void freight_connection_t::debug( log_t &file, string prefix )
{
	connection_t::debug(file, prefix);
	if (ziel.is_bound()) {
		file.message("frec", "%s to      %s(%s) - status(%d)", prefix.c_str(), ziel->get_name(), ziel->get_pos().get_str(), status );
	}
	else {
		file.message("frec", "%s to      %s(%s) - status(%d)", prefix.c_str(), "NULL", koord3d::invalid.get_str(), status );
	}
	file.message("frec", "%s freight %s", prefix.c_str(), freight->get_name() );
}




void withdrawn_connection_t::debug( log_t &file, string prefix )
{
	connection_t::debug(file, prefix);
	file.message("widr", "%d convois left", line->count_convoys());
}


report_t* withdrawn_connection_t::get_report(ai_wai_t *sp)
{
	uint32 n=0;
	for(uint32 i=0; i<line->count_convoys(); i++) {
		convoihandle_t cnv = line->get_convoy(i);
		if (!cnv->get_withdraw()) {
			cnv->set_withdraw(true);
			n++;
		}
	}
	sp->get_log().message( "withdrawn_connection_t::get_report()","line '%s' withdraws %d convois", line->get_name(), n);
	return NULL;
}