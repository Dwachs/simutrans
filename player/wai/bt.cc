#include "bt.h"

bt_sequential_t::bt_sequential_t() {
	next_to_step = 0;
}

bt_sequential_t::~bt_sequential_t() {
	for( uint32 i = 0; i < childs.get_count(); i++ ) {
		delete childs[i];
	}
}

return_code_t bt_sequential_t::step() {
	uint32 num_childs = childs.get_count();

	if(  num_childs == 0  ) {
		// We have nothing to do...
		return return_code_t(false, false, true);
	}

	return_code_t our_return(false, false, false);
	for( uint32 i = next_to_step; i < num_childs; i++ ) {
		return_code_t childs_return = childs[i]->step();
		if( childs_return.error ) {
			// We give back an error. Can be caught by inherited functions.
			our_return.error = true;
			return our_return;
		}
		if( childs_return.have_done_something ) {
			our_return.have_done_something = true;
			last_step = i;
			// Don't increase next_to_step, if child wants the next call.
			if( !childs_return.call_me_again ) {
				next_to_step = next_to_step+1;
				if( next_to_step == num_childs ) {
					// Our last child.
					next_to_step = 0;
					our_return.call_me_again = false;
				}
				else {
					// This wasn't our last child -> we want the next call, too.
					our_return.call_me_again = true;
				}
			}
			return our_return;
		}
	}
	// No child has done something...
	return our_return;
}
