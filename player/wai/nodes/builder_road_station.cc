#include "builder_road_station.h"

#include "../../../bauer/hausbauer.h"
#include "../../../besch/haus_besch.h"
#include "../../../simhalt.h"
#include "../../ai.h"
#include "../../ai_wai.h"

builder_road_station_t::builder_road_station_t( ai_wai_t *sp, const char *name, const koord3d place_ ) :
	bt_node_t( sp, name ),
	place( place_ )
{
	type = BT_ROAD_STATION;
}

void builder_road_station_t::rdwr( loadsave_t *file, const uint16 version )
{
	place.rdwr( file );
}

return_code builder_road_station_t::step()
{
// TODO: Debug
	const haus_besch_t* fh = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE);
	// FIXME: Only work on kartenboden (since get_2d)
	if( fh  &&  sp->call_general_tool(WKZ_STATION, place.get_2d(), fh->get_name())  )
	{
		return RT_SUCCESS;
	}
	else
	{
		return RT_ERROR;
	}
}
