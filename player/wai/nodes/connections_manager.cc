#include "connections_manager.h"
#include "vehikel_builder.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../dataobj/loadsave.h"
#include "../../../vehicle/simvehikel.h"
void connection_t::rdwr_connection(loadsave_t* file, const uint16 version, ai_wai_t *sp, connection_t* &c)
{
	uint8 t;
	if (file->is_saving()) {
		t = c ? (uint8) c->get_type() : CONN_NULL;
	}
	file->rdwr_byte(t,"");
	if (file->is_loading()) {
		c = alloc_connection((connection_types)t);
	}
	if (c) {
		c->rdwr(file,version,sp);
	}
}

connection_t* connection_t::alloc_connection(connection_types t)
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
			return new freight_connection_t();
		case CONN_WITHDRAWN:
			return new withdrawn_connection_t();
		case CONN_SIMPLE:
		default:
			assert(0);
			return NULL;
	}
}

void connection_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp)
{
	uint16 line_id;
	if (file->is_saving()) {
		line_id = line.is_bound() ? line->get_line_id() : INVALID_LINE_ID;
	}
	file->rdwr_short(line_id, " ");
	if (file->is_loading()) {
		if (line_id != INVALID_LINE_ID) {
			line = sp->simlinemgmt.get_line_by_id(line_id);
		}
	}
}

void connection_t::debug( log_t &file, cstring_t prefix )
{
	if (line.is_bound()) {
		file.message("conn", "%s line(%d) %s", (const char*)prefix, line->get_line_id(), line->get_name() );
	}
}


combined_connection_t::~combined_connection_t()
{
	for(uint32 i=0; i<connections.get_count(); i++) {
		delete connections[i];
		connections[i]=NULL;
	}
}

void combined_connection_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp)
{
	file->rdwr_long(next_to_report, "");

	uint32 count = connections.get_count();
	file->rdwr_long(count,"");
	for(uint32 i=0; i<count; i++) {
		if (file->is_loading()) {
			connections.append(NULL);
		}
		connection_t::rdwr_connection(file, version, sp, connections[i]);
	}
}
void combined_connection_t::debug( log_t &file, cstring_t prefix )
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
		return NULL;
	}
	if (next_to_report >= connections.get_count()) {
		next_to_report = 0;
	}
	return connections[next_to_report++]->get_report(sp);
}

report_t* parallel_connection_t::get_report(ai_wai_t *sp)
{
	if (connections.empty()) {
		return NULL;
	}
	if (next_to_report >= connections.get_count()) {
		next_to_report = 0;
	}
	return connections[next_to_report++]->get_report(sp);
}

report_t* freight_connection_t::get_report(ai_wai_t *sp)
{
	if (!line.is_bound()  ||  line->count_convoys()==0  ||  !ziel.is_bound()) {
		// TODO: remove the line, wird die vielleicht gebraucht??
		return NULL;
	}
	karte_t* welt = sp->get_welt();
	// check wether freight is available at loading stations (with ladegrad >0)
	// calculate capacities
	bool freight_available = false;

	convoihandle_t cnv0 = line->get_convoy(0);
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
	schedule_t* fpl = line->get_schedule();
	for(uint32 i=0; i<fpl->get_count(); i++) {
		if (fpl->eintrag[i].ladegrad>0) {
			grund_t* gr = welt->lookup(fpl->eintrag[i].pos);
			if (!gr) {
				// TODO: correct schedule
				sp->get_log().error( "freight_connection_t::get_report()","illegal entry[%d] in schedule of line '%s'", i, line->get_name() );
				return NULL;
			}
			halthandle_t h = gr->get_halt();
			if (!h.is_bound()) {
				// TODO: correct schedule
				sp->get_log().error( "freight_connection_t::get_report()","illegal entry[%d] in schedule of line '%s'", i, line->get_name() );
				return NULL;
			}
			uint32 cap_halt = h->get_capacity(2);
			uint32 goods_halt = h->get_ware_fuer_zielpos(freight, ziel->get_pos().get_2d());
			// freight available ?
			// either start is 2/3 full or more good available as one cnv can transort
			freight_available = (3*goods_halt > 2*cap_halt) || (goods_halt > (fpl->eintrag[i].ladegrad*cap_cnv)/100 );
			sp->get_log().message( "freight_connection_t::get_report()","line '%s' freight_available=%d (cap_cnv: %d, cap_halt: %d, goods_halt: %d)", line->get_name(), freight_available, cap_cnv, cap_halt, goods_halt);
		}
	}

	// count status of convois
	vector_tpl<convoihandle_t> stopped, empty, loss; uint32 newc=0;
	sint64 mitt_gewinn = 0;
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
		mitt_gewinn+=gewinn;
		if (welt->get_current_month()-cnv->get_vehikel(0)->get_insta_zeit()>2  &&  gewinn<=0  ) {
			loss.append(cnv);
		}
		switch (cnv->get_state()){
			case convoi_t::INITIAL:
			case convoi_t::NO_ROUTE:
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
	if (freight_available) {
		if (newc==0 && stopped.get_count()> max(line->count_convoys()/2, 2) ) {
			// traffic jam ..
			if (!bigger_convois_impossible()) {
				// try to find the bigger convois
				simple_prototype_designer_t *d = new simple_prototype_designer_t(cnv0, freight);
				d->proto->calc_data(freight);
				d->max_length = 1;
				d->production = 0;
				d->min_trans  = d->proto->max_speed * d->proto->get_capacity(freight)+1;
				d->update();

				if (d->proto->is_empty()) {
					sp->get_log().message( "freight_connection_t::get_report()","no bigger vehicles available for line '%s'", line->get_name());
					status |= 1;
					// TODO: clear this bit eventually
				}
				else {
					sp->get_log().message( "freight_connection_t::get_report()","line '%s' get %d new and bigger vehicles", line->get_name(), 3);
					// build three new vehicles
					vehikel_builder_t *v = new vehikel_builder_t(sp, d->proto->besch[0]->get_name(), d, line, cnv0->get_home_depot(), 3);
					v->set_withdraw(true);
					report_t *report = new report_t();
					report->action = v;
					// TODO: better estimation of the gain
					report->gain_per_v_m = min( 1000, mitt_gewinn/(12*line->count_convoys()) );
					report->nr_vehicles = 3;
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
			report->gain_per_v_m = mitt_gewinn/(12*line->count_convoys());
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


void freight_connection_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp)
{
	connection_t::rdwr(file, version, sp);

	ziel.rdwr(file, version, sp);
	ai_t::rdwr_ware_besch(file, freight);
	file->rdwr_byte(status, "");
}

void freight_connection_t::debug( log_t &file, cstring_t prefix )
{
	connection_t::debug(file, prefix);
	if (ziel.is_bound()) {
		file.message("frec", "%s to      %s(%s) - status(%d)", (const char*)prefix, ziel->get_name(), ziel->get_pos().get_str(), status );
	}
	else {
		file.message("frec", "%s to      %s(%s) - status(%d)", (const char*)prefix, "NULL", koord3d::invalid.get_str(), status );
	}
	file.message("frec", "%s freight %s", (const char*)prefix, freight->get_name() );
}




void withdrawn_connection_t::debug( log_t &file, cstring_t prefix )
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