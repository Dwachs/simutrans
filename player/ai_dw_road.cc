#include "ai_dw_road.h"
#include "ai_dw.h"

#include "../simworld.h"
#include "../simfab.h"
#include "../simhalt.h"
#include "../simmesg.h"
#include "../bauer/hausbauer.h"
#include "../bauer/warenbauer.h"
#include "../bauer/wegbauer.h"
#include "../besch/vehikel_besch.h"

#include "../tpl/vector_tpl.h"

void ait_road_connectf_t::work(ai_task_queue_t &queue) 
{
	if (start==NULL || ziel==NULL || freight==NULL) {
		sp->log->warning("ait_road_connectf_t::work", "lost data");
		state=READY;
	}
	else {
		char buf[40]; sprintf(buf, "%s",start->get_pos().get_str());
		sp->log->message("ait_road_connectf_t::work", "[time = %d] transport %s from %s(%s) to %s(%s)", get_time(), freight->get_name(), 
			start->get_name(), buf, ziel->get_name(), ziel->get_pos().get_str());
	}
	switch(state) {
		case SEARCH_PLACES: 
			search_places(); break;
		case CALC_PROD:
			calc_production(); break;
		case CALC_VEHIKEL:
			calc_vehicle(); break;
		case CALC_ROAD:
			calc_road(); break;
		case BUILD_ROAD:
			build_road(); break;
		case BUILD_DEPOT:
			build_depot(); break;
		case CREATE_LINE:
			create_line();
			// tell the player
			{
				char buf[256];
				const koord3d& spos = start->get_pos();
				const koord3d& zpos = ziel->get_pos();
				sprintf(buf, translator::translate("%s\nnow operates\n%i trucks between\n%s at (%s)\nand %s at (%s)."), sp->get_name(), count_road, 
					translator::translate(start->get_name()), spos.get_str(), translator::translate(ziel->get_name()), zpos.get_str());
				sp->get_welt()->get_message()->add_message(buf, spos.get_2d(), message_t::ai, PLAYER_FLAG| sp->get_player_nr(), proto->besch[0]->get_basis_bild());
			}
			break;
		case BUILD_VEHICLES:			
			build_vehicles(); break;
		case _ERROR:
			sp->log->warning("ait_road_connectf_t::work", "error encountered", state, get_time());
			if (start!=NULL  && ziel!=NULL && freight!=NULL) {
				sp->log->warning("ait_road_connectf_t::work", "forbid this connection");
				sp->add_forbidden(start, ziel, freight);
			}
		case READY:
			clean_markers(); 
			if (proto) {
				delete proto;
				proto = NULL;
			}
			sp->log->message("ait_road_connectf_t::work", "ready.", state, get_time());
			return;
		default:
			sp->log->warning("ait_road_connectf_t::work", "invalid state: %d", state);
			return;
	}

	set_time( get_time() + 2);
	queue.append(this);
	sp->log->message("ait_road_connectf_t::work", "new state=%d, new time=%d", state, get_time());
}

void ait_road_connectf_t::clean_markers() {
	if (size1.x+size1.y > 0) {
		sp->clean_marker(place1,size1);
	}
	if (size2.x+size2.y > 0) {
		sp->clean_marker(place2,size2);
	}
}
/* @from ai_goods */
void ait_road_connectf_t::search_places() 
{
	// factories in water not yet supported
	if(start->get_besch()->get_platzierung()==fabrik_besch_t::Wasser || ziel->get_besch()->get_platzierung()==fabrik_besch_t::Wasser) {
		sp->log->warning("ait_road_connectf_t::search_places", "no factories at water side supported");
		state = _ERROR;
		return;
	}
	int length = 0;
	place1 = koord( start->get_pos().get_2d() );
	size1  = koord( length, 0 );
	place2 = koord( ziel->get_pos().get_2d() );
	size2  = koord( length, 0 );

	bool ok = sp->suche_platz(place1, size1, place2, start->get_besch()->get_haus()->get_groesse(start->get_rotate()) );
	if(ok) {
		// found a place, search for target
		ok = sp->suche_platz(place2, size2, place1, ziel->get_besch()->get_haus()->get_groesse(ziel->get_rotate()) );
	}

	if( !ok ) {
		// keine Bauplaetze gefunden
		sp->log->warning( "ait_road_connectf_t::search_places", "no suitable locations found at (%s):(%s) and (%s):(%s)", place1.get_str(), size1.get_str(), place2.get_str(), size2.get_str());
		state = _ERROR;
	}
	else {
		sp->log->message( "ait_road_connectf_t::search_places", "platz1=%d,%d platz2=%d,%d", place1.x, place1.y, place2.x, place2.y );

		// reserve space with marker
		sp->set_marker( place1, size1 );
		sp->set_marker( place2, size2 );
		
		state = CALC_PROD;
	}
}

