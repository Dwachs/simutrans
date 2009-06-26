#include "vehikel_builder.h"
#include "../../../simworld.h"
#include "../../../dataobj/loadsave.h"

vehikel_builder_t::~vehikel_builder_t()
{
	delete prototyper;
	prototyper = NULL;
}

return_code vehikel_builder_t::step()
{
	if (nr_vehikel>0) {
		return RT_PARTIAL_SUCCESS;
	}
	else {
		return RT_TOTAL_SUCCESS;
	}
}

void vehikel_builder_t::rdwr( loadsave_t *file, const uint16 version )
{
	bt_node_t::rdwr(file,version);
	pos.rdwr(file);
	file->rdwr_byte(nr_vehikel,"");
	if (file->is_loading()) {
		prototyper = new simple_prototype_designer_t(sp);
	}
	prototyper->rdwr(file);
}

void vehikel_builder_t::rotate90( const sint16 y_size )
{
	pos.rotate90(y_size);
}