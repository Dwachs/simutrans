#include "manager.h"

#include "../../simtools.h"
#include "../../dataobj/loadsave.h"
#include "planner.h"

#include "../ai_wai.h"

return_value_t *manager_t::step()
{
	return_value_t *rc = bt_sequential_t::step();
	//sp->get_log().message("manager_t::step","%s: node %d returns %d", (const char*)name, rc->type, rc->code);
	if ( rc->code == RT_DONE_NOTHING || rc->code == RT_TOTAL_SUCCESS ) {
		delete rc;
		return work();
	}
	else {
		return rc;
	}
}

report_t* manager_t::get_report()
{
	if (reports.get_count()==0) {
		return NULL;
	}
	else {
		// find the report that maximizes
		// gain-per-month / cost-fix
		uint32 index = 0;
		report_t *best = reports[index];
		for(uint32 i=1; i<reports.get_count(); i++) {
			report_t *r = reports[i];
			if ( (best->gain_per_m * r->cost_fix < r->gain_per_m * best->cost_fix)
				|| (best->cost_fix==0  &&  r->cost_fix==0  &&  best->gain_per_m<r->gain_per_m) ) {
					best = r;
					index = i;
			}
		}
		reports.remove_at( index );
		// TODO: remove old reports
		return best;
	}
}

void manager_t::rdwr(loadsave_t* file, const uint16 version)
{
	bt_sequential_t::rdwr(file, version);

	uint32 count = reports.get_count();
	file->rdwr_long(count, " ");
	for(uint32 i=0; i<count; i++) {
		if(file->is_loading()) {
			reports.append(new report_t());
		}
		reports[i]->rdwr(file,version,sp);
	}

}
void manager_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);

	for(uint32 i=0; i<reports.get_count(); i++)
	{
		reports[i]->rotate90(y_size);
	}
}

void manager_t::debug( log_t &file, cstring_t prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("mana","%s reports received: %d", (const char*)prefix, reports.get_count());
	cstring_t next_prefix( prefix + "  " );
	for(uint32 i=0; i<reports.get_count(); i++)
	{
		char buf[40];
		sprintf(buf, "  report[%d] ", i);
		reports[i]->debug(file, prefix+buf);
	}
}
