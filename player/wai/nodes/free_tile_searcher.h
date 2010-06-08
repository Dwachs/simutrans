#ifndef FREE_TILE_SEARCHER_H
#define FREE_TILE_SEARCHER_H

#include "../bt.h"
#include "../../../dataobj/koord3d.h"

/*
 * Searches free tiles around given location
 * @author gerw/dwachs
 */

class free_tile_searcher_t : public bt_node_t
{
	/**
	 * Search for free tiles (suitable for stations) near pos
	 * -- if a factory is at pos then find places that reach the factory
	 * -- if a halt is at pos then find places connected to that halt
	 * -- otherwise check whether pos itself is suitable for placing a station
	 */
	koord3d pos;
	bool through; // force to search for places for through stations
public:
	free_tile_searcher_t( ai_wai_t *sp, const char* name );
	free_tile_searcher_t( ai_wai_t *sp, const char* name, koord3d pos, bool through = false );
	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual return_value_t *step();
};

#endif /* FREE_TILE_SEARCHER_H */
