#include "industry_manager.h"

#include "vehikel_builder.h"
#include "../../ai.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simline.h"
#include "../../../dataobj/loadsave.h"
#include "../../../vehicle/simvehikel.h"

industry_link_t::industry_link_t(ai_wai_t *sp, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
: status(0), start(s,sp), ziel(z,sp), freight(f)
{
	connections = new parallel_connection_t();
}

industry_link_t::~industry_link_t()
{
	if (connections) {
		delete connections;
		connections = NULL;
	}
}

report_t* industry_link_t::get_report(ai_wai_t *sp)
{
	if (!is(own)) {
		return NULL;
	}
	if (!start.is_bound()  ||  !ziel.is_bound()) {
		set(broken);
		return NULL;
	}
	return connections->get_report(sp);
}

report_t* industry_link_t::get_final_report(ai_wai_t *sp)
{
	if (!is(own)) {
		return NULL;
	}
	return connections->get_final_report(sp);
}

void industry_link_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp)
{
	file->rdwr_longlong(status, "");
	connections->rdwr(file, version, sp);
	start.rdwr(file, version, sp);
	ziel.rdwr(file, version, sp);
	ai_t::rdwr_ware_besch(file, freight);
}

void industry_link_t::debug( log_t &file, cstring_t prefix )
{
	if (start.is_bound()) file.message("indc", "%s from    %s(%s)", (const char*)prefix, start->get_name(), start->get_pos().get_str() );
	else                  file.message("indc", "%s from    %s(%s)", (const char*)prefix, "NULL", koord3d::invalid.get_str() );
	if (ziel.is_bound())  file.message("indc", "%s to      %s(%s)", (const char*)prefix, ziel->get_name(), ziel->get_pos().get_str() );
	else                  file.message("indc", "%s from    %s(%s)", (const char*)prefix, "NULL", koord3d::invalid.get_str() );
	file.message("indc", "%s freight %s", (const char*)prefix, freight->get_name() );
	connections->debug(file, prefix + "   ");
	file.message("indc", "%s status=%d", (const char*)prefix, status);
}

uint32 industry_manager_t::get_index(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) const
{
	uint32 ind = connections.get_count();
	if (s!=NULL && z!=NULL && f!=NULL) {
		for(uint32 i=0; i<connections.get_count(); i++) {
			if (connections[i]->is_equal(s,z,f)) {
				ind = i;
				break;
			}
		}
	}
	return ind;
}
uint32 industry_manager_t::get_index(industry_link_t *il) const
{
	uint32 ind = connections.get_count();
	if (il) {
		for(uint32 i=0; i<connections.get_count(); i++) {
			if ( (*il) == (*(connections[i]))) {
				ind = i;
				break;
			}
		}
	}
	return ind;
}

uint32 industry_manager_t::get_connection_id(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
{
	uint32 i = get_index(s,z,f);
	if (i<connections.get_count()) {
		return i;
	}
	else {
		connections.append(new industry_link_t(sp,s,z,f));
		return connections.get_count()-1;
	}
}

industry_link_t* industry_manager_t::get_connection(uint32 id)
{
	return connections[id];
}
industry_link_t* industry_manager_t::get_connection(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
{
	uint32 i = get_index(s,z,f);
	if (i<connections.get_count()) {
		return connections[i];
	}
	else {
		industry_link_t *ic = new industry_link_t(sp,s,z,f);
		connections.append(ic);
		return ic;
	}
}


/* returns false if the connection does not exist or if the corresponding bits are not set */
bool industry_manager_t::is_connection(connection_status cs, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) const
{
	uint32 i = get_index(s,z,f);
	if (i<connections.get_count()) {
		return connections[i]->is(cs);
	}
	else {
		return false;
	}
}

/* sets the status bit, creates connection if none exists */
void industry_manager_t::set_connection(connection_status cs, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
{
	industry_link_t* ic = get_connection(s,z,f);
	ic->set(cs);
}
/* unsets the status bit, creates no new connection */
void industry_manager_t::unset_connection(connection_status cs, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
{
	uint32 i = get_index(s,z,f);
	if (i<connections.get_count()) {
		connections[i]->unset(cs);
	}
}

void industry_manager_t::append_report(report_t *report)
{
	// TODO: do something more smarter here
	if(report) {
		if (report->gain_per_m > 0  ||  report->gain_per_v_m > 0) {
			if (sp->is_cash_available(report->cost_fix)) {
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
				// will be delivered by get_report()
				manager_t::append_report(report);
			}
		}
		else {
			sp->get_log().message( "industry_manager_t::append_report()","got a bad report, put it in trash bin");
			if (report->action) {
				report->action->debug(sp->get_log(), cstring_t("industry_manager_t::append_report() .. "));
			}
			delete report;
		}
	}
}
return_value_t *industry_manager_t::work()
{
	if (connections.get_count()==0) return new_return_value(RT_DONE_NOTHING);

	if (next_cid == connections.get_count()) next_cid = 0;

	sp->get_log().message("industry_manager_t::work","process connection %d", next_cid);

	report_t* report = connections[next_cid]->get_report(sp);

	if (report) {
		append_report(report);
	}

	if (connections[next_cid]->is(broken)) {
		report = connections[next_cid]->get_final_report(sp);
		if (report) {
			append_report(report);
		}
		delete connections[next_cid];
		connections.remove_at(next_cid);
	}
	else {
		next_cid ++;
	}
	return new_return_value(RT_SUCCESS);
}

void industry_manager_t::rdwr(loadsave_t* file, const uint16 version)
{
	manager_t::rdwr(file, version);

	file->rdwr_long(next_cid, "");

	uint32 count = connections.get_count();
	file->rdwr_long(count,"");
	for(uint32 i=0; i<count; i++) {
		if (file->is_loading()) {
			connections.append(new industry_link_t(sp,NULL,NULL,NULL));
		}
		connections[i]->rdwr(file,version,sp);
	}
}


void industry_manager_t::rotate90( const sint16 y_size)
{
	manager_t::rotate90(y_size);
	for(uint32 i=0; i<connections.get_count(); i++)
		connections[i]->rotate90(y_size);
}

void industry_manager_t::debug( log_t &file, cstring_t prefix )
{
	manager_t::debug(file,prefix);

	file.message("indm","%s connections: %d", (const char*)prefix, connections.get_count());
	for(uint32 i=0; i<connections.get_count(); i++)
	{
		char buf[40];
		sprintf(buf, "  connections[%d] ", i);

		connections[i]->debug(file, prefix + buf);
	}
}
