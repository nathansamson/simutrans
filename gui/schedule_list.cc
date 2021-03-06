/*
 * Copyright (c) 1997 - 2004 Hansj�rg Malthaner
 *
 * Line management
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <stdio.h>

#include "messagebox.h"
#include "schedule_list.h"
#include "line_management_gui.h"
#include "gui_convoiinfo.h"
#include "line_item.h"

#include "../simcolor.h"
#include "../simhalt.h"
#include "../simworld.h"
#include "../simevent.h"
#include "../simgraph.h"
#include "../simskin.h"
#include "../simconvoi.h"
#include "../vehicle/simvehikel.h"
#include "../simwin.h"
#include "../simlinemgmt.h"
#include "../utils/simstring.h"

#include "../bauer/vehikelbauer.h"

#include "../dataobj/fahrplan.h"
#include "../dataobj/translator.h"
#include "../dataobj/umgebung.h"

#include "../boden/wege/kanal.h"
#include "../boden/wege/maglev.h"
#include "../boden/wege/monorail.h"
#include "../boden/wege/narrowgauge.h"
#include "../boden/wege/runway.h"
#include "../boden/wege/schiene.h"
#include "../boden/wege/strasse.h"

#include "components/list_button.h"
#include "halt_list_stats.h"
#include "karte.h"


const char schedule_list_gui_t::cost_type[MAX_LINE_COST][64] =
{
	"Free Capacity",
	"Transported",
	"Revenue",
	"Operation",
	"Profit",
	"Convoys"
};

static uint8 tabs_to_lineindex[9];
static uint8 max_idx=0;

const int schedule_list_gui_t::cost_type_color[MAX_LINE_COST] =
{
	COL_FREE_CAPACITY, COL_TRANSPORTED, COL_REVENUE, COL_OPERATION, COL_PROFIT, COL_VEHICLE_ASSETS
};

uint8 schedule_list_gui_t::statistic[MAX_LINE_COST]={
	LINE_CAPACITY, LINE_TRANSPORTED_GOODS, LINE_REVENUE, LINE_OPERATIONS, LINE_PROFIT, LINE_CONVOIS
};

uint8 schedule_list_gui_t::statistic_type[MAX_LINE_COST]={
	STANDARD, STANDARD, MONEY, MONEY, MONEY, STANDARD
};

#define LINE_NAME_COLUMN_WIDTH ((BUTTON_WIDTH*3)+11+11)
#define SCL_HEIGHT (170)


// Hajo: 17-Jan-04: changed layout to make components fit into
// a width of 400 pixels -> original size was unuseable in 640x480

schedule_list_gui_t::schedule_list_gui_t(spieler_t* sp_) :
	gui_frame_t("Line Management", sp_),
	sp(sp_),
	scrolly(&cont),
	scrolly_haltestellen(&cont_haltestellen),
	scl(gui_scrolled_list_t::select)
{
	capacity = load = 0;
	selection = -1;
	loadfactor = 0;

	button_t button_def;

	// init scrolled list
	scl.set_groesse(koord(LINE_NAME_COLUMN_WIDTH-22, SCL_HEIGHT-18));
	scl.set_pos(koord(0,1));
	scl.set_highlight_color(sp->get_player_color1()+1);
	scl.add_listener(this);

	// tab panel
	tabs.set_pos(koord(11,5));
	tabs.set_groesse(koord(LINE_NAME_COLUMN_WIDTH-22, SCL_HEIGHT));
	tabs.add_tab(&scl, translator::translate("All"));
	max_idx = 0;
	tabs_to_lineindex[max_idx++] = simline_t::line;

	// now add all specific tabs
	if(maglev_t::default_maglev) {
		tabs.add_tab(&scl, translator::translate("Maglev"), skinverwaltung_t::maglevhaltsymbol, translator::translate("Maglev"));
		tabs_to_lineindex[max_idx++] = simline_t::maglevline;
	}
	if(monorail_t::default_monorail) {
		tabs.add_tab(&scl, translator::translate("Monorail"), skinverwaltung_t::monorailhaltsymbol, translator::translate("Monorail"));
		tabs_to_lineindex[max_idx++] = simline_t::monorailline;
	}
	if(schiene_t::default_schiene) {
		tabs.add_tab(&scl, translator::translate("Train"), skinverwaltung_t::zughaltsymbol, translator::translate("Train"));
		tabs_to_lineindex[max_idx++] = simline_t::trainline;
	}
	if(narrowgauge_t::default_narrowgauge) {
		tabs.add_tab(&scl, translator::translate("Narrowgauge"), skinverwaltung_t::narrowgaugehaltsymbol, translator::translate("Narrowgauge"));
		tabs_to_lineindex[max_idx++] = simline_t::narrowgaugeline;
	}
	if(vehikelbauer_t::get_info(tram_wt)!=NULL) {
		tabs.add_tab(&scl, translator::translate("Tram"), skinverwaltung_t::tramhaltsymbol, translator::translate("Tram"));
		tabs_to_lineindex[max_idx++] = simline_t::tramline;
	}
	if(strasse_t::default_strasse) {
		tabs.add_tab(&scl, translator::translate("Truck"), skinverwaltung_t::autohaltsymbol, translator::translate("Truck"));
		tabs_to_lineindex[max_idx++] = simline_t::truckline;
	}
	if(vehikelbauer_t::get_info(water_wt)!=NULL) {
		tabs.add_tab(&scl, translator::translate("Ship"), skinverwaltung_t::schiffshaltsymbol, translator::translate("Ship"));
		tabs_to_lineindex[max_idx++] = simline_t::shipline;
	}
	if(runway_t::default_runway) {
		tabs.add_tab(&scl, translator::translate("Air"), skinverwaltung_t::airhaltsymbol, translator::translate("Air"));
		tabs_to_lineindex[max_idx++] = simline_t::airline;
	}
	tabs.add_listener(this);
	add_komponente(&tabs);

	// editable line name
	inp_name.add_listener(this);
	inp_name.set_pos(koord(LINE_NAME_COLUMN_WIDTH, 14 + SCL_HEIGHT));
	inp_name.set_visible(false);
	add_komponente(&inp_name);

	// load display
	filled_bar.add_color_value(&loadfactor, COL_GREEN);
	filled_bar.set_pos(koord(LINE_NAME_COLUMN_WIDTH, 6 + SCL_HEIGHT));
	filled_bar.set_visible(false);
	add_komponente(&filled_bar);

	// vonvois?
	cont.set_groesse(koord(LINE_NAME_COLUMN_WIDTH, 40));

	// convoi list?
	scrolly.set_pos(koord(LINE_NAME_COLUMN_WIDTH-11, 14 + SCL_HEIGHT+14+4+2*LINESPACE+2));
	scrolly.set_visible(false);
	add_komponente(&scrolly);

	// halt list?
	cont_haltestellen.set_groesse(koord(500, 28));
	scrolly_haltestellen.set_pos(koord(0, 14 + SCL_HEIGHT+BUTTON_HEIGHT+2));
	scrolly_haltestellen.set_visible(false);
	add_komponente(&scrolly_haltestellen);

	// normal buttons edit new remove
	bt_new_line.set_pos(koord(11, 7 + SCL_HEIGHT));
	bt_new_line.set_groesse(koord(BUTTON_WIDTH,BUTTON_HEIGHT));
	bt_new_line.set_typ(button_t::roundbox);
	bt_new_line.set_text("New Line");
	add_komponente(&bt_new_line);
	bt_new_line.add_listener(this);

	bt_change_line.set_pos(koord(11+BUTTON_WIDTH, 7 + SCL_HEIGHT));
	bt_change_line.set_groesse(koord(BUTTON_WIDTH,BUTTON_HEIGHT));
	bt_change_line.set_typ(button_t::roundbox);
	bt_change_line.set_text("Update Line");
	bt_change_line.set_tooltip("Modify the selected line");
	add_komponente(&bt_change_line);
	bt_change_line.add_listener(this);
	bt_change_line.disable();

	bt_delete_line.set_pos(koord(11+2*BUTTON_WIDTH, 7 + SCL_HEIGHT));
	bt_delete_line.set_groesse(koord(BUTTON_WIDTH,BUTTON_HEIGHT));
	bt_delete_line.set_typ(button_t::roundbox);
	bt_delete_line.set_text("Delete Line");
	add_komponente(&bt_delete_line);
	bt_delete_line.add_listener(this);
	bt_delete_line.disable();

	bt_withdraw_line.set_pos(koord(11+0*BUTTON_WIDTH, 7 + SCL_HEIGHT+BUTTON_HEIGHT));
	bt_withdraw_line.set_groesse(koord(BUTTON_WIDTH,BUTTON_HEIGHT));
	bt_withdraw_line.set_typ(button_t::roundbox_state);
	bt_withdraw_line.set_text("Withdraw All");
	bt_withdraw_line.set_tooltip("Convoi is sold when all wagons are empty.");
	add_komponente(&bt_withdraw_line);
	bt_withdraw_line.add_listener(this);
	bt_withdraw_line.disable();

	//CHART
	chart.set_dimension(12, 1000);
	chart.set_pos( koord(LINE_NAME_COLUMN_WIDTH+50,11) );
	chart.set_seed(0);
	chart.set_background(MN_GREY1);
	chart.set_ltr(umgebung_t::other_ltr_graphs);
	add_komponente(&chart);

	// add filter buttons
	for (int i=0; i<MAX_LINE_COST; i++) {
		filterButtons[i].init(button_t::box_state,cost_type[i],koord(0,0), koord(BUTTON_WIDTH,BUTTON_HEIGHT));
		filterButtons[i].add_listener(this);
		filterButtons[i].background = cost_type_color[i];
		add_komponente(filterButtons + i);
	}

	update_lineinfo( linehandle_t() );

	// resize button
	set_min_windowsize(koord(488, 300));
	set_resizemode(diagonal_resize);
	resize(koord(0,0));
	resize(koord(0,100));

	build_line_list(0);
}



/**
 * Mausklicks werden hiermit an die GUI-Komponenten
 * gemeldet
 */
