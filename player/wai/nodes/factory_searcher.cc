#include "factory_searcher.h"

#include "industry_connection_planner.h"
#include "industry_manager.h"

#include "../../../simfab.h"
#include "../../../simtools.h"
#include "../../../simworld.h"

#include "../../ai.h"
#include "../../ai_wai.h"

#include "../../../tpl/weighted_vector_tpl.h"
#include "../../../tpl/slist_tpl.h"

void factory_searcher_t::append_report(report_t *report)
{ 
	if(report) {
		if (report->gain_per_m > 0) {
		/*   sp->get_log().message( "factory_searcher_t::append_report()","got a nice report for immediate execution");
			append_child( report->action );
			report->action = NULL;
			delete report;
		}
		else { */
			manager_t::append_report(report);
		}
	}
}

bool factory_searcher_t::is_forbidden( const fabrik_t * s, const fabrik_t * z, const ware_besch_t * f) const
{
	return sp->get_industry_manager()->is_connection(forbidden, s, z, f);
}

bool factory_searcher_t::is_planable( const fabrik_t * s, const fabrik_t * z, const ware_besch_t * f) const
{
	return !sp->get_industry_manager()->is_connection(unplanable, s,z,f);
}

// Copied from ai_goods
return_value_t *factory_searcher_t::work()
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
	const fabrik_t *root = NULL;
	if(start_fabs.get_count()>0) {
		root = pick_any_weighted(start_fabs);
	}

	if( root && get_factory_tree_lowest_missing( root ) ) {
		// create verbindungsplaner von start -> ziel
		char buf[200];
		sprintf(buf, "ind_conn_plan freight %s from %s to %s by %d", freight->get_name(), start->get_pos().get_str(), ziel->get_pos().get_2d().get_str(), road_wt);
		append_child( new industry_connection_planner_t(sp, buf, *start, *ziel, freight, road_wt ));

		sp->get_log().message( "factory_searcher_t::work()","found route %s -> %s", start->get_name(), ziel->get_name() );
		return new_return_value(RT_PARTIAL_SUCCESS);
	}
	else {
		sp->get_log().message( "factory_searcher_t::work()","found no route");
	}


	return new_return_value(RT_SUCCESS);
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
		const array_tpl<ware_production_t>& eingang = fab->get_eingang();
		uint ware_nr;
		for(  ware_nr=0;  ware_nr<eingang.get_count()  &&  eingang[ware_nr].get_typ()!=ware;  ware_nr++  ) ;
		if(ware_nr >= eingang.get_count()) {
			// something wrong here.
			return false;
		}
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
					  &&  is_planable( fabrik_t::get_fab(sp->get_welt(),sources[q]), fab, ware )
					  &&  !ai_t::is_connected( sources[q], fab->get_pos().get_2d(), ware )  ) {
					// find out how much is there
					const array_tpl<ware_production_t>& ausgang = qfab->get_ausgang();
					uint ware_nr;
					for(ware_nr=0;  ware_nr<ausgang.get_count()  &&  ausgang[ware_nr].get_typ()!=ware;  ware_nr++  )
						;
					// no suppliers for this good!
					if (ware_nr>=ausgang.get_count()) {
						assert(fab->get_besch()->get_produkte()==0);
						continue;
					}

					// ok, there is no connection and it is not banned, so we if there is enough for us
					if(  ((ausgang[ware_nr].menge*4)/3) > ausgang[ware_nr].max  ) {
						// bingo: soure
						start.set(qfab);
						ziel.set(fab);
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

		bool complete = false;	// found at least one supplier for ware
		const vector_tpl <koord> & sources = fab->get_suppliers();
		for( unsigned q=0;  q<sources.get_count();  q++  ) {
			fabrik_t *qfab = fabrik_t::get_fab(sp->get_welt(),sources[q]);
			assert( qfab );
			const fabrik_besch_t* const fb = qfab->get_besch();
			for (uint qq = 0; qq < fb->get_produkte(); qq++) {
				if (fb->get_produkt(qq)->get_ware() == ware)
				{
					if(is_forbidden( fabrik_t::get_fab(sp->get_welt(),sources[q]), fab, ware)) {
						return -1;
					}
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
		// some necessary good is not supplied
		if(!complete && fab->get_besch()->get_produkte()!=0) {
			return -1;
		}
	}
	return numbers;
}

void factory_searcher_t::rdwr( loadsave_t* file, const uint16 version)
{
	manager_t::rdwr(file, version);

	start.rdwr(file, version, sp);
	ziel.rdwr(file, version, sp);

	ai_t::rdwr_ware_besch(file, freight);
}

