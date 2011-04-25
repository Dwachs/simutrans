#include "ai_wai.h"

#include "wai/nodes/industry_manager.h"
#include "wai/nodes/factory_searcher.h"
#include "wai/utils/wrapper.h"

#include "../dataobj/loadsave.h"

static const char *names [16] =
{
	"wai0.log",
	"wai1.log",
	"wai2.log",
	"wai3.log",
	"wai4.log",
	"wai5.log",
	"wai6.log",
	"wai7.log",
	"wai8.log",
	"wai9.log",
	"wai10.log",
	"wai11.log",
	"wai12.log",
	"wai13.log",
	"wai14.log",
	"wai15.log"
};

ai_wai_t::~ai_wai_t()
{
}

void ai_wai_t::step()
{
	ai_t::step();

	if( !automat ) {
		// This ai is turned off.
		return;
	};

	if (bt_root.get_count()==0) {
		factory_searcher = new factory_searcher_t(this, "fac search");
		bt_root.append_child( factory_searcher );

		industry_manager = new industry_manager_t(this, "ind manager");
		bt_root.append_child(industry_manager);
		return;
	}
	log.message("", "");
	log.message("ai_wai_t::step", "next step:");
	return_value_t * rv = bt_root.step();
	delete rv;


	if(  ( get_welt()->get_steps() & 31 ) == 0 ) {
		report_t* report = factory_searcher->get_report();
		if( report==NULL ) {
			report = industry_manager->get_report();
		}
		if( report ) {
			sint64 cost = report->cost_fix;
			calc_finance_history();
			sint64 cash = get_finance_history_year(0, COST_NETWEALTH);

			// ausfuehren, falls genug Geld da ist
			if (is_cash_available(cost)) {
				bt_root.append_child( report->action );
				report->action = NULL;
				delete report;
				get_log().message( "ai_wai_t::step", "appended report (cost: %lld, cash: %lld)", cost, cash);
			}
			else {
				factory_searcher->append_report( report );
			}
		}
	}
}

bool ai_wai_t::is_cash_available(sint64 cost)
{
	// calc_finance_history(); done in step()
	sint64 cash = get_finance_history_year(0, COST_NETWEALTH);
	// future maintenance
	sint64 maint = ((sint64)get_maintenance()<<((sint64)get_welt()->ticks_per_world_month_shift-18l));
	// Mit Polster von ca. 20.000 + 2* monatliche Betriebskosten
	return( 10*(cost + 2000000 + 2* maint) < 9*cash );
}

void ai_wai_t::neuer_monat()
{
	ai_t::neuer_monat();
	string empty("");
	log.message("ai_wai_t::neuer_monat()", "debug output");
	bt_root.debug( log, empty );
}

void ai_wai_t::neues_jahr()
{
}

void ai_wai_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "ai_wai_t" );

	ai_t::rdwr(file);

	uint16 wai_version = WAI_VERSION;
	file->rdwr_short(wai_version);

	log.message("ai_wai_t::rdwr", "%s v.%d", file->is_saving() ? "save" : "load", wai_version);

	bt_root.rdwr(file, wai_version);

	bt_root.debug(log, "");
}

void ai_wai_t::laden_abschliessen()
{
	ai_t::laden_abschliessen();
}

void ai_wai_t::rotate90( const sint16 y_size )
{
	ai_t::rotate90( y_size );
	bt_root.rotate90( y_size );
}

void ai_wai_t::bescheid_vehikel_problem( convoihandle_t /*cnv*/, const koord3d /*ziel*/ )
{
}

ai_wai_t::ai_wai_t( karte_t *welt, uint8 nr ) :
	ai_t( welt, nr ),
	log(names[nr],true,true,true,"ai_wai_t: log started"),
	bt_root(this, "root node" )
{
	industry_manager = NULL;
	factory_searcher = NULL;
}

void ai_wai_t::register_wrapper(wrapper_t *wrap, const void *ptr)
{
	for(uint32 i=0; i<wraplist.get_count(); i++) {
		if (wraplist[i].wrap == wrap) {
			wraplist[i].ptr = ptr;
			return;
		}
	}
	wrapper_nodes_t node(wrap, const_cast<void*>(ptr));
	wraplist.append(node);
}
// remove all pointers to wrap class
void ai_wai_t::unregister_wrapper(wrapper_t *wrap)
{
	for(sint32 i=wraplist.get_count()-1; i>=0; i--) {
		if (wraplist[i].wrap == wrap) {
			wraplist.remove_at(i);
		}
	}
}

void ai_wai_t::notify_wrapper(const void *p)
{
	for(sint32 i=wraplist.get_count()-1; i>=0; i--) {
		if (wraplist[i].ptr == p) {
			wraplist[i].wrap->notify();
			wraplist.remove_at(i);
		}
	}
}


void ai_wai_t::notify_factory(notification_factory_t flag, const fabrik_t*p)
{
	if (flag==notify_delete) {
		notify_wrapper(p);
	}
}
