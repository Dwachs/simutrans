#include "industry_manager.h"

#include "vehikel_builder.h"
#include "../../ai.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simline.h"
#include "../../../dataobj/loadsave.h"
#include "../../../vehicle/simvehikel.h"

report_t* industry_connection_t::get_report(ai_wai_t *sp)
{
	// TODO: process the shipline here too
	if (!is<own>() || !line.is_bound()) {
		return NULL;
	}
	if (line->count_convoys()==0) {
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
				sp->get_log().error( "industry_connection_t::get_report()","illegal entry[%d] in schedule of line '%s'", i, line->get_name() );
				return NULL;
			}
			halthandle_t h = gr->get_halt();
			if (!h.is_bound()) {
				// TODO: correct schedule
				sp->get_log().error( "industry_connection_t::get_report()","illegal entry[%d] in schedule of line '%s'", i, line->get_name() );
				return NULL;
			}
			uint32 cap_halt = h->get_capacity(2);
			uint32 goods_halt = h->get_ware_fuer_zielpos(freight, ziel->get_pos().get_2d());
			// freight available ?
			// either start is 2/3 full or more good available as one cnv can transort
			freight_available = (3*goods_halt > 2*cap_halt) || (goods_halt > (fpl->eintrag[i].ladegrad*cap_cnv)/100 );
			sp->get_log().message( "industry_connection_t::get_report()","line '%s' freight_available=%d (cap_cnv: %d, cap_halt: %d, goods_halt: %d)", line->get_name(), freight_available, cap_cnv, cap_halt, goods_halt);
		}
	}

	// count status of convois
	vector_tpl<convoihandle_t> stopped, empty, loss;
	sint64 mitt_gewinn = 0;
	for(uint32 i=0; i<line->count_convoys(); i++) {
		convoihandle_t cnv = line->get_convoy(i);
		if (cnv->get_loading_level()==0) {
			empty.append(cnv);
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
	sp->get_log().message( "industry_connection_t::get_report()","line '%s' cnv=%d empty=%d loss=%d stopped=%d", line->get_name(), line->count_convoys(), empty.get_count(), loss.get_count(), stopped.get_count());
	// now decide something
	if (freight_available) {
		if (stopped.get_count()> max(line->count_convoys()/2, 2) ) {
			// TODO: traffic jam ..		
			sp->get_log().message( "industry_connection_t::get_report()","line '%s' sell %d convois due to traffic jam", line->get_name(), 1);	
			// sell one empty & stopped convois
			for(uint32 i=0; i<stopped.get_count(); i++) {
				if (stopped[i]->get_loading_level()==0) {
					stopped[i]->self_destruct();
					stopped[i]->step();	// to really get rid of it
					break;
				}
			}
		}
		else if (stopped.get_count()<=2) {
			// add one vehicle
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
		if (empty.get_count() > 1) {
			sp->get_log().message( "industry_connection_t::get_report()","line '%s' sell %d convois", line->get_name(), (empty.get_count()+2)/3);
			// sell one third of the empty convois, keep minmum one convoi
			for(uint32 i=1; i<empty.get_count(); i+=3) {
				empty[i]->self_destruct();
				empty[i]->step();	// to really get rid of it
			}
		}
	}
	return NULL;
}

void industry_connection_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp)
{
	file->rdwr_longlong(status, "");

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

	uint16 shipline_id;
	if (file->is_saving()) {
		shipline_id = shipline.is_bound() ? shipline->get_line_id() : INVALID_LINE_ID;
	}
	file->rdwr_short(shipline_id, " ");
	if (file->is_loading()) {
		if (shipline_id != INVALID_LINE_ID) {
			shipline = sp->simlinemgmt.get_line_by_id(shipline_id);
		}
	}

	ai_t::rdwr_fabrik(file, sp->get_welt(), start);
	ai_t::rdwr_fabrik(file, sp->get_welt(), ziel);
	ai_t::rdwr_ware_besch(file, freight);
}

void industry_connection_t::debug( log_t &file, cstring_t prefix )
{
	file.message("indc", "%s from    %s(%s)", (const char*)prefix, start->get_name(), start->get_pos().get_str() );
	file.message("indc", "%s to      %s(%s)", (const char*)prefix, ziel->get_name(), ziel->get_pos().get_str() );
	file.message("indc", "%s freight %s", (const char*)prefix, freight->get_name() );

	if (line.is_bound()) {
		file.message("indc", "%s line(%d) %s", (const char*)prefix, line->get_line_id(), line->get_name() );
	}
	if (shipline.is_bound()) {
		file.message("indc", "%s shipline(%d) %s", (const char*)prefix, shipline->get_line_id(), shipline->get_name() );
	}
	file.message("indc", "%s status=%d", (const char*)prefix, status);
}

uint32 industry_manager_t::get_connection_id(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
{
	industry_connection_t ic(s,z,f);
	if (connections.is_contained(ic)) {
		return connections.index_of(ic);
	}
	else {
		connections.append(ic);
		return connections.get_count()-1;
	}
}

industry_connection_t& industry_manager_t::get_connection(uint32 id)
{
	return connections[id];
}
industry_connection_t& industry_manager_t::get_connection(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
{
	industry_connection_t ic(s,z,f);
	if (connections.is_contained(ic)) {
		return connections[connections.index_of(ic)];
	}
	else {
		connections.append(ic);
		return connections[connections.get_count()-1];
	}
}
void industry_manager_t::append_report(report_t *report)
{ 
	// TODO: do something more smarter here
	if(report) {
		if (report->gain_per_v_m > 0) {
			sp->get_log().message( "industry_manager_t::append_report()","got a nice report for immediate execution");
			if (report->action) {
				report->action->debug(sp->get_log(), cstring_t("industry_manager_t::append_report() .. "));
			}
			else {
				sp->get_log().warning( "industry_manager_t::append_report()","empty action");
			}
			append_child( report->action );
			report->action = NULL;
			delete report;
		}
		else {
			delete report;
		}
	}
}
return_value_t *industry_manager_t::work()
{
	if (connections.get_count()==0) return new_return_value(RT_DONE_NOTHING);

	if (next_cid == connections.get_count()) next_cid = 0;

	sp->get_log().message("industry_manager_t::work","process connection %d", next_cid);

	report_t* report = connections[next_cid].get_report(sp);

	if (report) {
		append_report(report);
	}

	next_cid ++;
	return new_return_value(RT_SUCCESS);
}

void industry_manager_t::rdwr(loadsave_t* file, const uint16 version)
{
	manager_t::rdwr(file, version);

	file->rdwr_long(next_cid, "");
	uint32 count;
	if (file->is_saving()) {
		count = connections.get_count();
	}
	file->rdwr_long(count,"");
	for(uint32 i=0; i<count; i++) {
		if (file->is_loading()) {
			connections.append(industry_connection_t());
		}
		connections[i].rdwr(file,version,sp);
	}
}


void industry_manager_t::rotate90( const sint16 y_size)
{
	manager_t::rotate90(y_size);
	for(uint32 i=0; i<connections.get_count(); i++)
		connections[i].rotate90(y_size);
}

void industry_manager_t::debug( log_t &file, cstring_t prefix )
{
	manager_t::debug(file,prefix);

	file.message("indm","%s connections: %d", (const char*)prefix, connections.get_count());
	for(uint32 i=0; i<connections.get_count(); i++)
	{
		char buf[40];
		sprintf(buf, "  connections[%d] ", i);

		connections[i].debug(file, prefix + buf);
	}
}
