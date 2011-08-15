#ifndef _AI_WAI_H_
#define _AI_WAI_H_

#include "ai.h"
#include "simplay.h"
#include "wai/bt.h"

#include "../tpl/vector_tpl.h"
#include "../utils/log.h"

#define WAI_VERSION (1)

class industry_manager_t;
class factory_searcher_t;
class wrapper_t;

class ai_wai_t : public ai_t {
public:
	// Needed Interface:
	virtual uint8 get_ai_id() const { return AI_WAI; }
	virtual ~ai_wai_t();
	virtual void step();
	virtual void neuer_monat();
	virtual void neues_jahr();
	virtual void rdwr(loadsave_t *file);
	virtual void laden_abschliessen();
	virtual void rotate90( const sint16 y_size );
	virtual void bescheid_vehikel_problem( convoihandle_t cnv, const koord3d ziel );
	ai_wai_t( karte_t *welt, uint8 nr );

	bool is_cash_available(sint64 money);

	industry_manager_t* get_industry_manager() { return industry_manager; }
	void set_industry_manager(industry_manager_t *im) { industry_manager = im; }
	factory_searcher_t* get_factory_searcher() { return factory_searcher; }
	void set_factory_searcher(factory_searcher_t *fs) { factory_searcher = fs; }

	void notify_factory(notification_factory_t flag, const fabrik_t*);

	/**
	 * interface to deal with pointers to ingame objects
	 * that may get deleted between calls to step()
	 */
	void register_wrapper(wrapper_t *, const void *);
	void unregister_wrapper(wrapper_t *);
	void notify_wrapper(const void *);

private:
	log_t log;

	/**
	 * holds safe pointers to ingame objects (factories)
	 */
	struct wrapper_nodes_t {
		wrapper_nodes_t(wrapper_t *w=0, const void *p=0) : wrap(w), ptr(p) {}
		wrapper_t *wrap;
		const void *ptr;
	};
	vector_tpl<wrapper_nodes_t> wraplist;

	/**
	 * the root nodes of the tree containing all the action nodes
	 */
	bt_sequential_t bt_root;

	/**
	 * pointers to special nodes, do not delete them
	 */
	industry_manager_t* industry_manager;
	factory_searcher_t* factory_searcher;
public:
	log_t& get_log() { return log; };
};

#endif
