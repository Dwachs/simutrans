#ifndef _PLANNER_H_
#define _PLANNER_H_

#include "manager.h"
#include "report.h"

class planner_t : public bt_node_t {
public:
	planner_t(ai_wai_t *sp_, const char* name_) : bt_node_t(sp_, name_), report(NULL) { type = BT_PLANNER; };

	// returns report and resets the pointer to NULL
	virtual report_t* get_report() { report_t *r = report; report = NULL; return r; }

	virtual void rotate90( const sint16 y_size) { if (report) report->rotate90(y_size); };
	virtual void rdwr( loadsave_t* file, const uint16 version);
protected:
	report_t *report;
};

#endif
