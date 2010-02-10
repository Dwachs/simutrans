#include "wrapper.h"
#include "../../ai_wai.h"
#include "../../../simfab.h"
#include "../../../dataobj/loadsave.h"

template<class T> void rdwr(const T* &p, loadsave_t *file, const uint16, ai_wai_t *sp) {}

template<> void rdwr_tpl<class fabrik_t>(const fabrik_t* &ptr, loadsave_t *file, const uint16 version, ai_wai_t *sp)
{/*
	wrapper_rdwr::rdwr_fabrik_t(ptr,file,version,sp);
}

void wrapper_rdwr::rdwr_fabrik_t(const fabrik_t* &ptr, loadsave_t *file, const uint16, ai_wai_t *sp)
{*/
	koord3d pos(koord3d::invalid);
	if (file->is_saving() && ptr) {
		pos = ptr->get_pos();
	}
	pos.rdwr(file);
	if (file->is_loading()) {
		ptr = fabrik_t::get_fab(sp->get_welt(), pos.get_2d());
	}
}

/*
void wfabrik_t::rdwr(loadsave_t *file, const uint16 version, ai_wai_t *sp)
{
	wrap_tpl<fabrik_t>::rdwr(file, version, sp);
}*/