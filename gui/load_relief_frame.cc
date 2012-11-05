/*
 * Copyright (c) 1997 - 2001 Hansjörg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <string>
#include <stdio.h>

#include "../simworld.h"
#include "load_relief_frame.h"
#include "../dataobj/map_creator.h"
#include "../dataobj/translator.h"
#include "../dataobj/einstellungen.h"
#include "../dataobj/umgebung.h"
#include "../utils/cbuffer_t.h"

/**
 * Aktion, die nach Knopfdruck gestartet wird.
 * @author Hansjörg Malthaner
 */
void load_relief_frame_t::action(const char *fullpath)
{
	map_creator_t *mp = all_maps.remove(fullpath);
	if (mp) {
		sets->heightfield = mp->get_heightfield_filename();
		sets->creator = mp;
	}
}


load_relief_frame_t::load_relief_frame_t(settings_t* const sets) : savegame_frame_t(NULL, false, NULL)
{
	static cbuffer_t pakset_maps;
	static cbuffer_t addons_maps;

	pakset_maps.clear();
	pakset_maps.printf("%s%smaps/", umgebung_t::program_dir, umgebung_t::objfilename.c_str());

	addons_maps.clear();
	addons_maps.printf("%smaps/", umgebung_t::user_dir);

	this->add_path(pakset_maps);
	this->add_path(addons_maps);

	set_name(translator::translate("Laden"));
	this->sets = sets;
	sets->heightfield = "";
}


load_relief_frame_t::~load_relief_frame_t()
{
	while( !all_maps.empty() ) {
		map_creator_t *mp = all_maps.remove_first();
		delete mp;
	}
}


const char *load_relief_frame_t::get_info(const char *fullpath)
{
	if ( map_creator_t *mp = all_maps.get(fullpath) ) {
		return mp->get_info();
	}

	map_creator_t *mp = new map_creator_t(this->get_basename(fullpath).c_str(), this->get_filename(fullpath).c_str());

	if (mp->is_valid()) {
		all_maps.put(fullpath, mp);
		return mp->get_info();
	}
	delete mp;
	return "";
}


bool load_relief_frame_t::check_file( const char *fullpath, const char * )
{
	if ( map_creator_t *mp = all_maps.get(fullpath) ) {
		return mp->is_valid();
	}

	map_creator_t *mp = new map_creator_t(this->get_basename(fullpath).c_str(), this->get_filename(fullpath).c_str());

	if (mp->is_valid()) {
		all_maps.put(fullpath, mp);
		return true;
	}
	delete mp;
	return false;
}
