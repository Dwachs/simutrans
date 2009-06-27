#include "vehikel_prototype.h"

#include <math.h>
#include "../../besch/ware_besch.h"
#include "../../besch/vehikel_besch.h"
#include "../../bauer/vehikelbauer.h"
#include "../../dataobj/loadsave.h"
#include "../../player/ai.h"
#include "../../vehicle/simvehikel.h"
#include "../../simconvoi.h"
#include "../../simdebug.h"
#include "../../simworld.h"

// returns capacity for given freight
// if freight==NULL returns total capacity
uint32 vehikel_prototype_t::get_capacity(const ware_besch_t* freight) const{
	uint32 capacity = 0;
	for(uint8 i=0; i<besch.get_count(); i++) {
		if (besch[i]) {
			// compatible freights?
			if(freight==NULL || besch[i]->get_ware()->get_catg_index()==freight->get_catg_index()) {
				capacity += besch[i]->get_zuladung();
			}
		}
	}
	return capacity;
}

uint32 vehikel_prototype_t::get_maintenance() const{
	uint32 maintenance = 0;
	for(uint8 i=0; i<besch.get_count(); i++) {
		if (besch[i]) {
			maintenance += besch[i]->get_betriebskosten();
		}
	}
	return maintenance;
}

bool vehikel_prototype_t::is_electric() const{
	for(uint8 i=0; i<besch.get_count(); i++) {
		if (besch[i] && besch[i]->get_engine_type()==vehikel_besch_t::electric) {
			return true;
		}
	}
	return false;
}

waytype_t vehikel_prototype_t::get_waytype() const
{
	if (besch.empty()) {
		return invalid_wt;
	}
	else {
		return besch[1]->get_waytype();
	}
}

/* extended search for vehicles for KI *
 * checks also timeline and constraints
 *
 * @author dwachs
 */
