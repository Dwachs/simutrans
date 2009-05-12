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

enum return_code {
	RT_DONE_NOTHING, // Done nothing.
	RT_SUCCESS, // Done something.
	RT_PARTIAL_SUCCESS, // Done something, want to continue next step.
	RT_ERROR // Some error occured.
};

/*
 * This defines a node of a behaviour tree.
 * @author Daniel, Gerd Wachsmuth
 * @date  08.05.2009
 */

class bt_node_t {
public:
	virtual ~bt_node_t() {};

	virtual return_code step() = 0;
	virtual void rdwr(uint16 ai_version) = 0;
	virtual void rotate90( const sint16 y_size ) = 0;
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

	virtual return_code step();

	virtual void rdwr(uint16 ai_version);

	virtual void rotate90( const sint16 y_size );

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
