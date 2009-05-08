#ifndef BT_H
#define BT_H


#include "../../simtypes.h"
#include "../../tpl/vector_tpl.h"

/*
 * This class defines the return code of
 * bt_node_t::step().
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

/*
 * ## Gerd:
 * Sollten wir das mit Flags machen?
 */
class return_code_t {
public:
	return_code_t( bool hds, bool cma, bool err ) :
		have_done_something( hds ),
		call_me_again( cma ),
		error( err ) {};
	// This indicates, wether the step has done something.
	bool have_done_something;
	/*
	 * If this is set to true (only valid, if have_done_something==true)
	 * the same bt_node_t should be called the next step again.
	 */
	bool call_me_again;

	// Error during execution.
	bool error;
};

/*
 * This defines a node of a behaviour tree.
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

class bt_node_t {
public:
	virtual ~bt_node_t() {};

	virtual return_code_t step() = 0;
};

/*
 * This defines a node of a behaviour tree,
 * with childs.
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

class bt_sequential_t : bt_node_t {
public:
	bt_sequential_t();
	virtual ~bt_sequential_t();

	virtual return_code_t step();
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