// TODO: flags um verschiedene Heuristiken zu schalten
//		-- nur bestimmte anzahl verschiedener fahrzeuge
//		-- loks etc nur vorn
//		-- kann nur geg. Fracht transportieren
//		-- keine Permutationen (geht schlecht wenn <ab><ab>.. optimal ist)
// TODO: zus. Parameter: vector_tpl<vehikel_besch_t*> .. prototyp muss mind ein Fahrzeug aus dieser Liste enthalten
vehikel_prototype_t* vehikel_prototype_t::vehikel_search( vehikel_evaluator_t *eval, karte_t *world,
							  const waytype_t wt,
							  const uint32 min_speed, // in km/h
							  const uint8 max_length, // in tiles
							  const uint32 max_weight,
							  const slist_tpl<const ware_besch_t*> & freights,
							  const bool include_electric,
							  const bool not_obsolete )
{
	// only8 different freight categories supported
	assert(freights.get_count()<=8);

	// obey timeline
	const uint32 month_now = (world->use_timeline() ? world->get_current_month() : 0);

	// something available?
	if(vehikelbauer_t::get_info(wt)->empty()) {
		return new vehikel_prototype_t(0);
	}
	// search valid vehicles
	slist_iterator_tpl<const vehikel_besch_t*> vehinfo(vehikelbauer_t::get_info(wt));
	slist_tpl<const vehikel_besch_t*> vehicles;
	slist_tpl<const vehikel_besch_t*> front_vehicles;
	while (vehinfo.next()) {
		const vehikel_besch_t* test_besch = vehinfo.get_current();
		// filter unwanted vehicles
		// preliminary check: timeline, obsolete, electric (min_speed) (max_weight)
		if ( test_besch->is_future(month_now) ||						// vehicle not available
			 (not_obsolete  &&  test_besch->is_retired(month_now)) ||	// vehicle obsolete
			 (!include_electric && test_besch->get_engine_type()==vehikel_besch_t::electric) || // electric
			 (kmh_to_speed(test_besch->get_geschw()) < min_speed) ||	// too slow
			 (test_besch->get_gewicht() > max_weight) ) {				// too heavy
			// ignore this vehicle
		} else {
			// leave wagons out that do not transport our goods
			if (test_besch->can_follow_any() && test_besch->get_vorgaenger_count()==0 && test_besch->get_leistung()==0) {
				slist_iterator_tpl<const ware_besch_t*> waren(freights);
				bool ok = false;
				while(waren.next()){
					if (test_besch->get_ware()->is_interchangeable(waren.get_current())) ok = true;
				}
				if (!ok) continue;
			}
			// front vehicles
			if (test_besch->can_lead()) {
				front_vehicles.insert(test_besch);
			}
			// all other vehicles (without front-only vehicles)
			if (test_besch->get_vorgaenger_count()!=1 || test_besch->get_vorgaenger(0)!=NULL) {
				vehicles.insert(test_besch);
			}
			// dbg->message("VBAI", "approved vehicle %s", test_besch->get_name());
		}
	}
	// now vehicles contains only valid vehicles
	INT_CHECK("convoi_t* vehikelbauer_t::vehikel_search::1");
	// max number of vehicles in convoi
	const sint8 max_vehicles = 6; // depot->get_max_convoi_length();

	// the prototypes
	vehikel_prototype_t* convoi_tpl=new vehikel_prototype_t[max_vehicles+1];
	// initialize size
	for (uint8 i=1; i<=max_vehicles; i++) {
		convoi_tpl[i].besch.resize(i);
	}
	// dummy values
	convoi_tpl[0].set_data(0, 0, 0, 0xffff, 0, 0xff >> (8-freights.get_count()));

	// the best so far- empty
	vehikel_prototype_t* best = new vehikel_prototype_t();
	sint64 best_value = 0x8000000000000000ll;

	// Iterators for each position
	vector_tpl<slist_iterator_tpl<const vehikel_besch_t*>*> vehicle_loop(max_vehicles);
	// initialize
	vehicle_loop.insert_at(0, new slist_iterator_tpl<const vehikel_besch_t*>(&front_vehicles));
	for (uint8 i=1; i<max_vehicles; i++) {
		vehicle_loop.insert_at(i, new slist_iterator_tpl<const vehikel_besch_t*>(&vehicles));
	}

	// Blacklist
	vector_tpl<const vehikel_besch_t*> blacklist(max_vehicles); for (uint16 i=0;i<max_vehicles;i++) blacklist.append(NULL);

	uint32 steps = 0;
	sint8 i=0;
	while(i>=0) {
		if (vehicle_loop[i]->next()) {
			// last vehicle of the current prototype
			const vehikel_besch_t* prev_besch = i>0 ? convoi_tpl[i].besch[i-1] : NULL;
			// try to append this vehicle now
			const vehikel_besch_t* test_besch = vehicle_loop[i]->get_current();
			// valid coupling
			if ( (i==0 && test_besch->can_lead())
				|| (i>0 && prev_besch->can_lead(test_besch) && test_besch->can_follow(prev_besch) && !blacklist.is_contained(test_besch))) {
				// avoid some permutations - do we lose something here?
				if ( i>0 && prev_besch->can_follow(test_besch) && test_besch->can_lead(prev_besch) && prev_besch->get_gewicht()<test_besch->get_gewicht()) {
					// dbg->message("VBAI", "[%2d] vehicle %20s avoided", i, test_besch->get_name());
					continue;
				}
				// avoid loks in the middle of a convoi
				if (i>0 && test_besch->get_leistung()>0 && test_besch->can_follow_any() && prev_besch->get_leistung()==0){					dbg->message("VBAI", "[%2d] vehicle %20s avoided", i, test_besch->get_name());
					// dbg->message("VBAI", "[%2d] vehicle %20s avoided", i, test_besch->get_name());
					continue;
				}
				// test for valid convoi
				// .. max_len
				if (convoi_tpl[i].length+test_besch->get_length() > TILE_STEPS*max_length) {
					// dbg->message("VBAI", "[%2d] vehicle %20s too long", i, test_besch->get_name());
					continue;
				}
				// .. max_weight and freights
				//      find the heaviest good for this vehicle
				//		clear the bit in missing_freights
				slist_iterator_tpl<const ware_besch_t*> waren(freights);
				uint32 w=0;
				convoi_tpl[i+1].missing_freights=convoi_tpl[i].missing_freights;
				uint8 bit=1;
				while(waren.next()){
					const ware_besch_t* ware = waren.get_current();
					if (test_besch->get_ware()->is_interchangeable(ware)) {
						w = max(ware->get_weight_per_unit(), w);

						convoi_tpl[i+1].missing_freights &= !bit;
					}
					bit <<=1;
				}
				// correctly calculate weight
				w = (w*test_besch->get_zuladung() + 499)/1000 + test_besch->get_gewicht();

				if (convoi_tpl[i].weight + w > max_weight) {
					// dbg->message("VBAI", "[%2d] vehicle %20s too heavy", i, test_besch->get_name());
					continue;
				}
				convoi_tpl[i+1].weight = convoi_tpl[i].weight + w;

				// .. min_speed
				convoi_tpl[i+1].power = convoi_tpl[i].power + test_besch->get_leistung()*test_besch->get_gear()/64;
				convoi_tpl[i+1].min_top_speed = min( convoi_tpl[i].min_top_speed, kmh_to_speed(test_besch->get_geschw()) );

				convoi_tpl[i+1].max_speed = min(speed_to_kmh(convoi_tpl[i+1].min_top_speed),
										(uint32) sqrt((((double)convoi_tpl[i+1].power/convoi_tpl[i+1].weight)-1)*2500));


				convoi_tpl[i+1].length = convoi_tpl[i].length+test_besch->get_length();
				convoi_tpl[i+1].besch.store_at(i, test_besch);

				dbg->message("VBAI", "[%2d] test vehicle %20s, Fr=%d, W=%d, L=%d, P=%d, Vmin=%d, Vmax=%d", i, test_besch->get_name(), convoi_tpl[i+1].missing_freights,
					convoi_tpl[i+1].weight, convoi_tpl[i+1].length, convoi_tpl[i+1].power, speed_to_kmh(convoi_tpl[i+1].min_top_speed), convoi_tpl[i+1].max_speed );

				if (convoi_tpl[i+1].missing_freights==0 && convoi_tpl[i+1].max_speed >= min_speed ) {
					// evaluate
					sint64 value = eval->valuate(convoi_tpl[i+1]);
					dbg->message("VBAI", "[%2d] evalutated: %d", i, value);

					// take best
					if (value > best_value) {
						best->set_data(convoi_tpl[i+1]);
						best->value = value;

						// sofortkauf
						if (value == 0x7fffffff) {
							return best;
						}
						best_value = value;
					}
				}

				// append more
				if (i<max_vehicles-1) {
					// ignore something to reduce posible permutations
					if ((i>0) && (convoi_tpl[i+1].besch[i-1]!=test_besch) && (convoi_tpl[i+1].besch[i-1]->can_follow_any()) ){
						blacklist[i-1] = convoi_tpl[i+1].besch[i-1];
						// dbg->message("VBAI", "[%2d] ignore: %s", i, convoi_tpl[i+1].besch[i-1]->get_name());
					}
					i++;
					// copy tpl
					convoi_tpl[i+1].set_data(convoi_tpl[i]);


					// only one nachfolger (tender love)
					// if (convoi_tpl[i]->get_vorgaenger_count()==1 && vehicles.is_contained(convoi_tpl[i]->get_vorgaenger(1)) ) {

					// start iterator
					vehicle_loop[i]->begin(&vehicles);
				}
			}
			else {/*
				if (i>0)
					dbg->message("VBAI", "[%2d] vehicle %20s not suitable %d %d", i, test_besch->get_name(),convoi_tpl[i-1]->get_nachfolger_count(), test_besch->get_vorgaenger_count());
				else
					dbg->message("VBAI", "[%2d] vehicle %20s not suitable %d", i, test_besch->get_name(),test_besch->get_vorgaenger_count());
		*/
			}
		}
		else {
			// end of the ith loop
			i--;
			if (i>0) {
				blacklist[i-1] = NULL;
			}
		}


		steps++;
		if ( (steps&16)==0) INT_CHECK("convoi_t* vehikelprototype_t::vehikel_search");
	}
	dbg->message("VBAI", "== Steps: %d", steps);
	dbg->message("VBAI", "== Best : %d", best_value);
	for (uint8 i=0; i<best->besch.get_count(); i++) {
		dbg->message("VBAI", "-- [%2d]: %s", i, best->besch[i]->get_name() );

	}

	delete [] convoi_tpl;

	return best;
}