void
schedule_list_gui_t::infowin_event(const event_t *ev)
{
	if(ev->ev_class == INFOWIN) {
		if(ev->ev_code == WIN_CLOSE) {
			// hide schedule on minimap (may not current, but for safe)
			reliefkarte_t::get_karte()->set_current_fpl(NULL, 0); // (*fpl,player_nr)
		}
		else if(  (ev->ev_code==WIN_OPEN  ||  ev->ev_code==WIN_TOP)  &&  line.is_bound() ) {
			// set this schedule as current to show on minimap if possible
			reliefkarte_t::get_karte()->set_current_fpl(line->get_schedule(), sp->get_player_nr()); // (*fpl,player_nr)
		}
	}
	gui_frame_t::infowin_event(ev);
}



bool schedule_list_gui_t::action_triggered( gui_action_creator_t *komp,value_t /* */)           // 28-Dec-01    Markus Weber    Added
{
	if (komp == &bt_change_line) {
		if (line.is_bound()) {
			create_win( new line_management_gui_t(line, sp), w_info, (long)line.get_rep() );
		}
	}
	else if (komp == &bt_new_line) {
		if (tabs.get_active_tab_index() > 0) {
			// create typed line
			assert(tabs.get_active_tab_index()<max_idx);
			uint8 type=tabs_to_lineindex[tabs.get_active_tab_index()];
			linehandle_t new_line = sp->simlinemgmt.create_line(type,sp);
			create_win( new line_management_gui_t(new_line, sp), w_info, (long)line.get_rep());
			update_lineinfo( new_line );
			build_line_list( tabs.get_active_tab_index() );
		}
		else {
			create_win( new news_img("Cannot create generic line!\nSelect line type by\nusing filter tabs."), w_time_delete, magic_none);
		}
	}
	else if (komp == &bt_delete_line) {
		if (line.is_bound()) {
			// close a schedule window, if stil active
			gui_fenster_t *w = win_get_magic( (long)line.get_rep() );
			if(w) {
				destroy_win( w );
			}
			linehandle_t delete_line=line;
			update_lineinfo( linehandle_t() );
			sp->simlinemgmt.delete_line(delete_line);
			build_line_list(tabs.get_active_tab_index());
		}
	}
	else if (komp == &bt_withdraw_line) {
		bt_withdraw_line.pressed ^= 1;
		if (line.is_bound()) {
			line->set_withdraw( bt_withdraw_line.pressed );
		}
	}
	else if (komp == &tabs) {
		update_lineinfo( linehandle_t() );
		build_line_list(tabs.get_active_tab_index());
	}
	else if (komp == &scl) {
		// get selected line
		linehandle_t new_line = linehandle_t();
		selection = scl.get_selection();
		if(  (unsigned)selection < lines.get_count()  ) {
			new_line = lines[selection];
		}
		update_lineinfo(new_line);
		// brute force: just recalculate whole list on each click to keep it current
		build_line_list(tabs.get_active_tab_index());
	}
	else {
		if (line.is_bound()) {
			for ( int i = 0; i<MAX_LINE_COST; i++) {
				if (komp == &filterButtons[i]) {
					filterButtons[i].pressed ^= 1;
					if(filterButtons[i].pressed) {
						chart.show_curve(i);
					}
					else {
						chart.hide_curve(i);
					}
					break;
				}
			}
		}
	}

	return true;
}



