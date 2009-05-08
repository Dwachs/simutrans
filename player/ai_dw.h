/*
 * Copyright (c) 1997 - 2001 Hansjg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 *
 * Dwachs AI
 */


#include "ai.h"
#include "ai_dw_tasks.h"

#include "../utils/log.h"


class ai_dw_t : public ai_t
{
private:
	// KI helper class
	class fabconnection_t{
		const fabrik_t *fab1;
		const fabrik_t *fab2;	// koord1 must be always "smaller" than koord2
		const ware_besch_t *ware;

	public:
		fabconnection_t(const fabrik_t *k1=0, const fabrik_t *k2=0, const ware_besch_t *w=0 ) : fab1(k1), fab2(k2), ware(w) {}
		void rdwr( loadsave_t *file );

		bool operator != (const fabconnection_t & k) { return fab1 != k.fab1 || fab2 != k.fab2 || ware != k.ware; }
		bool operator == (const fabconnection_t & k) { return fab1 == k.fab1 && fab2 == k.fab2 && ware == k.ware; }
//		const bool operator < (const fabconnection_t & k) { return (abs(fab1.x)+abs(fab1.y)) - (abs(k.fab1.x)+abs(k.fab1.y)) < 0; }
	};

	slist_tpl<fabconnection_t> forbidden_connections;
public:
	// return true, if this a route to avoid (i.e. we did a construction without sucess here ...)
	bool is_forbidden( const fabrik_t *fab1, const fabrik_t *fab2, const ware_besch_t *w ) const {
		return forbidden_connections.is_contained( fabconnection_t( fab1, fab2, w ) );
	}
	void add_forbidden( const fabrik_t *fab1, const fabrik_t *fab2, const ware_besch_t *w ) {
		forbidden_connections.append( fabconnection_t( fab1, fab2, w ) );
	}

	ai_dw_t(karte_t *wl, uint8 nr);

	virtual ~ai_dw_t() { delete log;}

	// this type of AIs identifier
	virtual uint8 get_ai_id() { return AI_DW; }

	void rdwr(loadsave_t *file);

	virtual void bescheid_vehikel_problem(convoihandle_t cnv,const koord3d ziel);

	bool set_active( bool b );

	void step();

	void neues_jahr();

	virtual void rotate90( const sint16 y_size );

	log_t *log;

	ai_task_queue_t queue;

	// evaluate a convoi suggested by vehicle bauer
	//virtual sint64 valuate(const vehikel_prototype_t &proto);

	
	// returns an instance of a task
	ai_task_t *alloc_task(uint16 type);
};
