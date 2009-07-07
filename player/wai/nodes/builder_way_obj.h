#ifndef BUILDER_WAY_OBJ_H
#define BUILDER_WAY_OBJ_H

#include "../bt.h"
#include "../../../dataobj/koord3d.h"

class way_obj_besch_t;

class builder_wayobj_t : public bt_node_t
{
public:
	builder_wayobj_t( ai_wai_t* sp, const char *name, koord3d start_, koord3d ziel_, const way_obj_besch_t *e_ ) :
		bt_node_t( sp, name ),
		start( start_ ),
		ziel( ziel_ ),
		e( e_ ) { type = BT_WAYOBJ; };
	virtual return_code step();
	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual void rotate90( const sint16 y_size );
private:
	koord3d start, ziel;
	const way_obj_besch_t *e;
};

#endif /* BUILDER_WAY_OBJ_H */
