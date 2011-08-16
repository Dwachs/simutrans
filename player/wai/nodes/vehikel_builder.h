#ifndef _VEHIKEL_BUILDER_h_
#define _VEHIKEL_BUILDER_h_

#include "../bt.h"
#include "../vehikel_prototype.h"
#include "../../ai_wai.h"
#include "../../../convoihandle_t.h"
#include "../../../linehandle_t.h"

class vehikel_builder_t : public bt_node_t {
public:
	vehikel_builder_t( ai_wai_t *sp, const char *name, simple_prototype_designer_t *d=NULL, linehandle_t _line = linehandle_t(), koord3d p=koord3d::invalid, uint16 n=0 ) : bt_node_t(sp,name), prototyper(d), line(_line), pos(p), nr_vehikel(n), withdraw_old(false) { type = BT_VEH_BUILDER;};
	~vehikel_builder_t();
	virtual return_value_t *step();

	void set_withdraw(bool yn) { withdraw_old = yn; }
	void withdraw() const;

	virtual void debug( log_t &file, string prefix );
	virtual void rotate90( const sint16 y_size );
	virtual void rdwr( loadsave_t *file, const uint16 version );
private:
	simple_prototype_designer_t *prototyper;
	linehandle_t line;
	koord3d pos;
	uint16 nr_vehikel;
	convoihandle_t cnv;
	bool withdraw_old; // if true all vehicles of line will be withdrawn
};

#endif