/* @from ai_goods */
void ait_road_connectf_t::calc_production() 
{
	karte_t *welt = sp->get_welt();
	// calculate distance	
	koord zv = start->get_pos().get_2d() - ziel->get_pos().get_2d();
	dist = abs(zv.x) + abs(zv.y);

	// properly calculate production & consumption
	const vector_tpl<ware_production_t>& ausgang = start->get_ausgang();
	uint start_ware=0;
	while(  start_ware<ausgang.get_count()  &&  ausgang[start_ware].get_typ()!=freight  ) {
		start_ware++;
	}
	const vector_tpl<ware_production_t>& eingang = ziel->get_eingang();
	uint ziel_ware=0;
	while(  ziel_ware<eingang.get_count()  &&  eingang[ziel_ware].get_typ()!=freight  ) {
		ziel_ware++;
	}
	const uint8  shift = 8 - welt->ticks_bits_per_tag +10 +8; // >>(simfab) * welt::monatslaenge /PRODUCTION_DELTA_T +dummy
	const sint32 prod_z = (ziel->get_base_production()  *  ziel->get_besch()->get_lieferant(ziel_ware)->get_verbrauch() )>>shift;
	const sint32 prod_s = ((start->get_base_production() * start->get_besch()->get_produkt(start_ware)->get_faktor())>> shift) - start->get_abgabe_letzt(start_ware);
	prod = min(  prod_z,prod_s);	
	sp->log->message( "ait_road_connectf_t::calc_production", "estimated production / month= %d", prod);

	if(prod<0) {
		// too much supplied last time?!? => retry
		state = _ERROR;
	}
	else {
		state = CALC_VEHIKEL;
	}
}

/* @author dwachs */
void ait_road_connectf_t::calc_vehicle() 
{
// find the best road convoi
	slist_tpl<const ware_besch_t*> freights; freights.append(freight);

	proto = vehikelbauer_t::vehikel_search( this, sp->get_welt(), road_wt, 1, 1, 0xffffffff, freights, true, false);
	if (proto && !proto->is_empty()) {
		state = CALC_ROAD;
		sp->log->message( "ait_road_connectf_t::calc_vehicle", "found prototype");	
		sp->log->message("ait_road_connectf_t::calc_vehicle", "Weight       =%d", proto->weight);
		sp->log->message("ait_road_connectf_t::calc_vehicle", "Power        =%d", proto->power); 
		sp->log->message("ait_road_connectf_t::calc_vehicle", "Min.Topspeed =%d", speed_to_kmh(proto->min_top_speed)); 
		sp->log->message("ait_road_connectf_t::calc_vehicle", "Length       =%d", proto->length); 
		sp->log->message("ait_road_connectf_t::calc_vehicle", "Max.Speed    =%d", proto->max_speed);
		sp->log->message("ait_road_connectf_t::calc_vehicle", "Est.Netgain  =%d", proto->value);
		for (uint8 i=0; i<proto->besch.get_count(); i++) {
			sp->log->message("ait_road_connectf_t::calc_vehicle", "-- [%2d]: %s", i, proto->besch[i]->get_name() );
		}
	}
	else {
		state = _ERROR;
		sp->log->warning( "ait_road_connectf_t::calc_vehicle", "can't find prototype");
	}
}