void schedule_list_gui_t::zeichnen(koord pos, koord gr)
{
	gui_frame_t::zeichnen(pos, gr);
	if (line.is_bound()) {
		display(pos);
	}
}



void
schedule_list_gui_t::display(koord pos)
{
	int icnv = line->count_convoys();

	char buffer[8192];
	char ctmp[128];

	capacity = load = loadfactor = 0; // total capacity and load of line (=sum of all conv's cap/load)

	sint64 profit = line->get_finance_history(0,LINE_PROFIT);

	for (int i = 0; i<icnv; i++) {
		convoihandle_t cnv = line->get_convoy(i)->self;
		// we do not want to count the capacity of depot convois
		if (!cnv->in_depot()) {
			for (unsigned j = 0; j<cnv->get_vehikel_anzahl(); j++) {
				capacity += cnv->get_vehikel(j)->get_fracht_max();
				load += cnv->get_vehikel(j)->get_fracht_menge();
			}
		}
	}

	// we check if cap is zero, since theoretically a
	// conv can consist of only 1 vehicle, which has no cap (eg. locomotive)
	// and we do not like to divide by zero, do we?
	if (capacity > 0) {
		loadfactor = (load * 100) / capacity;
	}

	switch(icnv) {
		case 0: strcpy(buffer, translator::translate("no convois") );
			break;
		case 1: strcpy(buffer, translator::translate("1 convoi") );
			break;
		default: sprintf(buffer, translator::translate("%d convois"), icnv);
			break;
	}
	int len=display_proportional(pos.x+LINE_NAME_COLUMN_WIDTH-5, pos.y+16+14+SCL_HEIGHT+14+4, buffer, ALIGN_LEFT, COL_BLACK, true );

	int len2 = display_proportional(pos.x+LINE_NAME_COLUMN_WIDTH-5, pos.y+16+14+SCL_HEIGHT+14+4+LINESPACE, translator::translate("Gewinn"), ALIGN_LEFT, COL_BLACK, true );
	money_to_string(ctmp, profit/100.0);
	len2 += display_proportional(pos.x+LINE_NAME_COLUMN_WIDTH+len2, pos.y+16+14+SCL_HEIGHT+14+4+LINESPACE, ctmp, ALIGN_LEFT, profit>=0?MONEY_PLUS:MONEY_MINUS, true );

	int rest_width = max( (get_fenstergroesse().x-LINE_NAME_COLUMN_WIDTH)/2, max(len2,len) );
	number_to_string(ctmp, capacity);
	sprintf(buffer, translator::translate("Capacity: %s\nLoad: %d (%d%%)"), ctmp, load, loadfactor);
	display_multiline_text(pos.x + LINE_NAME_COLUMN_WIDTH + rest_width, pos.y+16 + 14 + SCL_HEIGHT + 14 +4 , buffer, COL_BLACK);
}



