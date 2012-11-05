#ifndef _MAP_CREATOR_H_
#define _MAP_CREATOR_H_

#include "../utils/plainstring.h"

class karte_t;
class koord;
class script_vm_t;
template<class T> class vector_tpl;


class map_creator_t {
	/// pointer to virtual machine
	script_vm_t *script;

	/// name of map definition file (without .nut extension)
	plainstring script_filename;

	/// name of heightfield file (with extension)
	plainstring heightfield_filename;

	plainstring info;

	sint16 hf_width;
	sint16 hf_height;
	sint8 hf_water_level;
public:
	/**
	 * @param filename name of ppm or nut file relative to user directory
	 */
	map_creator_t(const char* path, const char* filename);

	~map_creator_t();

	bool is_valid() const { return heightfield_filename!= NULL;}

	const char* get_info() { return info.c_str(); }

	plainstring const& get_heightfield_filename() const { return heightfield_filename; }

	void create_cities(karte_t *welt, uint32 count, vector_tpl<koord>& pos);
};


#endif
