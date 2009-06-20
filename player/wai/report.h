#ifndef REPORT_H
#define REPORT_H

#include "../../simtypes.h"

class bt_node_t;
class ai_wai_t;

class loadsave_t;
class log_t;
class cstring_t;
/*
 * A report consists of a list of actions which must be executed if this 
 * reports is selected for execution. 
 * It contains estimations of all costs and revenues.
 *
 * @author Dwachs
 * @inspired by NoCAP AI for OpenTTD by Morloth
 */

class report_t {
public:
	// tree that will be executed
	bt_node_t *action;

	// costs
	sint64 cost_fix;
	sint64 cost_monthly;

	// revenue per vehicle and month
	sint64 gain_per_v_m;

	// vehicles
	sint64 cost_per_vehicle;
	sint64 cost_monthly_per_vehicle;
	uint16 nr_vehicles;

	report_t() : action(NULL) {}

	~report_t() {
		if (action) delete action;
		action = NULL;
	}
	
	virtual void rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp_);
	virtual void rotate90( const sint16 y_size );
	virtual void debug( log_t &file, cstring_t prefix );
};

#endif
