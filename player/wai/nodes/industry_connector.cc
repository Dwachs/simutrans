#include "industry_connector.h"
#include "connections_manager.h"
#include "industry_manager.h"
#include "../datablock.h"
#include "../../ai_wai.h"



industry_connector_t::industry_connector_t( ai_wai_t *sp, const char *name) : 
bt_sequential_t(sp, name)
{
	connections = NULL;
	start = ziel = NULL;
	freight = NULL;
	type = BT_CON_IND;
}

industry_connector_t::industry_connector_t( ai_wai_t *sp, const char *name, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) : 
bt_sequential_t(sp, name)
{
	connections = NULL;
	start = s;
	ziel = z;
	freight = f;
	type = BT_CON_IND;
}

industry_connector_t::~industry_connector_t()
{
	if (connections) {
		delete connections; 
		connections = NULL;
	}
}

return_value_t *industry_connector_t::step()
{
	return_value_t *rv = bt_sequential_t::step();
	if (rv->is_failed()) {
		// tell the industry manager
		industry_link_t *ic = sp->get_industry_manager()->get_connection(start, ziel, freight);
		ic->unset(planned);
		ic->set(forbidden);
		// kill me
		rv->code = RT_TOTAL_FAIL;
	}
	else if (rv->code & (RT_SUCCESS | RT_PARTIAL_SUCCESS)) {
		if (rv->data) {
			if (rv->data->line.is_bound()) {
				connection_t *c = new freight_connection_t(ziel, freight);
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
	if (rv->code & (RT_SUCCESS | RT_TOTAL_SUCCESS)) {
		// tell the industry manager
		industry_link_t *ic = sp->get_industry_manager()->get_connection(start, ziel, freight);
		ic->unset(planned);
		if (connections) {
			ic->set(own);
			ic->append_connection(connections);
			connections = NULL;
		}
		else {
			sp->get_log().warning( "industry_connector_t::step", "no lines created for connection %d", sp->get_industry_manager()->get_connection_id(start, ziel, freight));
		}
		// kill me
		rv->code = RT_TOTAL_SUCCESS;
	}
	return rv;
}

void industry_connector_t::rdwr( loadsave_t *file, const uint16 version )
{
	ai_t::rdwr_fabrik(file, sp->get_welt(), start);
	ai_t::rdwr_fabrik(file, sp->get_welt(), ziel);
	ai_t::rdwr_ware_besch(file, freight);
	connection_t::rdwr_connection(file, version, sp, connections);
}

void industry_connector_t::rotate90( const sint16 y_size) 
{
	if (connections) {
		connections->rotate90(y_size);
	}
}

void industry_connector_t::debug( log_t &file, cstring_t prefix )
{
	bt_sequential_t::debug(file, prefix);
}