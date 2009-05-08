/*
 * Copyright (c) 1997 - 2002 Hansjörg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <algorithm>
#include <math.h>
#include "../simgraph.h"
#include "../vehicle/simvehikel.h"
#include "../player/simplay.h"
#include "../simworld.h"  // for year/month
#include "../simdebug.h"
#include "../simtools.h"  // for simrand
#include "../simtypes.h"
#include "../simconvoi.h"

#include "../dataobj/umgebung.h"
#include "../dataobj/tabfile.h"

#include "../besch/bildliste_besch.h"
#include "../besch/vehikel_besch.h"

#include "../bauer/warenbauer.h"
#include "../bauer/vehikelbauer.h"

#include "../utils/cstring_t.h"

#include "../tpl/inthashtable_tpl.h"
#include "../tpl/stringhashtable_tpl.h"


static stringhashtable_tpl<const vehikel_besch_t*> name_fahrzeuge;
static inthashtable_tpl<waytype_t, slist_tpl<const vehikel_besch_t*> > typ_fahrzeuge;

// returns capacity for freight
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

class bonus_record_t {
public:
	sint32 year;
	sint32 speed;
	bonus_record_t( sint32 y=0, sint32 kmh=0 ) {
		year = y*12;
		speed = kmh;
	};
};

// speed boni
static vector_tpl<bonus_record_t>speedbonus[8];

static sint32 default_speedbonus[8] =
{
	60,	// road
	80,	// track
	35,	// water
	350,	// air
	80,	// monorail
	200,	// maglev
	60,	// tram
	60	// narrowgauge
};

bool vehikelbauer_t::speedbonus_init(cstring_t objfilename)
{
	tabfile_t bonusconf;
	// first take user data, then user global data
	if (!bonusconf.open(objfilename+"config/speedbonus.tab")) {
		dbg->error("vehikelbauer_t::speedbonus_init()", "Can't read speedbonus.tab" );
		return false;
	}

	tabfileobj_t contents;
	bonusconf.read(contents);

	/* init the values from line with the form year, speed, year, speed
	 * must be increasing order!
	 */
	for(  int j=0;  j<8;  j++  ) {
		int *tracks = contents.get_ints(weg_t::waytype_to_string(j==3?air_wt:(waytype_t)(j+1)));
		if((tracks[0]&1)==1) {
			dbg->fatal( "vehikelbauer_t::speedbonus_init()", "Ill formed line in speedbonus.tab\nFormat is year,speed[year,speed]!" );
		}
		speedbonus[j].resize( tracks[0]/2 );
		for(  int i=1;  i<tracks[0];  i+=2  ) {
			bonus_record_t b( tracks[i], tracks[i+1] );
			speedbonus[j].append( b );
		}
		delete [] tracks;
	}

	return true;
}



sint32 vehikelbauer_t::get_speedbonus( sint32 monthyear, waytype_t wt )
{
	const int typ = wt==air_wt ? 3 : (wt-1)&7;

	if(  monthyear==0  ) {
		return default_speedbonus[typ];
	}

	// ok, now lets see if we have data for this
	if(speedbonus[typ].get_count()) {
		uint i=0;
		while(  i<speedbonus[typ].get_count()  &&  monthyear>=speedbonus[typ][i].year  ) {
			i++;
		}
		if(  i==speedbonus[typ].get_count()  ) {
			// maxspeed already?
			return speedbonus[typ][i-1].speed;
		}
		else if(i==0) {
			// minspeed below
			return speedbonus[typ][0].speed;
		}
		else {
			// interpolate linear
			const sint32 delta_speed = speedbonus[typ][i].speed - speedbonus[typ][i-1].speed;
			const sint32 delta_years = speedbonus[typ][i].year - speedbonus[typ][i-1].year;
			return ( (delta_speed*(monthyear-speedbonus[typ][i-1].year)) / delta_years ) + speedbonus[typ][i-1].speed;
		}
	}
	else {
		sint32 speed_sum = 0;
		sint32 num_averages = 0;
		// needs to do it the old way => iterate over all vehicles with this type ...
		if(typ_fahrzeuge.access(wt)) {
			slist_iterator_tpl<const vehikel_besch_t*> vehinfo(typ_fahrzeuge.access(wt));
			while (vehinfo.next()) {
				const vehikel_besch_t* info = vehinfo.get_current();
				if(info->get_leistung()>0  &&  !info->is_future(monthyear)  &&  !info->is_retired(monthyear)) {
					speed_sum += info->get_geschw();
					num_averages ++;
				}
			}
		}
		if(  num_averages>0  ) {
			return speed_sum / num_averages;
		}
	}

	// no vehicles => return default
	return default_speedbonus[typ];
}




