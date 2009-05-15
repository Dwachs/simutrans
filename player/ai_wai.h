#include "ai.h"
#include "simplay.h"
#include "wai/bt.h"

#include "../utils/log.h"

class ai_wai_t : public ai_t {
public:
	// Needed Interface:
	virtual uint8 get_ai_id() { return AI_WAI; }
	virtual ~ai_wai_t();
	virtual void step();
	virtual void neuer_monat();
	virtual void neues_jahr();
	virtual void rdwr(loadsave_t *file);
	virtual void laden_abschliessen();
	virtual void rotate90( const sint16 y_size );
	virtual void bescheid_vehikel_problem( convoihandle_t cnv, const koord3d ziel );
public:
	ai_wai_t( karte_t *welt, uint8 nr );
	log_t log;
	bt_sequential_t bt_root;
};

