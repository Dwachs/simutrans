
/* standard good AI code */

#include "../simtypes.h"

#include "simplay.h"

#include "../simhalt.h"
#include "../simmesg.h"
#include "../simtools.h"
#include "../simworld.h"
#include "../simwerkz.h"

#include "../bauer/brueckenbauer.h"
#include "../bauer/hausbauer.h"
#include "../bauer/tunnelbauer.h"
#include "../bauer/vehikelbauer.h"
#include "../bauer/wegbauer.h"

#include "../dataobj/loadsave.h"

#include "../dings/wayobj.h"

#include "../utils/simstring.h"

#include "../vehicle/simvehikel.h"

#include "ai_dw.h"
#include "ai_dw_tasks.h"
#include "ai_dw_factory.h"
#include "ai_dw_road.h"


/************************************************************************/
/* The AI */
ai_dw_t::ai_dw_t(karte_t *wl, uint8 nr) : ai_t(wl,nr)
{
	char logfile[20];
	sprintf(logfile, "dwai_%d.log", nr);
	log = new log_t(logfile, true, true);

	queue.append(new ait_fct_init_t(this) );
}




/**
 * Methode fuer jaehrliche Aktionen
 * @author Hj. Malthaner, Owen Rudge, hsiegeln
 */
void ai_dw_t::neues_jahr()
{
	spieler_t::neues_jahr();

	// AI will reconsider the oldes unbuiltable lines again
	uint remove = (uint)max(0,(int)forbidden_connections.get_count()-3);
	while(  remove < forbidden_connections.get_count()  ) {
		forbidden_connections.remove_first();
	}
}



void ai_dw_t::rotate90( const sint16 y_size )
{
	spieler_t::rotate90( y_size );

	queue.rotate90(y_size);
}



/* Activates/deactivates a player
 * @author prissi
 */
bool ai_dw_t::set_active(bool new_state)
{
	// something to change?
	if(automat!=new_state) {

		if(!new_state) {
			// deactivate AI
			automat = false;
		}
		else {
			// aktivate AI
			automat = true;
		}
	}
	return automat;
}






// the normal length procedure for freigth AI
void ai_dw_t::step()
{
	// needed for schedule of stops ...
	spieler_t::step();

	if(!automat) {
		// I am off ...
		return;
	}

	ai_task_t *ait = queue.get_first( welt->get_steps() );

	if (ait) {
		ait->work(queue);
	}

	if (queue.is_empty()) {
		ai_task_t *ait = new ait_fct_init_t(this);
		ait->set_time( welt->get_steps() + simrand( 8000 )+1000);
		queue.append(ait );
		log->message("step", "appended ait_fct_init_t time=%d", ait->get_time());
	}
}



void ai_dw_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "ai_dw_t" );

	// first: do all the administration
	spieler_t::rdwr(file);

	// the queue
	queue.rdwr(file, this);

	// finally: forbidden connection list
	sint32 cnt = forbidden_connections.get_count();
	file->rdwr_long(cnt,"Fc");
	if(file->is_saving()) {
		slist_iterator_tpl<fabconnection_t> iter(forbidden_connections);
		while(  iter.next()  ) {
			fabconnection_t fc = iter.get_current();
			fc.rdwr(file);
		}
	}
	else {
		forbidden_connections.clear();
		while(  cnt-->0  ) {
			fabconnection_t fc(0,0,0);
			fc.rdwr(file);
			forbidden_connections.append(fc);
		}
	}
}




void ai_dw_t::fabconnection_t::rdwr(loadsave_t *file)
{
	koord3d k3d;
	if(file->is_saving()) {
		k3d = fab1->get_pos();
		k3d.rdwr(file);
		k3d = fab2->get_pos();
		k3d.rdwr(file);
		const char *s = ware->get_name();
		file->rdwr_str( s );
	}
	else {
		k3d.rdwr(file);
		fab1 = fabrik_t::get_fab( welt, k3d.get_2d() );
		k3d.rdwr(file);
		fab2 = fabrik_t::get_fab( welt, k3d.get_2d() );
		const char *temp=NULL;
		file->rdwr_str( temp );
		ware = warenbauer_t::get_info(temp);
	}
}




/**
 * Dealing with stucked  or lost vehicles:
 * - delete lost ones
 * - ignore stucked ones
 * @author prissi
 * @date 30-Dec-2008
 */
void ai_dw_t::bescheid_vehikel_problem(convoihandle_t cnv,const koord3d ziel)
{
	switch(cnv->get_state()) {

		case convoi_t::NO_ROUTE:
DBG_MESSAGE("ai_dw_t::bescheid_vehikel_problem","Vehicle %s can't find a route to (%i,%i)!", cnv->get_name(),ziel.x,ziel.y);
			if(this==welt->get_active_player()) {
				char buf[256];
				sprintf(buf,translator::translate("Vehicle %s can't find a route!"), cnv->get_name());
				welt->get_message()->add_message(buf, cnv->get_pos().get_2d(),message_t::convoi,PLAYER_FLAG|player_nr,cnv->get_vehikel(0)->get_basis_bild());
			}
			else {
				cnv->self_destruct();
			}
			break;

		case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:
		case convoi_t::CAN_START_ONE_MONTH:
DBG_MESSAGE("ai_dw_t::bescheid_vehikel_problem","Vehicle %s stucked!", cnv->get_name(),ziel.x,ziel.y);
			if(this==welt->get_active_player()) {
				char buf[256];
				sprintf(buf,translator::translate("Vehicle %s is stucked!"), cnv->get_name());
				welt->get_message()->add_message(buf, cnv->get_pos().get_2d(),message_t::convoi,PLAYER_FLAG|player_nr,cnv->get_vehikel(0)->get_basis_bild());
			}
			break;

		default:
DBG_MESSAGE("ai_dw_t::bescheid_vehikel_problem","Vehicle %s, state %i!", cnv->get_name(), cnv->get_state());
	}
}

/**
 * evaluate a convoi suggested by vehicle bauer
 * @author Dwachs
 * /
sint64 ai_dw_t::valuate(const vehikel_prototype_t &proto) {
	sint32 value =0; 
	uint16 capacity = 0;
	uint16 maintenance = 0;
	// calculate capacity and maintenance
	for(uint8 i=0; i<proto.besch.get_count(); i++) {
		const vehikel_besch_t* besch = proto.besch[i];
		if (besch) {
			// compatible freights?			
			if(besch->get_ware()->get_catg_index()==freight->get_catg_index()) {
				capacity += besch->get_zuladung();
			}
			maintenance += besch->get_betriebskosten();
		}
	}
	sint64 net_gain = ((sint64)(1000 * capacity * freight->get_preis() - maintenance *3)) * (proto.max_speed*3 /4);

	return net_gain; 
}*/

/**
 * returns an instance of a task
 * @author Dwachs
 */
ai_task_t *ai_dw_t::alloc_task(uint16 type) {
	switch(type) {
		case ai_task_t::FACT_SEARCH_TREE: return(new ait_fct_init_t(this));		// init search of factory trees 
		case ai_task_t::FACT_SEARCH_MISS: return(new ait_fct_find_t(this));		// search for missing suppliers
		case ai_task_t::FACT_COMPLT_TREE: return(new ait_fct_complete_tree_t(this));	// complete one factory tree

		case ai_task_t::ROAD_CONNEC_FACT: return(new ait_road_connectf_t(this));
		default: 
			assert(0);
	}
	return(NULL);
}