// evaluate a convoi suggested by vehicle bauer
sint64 ait_road_connectf_t::valuate(const vehikel_prototype_t &proto) {
	if (proto.is_empty()) return 0x8000000000000000;

	const uint32 capacity = proto.get_capacity(freight);
	const uint32 maintenance = proto.get_maintenance();

	// simple net gain
	//sint64 net_gain = ((sint64)( (capacity * (freight->get_preis()<<7)) / 3000ll - maintenance *3)) * (proto.max_speed*3) /4;

	// speed bonus calculation see vehikel_t::calc_gewinn
	const sint32 ref_speed = sp->get_welt()->get_average_speed(road_wt );
	const sint32 speed_base = (100*speed_to_kmh(proto.min_top_speed))/ref_speed-100;
	const sint32 freight_price = freight->get_preis() * max( 128, 1000+speed_base*freight->get_speed_bonus());

	// net gain
	//const sint64 value = ((capacity * freight_price +1500ll )/3000ll - maintenance *3) * (proto.max_speed*3) /4;

	// net gain per transported unit (matching freight)
	const sint64 value = (((capacity * freight_price +1500ll )/3000ll - maintenance *3) * (proto.max_speed*3) /4 *1000) / capacity;

	for(uint8 i=0;i<proto.besch.get_count(); i++)
		sp->log->message("ait_road_connectf_t::valuate", "[%d] %s",i,proto.besch[i]->get_name() );
	sp->log->message("ait_road_connectf_t::valuate", "cap=%d/%d maint=%d speed=%d freightprice=%d value=%ld ",
		capacity, proto.get_capacity(NULL), maintenance, proto.max_speed, freight_price, value);
	return value; 
}


/* @from ai_goods */
void ait_road_connectf_t::calc_road() 
{
	if(proto && !proto->is_empty()) {
		uint32 best_road_speed = proto->max_speed;
		// find cheapest road
		weg = wegbauer_t::weg_search( road_wt, best_road_speed, sp->get_welt()->get_timeline_year_month(),weg_t::type_flat );
		if(  weg!=NULL  ) {
			sp->log->message( "ait_road_connectf_t::calc_road","take road %s", weg->get_name() );

			if(  best_road_speed>weg->get_topspeed()  ) {
				best_road_speed = weg->get_topspeed();
			}
			int tiles_per_month = (kmh_to_speed(best_road_speed) * sp->get_welt()->ticks_per_tag) >> (8+12); // 12: fahre_basis, 8: 2^8 steps per tile
			count_road = min( 254, (2*prod*dist) / (proto->get_capacity(freight)*tiles_per_month)+1 );

			// minimum vehicle is 1, maximum vehicle is 48, more just result in congestion
			//count_road = min( 254, (prod*dist) / (road_prototype->get_capacity(freight)*best_road_speed*2)+1 );
			count_road = min( count_road, dist);
			sp->log->message( "ait_road_connectf_t::calc_road","first guess: %d convois needed (distance %d)", count_road, dist);

			state = BUILD_ROAD;
		}
		else {
			// no roads there !?!
			state = _ERROR;
		}
	}
}


