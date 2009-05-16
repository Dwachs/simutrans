#ifndef MANAGER_H
#define MANAGER_H

#include "bt.h"

/*
meine Managervorschlaege:

-- industriemanager (fuer jede Fabrik)
-- linienmanager (fuer alle Linien)

-- bahnhofsmanager
-- powerlinemanager
*/
class manager_t : public bt_sequential_t {

	// if no childs are present
	// regular work will be done here
	virtual return_code step();
	virtual return_code work() {return RT_DONE_NOTHING;};

	// soll der Manager auch Reports erstellen?
	// vector_tpl<report_t*> get_reports(uint8 max_count);
};

#endif