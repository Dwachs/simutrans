#include "manager.h"

#include "../../simtools.h"


return_code manager_t::step()
{
	if (bt_sequential_t::step() == RT_DONE_NOTHING) {
		return work();
	}
	else {
		return RT_PARTIAL_SUCCESS;
	}
}

report_t* manager_t::get_report() 
{
	if (reports.get_count()==0) {
		return NULL;
	}
	else {
		// TODO: do something more reasonable here
		return reports[simrand(reports.get_count())];
	}
}