#include "industry_connector.h"
#include "connections_manager.h"
#include "industry_manager.h"
#include "factory_searcher.h"
#include "../datablock.h"
#include "../../ai_wai.h"



industry_connector_t::industry_connector_t( ai_wai_t *sp, const char *name) :
bt_sequential_t(sp, name), start(0, sp), ziel(0, sp)
{
	connections = NULL;
	freight = NULL;
	type = BT_CON_IND;
	alternative = NULL;
}

industry_connector_t::industry_connector_t( ai_wai_t *sp, const char *name, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) :
bt_sequential_t(sp, name), start(s, sp), ziel(z, sp)
{
	connections = NULL;
	freight = f;
	type = BT_CON_IND;
	alternative = NULL;
}

industry_connector_t::~industry_connector_t()
{
	if (connections) {
		delete connections;
		connections = NULL;
	}
	if (alternative) {
		delete alternative;
		alternative = NULL;
	}
}

void industry_connector_t::append_report(report_t *report)
{
	if (report) {
		if (alternative) {
			sp->get_log().warning("industry_connector_t::append_report", "delete alternative report");
			delete alternative;
		}
		alternative = report;
	}
}

return_value_t *industry_connector_t::step()
{
	if(!start.is_bound()  ||  !ziel.is_bound()) {
		sp->get_log().warning("industry_connector_t::step", "%s %s disappeared", start.is_bound() ? "" : "start", ziel.is_bound() ? "" : "ziel");
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	return_value_t *rv = bt_sequential_t::step();
	if (rv->is_failed()) {
		// tell the industry manager
		industry_link_t *ic = sp->get_industry_manager()->get_connection(*start, *ziel, freight);

		if (alternative) {
			sp->get_factory_searcher()->append_report(alternative);
			alternative = NULL;
		}
		else {
			ic->set(forbidden);
			ic->unset(planned);
		}
		// kill me
		rv->code = RT_TOTAL_FAIL;
		// remove already established connections: will be done if report is executed
		if (connections) {
			report_t *final = connections->get_final_report(sp);
			if (final) {
				sp->get_industry_manager()->append_report(final);
			}
		}
	}
	else if (rv->code & (RT_SUCCESS | RT_PARTIAL_SUCCESS)) {
		if (rv->data) {
			if (rv->data->line.is_bound()) {
				connection_t *c = new freight_connection_t(*ziel, freight, sp);
				c->set_line(rv->data->line);
				rv->data->line = linehandle_t();
				if (connections == NULL) {
					connections = c;
				}
				else if (connections->get_type() != CONN_SERIAL) {
					serial_connection_t *s = new serial_connection_t();
					s->append_connection(connections);
					s->append_connection(c);
					connections = s;
				}
				else {
					(dynamic_cast<serial_connection_t*>(connections))->append_connection(c);
				}
			}
		}
	}
	if (rv->code == RT_TOTAL_SUCCESS) {
		// tell the industry manager
		industry_link_t *ic = sp->get_industry_manager()->get_connection(*start, *ziel, freight);
		ic->unset(planned);
		if (connections) {
			ic->set(own);
			ic->append_connection(connections);
			connections = NULL;
		}
		else {
			sp->get_log().warning( "industry_connector_t::step", "no lines created for connection %d", sp->get_industry_manager()->get_connection_id(*start, *ziel, freight));
		}
		// kill me
		rv->code = RT_TOTAL_SUCCESS;
	}
	return rv;
}

void industry_connector_t::rdwr( loadsave_t *file, const uint16 version )
{
	start.rdwr(file, version, sp);
	ziel.rdwr(file, version, sp);
	ai_t::rdwr_ware_besch(file, freight);
	connection_t::rdwr_connection(file, version, sp, connections);
	bool have_alternative = alternative != NULL;
	file->rdwr_bool(have_alternative);
	if (have_alternative) {
		if (file->is_loading()) {
			alternative = new report_t();
		}
		alternative->rdwr(file, version, sp);
	}
}

void industry_connector_t::rotate90( const sint16 y_size)
{
	if (connections) {
		connections->rotate90(y_size);
	}
	if (alternative) {
		alternative->rotate90(y_size);
	}
}

void industry_connector_t::debug( log_t &file, string prefix )
{
	bt_sequential_t::debug(file, prefix);
}