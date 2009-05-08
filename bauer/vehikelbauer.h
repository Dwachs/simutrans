/*
 * Copyright (c) 1997 - 2002 Hansjörg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef vehikelbauer_t_h
#define vehikelbauer_t_h


#include "../dataobj/koord3d.h"
#include "../simimg.h"
#include "../simtypes.h"
#include "../tpl/vector_tpl.h"

class vehikel_t;
class cstring_t;
class spieler_t;
class karte_t;
class convoi_t;
class vehikel_besch_t;
class ware_besch_t;
template <class T> class slist_tpl;


//typedef vector_tpl<const vehikel_besch_t*>  vehikel_prototype_t;

class vehikel_prototype_t {
public:
	vehikel_prototype_t() : besch(0) {}

	explicit vehikel_prototype_t(uint8 len) : besch(len, NULL) {}

	explicit vehikel_prototype_t(vehikel_prototype_t &proto) : besch() {
		set_data(proto);
	}
	// copies data
	void set_data(vehikel_prototype_t &proto) 
	{
		besch.clear();
		besch.resize(proto.besch.get_count());
		for(uint8 i=0; i<proto.besch.get_count(); i++){
			if (proto.besch[i])
				besch.append(proto.besch[i]);
			else
				break;
		}
		set_data(proto.weight, proto.power, proto.length, proto.min_top_speed, proto.max_speed, proto.missing_freights);
	}
	// sets the vehicle characteristics
	void set_data(uint32 wei, uint32 pow,  uint16 len, uint16 mints, uint32 maxsp, uint8 msf) 
	{
		weight = wei; power = pow; length = len; min_top_speed = mints; max_speed = maxsp; missing_freights =msf;
	}

	// calculates capacity for a certain good
	uint32 get_capacity(const ware_besch_t*) const;
	uint32 get_maintenance() const;

	bool is_empty() const { return besch.get_count()==0; }

	vector_tpl<const vehikel_besch_t*> besch;
	uint32 weight;
	uint32 power; 
	uint16 min_top_speed; 
	uint16 length; 
	uint32 max_speed;
	uint8 missing_freights;

	sint64 value;
};

class vehikel_evaluator {
public:
	// evaluate a convoi suggested by vehicle bauer
	virtual sint64 valuate(const vehikel_prototype_t &proto) { return 0; }
};

/**
 * Baut Fahrzeuge. Fahrzeuge sollten nicht direct instanziiert werden
 * sondern immer von einem vehikelbauer_t erzeugt werden.
 *
 * @author Hj. Malthaner
 */
class vehikelbauer_t
{
public:
	static bool speedbonus_init(cstring_t objfilename);
	static sint32 get_speedbonus( sint32 monthyear, waytype_t wt );

	static bool register_besch(const vehikel_besch_t *besch);
	static bool alles_geladen();

	static vehikel_t* baue(koord3d k, spieler_t* sp, convoi_t* cnv, const vehikel_besch_t* vb );
	
	static convoi_t*  baue(koord3d k, spieler_t* sp, const vehikel_prototype_t* proto );

	static const vehikel_besch_t * get_info(const char *name);
	static slist_tpl<const vehikel_besch_t*>* get_info(waytype_t typ);

	/* extended sreach for vehicles for KI
	* @author prissi
	*/
	static const vehikel_besch_t *vehikel_search(waytype_t typ,const uint16 month_now,const uint32 target_power,const uint32 target_speed,const ware_besch_t * target_freight, bool include_electric, bool not_obsolete );

	/* for replacement during load time
	 * prev_veh==NULL equals leading of convoi
	 */
	static const vehikel_besch_t *get_best_matching( waytype_t wt, const uint16 month_now, const uint32 target_weight, const uint32 target_power, const uint32 target_speed, const ware_besch_t * target_freight, bool include_electric, bool not_obsolete, const vehikel_besch_t *prev_veh, bool is_last );

	static vehikel_prototype_t* vehikel_search( vehikel_evaluator *eval, karte_t *world,
							  const waytype_t wt,
							  const uint32 min_speeed,
							  const uint8 max_length, // in tiles
							  const uint32 max_weight, 
							  const slist_tpl<const ware_besch_t*> & freights,
							  const bool include_electric, 
							  const bool not_obsolete );

};

#endif