// build a given prototype-convoi.
convoi_t*  vehikel_prototype_t::baue(koord3d k, spieler_t* sp, const vehikel_prototype_t* proto )
{
	convoi_t* cnv = new convoi_t(sp);

	for (uint8 i=0; i<proto->besch.get_count() && proto->besch[i]; i++) {
		vehikel_t *v = vehikelbauer_t::baue(k, sp, cnv, proto->besch[i]);
		cnv->add_vehikel( v );

		if (i==0) {
			// V.Meyer: give the new convoi name from first vehicle
			cnv->set_name(v->get_besch()->get_name());
		}
	}
	return cnv;
}


void vehikel_prototype_t::rdwr(loadsave_t *file)
{
	bool ok = true;
	file->rdwr_long( weight, " ");
	file->rdwr_long( power, " ");
	file->rdwr_short(min_top_speed, " ");
	file->rdwr_short(length, " ");
	file->rdwr_long( max_speed, " ");
	file->rdwr_byte( missing_freights, " ");

	uint32 count = besch.get_count();
	file->rdwr_long( count, " ");

	const char *s;
	for(uint32 i=0; i<count; i++) {
		if (file->is_saving()) {
			s = besch[i]->get_name();
		}
		file->rdwr_str( s );
		if (file->is_loading()) {
			const vehikel_besch_t *b = vehikelbauer_t::get_info(s);
			if (b) {
				besch.append(b);
			}
			else {
				ok = false;
			}
		}
	}

	// prototype invalid ...
	if (!ok) {
		besch.clear();
	}
}




