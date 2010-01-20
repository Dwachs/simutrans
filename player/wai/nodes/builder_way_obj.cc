#include "builder_way_obj.h"
#include "../../ai_wai.h"
#include "../../../simwerkz.h"

return_value_t *builder_wayobj_t::step()
{
	const char* error;
	karte_t *welt = sp->get_welt();

	wkz_wayobj_t wkz;
	wkz.set_default_param(e->get_name());
	wkz.init( welt, sp );
	error = wkz.work( welt, sp, welt->lookup(start)->get_pos() );
	if( !error ) {
		wkz.work( welt, sp, welt->lookup(ziel)->get_pos() );
	}
	wkz.exit( welt, sp );
	if( error ) {
		sp->get_log().warning("builder_wayobj_t::step","Can't build from %s to %s", start.get_str(), ziel.get_str());
		return new_return_value(RT_ERROR);
	}
	else {
		sp->get_log().message("builder_wayobj_t::step","Build wayobj from %s to %s", start.get_str(), ziel.get_str());
		return new_return_value(RT_TOTAL_SUCCESS);
	}
}

void builder_wayobj_t::rdwr( loadsave_t *file, const uint16 version )
{
	start.rdwr( file );
	ziel.rdwr( file );
	/*
	 * TODO: rdwr e
	 */
}

void builder_wayobj_t::rotate90( const sint16 y_size )
{
	start.rotate90( y_size );
	ziel.rotate90( y_size );
}
