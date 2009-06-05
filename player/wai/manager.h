#ifndef MANAGER_H
#define MANAGER_H

#include "bt.h"
#include "report.h"
#include "../../tpl/vector_tpl.h"

/*
meine Managervorschlaege:

-- industriemanager (fuer jede Fabrik)
-- linienmanager (fuer alle Linien)

-- bahnhofsmanager
-- powerlinemanager
*/
class manager_t : public bt_sequential_t {
public:
	manager_t( ai_wai_t *sp, const char* name ) : bt_sequential_t(sp, name) { type = BT_MANAGER; }
	// regular work will be done here
	virtual return_code step();
	virtual return_code work() {return RT_DONE_NOTHING;};

	virtual void rdwr(loadsave_t* file, const uint16 version);
	virtual void rotate90( const sint16 /*y_size*/ );

	// reports
	vector_tpl<report_t*> reports;
	virtual void append_report(report_t *report) { if (report) reports.append(report); }
	virtual report_t* get_report();
	
};

#endif
