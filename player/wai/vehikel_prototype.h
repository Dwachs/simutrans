#ifndef vehikel_prototype_h
#define vehikel_prototype_h

#include "../../convoihandle_t.h"
#include "../../simtypes.h"
#include "../../tpl/vector_tpl.h"
#include "../../tpl/slist_tpl.h"
#include "../../utils/cstring_t.h"

class convoi_t;
class karte_t;
class koord3d;
class loadsave_t;
class spieler_t;
class vehikel_besch_t;
class vehikel_evaluator_t;
class ware_besch_t;

class vehikel_prototype_t {
public:
	/*
	 * Returns a prototype for given constraints and evaluator
	 * @author Dwachs
	 * @parameter min_speed - the full-loaded convoi should drive at least with this speed
	 * @parameter max_length - the max length f the convoi in tiles
	 * @parameter freights - the convoi has to be able to load all given freights
	 */
	static vehikel_prototype_t* vehikel_search( vehikel_evaluator_t *eval, karte_t *world,
							  const waytype_t wt,
							  const uint32 min_speed, // in km/h
							  const uint8 max_length, // in tiles
							  const uint32 max_weight,
							  const slist_tpl<const ware_besch_t*> & freights,
							  const bool include_electric,
							  const bool not_obsolete );

	static convoi_t* baue(koord3d k, spieler_t* sp, const vehikel_prototype_t* proto );
	/*
	 * protoypes for convois
	 * @author Dwachs
	 */
	vehikel_prototype_t() : besch(0) {}

	explicit vehikel_prototype_t(uint8 len) : besch(len) { for (uint16 i=0;i<len;i++) besch[i]=NULL; }

	explicit vehikel_prototype_t(vehikel_prototype_t &proto) : besch() {
		set_data(proto);
	}
	// copies data
	void set_data(vehikel_prototype_t &proto)
	{
		besch.clear();
		besch.resize(proto.besch.get_count());
		for(uint8 i=0; i<proto.besch.get_count(); i++){
			if (proto.besch[i]) {
				besch.append(proto.besch[i]);
			}
			else {
				break;
			}
		}
		set_data(proto.weight, proto.power, proto.length, proto.min_top_speed, proto.max_speed, proto.missing_freights);
	}
	// sets the vehicle characteristics
	void set_data(uint32 wei, uint32 pow,  uint16 len, uint16 mints, uint32 maxsp, uint8 msf)
	{
		weight = wei; power = pow; length = len; min_top_speed = mints; max_speed = maxsp; missing_freights =msf;
	}

	waytype_t get_waytype() const;

	// calculates capacity for a certain good
	uint32 get_capacity(const ware_besch_t*) const;
	uint32 get_maintenance() const;

	bool is_empty() const { return besch.get_count()==0; }
	bool is_electric() const;

	vector_tpl<const vehikel_besch_t*> besch;
	uint32 weight;
	uint32 power;
	uint16 min_top_speed;
	uint16 length;
	uint32 max_speed;
	uint8 missing_freights;

	sint64 value;

	void rdwr(loadsave_t *file);
};

class vehikel_evaluator_t {
public:
	// evaluate a convoi suggested by vehicle bauer
	virtual sint64 valuate(const vehikel_prototype_t &) { return 0; }
};

// generates prototype for one given freight
// saves the prototype parameters and the prototype itself
// updates the prototype if called
class simple_prototype_designer_t : public vehikel_evaluator_t {
public:
	waytype_t wt;
	uint32 min_speed; // in km/h
	uint8 max_length; // in tiles
	uint32 max_weight;
	const ware_besch_t* freight;
	bool include_electric;
	bool not_obsolete;

	spieler_t *sp;

	vehikel_prototype_t *proto;

	simple_prototype_designer_t(spieler_t *_sp) : sp(_sp) { proto = new vehikel_prototype_t(); }
	// creates a designer to clone the convoi
	simple_prototype_designer_t(convoihandle_t cnv, const ware_besch_t *f);
	~simple_prototype_designer_t() { if (proto) delete proto; }

	// computes / updates the prototype
	void update();

	// evaluate a convoi suggested by vehicle bauer
	virtual sint64 valuate(const vehikel_prototype_t &proto);

	void rdwr(loadsave_t *file);
	void debug( log_t &file, cstring_t prefix );
};



#endif
