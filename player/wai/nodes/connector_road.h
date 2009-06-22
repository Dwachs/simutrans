#ifndef CONNECTER_ROAD_H
#define CONNECTER_ROAD_H 

/*
 * This node builds a road connection.
 * @author Gerd Wachsmuth
 * @date 20.06.2009
 */

#include "../bt.h"

class fabrik_t;
class weg_besch_t;

class connector_road_t : public bt_sequential_t
{
public:
	connector_road_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1, const fabrik_t *fab2, const weg_besch_t *road_besch );
	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual return_code step();
private:
	const fabrik_t *fab1, *fab2;
	const weg_besch_t *road_besch;
};


#endif /* CONNECTER_ROAD_H */
