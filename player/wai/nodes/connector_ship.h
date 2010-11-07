#ifndef CONNECTOR_SHIP_H
#define CONNECTOR_SHIP_H

/*
 * This node builds a road connection.
 * @author gerw
 * @date 20.06.2009
 */

#include "../bt.h"
#include "../utils/wrapper.h"
#include "../../../bauer/hausbauer.h"
#include "../../../dataobj/koord3d.h"

class fabrik_t;
class weg_besch_t;
class simple_prototype_designer_t;
class way_obj_besch_t;

class connector_ship_t : public bt_sequential_t
{
public:
	connector_ship_t( ai_wai_t *sp, const char *name);
	/**
	 * the constructor
	 * @param fab1, fab2: factories to connect (only to check whether they still exists or not)
	 * @param start, ziel: coordinates, where harbours should be built
	 * @param d, nr_vehicles: passed to vehicle_builder
	 */
	connector_ship_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1, const fabrik_t *fab2, koord3d start_, koord3d ziel_, simple_prototype_designer_t *d, uint16 nr_veh);
	~connector_ship_t();
	virtual return_value_t *step();

	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual void rotate90( const sint16);
	virtual void debug( log_t &file, string prefix );
private:
	wfabrik_t fab1, fab2;
	simple_prototype_designer_t *prototyper;
	uint16 nr_vehicles;
	uint8 phase;
	koord3d deppos, harbour_pos, start_harbour_pos;

	bool build_harbour(koord3d &pos) const;
	// Helper function:
	const haus_besch_t* get_random_harbour(const uint16 time, const uint8 enables, uint32 max_len=1) const;
	// Get position for ship schedule
	koord3d get_ship_target(koord3d pos, koord3d target) const;
	koord3d get_water_tile(koord3d pos) const;
};

#endif /* CONNECTOR_SHIP_H */
