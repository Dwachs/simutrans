#ifndef _CONNECTOR_GENERIC_H_
#define _CONNECTOR_GENERIC_H_


#include "../bt.h"
#include "../../../dataobj/koord3d.h"
#include "../../../simtypes.h"

class weg_besch_t;
/**
 * Class that build a generic point-to-point connection
 * if depot position is specified then a depot is built near start
 */
class connector_generic_t : public bt_sequential_t
{
public:
	connector_generic_t( ai_wai_t *sp, const char *name);
	connector_generic_t( ai_wai_t *sp, const char *name, koord3d start, koord3d ziel, const weg_besch_t *wb);
	virtual return_value_t *step();

	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual void rotate90( const sint16);
	virtual void debug( log_t &file, cstring_t prefix );
protected:
	const weg_besch_t *weg_besch;
private:
	uint8 phase, force_through;
	koord3d start, ziel, depot_pos;
	waytype_t wt;

	koord3d_vector_t tile_list[2], through_tile_list[2];
};

#endif
