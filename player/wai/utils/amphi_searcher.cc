#include "amphi_searcher.h"
#include "../../../boden/grund.h"
#include "../../../simworld.h"

bool amphi_searcher_t::is_allowed_step( const grund_t *from, const grund_t *to, long *costs )
{
	assert( (bautyp&bautyp_mask) == strasse );

	const koord from_pos=from->get_pos().get_2d();
	const koord to_pos=to->get_pos().get_2d();
	const koord zv=to_pos-from_pos;

	// Switching only one times from water to ground:
	// (enforces also start on water!)
	if( to->ist_wasser()  &&  !from->ist_wasser() )
	{
		return false;
	}

	if(  !to->ist_wasser()  &&  from->ist_wasser()  ) {
		if( to->hat_wege() ) {
			// We want to build a harbour here.
			return false;
		}
		const grund_t *gr = welt->lookup_kartenboden(to_pos+zv);
		if(  !gr  ||  gr->get_grund_hang() != hang_t::flach  ||  gr->hat_wege()  ) {
			// We want to build a road station here.
			return false;
		}
	}

	// Fake the bautyp:
	bautyp_t old_bautyp = bautyp;
	bautyp = ( to->ist_wasser() ) ? wasser : (bautyp_t)(bautyp&bautyp_mask);
	bool ok = wegbauer_t::is_allowed_step( from, to, costs );
	bautyp = old_bautyp;
	// Better for A* if heuristic fits to real distance.
	*costs = welt->get_einstellungen()->way_count_straight + (to->get_weg_hang()!=0) ? welt->get_einstellungen()->way_count_slope : 0;
	return ok;
}
