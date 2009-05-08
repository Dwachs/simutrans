#include "ai_dw_factory.h"
#include "ai_dw.h"

#include "../simworld.h"
#include "../simfab.h"
#include "../simtools.h"


/*************************************
 * Find the 'best' factory tree
 *
 * @reportsto ait_fct_complete_tree_t
 * @from ai_goods 
 */
void ait_fct_init_t::work(ai_task_queue_t &queue) {
	karte_t *welt = sp->get_welt();
	fabrik_t *root = NULL;

	// find a tree root to complete
	weighted_vector_tpl<fabrik_t *> start_fabs(20);
	slist_iterator_tpl<fabrik_t *> fabiter( welt->get_fab_list() );
	while(fabiter.next()) {
		fabrik_t *fab = fabiter.get_current();
		// consumer and not completely overcrowded
		if(fab->get_besch()->get_produkte()==0  &&  fab->get_status()!=fabrik_t::bad) {
			int missing = get_factory_tree_missing_count( fab );
			if(missing>0) {
				start_fabs.append( fab, 100/(missing+1)+1, 20 );
			}
		}
	}
	if(start_fabs.get_count()>0) {
		fabrik_t *root = start_fabs.at_weight( simrand( start_fabs.get_sum_weight() ) );		
		// now complete the factory tree
		queue.append(new ait_fct_complete_tree_t(get_time() + 3, sp, root));

		/* ?? try again later (~1.5 months)
		if (start_fabs.get_count()>1) {
			long delay = (sp->get_welt()->ticks_per_tag * simrand(512) ) >>8;
			ai_task_t *ait = (ai_task_t*)new ait_fct_complete_tree_t(get_time() + delay, sp, root);
			queue.append(ait);
		} */
	}

}



/* recursive lookup of a tree and how many factories must be at least connected
 * returns -1, if this tree is incomplete
 * @from ai_goods
 */
int ait_fct_init_t::get_factory_tree_missing_count( fabrik_t *fab )
{
	karte_t *welt = sp->get_welt();

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
			fabrik_t *qfab = fabrik_t::get_fab(welt,sources[q]);
			if(!fab) {
				dbg->error( "fabrik_t::get_fab()","fab %s at %s does not find supplier at %s.", fab->get_name(), fab->get_pos().get_str(), sources[q].get_str() );
				continue;
			}
			const fabrik_besch_t* const fb = qfab->get_besch();
			for (uint qq = 0; qq < fb->get_produkte(); qq++) {
				if (fb->get_produkt(qq)->get_ware() == ware  && !sp->is_forbidden( fabrik_t::get_fab(welt,sources[q]), fab, ware)) {
					int n = get_factory_tree_missing_count( qfab );
					if(n>=0) {
						complete = true;
						if(  !sp->is_connected( sources[q], fab->get_pos().get_2d(), ware )  ) {
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



/*************************************
 * Find factory that needs suppliers 
 *  in a given factory tree
 */
void ait_fct_find_t::work(ai_task_queue_t &queue)
{
	sp->log->message("ait_fct_find_t::work", "[time = %d] complete factory tree for %s(%s)", get_time(),  
			root->get_name(), root->get_pos().get_str());
	// not-delivered supplier?
	if (root && get_factory_tree_lowest_missing(root)) {
		sp->log->message("ait_fct_find_t::work", "create road transport from %s(%s) to  %s(%s)", start->get_name(), start->get_pos().get_str(), ziel->get_name(), ziel->get_pos().get_str());
		//invoke road connection builder
		queue.append(new ait_road_connectf_t(get_time()+5, sp, start, ziel, freight));

		// complete this tree 
		ai_task_t *ait= new ait_fct_complete_tree_t(get_time() + simrand(4000)+5234, sp, root);
		queue.append(ait);

		sp->log->message("ait_fct_find_t::work", "complete tree later at %d", ait->get_time());	
	}
	else {
		// find new factory tree
		ai_task_t *ait= new ait_fct_init_t(get_time() + simrand(50000)+6435, sp);
		queue.append(ait);
		
		sp->log->message("ait_fct_find_t::work", "found nothing, start new search at %d", ait->get_time());
	}
}

/* recursive lookup of a factory tree:
 * sets start and ziel to the next needed supplier
 * start always with the first branch, if there are more goods
 * @from ai_goods
 */
bool ait_fct_find_t::get_factory_tree_lowest_missing( fabrik_t *fab )
{
	sp->log->message("ait_fct_find_t::get_factory_tree_lowest_missing","fabrik %s at (%s)", fab->get_name(), fab->get_pos().get_str());

	karte_t *welt = sp->get_welt();
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
			fabrik_t *qfab = fabrik_t::get_fab(welt,sources[q]);
			const fabrik_besch_t* const fb = qfab->get_besch();
			for (uint qq = 0; qq < fb->get_produkte(); qq++) {
				if (fb->get_produkt(qq)->get_ware() == ware
					  &&  !sp->is_forbidden( qfab, fab, ware )
					  &&  !sp->is_connected( sources[q], fab->get_pos().get_2d(), ware )  ) {
					// find out how much is there
					const vector_tpl<ware_production_t>& ausgang = qfab->get_ausgang();
					uint ware_nr;
					for(ware_nr=0;  ware_nr<ausgang.get_count()  &&  ausgang[ware_nr].get_typ()!=ware;  ware_nr++  )
						;
					// ok, there is no connection and it is not banned, so we if there is enough for us
					if(  ((ausgang[ware_nr].menge*4)/3) > ausgang[ware_nr].max  ) {
						// bingo: 
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

void ait_fct_find_t::rdwr(loadsave_t* file) 
{
	ai_task_t::rdwr(file); 
	koord pos;
	if (file->is_saving()) { // save only position
		pos = root->get_pos().get_2d();
	}
	pos.rdwr(file);
	if (file->is_loading()) // factories are not loaded yet
	{
		root = fabrik_t::get_fab(sp->get_welt(), pos);
	}
}

/************************************
 * Completes a factory tree
 */
void ait_fct_complete_tree_t::work(ai_task_queue_t &queue)
{
	sp->log->message("ait_fct_complete_tree_t::work", "[time = %d] complete factory tree for %s(%s)", get_time(),  
			root->get_name(), root->get_pos().get_str());
	queue.append(new ait_fct_find_t(get_time() + 2, sp, root));

	// put on wait
	/* set_time(get_time() + simrand(4000)+1234);
	queue.append(this);
	sp->log->message("ait_fct_complete_tree_t::work", "wait until %d", get_time()); */
}

