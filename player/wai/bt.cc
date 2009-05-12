#include "bt.h"

bt_sequential_t::bt_sequential_t() {
	next_to_step = 0;
}

bt_sequential_t::~bt_sequential_t() {
	for( uint32 i = 0; i < childs.get_count(); i++ ) {
		delete childs[i];
	}
}

return_code bt_sequential_t::step() {
	uint32 num_childs = childs.get_count();

	if(  num_childs == 0  ) {
		// We have nothing to do...
		return RT_DONE_NOTHING;
	}

	for( uint32 i = next_to_step; i < num_childs; i++ ) {
		return_code childs_return = childs[i]->step();
		if( childs_return == RT_ERROR ) {
			// We give back an error. Can be caught by inherited functions.
			return RT_ERROR;
		}
		if( childs_return == RT_SUCCESS  ||  childs_return == RT_PARTIAL_SUCCESS ) {
			last_step = i;
			// Don't increase next_to_step, if child wants the next call.
			if( childs_return == RT_SUCCESS ) {
				next_to_step = next_to_step+1;
				if( next_to_step == num_childs ) {
					// Our last child.
					next_to_step = 0;
					return RT_SUCCESS;
				}
				else {
					// This wasn't our last child -> we want the next call, too.
					return RT_PARTIAL_SUCCESS;
				}
			}
		}
	}
	// No child has done something...
	return RT_DONE_NOTHING;
}

void bt_sequential_t::rdwr( uint16 ai_version )
{
	// 1. Schritt: Anzahl Kinder schreiben / lesen.
	// 2. Schritt: Kinder mit dem richtigen Konstruktor aufrufen (siehe simconvoi.cc, rdwr).
}

void bt_sequential_t::rotate90( sint16 size_y )
{
	for( uint32 i = 0; i < childs.get_size(); i++ ) {
		childs[i]->rotate90( size_y );
	}
}