vehikel_t* vehikelbauer_t::baue(koord3d k, spieler_t* sp, convoi_t* cnv, const vehikel_besch_t* vb )
{
	vehikel_t* v;
	switch (vb->get_waytype()) {
		case road_wt:     v = new automobil_t(      k, vb, sp, cnv); break;
		case monorail_wt: v = new monorail_waggon_t(k, vb, sp, cnv); break;
		case track_wt:
		case tram_wt:     v = new waggon_t(         k, vb, sp, cnv); break;
		case water_wt:    v = new schiff_t(         k, vb, sp, cnv); break;
		case air_wt:      v = new aircraft_t(       k, vb, sp, cnv); break;
		case maglev_wt:   v = new maglev_waggon_t(  k, vb, sp, cnv); break;
		case narrowgauge_wt:v = new narrowgauge_waggon_t(k, vb, sp, cnv); break;

		default:
			dbg->fatal("vehikelbauer_t::baue()", "cannot built a vehicle with waytype %i", vb->get_waytype());
	}

	sp->buche(-(sint32)vb->get_preis(), k.get_2d(), COST_NEW_VEHICLE );
	sp->buche( (sint32)vb->get_preis(), COST_ASSETS );

	return v;
}


// build a given prototype-convoi. 
convoi_t*  vehikelbauer_t::baue(koord3d k, spieler_t* sp, const vehikel_prototype_t* proto )
{
	convoi_t* cnv = new convoi_t(sp);

	for (uint8 i=0; i<proto->besch.get_count() && proto->besch[i]; i++) {
		vehikel_t *v = baue(k, sp, cnv, proto->besch[i]);
		cnv->add_vehikel( v );

		if (i==0) {
			// V.Meyer: give the new convoi name from first vehicle
			cnv->set_name(v->get_besch()->get_name());
		}
	}
	return cnv;
}

