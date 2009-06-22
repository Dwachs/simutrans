#ifndef _INDUSTRY_MANAGER_H_
#define _INDUSTRY_MANAGER_H_

#include "../manager.h"
#include "../../../linehandle_t.h"

class fabrik_t;
class ware_besch_t;

enum connection_status {
	none = 0,		// no connection
	own = 1,		// own connection
	competitor = 2,	// connection of other player
	exists = 3,		// connection exists
	planned = 4,	// connection planned
	
	forbidden = 256
};



class industry_connection_t {
public:	
	industry_connection_t(const fabrik_t *s=0, const fabrik_t *z=0, const ware_besch_t *f=0) : status(0), line(0), start(s), ziel(z), freight(f) {}

	void set_line(linehandle_t l) { line = l; }
	linehandle_t get_line() const { return line; }

	template<connection_status cs> bool is() const { return (status & cs)!=0; }
	template<connection_status cs> void set() { status |= cs; }

	bool operator != (const industry_connection_t & k) { return start != k.start || ziel != k.ziel || freight != k.freight; }
	bool operator == (const industry_connection_t & k) { return start == k.start && ziel == k.ziel && freight == k.freight; }

	void rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp);
	void rotate90( const sint16 /*y_size*/ ) {}
	void debug( log_t &file, cstring_t prefix );
private:
	sint64 status;
	linehandle_t line;

	const fabrik_t *start;
	const fabrik_t *ziel;
	const ware_besch_t *freight;	
};

class industry_manager_t : public manager_t {
public:
	industry_manager_t(ai_wai_t *sp_, const char* name_) : manager_t(sp_,name_) { type = BT_IND_MNGR; }
	
	/* these methods return the associated connection,
	 *  if there is none, create it
	 */
	uint32 get_connection_id(const fabrik_t *, const fabrik_t *, const ware_besch_t *);
	industry_connection_t& get_connection(uint32 id);
	industry_connection_t& get_connection(const fabrik_t *, const fabrik_t *, const ware_besch_t *);

	/* returns false if the connection does not exist or if the corresponding bits are not set */
	template<connection_status cs> 
	bool is_connection(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f) const
	{
		industry_connection_t ic(s,z,f);
		if (connections.is_contained(ic)) {
			return connections[connections.index_of(ic)].is<cs>();
		}
		else {
			return false;
		}
	}
	/* sets the status bit, creates connection if none exists */
	template<connection_status cs> 
	void set_connection(const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f)
	{
		industry_connection_t& ic = get_connection(s,z,f);
		ic.set<cs>();
	}
	
	virtual void rdwr(loadsave_t* file, const uint16 version);
	virtual void rotate90( const sint16 /*y_size*/ );
	virtual void debug( log_t &file, cstring_t prefix );
private:
	vector_tpl<industry_connection_t> connections;
};


#endif
