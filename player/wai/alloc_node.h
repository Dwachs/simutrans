#ifndef ALLOC_NODE_H_
#define ALLOC_NODE_H_

// allocates the right nodes during loading of games
// all nodes must encoded here!


#include "../../simtypes.h"
/*
 * all types of nodes
 */
enum bt_types {
	BT_NULL          = 0,
	BT_NODE          = 1,
	BT_SEQUENTIAL    = 2,
	// Planner:
	BT_PLANNER       = 100,
	BT_IND_CONN_PLN  = 101,
	// Manager:
	BT_MANAGER       = 200,
	BT_FACT_SRCH     = 201,
	BT_IND_MNGR      = 202,
	// Builder:
	BT_CON_ROAD      = 301,
	BT_ROAD_STATION  = 302,
	BT_WAYOBJ        = 303,
	BT_CON_SHIP      = 304,
	BT_VEH_BUILDER   = 305,
	BT_FREE_TILE     = 306,
	BT_CON_IND       = 307
};

class bt_node_t;
class ai_wai_t;

/*
 * Returns a new instance of a node from the right class
 *
 */
bt_node_t *alloc_bt_node(uint16 type, ai_wai_t *sp);

#endif
