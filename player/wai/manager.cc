#include "manager.h"

#include "../../simtools.h"
#include "../../dataobj/loadsave.h"
#include "planner.h"

return_code manager_t::step()
{
	if (bt_sequential_t::step() == RT_DONE_NOTHING) {
		return work();
	}
	else {
		return RT_PARTIAL_SUCCESS;
	}
}

report_t* manager_t::get_report() 
{
	if (reports.get_count()==0) {
		return NULL;
	}
	else {
		// TODO: do something more reasonable here
		return reports[simrand(reports.get_count())];
	}
}


void manager_t::rdwr(loadsave_t* file, const uint16 version)
{
	bt_sequential_t::rdwr(file, version);

	uint32 count = reports.get_count();
	file->rdwr_long(count, " ");
	for(uint32 i=0; i<count; i++) {
		if(file->is_loading()) {
			reports.append(new report_t());
		}
		reports[i]->rdwr(file,version,sp);
	}

}
void manager_t::rotate90( const sint16 y_size)
{
	bt_sequential_t::rotate90(y_size);

	for(uint32 i=0; i<reports.get_count(); i++)
	{
		reports[i]->rotate90(y_size);
	}
}