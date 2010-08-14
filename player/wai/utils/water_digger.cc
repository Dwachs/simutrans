#include "water_digger.h"
#include "../../ai_wai.h"

bool water_digger_t::is_allowed_step( const grund_t *from, const grund_t *to, long *costs )
{
	assert( (bautyp&bautyp_mask) == wasser );

	const sint8 sea_level = welt->get_grundwasser();

	int cost = 0;
	if(!from->ist_wasser()  &&  !welt->can_ebne_planquadrat(from->get_pos().get_2d(), sea_level, cost)) {
		return false;
	}
	cost = 0;
	if(from!=to  &&  !to->ist_wasser()  &&  !welt->can_ebne_planquadrat(to->get_pos().get_2d(), sea_level, cost)) {
		return false;
	}

	if(to->ist_wasser()) {
		*costs = 1;
	}
	else {
		*costs = 1 + cost;
	}
	return true;
}


sint64 water_digger_t::calc_costs()
{
	sint64 cost = 0;
	const sint8 sea_level = welt->get_grundwasser();
	int estimate = 0;

	if (route.get_count()>1) {
		for(uint32 i=1; i<route.get_count()-1; i++) {
			bool ok = welt->can_ebne_planquadrat(route[i].get_2d(), sea_level, estimate);
		}
	}
	return  -(estimate*welt->get_einstellungen()->cst_alter_land);
}			


bool water_digger_t::terraform()
{
	const sint8 sea_level = welt->get_grundwasser();
	ai_wai_t *ai = dynamic_cast<ai_wai_t*>(sp);

	if (route.get_count()>1) {
		for(uint32 i=1; i<route.get_count()-1; i++) {
			int estimate = 0;
			bool ok = welt->can_ebne_planquadrat(route[i].get_2d(), sea_level, estimate);
			sint64 money_before = sp->get_finance_history_month(0, COST_CASH);

			ok  = welt->ebne_planquadrat(sp, route[i].get_2d(), sea_level);

			sint64 money_after  = sp->get_finance_history_month(0, COST_CASH);
			int paid = (money_before - money_after) / 100;
			int estimated_cost = -(estimate*welt->get_einstellungen()->cst_alter_land) / 100;
			
			if (ai) {
				if (ok  &&  (paid>0  ||  estimated_cost>0)) {
					ai->get_log().message("water_digger_t::terraform()", "terraformed at (%s) estimated %d paid %d", route[i].get_str(), estimated_cost, paid);
				}
				if (!ok) {
					ai->get_log().warning("water_digger_t::terraform()", "terraforming failed at (%s)", route[i].get_str());
				}
			}
			if (!ok) {
				return false;
			}
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
