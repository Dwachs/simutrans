#ifndef BT_H
#define BT_H


#include "../../simtypes.h"
#include "../../tpl/vector_tpl.h"
#include "../../utils/cstring_t.h"
#include "../../utils/log.h"

class ai_wai_t;
class loadsave_t;
class report_t;

/*
 * all types of nodes 
 */
enum bt_types {
	BT_NULL			= 0,
	BT_NODE			= 1,
	BT_SEQUENTIAL	= 2,
	BT_PLANNER		= 100,
	BT_IND_CONN_PLN	= 101,
	BT_MANAGER		= 200,
	BT_FACT_SRCH	= 201
};

/*
 * This class defines the return code of
 * bt_node_t::step().
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

enum return_code {
	RT_DONE_NOTHING,    // Done nothing.
	RT_SUCCESS,         // Done something.
	RT_PARTIAL_SUCCESS, // Done something, want to continue next step.
	RT_TOTAL_SUCCESS,   // Done all, can be destroyed by parent.
	RT_ERROR            // Some error occured.
};

static const char *return_code_to_char [] =
{
	"Done nothing.",
	"Success.",
	"Partial success.",
	"Total success.",
	"Error."
};

/*
 * This defines a node of a behaviour tree.
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

class bt_node_t {
protected:
	cstring_t name; // for debugging purposes
	uint16 type;    // to get the right class for loading / saving
	ai_wai_t *sp;
public:
	bt_node_t( ai_wai_t *sp_) : type(BT_NODE), sp(sp_) {}
	bt_node_t( ai_wai_t *sp_, const char* name_) : name( name_ ), type(BT_NODE), sp(sp_) {};
	virtual ~bt_node_t() {};

	virtual return_code step() {return RT_DONE_NOTHING;};

	virtual report_t* get_report() { return NULL; };
	virtual void append_report(report_t * /*report*/) {};

	virtual void rdwr(loadsave_t* file, const uint16 version);
	virtual void rotate90( const sint16 /*y_size*/ ) {};
	virtual void debug( log_t &file, cstring_t prefix );
	
	uint16 get_type() const { return type;}
	/*
	 * Returns a new instance of a node from the right class
	 * 
	 */
	static bt_node_t *alloc_bt_node(uint16 type, ai_wai_t *sp_);

	/*
	 * Saves the given child / loads the next child
	 * .. sets: sp, type
	 */
	static void rdwr_child(loadsave_t* file, const uint16 version, ai_wai_t *sp_, bt_node_t* &child);
};

/*
 * This defines a node of a behaviour tree,
 * with childs.
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

class bt_sequential_t : public bt_node_t {
public:
	bt_sequential_t( ai_wai_t *sp_, const char* name_ );
	virtual ~bt_sequential_t();

	virtual return_code step();

	void append_child( bt_node_t *new_child );

	virtual void rdwr(loadsave_t* file, const uint16 version);
	virtual void rotate90( const sint16 y_size );
	virtual void debug( log_t &file, cstring_t prefix );

protected:
	vector_tpl< bt_node_t* > childs;

private:
	// Which child should get the next step?
	uint32 next_to_step;

	// Which child has done something?
	uint32 last_step;
};

/*
 * ## Gerd:
 * Sollten wir noch eine Klasse caller_t einführen, von
 * der bt_sequential abgelitten wird und die an ::step()
 * übergeben wird?
 * 
 * Damit könnte dann ein bt_node_t auf seinen 'Vater'
 * zurückgreifen und ihm etwas mitteilen.
 *
 * (Dann müssten wir auch die AI selber davon ableiten,
 * da sie ja den ersten call macht, aber das stört ja nicht
 * weiter).
 */

#endif
