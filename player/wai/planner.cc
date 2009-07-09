#include "planner.h"
#include "../../dataobj/loadsave.h"

return_value_t* planner_t::new_return_value(return_code rc)
{
	return_value_t* rv = bt_node_t::new_return_value(rc);
	rv->set_report(report);
	report = NULL;
	return rv;
}

void planner_t::rdwr( loadsave_t* file, const uint16 version)
{
	bt_node_t::rdwr(file, version);

	bool rdwr_report;
	if (file->is_saving()) {
		rdwr_report = report != NULL;
	}
	file->rdwr_bool(rdwr_report, "");
	if (rdwr_report) {
		if (file->is_loading()) {
			report = new report_t();
		}
		report->rdwr(file, version, sp);
	}
}