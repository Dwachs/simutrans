#include "vehikel_builder.h"
#include "../../../simworld.h"
#include "../../../bauer/vehikelbauer.h"
#include "../../../besch/vehikel_besch.h"
#include "../../../dataobj/loadsave.h"

const char start_name[30] = "[S] awaiting start";

vehikel_builder_t::~vehikel_builder_t()
{
	delete prototyper;
	prototyper = NULL;
}

return_value_t *vehikel_builder_t::step()
{
	sp->get_log().message("vehikel_builder::step","build %s for line %s", (const char*)name, line->get_name());
	if (!line.is_bound()) {
		sp->get_log().warning("vehikel_builder::step", "invalid linehandle");
		return new_return_value(RT_TOTAL_FAIL);
	}
	waytype_t wt = prototyper->proto->get_waytype();
	// prototype empty
	if (wt == invalid_wt) {
		sp->get_log().warning("vehikel_builder::step", "invalid prototype waytype");
		return new_return_value(RT_TOTAL_FAIL);
	}
	// valid ground ?
	grund_t* gr = sp->get_welt()->lookup(pos);
	if(  gr == NULL  ) {
		sp->get_log().warning("vehikel_builder::step", "no valid starting position (%s)", pos.get_str());
		return new_return_value(RT_TOTAL_FAIL);
	}
	// depot ?
	depot_t *dp = gr->get_depot();
	if (dp==NULL || dp->get_besitzer()!=sp || dp->get_wegtyp()!=wt) {
		sp->get_log().warning("vehikel_builder::step", "no valid depot at (%s)", pos.get_str());
		return new_return_value(RT_TOTAL_FAIL);
	}
	
	if (!cnv.is_bound() && nr_vehikel>0) {
		sp->get_log().message("vehikel_builder::step","create convoi for line %s", line->get_name());
		// create a new one
		cnv = dp->add_convoi();
		bool ok= true;
		for (uint8 i=0; i<prototyper->proto->besch.get_count() && prototyper->proto->besch[i] && ok; i++) {
			vehikel_t *v = vehikelbauer_t::baue(pos, sp, NULL, prototyper->proto->besch[i]);
			ok = cnv->add_vehikel( v ); // returns false only if convoi too long
		}
		// indicate the current status (for saving / loading)
		cnv->set_name(start_name);
		cnv->set_line(line);
		nr_vehikel--;
	}
	// try to start the convoi
	if (cnv.is_bound() && cnv->get_pos()==pos) {
		if (dp->start_convoi(cnv)) {
			sp->get_log().message("vehikel_builder::step","started convoi");
			// now give the new convoi name from first vehicle
			cnv->set_name(prototyper->proto->besch[0]->get_name());
			// unset cnv, the next step can create a new one
			cnv = convoihandle_t();
		}
		else {
			sp->get_log().message("vehikel_builder::step","convoi not started");
			// TODO: add some delay for retrying
			return new_return_value(RT_DONE_NOTHING);
		}
	}
	
	if (nr_vehikel>0 || cnv.is_bound()) {
		// TODO: add some delay until next vehicle is created
		sp->get_log().message("vehikel_builder::step", "built a convoy.");
		return new_return_value(RT_PARTIAL_SUCCESS);
	}
	else {
		sp->get_log().warning("vehikel_builder::step", "error");
		return new_return_value(RT_TOTAL_SUCCESS);
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
	// save linehandle
	uint16 line_id;
	if (file->is_saving()) {
		line_id = line.is_bound() ? line->get_line_id() : INVALID_LINE_ID;
	}
	file->rdwr_short(line_id, " ");
	if (file->is_loading()) {
		if (line_id != INVALID_LINE_ID) {
			line = sp->simlinemgmt.get_line_by_id(line_id);
		} 
		else {
			line = linehandle_t();
		}
	}
	// cannot save convoihandle - try to reconstruct during loading
	if (file->is_loading() && line.is_bound()) {
		for(uint32 i=0; i<line->count_convoys(); i++) {
			convoihandle_t ccc = line->get_convoy(i);
			if (strcmp(ccc->get_name(), start_name)==0) {
				cnv = ccc;
				break;
			}
		}
	}
}

void vehikel_builder_t::rotate90( const sint16 y_size )
{
	pos.rotate90(y_size);
}


void vehikel_builder_t::debug( log_t &file, cstring_t prefix )
{
	file.message("vehb","%s%s build %d for line %s", (const char*)prefix, (const char*)name, nr_vehikel, line.is_bound() ? line->get_name() : "<error>");
	prototyper->debug(file,prefix+ "  " );
}