void schedule_list_gui_t::resize(const koord delta)
{
	gui_frame_t::resize(delta);

	int rest_width = get_fenstergroesse().x-LINE_NAME_COLUMN_WIDTH;
	int button_per_row=max(1,rest_width/(BUTTON_WIDTH+BUTTON_SPACER));
	int button_rows= MAX_LINE_COST/button_per_row + ((MAX_LINE_COST%button_per_row)!=0);

	scrolly.set_groesse( koord(rest_width+11, get_client_windowsize().y-scrolly.get_pos().y) );
	scrolly_haltestellen.set_groesse( koord(LINE_NAME_COLUMN_WIDTH-11, get_client_windowsize().y-scrolly_haltestellen.get_pos().y) );

	chart.set_groesse(koord(rest_width-11-50, SCL_HEIGHT-11-14-(button_rows*(BUTTON_HEIGHT+BUTTON_SPACER))));
	inp_name.set_groesse(koord(rest_width-11, 14));
	filled_bar.set_groesse(koord(rest_width-11, 4));

	int y=SCL_HEIGHT-11-(button_rows*(BUTTON_HEIGHT+BUTTON_SPACER))+14;
	for (int i=0; i<MAX_LINE_COST; i++) {
		filterButtons[i].set_pos( koord(LINE_NAME_COLUMN_WIDTH+(i%button_per_row)*(BUTTON_WIDTH+BUTTON_SPACER),y+(i/button_per_row)*(BUTTON_HEIGHT+BUTTON_SPACER))  );
	}
}



void schedule_list_gui_t::build_line_list(int filter)
{
	const sint32 sb_offset = line.is_bound() ? scl.get_sb_offset() : 0;
	sint32 sel = -1;
	sp->simlinemgmt.sort_lines();	// to take care of renaming ...
	scl.clear_elements();
	sp->simlinemgmt.get_lines(tabs_to_lineindex[filter], &lines);
	for (vector_tpl<linehandle_t>::const_iterator i = lines.begin(), end = lines.end(); i != end; i++) {
		linehandle_t l = *i;
		scl.append_element( new line_scrollitem_t(l) );
		if (line == l) {
			sel = scl.get_count() - 1;
		}
	}
	scl.set_sb_offset( sb_offset );
	if(  sel>=0  ) {
		scl.set_selection( sel );
		scl.show_selection( sel );
	}
}




