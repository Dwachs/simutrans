#include "bt.h"

#include "manager.h"
#include "planner.h"
#include "nodes/factory_searcher.h"
#include "nodes/industry_connection_planner.h"
#include "nodes/industry_manager.h"
#include "../ai_wai.h"

#include <string.h>
#include "../../dataobj/loadsave.h"

return_code bt_node_t::step()
{
	sp->get_log().message( name, "dummy step, done nothing" );
	return RT_DONE_NOTHING;
}

void bt_node_t::debug( log_t &file, cstring_t prefix )
{
	file.message("node","%s%s", (const char*)prefix, (const char*)name);
}

bt_node_t* bt_node_t::alloc_bt_node(uint16 type, ai_wai_t *sp_)
{
	switch(type) {
		case BT_NULL:			return NULL;
		case BT_NODE:			return new bt_node_t(sp_);
		case BT_SEQUENTIAL:		return new bt_sequential_t(sp_, NULL);
		case BT_PLANNER:		return new planner_t(sp_, NULL);
		case BT_IND_CONN_PLN:	return new industry_connection_planner_t(sp_, NULL);
		case BT_MANAGER:		return new manager_t(sp_, NULL);
		case BT_FACT_SRCH:		return new factory_searcher_t(sp_, NULL);
		case BT_IND_MNGR:		{ industry_manager_t *im = new industry_manager_t(sp_, NULL); sp_->set_industry_manager(im); return im; }

		default:
			assert(0);
			return NULL;
	}
}

void bt_node_t::rdwr_child(loadsave_t* file, const uint16 version, ai_wai_t *sp_, bt_node_t* &child)
{
	if (file->is_saving()) {
		uint16 t = child ? (bt_types)child->get_type() : BT_NULL;
		file->rdwr_short(t, " ");
	}
	else {
		uint16 t=0;
		file->rdwr_short(t, " ");
		if( child ) {
			delete child;
		}
		child = alloc_bt_node(t, sp_);
	}
	if (child) {
		child->rdwr(file, version);
	}
}

void bt_node_t::rdwr(loadsave_t* file, const uint16 /*version*/)
{
	if (file->is_saving()) {
		const char* t = name;
		file->rdwr_str(t);
	}
	else {
		const char *t = NULL;
		file->rdwr_str(t);
		name = t;
		delete [] t;
	}
}

bt_sequential_t::bt_sequential_t( ai_wai_t *sp_, const char* name_ ) :
	bt_node_t( sp_, name_)
{
	type = BT_SEQUENTIAL;

	next_to_step = 0;
	last_step = 0;
}

bt_sequential_t::~bt_sequential_t()
{
	for( uint32 i = 0; i < childs.get_count(); i++ ) {
		delete childs[i];
	}
}

return_code bt_sequential_t::step()
{
	uint32 num_childs = childs.get_count();

	if(  num_childs == 0  ) {
		// We have nothing to do...
		return RT_DONE_NOTHING;
	}

	for( uint32 i = next_to_step; i < num_childs; i++ ) {
		// step the child
		return_code childs_return = childs[i]->step();

		// successfull: get and append report of child
		if( childs_return == RT_SUCCESS  ||  childs_return == RT_TOTAL_SUCCESS ) {
			/* report_t* report = childs[i]->get_report();
			if (report) {
				append_report(report);
			} */
		}
		// Do step counter things
		// Don't increase next_to_step, if child wants the next call.
		if( childs_return == RT_DONE_NOTHING || childs_return == RT_SUCCESS || childs_return == RT_ERROR ) {
			next_to_step = next_to_step+1;
		}
		if( childs_return == RT_TOTAL_SUCCESS ) {
			delete childs[i];
			childs.remove_at(i);
			num_childs = childs.get_count();
		}
		return_code our_return_code;
		if( next_to_step == num_childs ) {
			// Our last child.
			// why reset it?
			// If we are called the next time, we start again with first child.
			next_to_step = 0;
			our_return_code = RT_SUCCESS;
		}
		else {
			// This wasn't our last child -> we want the next call, too.
			our_return_code = RT_PARTIAL_SUCCESS;
		}
		if( childs_return == RT_ERROR ) {
			// We _always_ give back an error if our child does so. Can be caught by inherited functions.
			our_return_code = RT_ERROR;
		}
		return our_return_code;
	}
	// No child has done something...
	return RT_DONE_NOTHING;
}

void bt_sequential_t::rdwr( loadsave_t* file, const uint16 version )
{
	bt_node_t::rdwr(file, version);

	file->rdwr_long(next_to_step, " ");
	file->rdwr_long(last_step, " ");

	// 1. Schritt: Anzahl Kinder schreiben / lesen.
	uint32 count = childs.get_count();
	file->rdwr_long(count, " ");
	// 2. Schritt: Kinder mit dem richtigen Konstruktor aufrufen (siehe simconvoi.cc, rdwr).
	for(uint32 i=0; i<count; i++) {
		if(file->is_loading()) {
			childs.append(NULL);
		}
		rdwr_child(file, version, sp, childs[i]);
	}
}

void bt_sequential_t::rotate90( sint16 size_y )
{
	for( uint32 i = 0; i < childs.get_count(); i++ ) {
		childs[i]->rotate90( size_y );
	}
}

void bt_sequential_t::debug( log_t &file, cstring_t prefix )
{
	file.message("sequ","%s%s", (const char*)prefix, (const char*)name);
	cstring_t next_prefix( prefix + "  " );
	for( uint32 i = 0; i < childs.get_count(); i++ ) {
		childs[i]->debug( file, next_prefix );
	}
}

void bt_sequential_t::append_child( bt_node_t *new_child )
{
	childs.append(new_child);
}
