#ifndef BT_H
#define BT_H

#include <string>
#include "alloc_node.h"
#include "return_value.h"
#include "../../simtypes.h"
#include "../../tpl/vector_tpl.h"
#include "../../utils/log.h"

class ai_wai_t;
class loadsave_t;
class report_t;

using std::string;

/*
 * This defines a node of a behaviour tree.
 * @author dwachs, gerw
 * @date  08.05.2009
 */

class bt_node_t {
protected:
	string name; // for debugging purposes
	uint16 type;    // to get the right class for loading / saving
	ai_wai_t *sp;
	virtual return_value_t* new_return_value(return_code rc);
public:
	bt_node_t( ai_wai_t *sp_) : type(BT_NODE), sp(sp_) {}
	bt_node_t( ai_wai_t *sp_, const char* name_) : name( name_ ), type(BT_NODE), sp(sp_) {};
	virtual ~bt_node_t() {};

	/*
	 * The MAIN routine of all nodes
	 */
	virtual return_value_t* step();

	/*
	 * Processes return values
	 * TODO: append_undo, append_successor etc
	 */
	virtual void append_report(report_t * /*report*/) {};
	virtual report_t* get_report() { return NULL; };

	/*
	 * Auxiliary functions: load/save, rotate, debug output
	 */
	virtual void rdwr(loadsave_t* file, const uint16 version);
	virtual void rotate90( const sint16 /*y_size*/ ) {};
	virtual void debug( log_t &file, string prefix );

	ai_wai_t *get_sp() const { return sp; }


	/*
	 * Identifies the node, used for loading / saving purposes
	 */
	uint16 get_type() const { return type;}
	/*
	 * Saves the given child / loads the next child
	 * .. sets: sp, type
	 */
	static void rdwr_child(loadsave_t* file, const uint16 version, ai_wai_t *sp_, bt_node_t* &child);
};

/*
 * This defines a node of a behaviour tree,
 * with childs.
 * @author dwachs, gerw
 * @date  08.05.2009
 */

class bt_sequential_t : public bt_node_t {
public:
	bt_sequential_t( ai_wai_t *sp_, const char* name_ );
	virtual ~bt_sequential_t();

	virtual return_value_t*  step();

	void append_child( bt_node_t *new_child );

	virtual void rdwr(loadsave_t* file, const uint16 version);
	virtual void rotate90( const sint16 y_size );
	virtual void debug( log_t &file, string prefix );

	uint32 get_count() const { return childs.get_count(); }
protected:
	vector_tpl< bt_node_t* > childs;

private:
	// Which child should get the next step?
	uint32 next_to_step;
};
#endif
