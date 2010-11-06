#ifndef CONNECTER_ROAD_H
#define CONNECTER_ROAD_H

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
	connector_road_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1, const fabrik_t *fab2, const weg_besch_t *road_besch, simple_prototype_designer_t *d, uint16 nr_veh, const koord3d harbour_pos = koord3d::invalid);
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
	uint8 phase;
	koord3d start, ziel, deppos, harbour_pos;
};


#endif /* CONNECTER_ROAD_H */
