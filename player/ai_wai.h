#include "ai.h"
#include "simplay.h"
#include "wai/bt.h"
#include "wai/nodes/industry_manager.h"

#include "../utils/log.h"

#define WAI_VERSION (1)

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
	ai_wai_t( karte_t *welt, uint8 nr );

	industry_manager_t* get_industry_manager() { return industry_manager; }
	void set_industry_manager(industry_manager_t *im) { industry_manager = im; }
private:
	log_t log;
	bt_sequential_t bt_root;

	industry_manager_t* industry_manager;
public:
	log_t& get_log() { return log; };
};