bool vehikelbauer_t::register_besch(const vehikel_besch_t *besch)
{
	// printf("N=%s T=%d V=%d P=%d\n", besch->get_name(), besch->get_typ(), besch->get_geschw(), besch->get_leistung());

	name_fahrzeuge.put(besch->get_name(), besch);

	// register waytype liste
	waytype_t typ = besch->get_waytype();
	slist_tpl<const vehikel_besch_t *> *typ_liste = typ_fahrzeuge.access(typ);
	if(!typ_liste) {
		typ_fahrzeuge.put(typ, slist_tpl<const vehikel_besch_t *>());
		typ_liste = typ_fahrzeuge.access(typ);
	}
	typ_liste->append(besch);

	// correct for driving on left side
	if(typ==road_wt  &&  umgebung_t::drive_on_left) {
		const int XOFF=(12*get_tile_raster_width())/64;
		const int YOFF=(6*get_tile_raster_width())/64;

		// empty image: shift to left
		display_set_base_image_offset( besch->get_bild_nr(0,NULL), +XOFF, +YOFF );
		display_set_base_image_offset( besch->get_bild_nr(1,NULL), -XOFF, +YOFF );
		display_set_base_image_offset( besch->get_bild_nr(2,NULL), 0, +YOFF );
		display_set_base_image_offset( besch->get_bild_nr(3,NULL), +XOFF, 0 );
		display_set_base_image_offset( besch->get_bild_nr(4,NULL), -XOFF, -YOFF );
		display_set_base_image_offset( besch->get_bild_nr(5,NULL), +XOFF, -YOFF );
		display_set_base_image_offset( besch->get_bild_nr(6,NULL), 0, -YOFF );
		display_set_base_image_offset( besch->get_bild_nr(7,NULL), -XOFF-YOFF, 0 );

		if(besch->freight_image_type>0) {
			const bildliste2d_besch_t *liste2d = static_cast<const bildliste2d_besch_t *>(besch->get_child(5));
			for(int i=0; i<besch->freight_image_type; i++) {
				display_set_base_image_offset(liste2d->get_bild(0, i)->get_nummer(),        +XOFF, +YOFF);
				display_set_base_image_offset(liste2d->get_bild(1, i)->get_nummer(),        -XOFF, +YOFF);
				display_set_base_image_offset(liste2d->get_bild(2, i)->get_nummer(),            0, +YOFF);
				display_set_base_image_offset(liste2d->get_bild(3, i)->get_nummer(),        +XOFF,     0);
				display_set_base_image_offset(liste2d->get_bild(4, i)->get_nummer(),        -XOFF, -YOFF);
				display_set_base_image_offset(liste2d->get_bild(5, i)->get_nummer(),        +XOFF, -YOFF);
				display_set_base_image_offset(liste2d->get_bild(6, i)->get_nummer(),            0, -YOFF);
				display_set_base_image_offset(liste2d->get_bild(7, i)->get_nummer(), -XOFF - YOFF,     0);
			}
		}
		else {
			const bildliste_besch_t *liste = static_cast<const bildliste_besch_t *>(besch->get_child(5));
			if(liste) {
				display_set_base_image_offset(liste->get_bild(0)->get_nummer(),        +XOFF, +YOFF);
				display_set_base_image_offset(liste->get_bild(1)->get_nummer(),        -XOFF, +YOFF);
				display_set_base_image_offset(liste->get_bild(2)->get_nummer(),            0, +YOFF);
				display_set_base_image_offset(liste->get_bild(3)->get_nummer(),        +XOFF,     0);
				display_set_base_image_offset(liste->get_bild(4)->get_nummer(),        -XOFF, -YOFF);
				display_set_base_image_offset(liste->get_bild(5)->get_nummer(),        +XOFF, -YOFF);
				display_set_base_image_offset(liste->get_bild(6)->get_nummer(),            0, -YOFF);
				display_set_base_image_offset(liste->get_bild(7)->get_nummer(), -XOFF - YOFF,     0);
			}
		}

	}

	return true;
}


static bool compare_vehikel_besch(const vehikel_besch_t* a, const vehikel_besch_t* b)
{
	// Sort by:
	//  1. cargo category
	//  2. cargo (if special freight)
	//  3. engine_type
	//  4. speed
	//  5. power
	//  6. intro date
	//  7. name
	int cmp = a->get_ware()->get_catg() - b->get_ware()->get_catg();
	if (cmp == 0) {
		if (a->get_ware()->get_catg() == 0) {
			cmp = a->get_ware()->get_index() - b->get_ware()->get_index();
		}
		if (cmp == 0) {
			cmp = a->get_zuladung() - b->get_zuladung();
			if (cmp == 0) {
				// to handle tender correctly
				uint8 b_engine = (a->get_zuladung() + a->get_leistung() == 0 ? (uint8)vehikel_besch_t::steam : a->get_engine_type());
				uint8 a_engine = (b->get_zuladung() + b->get_leistung() == 0 ? (uint8)vehikel_besch_t::steam : b->get_engine_type());
				cmp = b_engine - a_engine;
				if (cmp == 0) {
					cmp = a->get_geschw() - b->get_geschw();
					if (cmp == 0) {
						// put tender at the end of the list ...
						int b_leistung = (a->get_leistung() == 0 ? 0x7FFFFFF : a->get_leistung());
						int a_leistung = (b->get_leistung() == 0 ? 0x7FFFFFF : b->get_leistung());
						cmp = b_leistung - a_leistung;
						if (cmp == 0) {
							cmp = a->get_intro_year_month() - b->get_intro_year_month();
							if (cmp == 0) {
								cmp = strcmp(a->get_name(), b->get_name());
							}
						}
					}
				}
			}
		}
	}
	return cmp < 0;
}


