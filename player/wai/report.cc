#include "report.h"
#include "bt.h"

#include "../../dataobj/loadsave.h"
#include "../../utils/cstring_t.h"
#include "../../utils/log.h"

void report_t::merge_report(report_t* r)
{
	cost_fix     += r->cost_fix;
	cost_monthly += r->cost_monthly;
	gain_per_m   += r->gain_per_m;
	if (nr_vehicles + r->nr_vehicles > 0) {
		gain_per_v_m = (gain_per_v_m * nr_vehicles + r->gain_per_v_m * r->nr_vehicles) / (nr_vehicles + r->nr_vehicles);
	}
	nr_vehicles  += r->nr_vehicles;
	// merge actions, create bt_sequential root node if needed
	if (r->action) {
		if (action) {
			bt_node_t *root_action = action;
			if (action->get_type()!=BT_SEQUENTIAL) {
				root_action = new bt_sequential_t(action->get_sp(), "merged report action");
				((bt_sequential_t*)root_action)->append_child(action);
			}
			((bt_sequential_t*)root_action)->append_child(r->action);
			action = root_action;
		}
		else {
			action = r->action;
		}
		r->action = NULL;
	}
}

void report_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp_)
{
	file->rdwr_longlong(cost_fix, "");
	file->rdwr_longlong(cost_monthly, "");
	file->rdwr_longlong(gain_per_v_m, "");
	file->rdwr_longlong(gain_per_m, "");
	file->rdwr_short(nr_vehicles, "");

	bt_node_t::rdwr_child(file, version, sp_, action);
}

void report_t::rotate90( const sint16 y_size )
{
	if (action) {
		action->rotate90(y_size);
	}
}

void report_t::debug( log_t &file, cstring_t prefix )
{
	file.message("rprt","%s cost %d", (const char*)prefix, cost_fix);
	// TODO: complete this
	cstring_t next_prefix( prefix + "  " );
	if (action) {
		action->debug(file, next_prefix );
	}
}