void ait_road_connectf_t::build_road() 
{	
	// TODO: kontostand checken, terminal/durchgangsstation
	const haus_besch_t* fh = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, sp->get_welt()->get_timeline_year_month(), haltestelle_t::WARE);

	bool ok = (fh != NULL);
	
	if (ok) {
		ok = sp->create_simple_road_transport(place1, size1, place2, size2, weg);
		if (ok) {
			sp->log->message( "ait_road_connectf_t::build_road","build stations %s", fh->get_name());
			ok = sp->call_general_tool(WKZ_STATION, place1, fh->get_name());
			if (ok) {
				ok = sp->call_general_tool(WKZ_STATION, place2, fh->get_name());
				if (!ok) 
					sp->log->warning( "ait_road_connectf_t::build_road", "station construction at %s failed", place2.get_str());
			} 
			else 
				sp->log->warning( "ait_road_connectf_t::build_road", "station construction at %s failed", place1.get_str());
		}
		else
			sp->log->warning( "ait_road_connectf_t::build_road", "road construction from %s to %s failed", place1.get_str(), place2.get_str());
	}
	else
		sp->log->warning( "ait_road_connectf_t::build_road", "no station found");

	state = ok ? BUILD_DEPOT : _ERROR;
}
void ait_road_connectf_t::build_depot() 
{	
	karte_t *const welt = sp->get_welt();

	uint8 start_location=0;
	// sometimes, when factories are very close, we need exakt calculation
	// @from ai_goods
	const koord3d& qpos = start->get_pos();
	if ((qpos.x - place1.x) * (qpos.x - place1.x) + (qpos.y - place1.y) * (qpos.y - place1.y) >
			(qpos.x - place2.x) * (qpos.x - place2.x) + (qpos.y - place2.y) * (qpos.y - place2.y)) {
		start_location = 1;
	}
	// calculate vehicle start position
	start_pos=(start_location==0)? welt->lookup(place1)->get_kartenboden()->get_pos() : welt->lookup(place2)->get_kartenboden()->get_pos();
	ribi_t::ribi w_ribi = welt->lookup(start_pos)->get_weg_ribi_unmasked(road_wt);
	// now start all vehicle one field before, so they load immediately
	start_pos = welt->lookup(koord(start_pos.get_2d())+koord(w_ribi))->get_kartenboden()->get_pos();

	// built depot
	const haus_besch_t* dep = hausbauer_t::get_random_station(haus_besch_t::depot, road_wt, welt->get_timeline_year_month(), 0);
	bool ok = dep;
	if (dep) {
		wegbauer_t bauigel(welt, sp);
		bauigel.route_fuer( (wegbauer_t::bautyp_t)(wegbauer_t::strasse | wegbauer_t::depot_flag), weg, NULL, NULL);

		// we won't destroy cities (and save the money)
		bauigel.set_keep_existing_faster_ways(true);
		bauigel.set_keep_city_roads(true);
		bauigel.set_maximum(10000);

		INT_CHECK("simplay 846");

		bauigel.calc_route(start_pos, start_pos+koord(w_ribi));
				//sp->log->message( "ait_road_connectf_t::build_depot","start search at (%s)", start_pos.get_str());
				//sp->log->message( "ait_road_connectf_t::build_depot",".. in direction (%s)", (start_pos+koord(w_ribi)).get_str());
		koord deppos;
		if(bauigel.max_n > 0) {
			deppos = bauigel.get_route_bei(0).get_2d();
			bauigel.baue();

			ok = true;
			sp->log->message( "ait_road_connectf_t::build_depot","found depot location (%s)", deppos.get_str());
		} 
		else {
			ok = false;
			sp->log->warning( "ait_road_connectf_t::build_depot","no route found to depot location");
		}
		// build depot
		if (ok){
			depot_t *dp = welt->lookup_kartenboden(deppos)->get_depot();
			if (!dp) {
				ok = sp->call_general_tool(WKZ_DEPOT, deppos, dep->get_name());
				if (ok) {
					// start convois in depot
					start_pos = welt->lookup_kartenboden(deppos)->get_pos();
				}
				else {
					sp->log->warning( "ait_road_connectf_t::build_depot","build depot failed at %s", welt->lookup_kartenboden(deppos)->get_pos().get_str());
				}
			}
		}
	}else {
		ok = false;
		sp->log->warning( "ait_road_connectf_t::build_depot","no depotbesch found");
	}
	state = ok ? CREATE_LINE : _ERROR;
}

void ait_road_connectf_t::create_line() 
{
	uint8 start_location=0;
	// sometimes, when factories are very close, we need exakt calculation
	// @from ai_goods
	const koord3d& qpos = start->get_pos();
	if ((qpos.x - place1.x) * (qpos.x - place1.x) + (qpos.y - place1.y) * (qpos.y - place1.y) >
			(qpos.x - place2.x) * (qpos.x - place2.x) + (qpos.y - place2.y) * (qpos.y - place2.y)) {
		start_location = 1;
	}
	
	karte_t *const welt = sp->get_welt();
	// create line
	schedule_t *fpl=new autofahrplan_t();

	// full load? or do we have unused capacity?
	const uint8 ladegrad = ( 100*proto->get_capacity(freight) )/ proto->get_capacity(NULL);

	fpl->append(welt->lookup(place1)->get_kartenboden(), start_location == 0 ? ladegrad : 0);
	fpl->append(welt->lookup(place2)->get_kartenboden(), start_location == 1 ? ladegrad : 0);
	fpl->set_aktuell( start_location );
	fpl->eingabe_abschliessen();

	line=sp->simlinemgmt.create_line(simline_t::truckline, sp, fpl);
	delete fpl;

	state= BUILD_VEHICLES;
}

