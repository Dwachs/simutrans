#include "return_value.h"
#include "bt.h"
#include "report.h"

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
}