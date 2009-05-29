#ifndef FACTORY_SEARCHER_H
#define FACTORY_SEARCHER_H 

#include "../bt.h"
#include "../../../tpl/vector_tpl.h"

/*
 * The factory searcher searches new factory
 * connections.
 */

class fabrik_t;
class karte_t;
class ware_besch_t;

class factory_searcher_t : public bt_node_t {
public:
	virtual return_code step();
	virtual void rotate90( const sint16 ) {};
	virtual void rdwr( loadsave_t* file, const uint16 version);

private:

	bool get_factory_tree_lowest_missing( const fabrik_t *root );
	int get_factory_tree_missing_count( const fabrik_t *root );

	bool is_forbidden( const fabrik_t * /*start*/, const fabrik_t * /*end*/, const ware_besch_t * /*w*/ ) const { return false; };

	const fabrik_t *start;
	const fabrik_t *ziel;
	const ware_besch_t *freight;
};

#endif /* FACTORY_SEARCHER_H */
