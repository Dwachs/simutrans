#ifndef _VEHIKEL_BUILDER_h_
#define _VEHIKEL_BUILDER_h_

#include "../bt.h"
#include "../vehikel_prototype.h"
#include "../../ai_wai.h"
#include "../../../convoihandle_t.h"
#include "../../../linehandle_t.h"

class vehikel_builder_t : public bt_node_t {
public:
	vehikel_builder_t( ai_wai_t *sp, const char *name, simple_prototype_designer_t *d, linehandle_t _line, koord3d p, uint8 n ) : bt_node_t(sp,name), prototyper(d), line(_line), pos(p), nr_vehikel(n) {};
	~vehikel_builder_t();
	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual return_code step();
	virtual void rotate90( const sint16 y_size );
private:
	simple_prototype_designer_t *prototyper;
	linehandle_t line;
	koord3d pos;
	uint8 nr_vehikel;
	convoihandle_t cnv;
};

#endif