bool vehikelbauer_t::alles_geladen()
{
	// first: check for bonus tables
	DBG_MESSAGE("vehikelbauer_t::sort_lists()","called");
	inthashtable_iterator_tpl<waytype_t, slist_tpl<const vehikel_besch_t*> > typ_iter(typ_fahrzeuge);
	while (typ_iter.next()) {
		slist_tpl<const vehikel_besch_t*>& typ_liste = typ_iter.access_current_value();
		uint count = typ_liste.get_count();
		if (count == 0) continue;
		const vehikel_besch_t** const tmp     = new const vehikel_besch_t*[count];
		const vehikel_besch_t** const tmp_end = tmp + count;
		for (const vehikel_besch_t** i = tmp; i != tmp_end; i++) {
			*i = typ_liste.remove_first();
		}
		std::sort(tmp, tmp_end, compare_vehikel_besch);
		for (const vehikel_besch_t** i = tmp; i != tmp_end; i++) {
			typ_liste.append(*i);
		}
		delete [] tmp;
	}
	return true;
}



const vehikel_besch_t *vehikelbauer_t::get_info(const char *name)
{
	return name_fahrzeuge.get(name);
}



slist_tpl<const vehikel_besch_t*>* vehikelbauer_t::get_info(waytype_t typ)
{
	return typ_fahrzeuge.access(typ);
}



/* extended sreach for vehicles for KI *
 * checks also timeline and contraits
 * tries to get best with but adds a little random action
 * @author prissi
 */
const vehikel_besch_t *vehikelbauer_t::vehikel_search( waytype_t wt, const uint16 month_now, const uint32 target_weight, const uint32 target_speed, const ware_besch_t * target_freight, bool include_electric, bool not_obsolete )
{
	const vehikel_besch_t *besch = NULL;
	long besch_index=-100000;

	if(  target_freight==NULL  &&  target_weight==0  ) {
		// no power, no freight => no vehikel to search
		return NULL;
	}

	if(typ_fahrzeuge.access(wt)) {
		slist_iterator_tpl<const vehikel_besch_t*> vehinfo(typ_fahrzeuge.access(wt));
		while (vehinfo.next()) {
			const vehikel_besch_t* test_besch = vehinfo.get_current();

			// no constricts allow for rail vehicles concerning following engines
			if(wt==track_wt  &&  !test_besch->can_follow_any()  ) {
				continue;
			}

			// engine, but not allowed to lead a convoi, or no power at all or no electrics allowed
			if(target_weight) {
				if(test_besch->get_leistung()==0  ||  !test_besch->can_lead()  ||  (!include_electric  &&  test_besch->get_engine_type()==vehikel_besch_t::electric) ) {
					continue;
				}
			}

			// check for wegetype/too new
			if(test_besch->get_waytype()!=wt  ||  test_besch->is_future(month_now)  ) {
				continue;
			}

			if(  not_obsolete  &&  test_besch->is_retired(month_now)  ) {
				// not using vintage cars here!
				continue;
			}

			uint32 power = (test_besch->get_leistung()*test_besch->get_gear())/64;
			if(target_freight) {
				// this is either a railcar/trailer or a truck/boat/plane
				if(  test_besch->get_zuladung()==0  ||  !test_besch->get_ware()->is_interchangeable(target_freight)  ) {
					continue;
				}

				sint32 difference=0;	// smaller is better
				// assign this vehicle, if we have none found one yet, or we found only a too week one
				if(  besch!=NULL  ) {
					// it is cheaper to run? (this is most important)
					difference += (besch->get_zuladung()*1000)/besch->get_betriebskosten() < (test_besch->get_zuladung()*1000)/test_besch->get_betriebskosten() ? -20 : 20;
					if(  target_weight>0  ) {
						// it is strongerer?
						difference += (besch->get_leistung()*besch->get_gear())/64 < power ? -10 : 10;
					}
					// it is faster? (although we support only up to 120km/h for goods)
					difference += (besch->get_geschw() < test_besch->get_geschw())? -10 : 10;
					// it is cheaper? (not so important)
					difference += (besch->get_preis() > test_besch->get_preis())? -5 : 5;
					// add some malus for obsolete vehicles
					if(test_besch->is_retired(month_now)) {
						difference += 5;
					}
				}
				// ok, final check
				if(  besch==NULL  ||  difference<(int)simrand(25)    ) {
					// then we want this vehicle!
					besch = test_besch;
					DBG_MESSAGE( "vehikelbauer_t::vehikel_search","Found car %s",besch->get_name());
				}
			}

			else {
				// engine/tugboat/truck for trailer
				if(  test_besch->get_zuladung()!=0  ||  !test_besch->can_lead()  ) {
					continue;
				}
				// finally, we might be able to use this vehicle
				uint32 speed = test_besch->get_geschw();
				uint32 max_weight = power/( (speed*speed)/2500 + 1 );

				// we found a useful engine
				long current_index = (power*100)/test_besch->get_betriebskosten() + test_besch->get_geschw() - (sint16)test_besch->get_gewicht() - (sint32)(test_besch->get_preis()/25000);
				// too slow?
				if(speed < target_speed) {
					current_index -= 250;
				}
				// too weak to to reach full speed?
				if(  max_weight < target_weight+test_besch->get_gewicht()  ) {
					current_index += max_weight - (sint32)(target_weight+test_besch->get_gewicht());
				}
				current_index += simrand(100);
				if(  current_index > besch_index  ) {
					// then we want this vehicle!
					besch = test_besch;
					besch_index = current_index;
					DBG_MESSAGE( "vehikelbauer_t::vehikel_search","Found engine %s",besch->get_name());
				}
			}
		}
	}
	// no vehicle found!
	if(  besch==NULL  ) {
		DBG_MESSAGE( "vehikelbauer_t::vehikel_search()","could not find a suitable vehicle! (speed %i, weight %i)",target_speed,target_weight);
	}
	return besch;
}


