#include "return_value.h"

#include "bt.h"
#include "datablock.h"
#include "report.h"
#include "../../dataobj/freelist.h"

return_value_t::~return_value_t() 
{
	if (report) {
		delete report;
		report = NULL;
	}
	if (successor) {
		delete successor;
		successor = NULL;
	}
	if (undo) {
		delete undo;
		undo = NULL;
	}
	if( data ) {
		delete data;
		data = NULL;
	}
}


void *return_value_t::operator new(size_t /*s*/)
{
	return freelist_t::gimme_node(sizeof(return_value_t));
}



void return_value_t::operator delete(void *p)
{
	freelist_t::putback_node(sizeof(return_value_t),p);
}
