#ifndef REPORT_H
#define REPORT_H

class bt_node_t;

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

	// revenue
	sint64 gain_per_v_m;

	// vehicles
	sint64 cost_per_vehicle;
	sint64 cost_montly_per_vehicle;
	uint16 nr_vehicles;

	// vehikel_besch_t *vehickelbesh;

	// destructor ? 
};

#endif