#include "alloc_node.h"
#include "bt.h"
#include "../ai_wai.h"
#include "manager.h"
#include "planner.h"
#include "nodes/builder_road_station.h"
#include "nodes/builder_way_obj.h"
#include "nodes/connector_road.h"
#include "nodes/connector_ship.h"
#include "nodes/factory_searcher.h"
#include "nodes/free_tile_searcher.h"
#include "nodes/industry_connection_planner.h"
#include "nodes/industry_manager.h"
#include "nodes/vehikel_builder.h"

bt_node_t* alloc_bt_node(uint16 type, ai_wai_t *sp)
{
	switch(type) {
		case BT_NULL:			return NULL;
		case BT_NODE:			return new bt_node_t(sp);
		case BT_SEQUENTIAL:		return new bt_sequential_t(sp, NULL);

		case BT_PLANNER:		return new planner_t(sp, NULL);
		case BT_IND_CONN_PLN:	return new industry_connection_planner_t(sp, NULL);

		case BT_MANAGER:		return new manager_t(sp, NULL);
		case BT_FACT_SRCH:		{ factory_searcher_t *fs = new factory_searcher_t(sp, NULL); sp->set_factory_searcher(fs); return fs; }
		case BT_IND_MNGR:		{ industry_manager_t *im = new industry_manager_t(sp, NULL); sp->set_industry_manager(im); return im; }

		case BT_CON_ROAD:		return new connector_road_t(sp, NULL);
		case BT_ROAD_STATION:	return new builder_road_station_t(sp, NULL, koord3d::invalid);
		case BT_WAYOBJ:			return new builder_wayobj_t(sp, NULL, koord3d::invalid, koord3d::invalid, NULL);
		case BT_CON_SHIP:		return new connector_ship_t(sp, NULL);
		case BT_VEH_BUILDER:	return new vehikel_builder_t(sp, NULL);
		case BT_FREE_TILE:		return new free_tile_searcher_t( sp, NULL );
		default:
			assert(0);
			return NULL;
	}
}