/* hides show components */
void schedule_list_gui_t::update_lineinfo(linehandle_t new_line)
{
	if(new_line.is_bound()) {
		// ok, this line is visible
		scrolly.set_visible(true);
		scrolly_haltestellen.set_visible(true);
		inp_name.set_text(new_line->get_name(), 128);
		inp_name.set_visible(true);
		filled_bar.set_visible(true);

		// fill container with info of line's convoys
		// we do it here, since this list needs only to be
		// refreshed when the user selects a new line
		int i, icnv = 0;
		icnv = new_line->count_convoys();
		// display convoys of line
		cont.remove_all();
		int ypos = 5;
		for(i = 0;  i<icnv;  i++  ) {
			gui_convoiinfo_t *cinfo = new gui_convoiinfo_t(new_line->get_convoy(i)->self, i + 1);
			cinfo->set_pos(koord(0, ypos));
			cinfo->set_groesse(koord(400, 40));
			cont.add_komponente(cinfo);
			ypos += 40;
		}
		cont.set_groesse(koord(500, ypos));

		bt_delete_line.disable();
		bt_withdraw_line.disable();
		if(icnv>0) {
			bt_withdraw_line.enable();
		}
		else {
			bt_delete_line.enable();
		}
		bt_change_line.enable();

		new_line->recalc_catg_index();	// update withdraw info
		bt_withdraw_line.pressed = new_line->get_withdraw();

		// fill haltestellen container with info of line's haltestellen
		cont_haltestellen.remove_all();
		ypos = 5;
//		slist_tpl<koord3d> tmp; // stores koords of stops that are allready displayed
		for(i=0; i<new_line->get_schedule()->get_count(); i++) {
			const koord3d fahrplan_koord = new_line->get_schedule()->eintrag[i].pos;
			halthandle_t halt = haltestelle_t::get_halt(sp->get_welt(),fahrplan_koord, sp);
			if (halt.is_bound()) {
//				// only add a haltestelle to the list, if it is not in the list allready
//				if (!tmp.is_contained(fahrplan_koord)) {
					halt_list_stats_t* cinfo = new halt_list_stats_t(halt);
					cinfo->set_pos(koord(0, ypos));
					cinfo->set_groesse(koord(500, 28));
					cont_haltestellen.add_komponente(cinfo);
					ypos += 28;
//					tmp.append(fahrplan_koord);
//				}
			}
		}
		cont_haltestellen.set_groesse(koord(500, ypos));

		// chart
		chart.remove_curves();
		for(i=0; i<MAX_LINE_COST; i++)  {
			chart.add_curve(cost_type_color[i], new_line->get_finance_history(), MAX_LINE_COST, statistic[i], MAX_MONTHS, statistic_type[i], filterButtons[i].pressed, true);
			if(filterButtons[i].pressed) {
				chart.show_curve(i);
			}
		}
		chart.set_visible(true);

		// set this schedule as current to show on minimap if possible
		reliefkarte_t::get_karte()->set_current_fpl(new_line->get_schedule(), sp->get_player_nr()); // (*fpl,player_nr)
	}
	else if(line.is_bound()) {
		// previously a line was visible
		// thus the need to hide everything
		cont.remove_all();
		inp_name.set_visible(false);
		filled_bar.set_visible(false);
		scrolly.set_visible(false);
		scrolly_haltestellen.set_visible(false);
		inp_name.set_visible(false);
		filled_bar.set_visible(false);
		scl.set_selection(-1);
		bt_withdraw_line.disable();
		bt_withdraw_line.pressed = false;
		bt_delete_line.disable();
		bt_change_line.disable();
		for(int i=0; i<MAX_LINE_COST; i++)  {
			chart.hide_curve(i);
		}
		chart.set_visible(true);

		// hide schedule on minimap (may not current, but for safe)
		reliefkarte_t::get_karte()->set_current_fpl(NULL, 0); // (*fpl,player_nr)
	}
	line = new_line;
}

void schedule_list_gui_t::show_lineinfo(linehandle_t line)
{
	update_lineinfo(line);
	// rebuilding line list will also show selection
	for(  uint8 i=0;  i<max_idx;  i++  ) {
		if(  tabs_to_lineindex[i]==line->get_linetype()  ) {
			tabs.set_active_tab_index( i );
			build_line_list( i );
			break;
		}
	}
}

