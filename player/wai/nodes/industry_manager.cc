#include "industry_manager.h"

#include "../../ai.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simline.h"
#include "../../../dataobj/loadsave.h"

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

	ai_t::rdwr_fabrik(file, sp->get_welt(), start);
	ai_t::rdwr_fabrik(file, sp->get_welt(), ziel);
	ai_t::rdwr_freight(file, freight);
}

void industry_connection_t::debug( log_t &file, cstring_t prefix ) 
{
	file.message("indc", "%s from %s(%s)\n", prefix, start->get_name(), start->get_pos().get_str() );
	file.message("indc", "%s to %s(%s)\n", prefix, ziel->get_name(), ziel->get_pos().get_str() );
	file.message("indc", "%s freight %s\n", prefix, freight->get_name() );

	if (line.is_bound()) {
		file.message("indc", "%s line(%d) %s\n", prefix, line->get_line_id(), line->get_name() );
	}
	file.message("indc", "%s status=%d\n", prefix, status);
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


template<connection_status cs> 
bool industry_manager_t::is_connection(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
{
	industry_connection_t ic(s,z,f);
	if (connections.is_contained(ic)) {
		return connections[connections.index_of(ic)].is<cs>();
	}
	else {
		return false;
	}
}

	
void industry_manager_t::rdwr(loadsave_t* file, const uint16 version)
{
	manager_t::rdwr(file, version);

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

	for(uint32 i=0; i<connections.get_count(); i++) 
	{
		char buf[40];
		sprintf(buf, "  connections[%d] ", i);

		connections[i].debug(file, prefix + buf);
	}
}