/* extended search for vehicles for KI *
 * checks also timeline and contraits
 * 
 * @author dwachs
 */
vehikel_prototype_t* vehikelbauer_t::vehikel_search( vehikel_evaluator *eval, karte_t *world,
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
	if(typ_fahrzeuge.access(wt)->empty()) {
		return new vehikel_prototype_t(0);
	}
	// are there engines for front, middle, or tail mode?
	//bool eng_front = false, eng_tail = false;
	// search valid vehicles
	slist_iterator_tpl<const vehikel_besch_t*> vehinfo(typ_fahrzeuge.access(wt));
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
	sint64 best_value = 0x8000000000000000;

	// Iterators for each position
	vector_tpl<slist_iterator_tpl<const vehikel_besch_t*>*> vehicle_loop(max_vehicles);
	// initialize
	vehicle_loop.insert_at(0, new slist_iterator_tpl<const vehikel_besch_t*>(&front_vehicles));
	for (uint8 i=1; i<max_vehicles; i++) {
		vehicle_loop.insert_at(i, new slist_iterator_tpl<const vehikel_besch_t*>(&vehicles));
	}

	// Blacklist
	vector_tpl<const vehikel_besch_t*> blacklist(max_vehicles, NULL);

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
				w *=test_besch->get_zuladung();
				w +=499;
				w /=1000;
				w +=test_besch->get_gewicht();
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
		if ( (steps&8)==0) INT_CHECK("convoi_t* vehikelbauer_t::vehikel_search::2");
	}	
	dbg->message("VBAI", "== Steps: %d", steps);
	dbg->message("VBAI", "== Best : %d", best_value);
	for (uint8 i=0; i<best->besch.get_count(); i++) {
		dbg->message("VBAI", "-- [%2d]: %s", i, best->besch[i]->get_name() );

	}

	delete [] convoi_tpl;

	return best;
}

/* extended search for vehicles for replacement on load time
 * tries to get best match (no random action)
 * if prev_besch==NULL, then the convoi must be able to lead a convoi
 * @author prissi
 */
