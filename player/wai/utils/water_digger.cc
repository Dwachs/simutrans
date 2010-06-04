#include "water_digger.h"
#include "../../ai_wai.h"

bool water_digger_t::is_allowed_step( const grund_t *from, const grund_t *to, long *costs )
{
	assert( (bautyp&bautyp_mask) == wasser );

	const sint8 sea_level = welt->get_grundwasser();

	if(!from->ist_wasser()  &&  !welt->can_ebne_planquadrat(from->get_pos().get_2d(), sea_level)) {
		return false;
	}
	if(from!=to  &&  !to->ist_wasser()  &&  !welt->can_ebne_planquadrat(to->get_pos().get_2d(), sea_level)) {
		return false;
	}

	if(to->ist_wasser()) {
		*costs = 1;
	}
	else {
		const sint8 hgt_diff = to->get_hoehe() - sea_level;

		*costs = hgt_diff*(hgt_diff + 1);
	}
	return true;
}

bool water_digger_t::terraform()
{
	const sint8 sea_level = welt->get_grundwasser();
	ai_wai_t *ai = dynamic_cast<ai_wai_t*>(sp);

	if (route.get_count()>1) {
		for(uint32 i=1; i<route.get_count()-1; i++) {
			bool ok = welt->ebne_planquadrat(sp, route[i].get_2d(), sea_level);
			if (!ok  &&  ai) {
				ai->get_log().warning("water_digger_t::terraform()", "terraforming failed at (%s)", route[i].get_str());
				return false;
			}
			ai->get_log().message("water_digger_t::terraform()", "terraformed at (%s)", route[i].get_str());
		}
		// TODO: remove later
		for(uint32 i=0; i<route.get_count(); i++) {
			grund_t *gr = welt->lookup_kartenboden(route[i].get_2d());
			if (gr  &&  gr->ist_wasser()) {
				gr->calc_bild();  // to get ribis right
			}
		}
		return true;
	}
	return false;
}
