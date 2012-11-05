#include "map_creator.h"
#include "umgebung.h"
#include "../simcity.h"
#include "../simworld.h"
#include "../simsys.h"
#include "../utils/cbuffer_t.h"
// scripting
#include "../script/script.h"
#include "../script/api_param.h"
#include "../squirrel/squirrel.h"

map_creator_t::map_creator_t(const char* path, const char* filename)
{
	hf_water_level = 0;
	bool scripted = (strlen(filename) > 4)  &&  (strcmp(filename + strlen(filename) -4, ".nut")==0);

	if (!scripted) {
		script = NULL;
		script_filename = NULL;
		heightfield_filename = filename;
	}
	else {
		// start script
		script = new script_vm_t();

		// load base file
		chdir(umgebung_t::program_dir);
		const char* basefile = "script/map_base.nut";
		const char* err = script->call_script(basefile);
		chdir( umgebung_t::user_dir );
		if (err) { // should not happen ...
			dbg->error("map_creator_t::map_creator_t", "error [%s] calling %s", err, basefile);
			goto err;
		}

		// load map script
		cbuffer_t buf;
		buf.printf("%s/%s", path, filename);
		err = script->call_script( (const char*)buf );
		if (err) {
			dbg->error("map_creator_t::map_creator_t", "error [%s] calling %s", err, filename);
			goto err;
		}

		// get heightfield name
		if ((err = script->call_function("get_heightmap_file", heightfield_filename))) {
			dbg->warning("map_creator_t::map_creator_t", "error [%s] calling get_heightmap_file", err, filename);
			goto err;
		}

		// get water level
		hf_water_level = 0;
		script->call_function("get_water_height", hf_water_level);
	}
	// new block for variables
	{
		cbuffer_t buf;
		buf.printf("%s/%s", path, heightfield_filename.c_str());
		heightfield_filename = (const char*)buf;
	}

	// load heightfield
	sint8 *h_field ;
	// raw height data: file.ppm
	if(karte_t::get_height_data_from_file(heightfield_filename, hf_water_level, h_field, hf_width, hf_height, true )) {
		cbuffer_t buf;
		buf.printf("%d x %d", hf_width, hf_height);
		info = (const char*) buf;
		return;
	}
	// error during loading
err:
	delete script;
	script = NULL;
	script_filename = NULL;
	heightfield_filename = NULL;
}

map_creator_t::~map_creator_t()
{
	delete script;
}


void map_creator_t::create_cities(karte_t *welt, uint32 count, vector_tpl<koord>& locations)
{
	if (script == NULL) {
		return;
	}
	HSQUIRRELVM vm = script->get_vm();
	script_api::welt = welt;

	sq_pushroottable(vm);
	sq_pushstring(vm, "cities", -1);
	if (!SQ_SUCCEEDED(sq_get(vm, -2))) {
		goto end;
	}

	for(uint32 i=0; i<count; i++) {
		sq_pushinteger(vm, i);
		if (!SQ_SUCCEEDED(sq_get(vm, -2))) {
			break;
		}
		koord pos(koord::invalid);
		sq_pushstring(vm, "pos", -1);
		if (SQ_SUCCEEDED(sq_get(vm, -2))) {
			pos = script_api::param<koord>::get(vm, -1);
			sq_pop(vm, 1);
		}
		sint32 size = 1;
		sq_pushstring(vm, "size", -1);
		if (SQ_SUCCEEDED(sq_get(vm, -2))) {
			size = script_api::param<sint32>::get(vm, -1);
			sq_pop(vm, 1);
		}

		if (welt->ist_in_kartengrenzen(pos)) {
			stadt_t* s = new stadt_t( welt->get_spieler(1), pos, size );
			welt->add_stadt(s);
			s->laden_abschliessen();

			const char* name = NULL;
			sq_pushstring(vm, "name", -1);
			if (SQ_SUCCEEDED(sq_get(vm, -2))) {
				name = script_api::param<const char*>::get(vm, -1);
				if (name) {
					s->set_name(name);
				}
				sq_pop(vm, 1);
			}
			locations.append(pos);
		}
		sq_pop(vm, 1);
	}
	// pop cities array
	sq_pop(vm, 1);
end:	// pop root table
	sq_pop(vm, 1);
}

