#include "ai_dw.h"
#include "ai_dw_tasks.h"

/************************************************************************
  Queue of AI tasks 
*************************************************************************/
void ai_task_queue_t::rotate90( const sint16 y_size ) {
	for(uint16 i=0; i<queue.count(); i++) {
		(queue.get_data())[i]->rotate90(y_size);
	}
}

void ai_task_queue_t::rdwr(loadsave_t *file, ai_dw_t* sp) {
	uint32 count = queue.count();
	file->rdwr_long(count, " ");

	for(uint32 i=0;i<count;i++) {
		if (file->is_loading()) {
			uint16 type=0;
			file->rdwr_short(type, " ");

			ai_task_t *t = sp->alloc_task(type);
			assert(t!=NULL);
			t->rdwr(file);
		}
		else {
			ai_task_t *t = (queue.get_data())[i];

			uint16 type = t->get_type();
			file->rdwr_short(type , " ");

			t->rdwr(file);
		}
	}
}

// gets the first task ... if time has come
ai_task_t *ai_task_queue_t::get_first(uint32 time) {
	if (queue.empty()) {
		return NULL;
	}
	else {
		if (queue.get_first()->get_time() <= time) {
			ai_task_t *ait = queue.pop();
			ait->set_time(time);
			return ait;
		}
		else {
			return(NULL);
		}			
	}
}

void ai_task_queue_t::append(ai_task_t *ait)
{
	queue.insert(ait);
}
