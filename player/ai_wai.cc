#include "ai_wai.h"

static const char *names [10] =
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
	"wai9.log"
};

ai_wai_t::~ai_wai_t()
{
}

void ai_wai_t::step()
{
}

void ai_wai_t::neuer_monat()
{
	cstring_t empty("");
	bt_root.debug( log, empty );
}

void ai_wai_t::neues_jahr()
{
}

void ai_wai_t::laden_abschliessen()
{
}

void ai_wai_t::rotate90( const sint16 y_size )
{
}

void ai_wai_t::bescheid_vehikel_problem( convoihandle_t cnv, const koord3d ziel )
{
}

ai_wai_t::ai_wai_t( karte_t *welt, uint8 nr ) :
	ai_t( welt, nr ),
	log(names[nr],true,true),
	bt_root( "root node" )
{
	log.message("ai_wai_t","log started.");

	bt_sequential_t *test = new bt_sequential_t("hansi");
	bt_sequential_t *test2 = new bt_sequential_t("hanswurst");
	test->append_child( test2 );
	test2 = new bt_sequential_t("hanswurst2");
	test->append_child( test2 );
	bt_root.append_child(test);
	test2 = new bt_sequential_t("hanswurst3");
	bt_root.append_child(test2);
}
