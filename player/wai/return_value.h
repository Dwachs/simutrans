#ifndef _RETURN_VALUE_H_
#define _RETURN_VALUE_H_

enum return_code {
	RT_DONE_NOTHING		=1,	// Done nothing.
	RT_PARTIAL_SUCCESS	=2,	// Done something, want to continue next 
	RT_SUCCESS			=3,	// Done something.
	RT_ERROR			=4,	// Some error occured.
	RT_KILL_ME			=8,	// Can be destroyed by parent.
	RT_TOTAL_SUCCESS	=RT_SUCCESS | RT_KILL_ME
};

#include "bt.h"
/*
 * This class defines the return code of
 * bt_node_t::step().
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */


class report_t;

class return_value_t {
public:
	return_code code;
	uint16 type;			// who returns this
	report_t *report;
	bt_node_t *successor;	// successor node of the original node
	bt_node_t *undo;		// undo node for the original node

	return_value_t(return_code c, uint16 t) : code(c), type(t), report(NULL), successor(NULL), undo(NULL) {}
	~return_value_t();
};

#endif