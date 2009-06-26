#ifndef BUILDER_ROAD_STATION_H
#define BUILDER_ROAD_STATION_H

/*
 * This class builds a road station.
 * @author Gerd Wachsmuth
 * @date 26.6.2009
 */

#include "../bt.h"
#include "../../../dataobj/koord.h"

class ware_besch_t;

class builder_road_station_t : public bt_node_t
{
public:
	builder_road_station_t( ai_wai_t *sp, const char *name, const koord &place );
	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual void rotate90( const sint16 y_size ) { place.rotate90(y_size); };
	virtual return_code step();
private:
	const ware_besch_t *ware;
	koord place;
};

#endif /* BUILDER_ROAD_STATION_H */
