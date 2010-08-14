#include "bt.h"

#include "../ai_wai.h"

#include <string.h>
#include "../../dataobj/loadsave.h"

// erzeugt einen return-value-instanz mit dem richtigen node-typ
return_value_t* bt_node_t::new_return_value(return_code rc)
{
	return new return_value_t(rc, get_type());
}

return_value_t* bt_node_t::step()
{
	sp->get_log().message( name.c_str(), "dummy step, done nothing" );
	return new_return_value(RT_DONE_NOTHING);
}

void bt_node_t::debug( log_t &file, string prefix )
{
	file.message("node","%s%s", prefix.c_str(), name.c_str());
}

void bt_node_t::rdwr_child(loadsave_t* file, const uint16 version, ai_wai_t *sp_, bt_node_t* &child)
{
	if (file->is_saving()) {
		uint16 t = child ? (bt_types)child->get_type() : BT_NULL;
		file->rdwr_short(t);
	}
	else {
		uint16 t=0;
		file->rdwr_short(t);
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
		const char* t = name.c_str();
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
}

bt_sequential_t::~bt_sequential_t()
{
	for( uint32 i = 0; i < childs.get_count(); i++ ) {
		delete childs[i];
	}
}

return_value_t* bt_sequential_t::step()
{
	uint32 num_childs = childs.get_count();
	sp->get_log().message("bt_sequential_t::step","%s: next %d of %d nodes", name.c_str(), next_to_step, num_childs);

	if(  num_childs == 0  ) {
		// We have nothing to do... => Kill us.
		return new_return_value(RT_TOTAL_SUCCESS);
	}
	if( next_to_step >= num_childs) {
		next_to_step = 0;
		return new_return_value(RT_DONE_NOTHING);
	}
	uint32 i = next_to_step;
	// step the child
	return_value_t *childs_return = childs[i]->step();

	// get and append report of child
	if( childs_return->get_report() ) {
		append_report(childs_return->get_report());
		childs_return->set_report(NULL); // invalidate
	}
	// Do step counter things
	if( childs_return->can_be_deleted() ) {
		delete childs[i];
		childs.remove_at(i);
		num_childs = childs.get_count();
	}
	else {
	// Don't increase next_to_step, if child wants the next call or if it was deleted.
		if( childs_return->is_ready() ) {
			next_to_step = next_to_step+1;
		}
	}
	return_code our_return_code;
	if( next_to_step >= num_childs ) {
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
	if( childs_return->is_failed() ) {
		// We _always_ give back an error if our child does so. Can be caught by inherited functions.
		our_return_code = RT_ERROR;
	}
	/// TODO: was sollte hier gemacht werden? Wenn der return_value gekillt wird,
	// bekommt der Elternknoten davon auch nichts mit...

	/* delete childs_return;
	return new_return_value(our_return_code); */
	childs_return->code = our_return_code;
	return childs_return;
}

void bt_sequential_t::rdwr( loadsave_t* file, const uint16 version )
{
	bt_node_t::rdwr(file, version);

	file->rdwr_long(next_to_step);

	// 1. Schritt: Anzahl Kinder schreiben / lesen.
	uint32 count = childs.get_count();
	file->rdwr_long(count);
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

void bt_sequential_t::debug( log_t &file, string prefix )
{
	file.message("sequ","%s%s", prefix.c_str(), name.c_str());
	string next_prefix( prefix + "  " );
	for( uint32 i = 0; i < childs.get_count(); i++ ) {
		childs[i]->debug( file, next_prefix );
	}
}

void bt_sequential_t::append_child( bt_node_t *new_child )
{
	childs.append(new_child);
}
