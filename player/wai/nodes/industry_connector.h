#ifndef _IND_CONNECTOR_H_
#define _IND_CONNECTOR_H_

#include "../bt.h"
class connection_t;
class fabrik_t;
class ware_besch_t;
/*
 * steps his childs (that have to be some connector-nodes)
 * and gives the industry-manager the correct industry-connection
 */
class industry_connector_t : public bt_sequential_t
{	
public:
	industry_connector_t( ai_wai_t *sp, const char *name);
	industry_connector_t( ai_wai_t *sp, const char *name, const fabrik_t *s, const fabrik_t *z, const ware_besch_t *f);
	~industry_connector_t();

	virtual return_value_t *step();

	virtual void rdwr( loadsave_t *file, const uint16 version );
	virtual void rotate90( const sint16);
	virtual void debug( log_t &file, cstring_t prefix );
private:
	const fabrik_t *start, *ziel;
	const ware_besch_t *freight;
	connection_t *connections;
};

#endif
