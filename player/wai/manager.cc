#include "manager.h"

#include "../../simtools.h"


return_code manager_t::step()
{
	if (childs.get_count()>0) {
		return bt_sequential_t::step();
	}

	return work();
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