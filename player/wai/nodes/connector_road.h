#ifndef CONNECTOR_ROAD_H
#define CONNECTOR_ROAD_H

/*
 * This node builds a road connection.
 * @author gerw
 * @date 20.06.2009
 */

#include "../bt.h"
#include "../utils/wrapper.h"
#include "../../../dataobj/koord3d.h"

class fabrik_t;
class weg_besch_t;
class simple_prototype_designer_t;
class way_obj_besch_t;

class connector_road_t : public bt_sequential_t
{
public:
	connector_road_t( ai_wai_t *sp, const char *name);
	/**
	 * the constructor
	 * @param fab1, fab2: factories to connect (only to check whether they still exists or not)
	 * @param start, ziel: coordinates that should be connected by the class, will be passed to connector_generic
	 * @param road_besch: road type to be built
	 * @param d, nr_vehicles: passed to vehicle_builder
	 */
	connector_road_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1, const fabrik_t *fab2, koord3d start_, koord3d ziel_, const weg_besch_t *road_besch, simple_prototype_designer_t *d, uint16 nr_veh);
	~connector_road_t();
	virtual return_value_t *step();

	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual void rotate90( const sint16);
	virtual void debug( log_t &file, string prefix );
private:
	wfabrik_t fab1, fab2;
	const weg_besch_t *road_besch;
	simple_prototype_designer_t *prototyper;
	uint16 nr_vehicles;
	// entries in the schedule
	koord3d start, ziel, deppos;
	// progress counter
	uint8 phase;
};


#endif /* CONNECTOR_ROAD_H */
