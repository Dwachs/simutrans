#ifndef _remover_h_
#define _remover_h_


#include "../bt.h"
#include "../../../dataobj/koord3d.h"

class loadsave_t;

/*
 * Removes the route from both ends
 */
class remover_t : public bt_node_t {
public:
	waytype_t wt;
	koord3d start, end;

	remover_t( ai_wai_t *sp, waytype_t wt_=invalid_wt, koord3d start_=koord3d::invalid, koord3d end_=koord3d::invalid) : bt_node_t(sp), wt(wt_), start(start_), end(end_) { type = BT_REMOVER; }

	return_value_t* step();

	void rdwr(loadsave_t* file, const uint16 version);
	void rotate90( const sint16 y_size);
	void debug( log_t &file, cstring_t prefix );
private:
	uint8 check_position(koord3d pos);
};


#endif
