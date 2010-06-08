#ifndef _INDUSTRY_MANAGER_H_
#define _INDUSTRY_MANAGER_H_

#include "connections_manager.h"
#include "../manager.h"
#include "../utils/wrapper.h"
#include "../../../linehandle_t.h"

class fabrik_t;
class ware_besch_t;

enum connection_status {
	none       = 0,	// no connection
	own        = 1,	// own connection
	competitor = 2,	// connection of other player
	exists     = 3,	// connection exists
	planned    = 4,	// connection planned
	broken     = 8, // connection has to be removed

	forbidden  = 256,
	unplanable = planned | exists | forbidden
};



class industry_link_t {
public:
	industry_link_t(ai_wai_t *sp, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f);
	~industry_link_t();

	report_t* get_report(ai_wai_t *sp);
	// prepare report to remove all infrastructure
	report_t* get_final_report(ai_wai_t *sp);

	void append_connection(connection_t* c) { connections->append_connection(c); }

	bool is(connection_status cs) const { return (status & cs)!=0; }
	void set(connection_status cs) { status |= cs; }
	void unset(connection_status cs) { status &= ~cs; }

	bool operator != (const industry_link_t & k) { return start != k.start || ziel != k.ziel || freight != k.freight; }
	bool operator == (const industry_link_t & k) { return start == k.start && ziel == k.ziel && freight == k.freight; }
	bool is_equal(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) const { return start == s && ziel == z && freight == f; }

	void rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp);
	void rotate90( const sint16 /*y_size*/ ) {}
	void debug( log_t &file, cstring_t prefix );
private:
	sint64 status;
	parallel_connection_t *connections;

	wfabrik_t start;
	wfabrik_t ziel;
	const ware_besch_t *freight;
};

class industry_manager_t : public manager_t {
public:
	industry_manager_t(ai_wai_t *sp_, const char* name_) : manager_t(sp_,name_), connections(100), next_cid(0) { type = BT_IND_MNGR; }

	// will check each line and generate reports
	virtual return_value_t *work();
	// will immediately execute positive reports (ie buy vehicles)
	virtual void append_report(report_t *report);

	/* these methods return the associated connection,
	 *  if there is none, create it
	 */
	uint32 get_connection_id(const fabrik_t *, const fabrik_t *, const ware_besch_t *);
	industry_link_t* get_connection(uint32 id);
	industry_link_t* get_connection(const fabrik_t *, const fabrik_t *, const ware_besch_t *);

	/* returns false if the connection does not exist or if the corresponding bits are not set */
	bool is_connection(connection_status cs, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) const;

	/* sets the status bit, creates connection if none exists */
	void set_connection(connection_status cs, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f);

	/* unsets the status bit, creates no new connection */
	void unset_connection(connection_status cs, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f);

	virtual void rdwr(loadsave_t* file, const uint16 version);
	virtual void rotate90( const sint16 /*y_size*/ );
	virtual void debug( log_t &file, cstring_t prefix );
private:
	vector_tpl<industry_link_t *> connections;
	// returns the index of the link, if get_index>=get_count then link is not in the list
	uint32 get_index(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) const;
	uint32 get_index(industry_link_t *) const;
	uint32 next_cid;
};


#endif
