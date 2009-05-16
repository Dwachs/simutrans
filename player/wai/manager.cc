#include "manager.h"

return_code manager_t::step()
{
	if (childs.get_count()>0) {
		return bt_sequential_t::step();
	}

	return work();
}