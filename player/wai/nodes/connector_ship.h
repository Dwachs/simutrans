#ifndef CONNECTER_SHIP_H
#define CONNECTER_SHIP_H

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
	connector_ship_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1, const fabrik_t *fab2, simple_prototype_designer_t *d, uint16 nr_veh, const koord3d &harbour_pos );
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
	koord3d start, deppos, harbour_pos;

	// Helper function:
	const haus_besch_t* get_random_harbour(const uint16 time, const uint8 enables, uint32 max_len=1) const;
	// Get position for ship schedule
	koord3d get_ship_target();
};

#endif /* CONNECTER_SHIP_H */
