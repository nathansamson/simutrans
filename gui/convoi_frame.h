/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef __convoi_frame_h
#define __convoi_frame_h

#include "gui_frame.h"
#include "gui_container.h"
#include "components/gui_scrollbar.h"
#include "components/gui_speedbar.h"
#include "components/gui_label.h"
#include "components/action_listener.h"                                // 28-Dec-2001  Markus Weber    Added
#include "components/gui_button.h"

class spieler_t;
class ware_besch_t;

/**
 * Displays a scrollable list of all convois of a player
 *
 * @author Hj. Malthaner, Sort/Filtering by V. Meyer
 * @date 15-Jun-01
 */
class convoi_frame_t :
	public gui_frame_t ,
	private action_listener_t           //28-Dec-01     Markus Weber    Added , private action_listener_t
{
public:
	enum sort_mode_t { nach_name=0, nach_gewinn=1, nach_typ=2, nach_id=3, SORT_MODES=4 };
	enum filter_flag_t { any_filter=1, name_filter=2, typ_filter=4, ware_filter=8, spezial_filter=16,
		sub_filter=32,	// Ab hier beginnen die Unterfilter!
		lkws_filter=32, zuege_filter=64, schiffe_filter=128, aircraft_filter=256,
		noroute_filter=512, nofpl_filter=1024, noincome_filter=2048, indepot_filter=4096, noline_filter=8192, stucked_filter=16384, monorail_filter=32768, maglev_filter=65536, narrowgauge_filter=131072, tram_filter=131072*2 };

private:
	spieler_t *owner;

	static const char *sort_text[SORT_MODES];

	/**
	* Handle des anzuzeigenden Convois.
	* @author Hj. Malthaner
	*/
	vector_tpl<convoihandle_t> convois;

	// since the scrollpane can be larger than 32767, we use explicitly a scroll bar
	scrollbar_t vscroll;

	// these are part of the top UI
	gui_label_t sort_label;
	button_t	sortedby;
	button_t	sorteddir;
	gui_label_t filter_label;
	button_t	filter_on;
	button_t	filter_details;

	/*
	 * Child window, if open
	 */
	gui_fenster_t *filter_frame;

	/*
	 * All filter settings are static, so they are not reset each
	 * time the window closes.
	 */
	static sort_mode_t sortby;
	static bool sortreverse;

	static uint32 filter_flags;

	static char name_filter_value[64];

	static slist_tpl<const ware_besch_t *> waren_filter;

	static int compare_convois(const void *a, const void *b);

	/**
	 * Check all filters for one convoi.
	 * returns true, if it is not filtered away.
	 * @author V. Meyer
	 */
	static bool passes_filter(convoihandle_t cnv);

public:
	/**
	 * Resorts convois
	 */
	void sort_list();

	convoi_frame_t(spieler_t *sp);

	~convoi_frame_t();

	/**
	 * Events werden hiermit an die GUI-Komponenten
	 * gemeldet
	 * @author V. Meyer
	 */
	void infowin_event(const event_t *ev);

	/**
	 * The filter frame tells us when it is closed.
	 * @author V. Meyer
	 */
	void filter_frame_closed() { filter_frame = NULL; }

	/**
	 * This method is called if the size of the window should be changed
	 * @author Markus Weber
	 */
	void resize(const koord size_change);                       // 28-Dec-01        Markus Weber Added

	/**
	 * komponente neu zeichnen. Die �bergebenen Werte beziehen sich auf
	 * das Fenster, d.h. es sind die Bildschirkoordinaten des Fensters
	 * in dem die Komponente dargestellt wird.
	 * @author Hj. Malthaner
	 */
	void zeichnen(koord pos, koord gr);

	/**
	 * Manche Fenster haben einen Hilfetext assoziiert.
	 * @return den Dateinamen f�r die Hilfe, oder NULL
	 * @author V. Meyer
	 */
	const char * get_hilfe_datei() const {return "convoi.txt"; }

	static sort_mode_t get_sortierung() { return sortby; }
	static void set_sortierung(sort_mode_t sm) { sortby = sm; }

	static bool get_reverse() { return sortreverse; }
	static void set_reverse(bool reverse) { sortreverse = reverse; }

	static bool get_filter(filter_flag_t filter) { return (filter_flags & filter) != 0; }
	static void set_filter(filter_flag_t filter, bool on) { filter_flags = on ? (filter_flags | filter) : (filter_flags & ~filter); }

	static char *access_name_filter() { return name_filter_value; }

	static bool get_ware_filter(const ware_besch_t *ware) { return waren_filter.is_contained(ware); }
	// mode: 0=off, 1=on, -1=toggle
	static void set_ware_filter(const ware_besch_t *ware, int mode);
	static void set_alle_ware_filter(int mode);

	/**
	 * This method is called if an action is triggered
	 * @author Hj. Malthaner
	 *
	 * Returns true, if action is done and no more
	 * components should be triggered.
	 * V.Meyer
	 */
	bool action_triggered(gui_action_creator_t *komp, value_t extra);
};

#endif