// the prototype designer

void simple_prototype_designer_t::update()
{
	if (proto) {
		delete proto;
	}
	slist_tpl<const ware_besch_t *> freights;
	freights.append(freight);

	proto = vehikel_prototype_t::vehikel_search(this, sp->get_welt(),
							  wt, min_speed, max_length, max_weight, freights, include_electric, not_obsolete );
}

void simple_prototype_designer_t::rdwr(loadsave_t *file)
{
	sint16 _wt = (uint8)wt;
	file->rdwr_short(_wt, "");
	if (file->is_loading()) {
		wt = (waytype_t)_wt;
	}
	file->rdwr_long(min_speed, "");
	file->rdwr_byte(max_length, ""); 
	file->rdwr_long(max_weight, "");
	ai_t::rdwr_ware_besch(file, freight);
	file->rdwr_bool(include_electric, "");
	file->rdwr_bool(not_obsolete, "");

	proto = new vehikel_prototype_t();
	proto->rdwr(file);
}

// evaluate a convoi suggested by vehicle bauer
sint64 simple_prototype_designer_t::valuate(const vehikel_prototype_t &proto)
{
	// TODO: wartungskosten fuer Strecke und Elektrifizierung einkalkulieren
	if (proto.is_empty()) return 0x8000000000000000ll;

	const uint32 capacity = proto.get_capacity(freight);
	const uint32 maintenance = proto.get_maintenance();

	// simple net gain
	//sint64 net_gain = ((sint64)( (capacity * (freight->get_preis()<<7)) / 3000ll - maintenance *3)) * (proto.max_speed*3) /4;

	// speed bonus calculation see vehikel_t::calc_gewinn
	const sint32 ref_speed = sp->get_welt()->get_average_speed( wt );
	const sint32 speed_base = (100*speed_to_kmh(proto.min_top_speed))/ref_speed-100;
	const sint32 freight_price = freight->get_preis() * max( 128, 1000+speed_base*freight->get_speed_bonus());

	// net gain
	//const sint64 value = ((capacity * freight_price +1500ll )/3000ll - maintenance *3) * (proto.max_speed*3) /4;

	// net gain per transported unit (matching freight)
	const sint64 value = (((capacity * freight_price +1500ll )/3000ll - maintenance *3) * (proto.max_speed*3) /4 *1000) / capacity;

//	for(uint8 i=0;i<proto.besch.get_count(); i++)
//		sp->log->message("ait_road_connectf_t::valuate", "[%d] %s",i,proto.besch[i]->get_name() );
//	sp->log->message("ait_road_connectf_t::valuate", "cap=%d/%d maint=%d speed=%d freightprice=%d value=%ld ",
//		capacity, proto.get_capacity(NULL), maintenance, proto.max_speed, freight_price, value);

	return value;
}