const vehikel_besch_t *vehikelbauer_t::get_best_matching( waytype_t wt, const uint16 month_now, const uint32 target_weight, const uint32 target_power, const uint32 target_speed, const ware_besch_t * target_freight, bool include_electric, bool not_obsolete, const vehikel_besch_t *prev_veh, bool is_last )
{
	const vehikel_besch_t *besch = NULL;
	long besch_index=-100000;

	if(typ_fahrzeuge.access(wt)) {
		slist_iterator_tpl<const vehikel_besch_t*> vehinfo(typ_fahrzeuge.access(wt));
		while (vehinfo.next()) {
			const vehikel_besch_t* test_besch = vehinfo.get_current();

			if(target_power>0  &&  test_besch->get_leistung()==0) {
				continue;
			}

			// will test for first (prev_veh==NULL) or matching following vehicle
			if(!test_besch->can_follow(prev_veh)) {
				continue;
			}

			// not allowed as last vehicle
			if(is_last  &&  test_besch->get_nachfolger_count()>0  &&  test_besch->get_nachfolger(0)!=NULL  ) {
				continue;
			}

			// not allowed as non-last vehicle
			if(!is_last  &&  test_besch->get_nachfolger_count()==1  &&  test_besch->get_nachfolger(0)==NULL  ) {
				continue;
			}

			// check for wegetype/too new
			if(test_besch->get_waytype()!=wt  ||  test_besch->is_future(month_now)  ) {
				continue;
			}

			// likely tender => replace with some engine ...
			if(target_freight==0  &&  target_weight==0) {
				if(  test_besch->get_zuladung()!=0  ) {
					continue;
				}
			}

			if(  not_obsolete  &&  test_besch->is_retired(month_now)  ) {
				// not using vintage cars here!
				continue;
			}

			uint32 power = (test_besch->get_leistung()*test_besch->get_gear())/64;
			if(target_freight) {
				// this is either a railcar/trailer or a truck/boat/plane
				if(  test_besch->get_zuladung()==0  ||  !test_besch->get_ware()->is_interchangeable(target_freight)  ) {
					continue;
				}

				sint32 difference=0;	// smaller is better
				// assign this vehicle, if we have none found one yet, or we found only a too week one
				if(  besch!=NULL  ) {
					// it is cheaper to run? (this is most important)
					difference += (besch->get_zuladung()*1000)/besch->get_betriebskosten() < (test_besch->get_zuladung()*1000)/test_besch->get_betriebskosten() ? -20 : 20;
					if(  target_weight>0  ) {
						// it is strongerer?
						difference += (besch->get_leistung()*besch->get_gear())/64 < power ? -10 : 10;
					}
					// it is faster? (although we support only up to 120km/h for goods)
					difference += (besch->get_geschw() < test_besch->get_geschw())? -10 : 10;
					// it is cheaper? (not so important)
					difference += (besch->get_preis() > test_besch->get_preis())? -5 : 5;
					// add some malus for obsolete vehicles
					if(test_besch->is_retired(month_now)) {
						difference += 5;
					}
				}
				// ok, final check
				if(  besch==NULL  ||  difference<12    ) {
					// then we want this vehicle!
					besch = test_besch;
					DBG_MESSAGE( "vehikelbauer_t::get_best_matching","Found car %s",besch->get_name());
				}
			}
			else {
				// finally, we might be able to use this vehicle
				uint32 speed = test_besch->get_geschw();
				uint32 max_weight = power/( (speed*speed)/2500 + 1 );

				// we found a useful engine
				long current_index = (power*100)/test_besch->get_betriebskosten() + test_besch->get_geschw() - (sint16)test_besch->get_gewicht() - (sint32)(test_besch->get_preis()/25000);
				// too slow?
				if(speed < target_speed) {
					current_index -= 250;
				}
				// too weak to to reach full speed?
				if(  max_weight < target_weight+test_besch->get_gewicht()  ) {
					current_index += max_weight - (sint32)(target_weight+test_besch->get_gewicht());
				}
				current_index += 50;
				if(  current_index > besch_index  ) {
					// then we want this vehicle!
					besch = test_besch;
					besch_index = current_index;
					DBG_MESSAGE( "vehikelbauer_t::get_best_matching","Found engine %s",besch->get_name());
				}
			}
		}
	}
	// no vehicle found!
	if(  besch==NULL  ) {
		DBG_MESSAGE( "vehikelbauer_t::get_best_matching()","could not find a suitable vehicle! (speed %i, weight %i)",target_speed,target_weight);
	}
	return besch;
}
