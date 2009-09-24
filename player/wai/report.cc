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
	nr_ships     += r->nr_vehicles;
	gain_per_v_m += (r->gain_per_v_m * nr_ships) / nr_vehicles;
}

void report_t::rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp_)
{
	file->rdwr_longlong(cost_fix, "");
	file->rdwr_longlong(cost_monthly, "");
	file->rdwr_longlong(gain_per_v_m, "");
	file->rdwr_longlong(gain_per_m, "");
	file->rdwr_short(nr_vehicles, "");
	file->rdwr_short(nr_ships, "");

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
