#include "factory_searcher.h"

#include "../../../simfab.h"
#include "../../../simtools.h"
#include "../../../simworld.h"

#include "../../ai.h"

#include "../../../tpl/weighted_vector_tpl.h"
#include "../../../tpl/slist_tpl.h"

// Copied from ai_goods

return_code factory_searcher_t::step()
{

	// find a tree root to complete
	weighted_vector_tpl<const fabrik_t *> start_fabs(20);
	slist_iterator_tpl<fabrik_t *> fabiter( sp->get_welt()->get_fab_list() );
	while(fabiter.next()) {
		const fabrik_t *fab = fabiter.get_current();
		// consumer and not completely overcrowded
		if(fab->get_besch()->get_produkte()==0  &&  fab->get_status()!=fabrik_t::bad) {
			int missing = get_factory_tree_missing_count( fab );
			if(missing>0) {
				start_fabs.append( fab, 100/(missing+1)+1, 20 );
			}
		}
	}
	const fabrik_t *root;
	if(start_fabs.get_count()>0) {
		root = start_fabs.at_weight( simrand( start_fabs.get_sum_weight() ) );
	}

	if( get_factory_tree_lowest_missing( root ) ) {
		///////////////////////
		// TODO
		// create verbindungsplaner von start -> ziel
		///////////////////////
	}


	return RT_SUCCESS;
}


/* recursive lookup of a factory tree:
 * sets start and ziel to the next needed supplier
 * start always with the first branch, if there are more goods
 */
bool factory_searcher_t::get_factory_tree_lowest_missing( const fabrik_t *fab )
{
	// now check for all products (should be changed later for the root)
	for( int i=0;  i<fab->get_besch()->get_lieferanten();  i++  ) {
		const ware_besch_t *ware = fab->get_besch()->get_lieferant(i)->get_ware();

		// find out how much is there
		const vector_tpl<ware_production_t>& eingang = fab->get_eingang();
		uint ware_nr;
		for(  ware_nr=0;  ware_nr<eingang.get_count()  &&  eingang[ware_nr].get_typ()!=ware;  ware_nr++  ) ;
		if(  eingang[ware_nr].menge > eingang[ware_nr].max/10  ) {
			// already enough supplied to
			continue;
		}

		const vector_tpl <koord> & sources = fab->get_suppliers();
		for( unsigned q=0;  q<sources.get_count();  q++  ) {
			fabrik_t *qfab = fabrik_t::get_fab(sp->get_welt(),sources[q]);
			const fabrik_besch_t* const fb = qfab->get_besch();
			for (uint qq = 0; qq < fb->get_produkte(); qq++) {
				if (fb->get_produkt(qq)->get_ware() == ware
					  &&  !is_forbidden( fabrik_t::get_fab(sp->get_welt(),sources[q]), fab, ware )
					  &&  !ai_t::is_connected( sources[q], fab->get_pos().get_2d(), ware )  ) {
					// find out how much is there
					const vector_tpl<ware_production_t>& ausgang = qfab->get_ausgang();
					uint ware_nr;
					for(ware_nr=0;  ware_nr<ausgang.get_count()  &&  ausgang[ware_nr].get_typ()!=ware;  ware_nr++  )
						;
					// ok, there is no connection and it is not banned, so we if there is enough for us
					if(  ((ausgang[ware_nr].menge*4)/3) > ausgang[ware_nr].max  ) {
						// bingo: soure
						start = qfab;
						ziel = fab;
						freight = ware;
						return true;
					}
					else {
						// try something else ...
						if(get_factory_tree_lowest_missing( qfab )) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}


/* recursive lookup of a tree and how many factories must be at least connected
 * returns -1, if this tree is incomplete
 */
int factory_searcher_t::get_factory_tree_missing_count( const fabrik_t *fab )
{
	int numbers=0;	// how many missing?

	// ok, this is a source ...
	if(fab->get_besch()->get_lieferanten()==0) {
		return 0;
	}

	// now check for all
	for( int i=0;  i<fab->get_besch()->get_lieferanten();  i++  ) {
		const ware_besch_t *ware = fab->get_besch()->get_lieferant(i)->get_ware();

		bool complete = false;	// found at least one factory
		const vector_tpl <koord> & sources = fab->get_suppliers();
		for( unsigned q=0;  q<sources.get_count();  q++  ) {
			fabrik_t *qfab = fabrik_t::get_fab(sp->get_welt(),sources[q]);
			assert( qfab );
			const fabrik_besch_t* const fb = qfab->get_besch();
			for (uint qq = 0; qq < fb->get_produkte(); qq++) {
				if (fb->get_produkt(qq)->get_ware() == ware 
						&& !is_forbidden( fabrik_t::get_fab(sp->get_welt(),sources[q]), fab, ware)) {
					int n = get_factory_tree_missing_count( qfab );
					if(n>=0) {
						complete = true;
						if(  !ai_t::is_connected( sources[q], fab->get_pos().get_2d(), ware )  ) {
							numbers += 1;
						}
						numbers += n;
					}
				}
			}
		}
		if(!complete) {
			if(fab->get_besch()->get_lieferanten()==0  ||  numbers==0) {
				return -1;
			}
		}
	}
	return numbers;
}

void factory_searcher_t::rdwr( loadsave_t* file, const uint16 /*version*/)
{
	// Blacklist speichern?
}
