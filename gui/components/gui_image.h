/*
 * just displays an image
 *
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef gui_image_h
#define gui_image_h

#include "../../display/simimg.h"
#include "../../display/simgraph.h"
#include "gui_komponente.h"



class gui_image_t : public gui_komponente_t
{
	public:
		enum size_modes {
			size_mode_normal, // Do not change size with image
			size_mode_auto,   // Set size to image size
			size_mode_stretch // Stretch image to control size (not implemented yet)
		};

	private:
		control_alignment_t alignment;
		size_modes          size_mode;
		image_id            id;
		uint16              player_nr;
		koord               remove_offset;
		bool                remove_enabled;

	public:
		gui_image_t( const image_id i=IMG_LEER, const uint8 p=0, control_alignment_t alignment_par = ALIGN_NONE, bool remove_offset = false );
		void set_groesse( koord size_par ) OVERRIDE;
		void set_image( const image_id i, bool remove_offsets = false );

		void enable_offset_removal(bool remove_offsets) { set_image(id,remove_offsets); }

		/**
		 * Zeichnet die Komponente
		 * @author Hj. Malthaner
		 */
		void zeichnen( koord offset );
};

#endif