void ait_road_connectf_t::build_vehicles() 
{
	if (count_road>0 && line.is_bound()) {
		convoi_t* cnv = vehikelbauer_t::baue(start_pos, sp, proto);

		sp->get_welt()->sync_add( cnv );
		cnv->set_line(line);
		cnv->start();

		count_road --;	

		set_time(get_time()+31);
	}
	else {
		state = READY;
	}
}

void ait_road_connectf_t::rdwr(loadsave_t* file) 
{
	ai_task_t::rdwr(file); 

	const char *s;
	bool proto_ok=true;
	file->rdwr_byte(state," ");

	// save/ load backward
	switch(state) {
		case _ERROR:
		case READY: 
		case BUILD_VEHICLES:
			uint16 line_id;
			if (file->is_saving()) {
				line_id = line.is_bound() ? line->get_line_id() : INVALID_LINE_ID;
			}
			file->rdwr_short(line_id, " ");
			if (file->is_loading()) {
				if (line_id != INVALID_LINE_ID) {
					line = sp->simlinemgmt.get_line_by_id(line_id);
				} 
				else {
					line = linehandle_t();
				}
			}
		case CREATE_LINE:
			start_pos.rdwr(file);
		case BUILD_DEPOT:
		case BUILD_ROAD:
			file->rdwr_short(count_road, " ");
			if (file->is_saving()) {
				s = weg->get_name();
			}
			file->rdwr_str(s);
			if (file->is_loading()) {
				weg = wegbauer_t::get_besch(s, 0);
			}

		case CALC_ROAD: {
			// the vehikel_prototype
			if (file->is_loading()) {
				proto = new vehikel_prototype_t();
			}

			file->rdwr_long( proto->weight, " ");
			file->rdwr_long( proto->power, " "); 
			file->rdwr_short(proto->min_top_speed, " "); 
			file->rdwr_short(proto->length, " "); 
			file->rdwr_long( proto->max_speed, " ");
			file->rdwr_byte( proto->missing_freights, " ");

			uint32 count = proto->besch.get_count();
			file->rdwr_long( count, " ");
			for(uint32 i=0; i<count; i++) {
				if (file->is_saving()) {
					s = proto->besch[i]->get_name();
				}
				file->rdwr_str( s );
				if (file->is_loading()) {
					const vehikel_besch_t *b = vehikelbauer_t::get_info(s);
					proto->besch.append(b);
					if (b==NULL) proto_ok=false;
				}
			}
						}
		case CALC_VEHIKEL:;
			// production
			file->rdwr_long(dist, " ");
			file->rdwr_long(prod, " ");
		case CALC_PROD:;
			place1.rdwr(file);
			size1.rdwr(file);
			place2.rdwr(file);
			size2.rdwr(file);
		case SEARCH_PLACES:
		default: koord pos[2];
			// factories & freight
			if (file->is_saving()) { // save only position
				pos[0] = start->get_pos().get_2d();
				pos[1] = ziel ->get_pos().get_2d();
				s =  freight->get_name();
			}
			pos[0].rdwr(file);
			pos[1].rdwr(file);
			file->rdwr_str( s );

			if (file->is_loading())
			{
				start = fabrik_t::get_fab(sp->get_welt(), pos[0]);
				ziel  = fabrik_t::get_fab(sp->get_welt(), pos[1]);
				freight = s ? warenbauer_t::get_info(s) : NULL;
			}
	}

	// errors while loading?
	if (file->is_loading()) {
		if (!line.is_bound() && state>CREATE_LINE) {
			state = CREATE_LINE;
		}
		if (weg==NULL && state>CALC_ROAD) {
			state = CALC_ROAD;
		}
		if (!proto_ok && state>CALC_VEHIKEL) {
			delete proto; 
			proto = NULL;
			state=CALC_VEHIKEL;
		}		
		if (start==NULL || ziel==NULL || freight==NULL) {
			state = READY;
		}
	}
}
