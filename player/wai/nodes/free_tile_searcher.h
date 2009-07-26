#ifndef FREE_TILE_SEARCHER_H
#define FREE_TILE_SEARCHER_H

#include "../bt.h"

/*
 * Searches free tiles around a factory.
 * @author Gerd Wachsmuth
 */

class fabrik_t;
class return_value_t;

class free_tile_searcher_t : public bt_node_t
{
	const fabrik_t *fab;
	bool through; // force to search for places for through stations
public:
	free_tile_searcher_t( ai_wai_t *sp, const char* name );
	free_tile_searcher_t( ai_wai_t *sp, const char* name, const fabrik_t *fab, bool through = false );
	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual return_value_t *step();
};

#endif /* FREE_TILE_SEARCHER_H */
