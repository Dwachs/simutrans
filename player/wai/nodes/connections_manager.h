#ifndef _CONNECTION_MANAGER_H_
#define _CONNECTION_MANAGER_H_

#include "../manager.h"
#include "../../../linehandle_t.h"
class fabrik_t;
class ware_besch_t;

enum connection_types {
	CONN_SIMPLE		= 1,
	CONN_COMBINED	= 2,
	CONN_SERIAL		= 3,
	CONN_PARALLEL	= 4,
	CONN_FREIGHT	= 5,
	CONN_WITHDRAWN	= 6
};

class connection_t {
public:
	connection_t() : type(CONN_SIMPLE) {}

	void set_line(linehandle_t l) { line = l; }
	linehandle_t get_line() const { return line; }

	virtual report_t* get_report(ai_wai_t *sp) { return NULL; }

	virtual void rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp);
	virtual void rotate90( const sint16 /*y_size*/ ) {}
	virtual void debug( log_t &file, cstring_t prefix );

	static connection_t* alloc_connection(connection_types t);
	static void rdwr_connection(loadsave_t* file, const uint16 version, ai_wai_t *sp, connection_t* &c);

	connection_types get_type() { return type; }
protected:
	linehandle_t line;	
	connection_types type;
};

class combined_connection_t : public connection_t {
public:
	combined_connection_t() : connection_t(), next_to_report(0) { type=CONN_COMBINED; }
	~combined_connection_t();
	void append_connection(connection_t* c) { connections.append(c); }

	virtual void rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp);
	virtual void debug( log_t &file, cstring_t prefix );
protected:
	// process one child per get_report call
	uint32 next_to_report;
	vector_tpl<connection_t*> connections;
};

/*
 * contains a chain of connections
 */
class serial_connection_t : public combined_connection_t {
public:
	serial_connection_t() : combined_connection_t() { type=CONN_SERIAL; next_to_report=0;}
	virtual report_t* get_report(ai_wai_t *sp);
};

class parallel_connection_t : public combined_connection_t {
public:
	parallel_connection_t() : combined_connection_t(){ type=CONN_PARALLEL;  next_to_report=0;}
	virtual report_t* get_report(ai_wai_t *sp);
};

/*
 * connection for a certain freight to a certain factory 
 * TODO: allow multiple freights / targets
 */
class freight_connection_t : public connection_t {
public:
	freight_connection_t() : connection_t() { type=CONN_FREIGHT; }
	freight_connection_t(const fabrik_t *z,	const ware_besch_t *f) : connection_t(), ziel(z), freight(f), status(0) { type=CONN_FREIGHT; }
	virtual report_t* get_report(ai_wai_t *sp);

	virtual void rdwr(loadsave_t* file, const uint16 version, ai_wai_t *sp);
	virtual void debug( log_t &file, cstring_t prefix );
private:
	const fabrik_t *ziel;
	const ware_besch_t *freight;
	uint8 status; // 1: keine groesseren Fahrzeuge verfuegbar
	bool bigger_convois_impossible() { return status&1; }
};

/*
 * connection that goes out of order soon, 
 * it waits until all vehicles are gone
 * UNUSED
 */
class withdrawn_connection_t : public connection_t {
public:
	withdrawn_connection_t() : connection_t() { type=CONN_WITHDRAWN; }
	virtual report_t* get_report(ai_wai_t *sp);

	virtual void debug( log_t &file, cstring_t prefix );
};



#endif

