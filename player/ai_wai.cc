#include "ai_wai.h"

#include "wai/nodes/factory_searcher.h"

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
		bt_root.append_child(new factory_searcher_t(this, "fac search"));

		industry_manager = new industry_manager_t(this, "ind manager");
		bt_root.append_child(industry_manager);
		return;
	}
	log.message("", "");
	log.message("ai_wai_t::step", "next step:");
	return_value_t * rv = bt_root.step();
	delete rv;
}

void ai_wai_t::neuer_monat()
{
	ai_t::neuer_monat();
	cstring_t empty("");
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
	file->rdwr_short(wai_version, " ");

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
	log(names[nr],true,true),
	bt_root(this, "root node" )
{
	log.message("ai_wai_t","log started.");
	industry_manager = NULL;
}
