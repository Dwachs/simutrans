#include "connector_road.h"

#include "connector_generic.h"
#include "builder_road_station.h"
#include "builder_way_obj.h"
#include "free_tile_searcher.h"
#include "vehikel_builder.h"

#include "../bt.h"
#include "../datablock.h"
#include "../vehikel_prototype.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../simhalt.h"
#include "../../../simmenu.h"
#include "../../../simmesg.h"
#include "../../../bauer/brueckenbauer.h"
#include "../../../bauer/hausbauer.h"
#include "../../../bauer/tunnelbauer.h"
#include "../../../bauer/wegbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/loadsave.h"

enum connector_road_phases {
	CREATE_SCHEDULE = 0,
	WAIT_FOR_VEHICLES= 1
};

connector_road_t::connector_road_t( ai_wai_t *sp, const char *name) :
	bt_sequential_t( sp, name), fab1(NULL, sp), fab2(NULL, sp)
{
	type = BT_CON_ROAD;
	road_besch = NULL;
	prototyper = NULL;
	nr_vehicles = 0;
	phase = CREATE_SCHEDULE;
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	harbour_pos = koord3d::invalid;
}

connector_road_t::connector_road_t( ai_wai_t *sp, const char *name, const fabrik_t *fab1_, const fabrik_t *fab2_, const weg_besch_t *road_besch_, simple_prototype_designer_t *d, uint16 nr_veh, const koord3d harbour_pos_ ) :
	bt_sequential_t( sp, name ), fab1(fab1_, sp), fab2(fab2_, sp)
{
	type = BT_CON_ROAD;
	road_besch = road_besch_;
	prototyper = d;
	nr_vehicles = nr_veh;
	phase = CREATE_SCHEDULE;
	start = koord3d::invalid;
	ziel = koord3d::invalid;
	harbour_pos = harbour_pos_;

	koord3d start_pos = fab1->get_pos();
	if( harbour_pos != koord3d::invalid ) {
		const grund_t *gr = sp->get_welt()->lookup(harbour_pos);
		start_pos = harbour_pos + koord(gr->get_grund_hang()) + koord3d(0,0,1);
	}
	append_child(new connector_generic_t(sp, "connector_generic(road)", start_pos, fab2->get_pos(), road_besch));
}

connector_road_t::~connector_road_t()
{
	if (prototyper && phase<=2) delete prototyper;
	prototyper = NULL;
}

void connector_road_t::rdwr( loadsave_t *file, const uint16 version )
{
	bt_sequential_t::rdwr( file, version );
	file->rdwr_byte(phase);
	fab1.rdwr(file, version, sp);
	fab2.rdwr(file, version, sp);
	ai_t::rdwr_weg_besch(file, road_besch);
	file->rdwr_short(nr_vehicles);
	if (phase<=CREATE_SCHEDULE) {
		if (file->is_loading()) {
			prototyper = new simple_prototype_designer_t(sp);
		}
		prototyper->rdwr(file);
	}
	else {
		prototyper = NULL;
	}
	start.rdwr(file);
	ziel.rdwr(file);
	deppos.rdwr(file);
	harbour_pos.rdwr(file);
}

void connector_road_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);
	start.rotate90(y_size);
	ziel.rotate90(y_size);
	deppos.rotate90(y_size);
	harbour_pos.rotate90(y_size);
}

return_value_t *connector_road_t::step()
{
	if(!fab1.is_bound()  ||  !fab2.is_bound()) {	
		sp->get_log().warning("connector_road_t::step", "%s %s disappeared", fab1.is_bound() ? "" : "start", fab2.is_bound() ? "" : "ziel");
		return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
	}
	if( childs.empty() ) {

		datablock_t *data = NULL;
		sp->get_log().message("connector_road_t::step", "phase %d of build route %s => %s", phase, fab1->get_name(), fab2->get_name() );
		
		switch(phase) {
			case CREATE_SCHEDULE: {
				// create line
				schedule_t *fpl=new autofahrplan_t();

				// full load? or do we have unused capacity?
				const uint8 ladegrad = ( 100*prototyper->proto->get_capacity(prototyper->freight) )/ prototyper->proto->get_capacity(NULL);

				fpl->append(sp->get_welt()->lookup(start), ladegrad);
				fpl->append(sp->get_welt()->lookup(ziel), 0);
				fpl->set_aktuell( 0 );
				fpl->eingabe_abschliessen();

				linehandle_t line=sp->simlinemgmt.create_line(simline_t::truckline, sp, fpl);
				delete fpl;

				append_child( new vehikel_builder_t(sp, "vehikel builder", prototyper, line, deppos, min(nr_vehicles,3) ) );
				
				// tell the player
				char buf[256];
				sprintf(buf, translator::translate("%s\nnow operates\n%i trucks between\n%s at (%i,%i)\nand %s at (%i,%i)."), sp->get_name(), nr_vehicles, translator::translate(fab1->get_name()), start.x, start.y, translator::translate(fab2->get_name()), ziel.x, ziel.y);
				sp->get_welt()->get_message()->add_message(buf, start.get_2d(), message_t::ai, PLAYER_FLAG|sp->get_player_nr(), prototyper->proto->besch[0]->get_basis_bild());

				// tell the industrymanager
				data = new datablock_t();
				data->line = line;

				// reset prototyper, will be deleted in vehikel_builder
				prototyper = NULL;
				break;
			}
			default: 
				break;
		}
		sp->get_log().message( "connector_road_t::step", "completed phase %d", phase);
		return_value_t *rv = new_return_value(phase>2 ? RT_TOTAL_SUCCESS : RT_PARTIAL_SUCCESS);
		rv->data = data;

		phase ++; 
		return rv;
	}
	else {
		// Step the childs.
		return_value_t *rv = bt_sequential_t::step();
		if (rv->type == BT_CON_GENERIC) {
			if (rv->is_ready() &&  rv->data  &&  rv->data->pos1.get_count()>2) {
				start  = rv->data->pos1[0];
				ziel   = rv->data->pos1[1];
				deppos = rv->data->pos1[2];
				phase = CREATE_SCHEDULE;
				delete rv;
				return new_return_value( RT_PARTIAL_SUCCESS );
			}
			else if (rv->is_failed()) {
				sp->get_log().warning("connector_road_t::step", "connector_generic failed");
				delete rv;
				return new_return_value(RT_TOTAL_FAIL); // .. to kill this instance
			}
		}
		return rv;
	}
}

void connector_road_t::debug( log_t &file, string prefix )
{
	bt_sequential_t::debug(file, prefix);
	file.message("conr","%s phase=%d", prefix.c_str(), phase);
	if (prototyper && phase<=CREATE_SCHEDULE) prototyper->debug(file, string( prefix + " prototyp"));
}
