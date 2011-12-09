#ifndef _PLANNER_H_
#define _PLANNER_H_

#include "planner.h"
#include "report.h"

class planner_t : public bt_node_t {
public:
	planner_t(ai_wai_t *sp_, const char* name_) : bt_node_t(sp_, name_) , report(NULL) { type = BT_PLANNER; };

	// returns report and resets the pointer to NULL
	virtual return_value_t* new_return_value(return_code rc);

	virtual void rotate90( const sint16 y_size) { if (report) report->rotate90(y_size); };
	virtual void rdwr( loadsave_t* file, const uint16 version);
protected:
	report_t *report;
};

#endif