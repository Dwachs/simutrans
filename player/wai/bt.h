#ifndef BT_H
#define BT_H


#include "../../simtypes.h"
#include "../../tpl/vector_tpl.h"
#include "../../utils/cstring_t.h"
#include "../../utils/log.h"

class ai_t;
class loadsave_t;

/*
 * all types of nodes 
 */
enum bt_types {
	BT_NODE=0,
	BT_SEQUENTIAL=1 
};

/*
 * This class defines the return code of
 * bt_node_t::step().
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

enum return_code {
	RT_DONE_NOTHING,		// Done nothing.
	RT_SUCCESS,				// Done something.
	RT_PARTIAL_SUCCESS,		// Done something, want to continue next step.
	RT_ERROR				// Some error occured.
};

/*
 * This defines a node of a behaviour tree.
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

class bt_node_t {
protected:
	cstring_t name;	// for debugging purposes -- wie soll man den scheiss rdwr'en??
	uint16	type;	// to get the right class for loading / saving
	ai_t *sp;
public:	
	bt_node_t( ai_t *sp_) : sp(sp_), type(BT_NODE) {}
	bt_node_t( ai_t *sp_, const char* name_) : name( name_ ), sp(sp_), type(BT_NODE) {};
	virtual ~bt_node_t() {};

	virtual return_code step() {return RT_DONE_NOTHING;};

	virtual void rdwr(loadsave_t* file);
	virtual void rotate90( const sint16 y_size ) {};
	virtual void debug( log_t &file, cstring_t prefix );
	
	uint16 get_type() const { return type;}
	/*
	 * Returns a new instance of a node from the right class
	 * 
	 */
	static bt_node_t *alloc_bt_node(uint16 type, ai_t *sp_);

	/*
	 * Saves the given child / loads the next child
	 * .. sets: sp, type
	 */
	void rdwr_child(loadsave_t* file, bt_node_t* &child);
};

/*
 * This defines a node of a behaviour tree,
 * with childs.
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

class bt_sequential_t : public bt_node_t {
public:
	bt_sequential_t( ai_t *sp_, const char* name_ );
	virtual ~bt_sequential_t();

	virtual return_code step();

	void append_child( bt_node_t *new_child );

	virtual void rdwr(loadsave_t* file);
	virtual void rotate90( const sint16 y_size );
	virtual void debug( log_t &file, cstring_t prefix );
private:
	vector_tpl< bt_node_t* > childs;

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
