/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 *
 * Renovation in dec 2004 for other vehicles, timeline
 * @author prissi
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../simcity.h"
#include "../simcolor.h"
#include "../simconvoi.h"
#include "../simdebug.h"
#include "../simfab.h"
#include "../simgraph.h"
#include "../simhalt.h"
#include "../simimg.h"
#include "../simintr.h"
#include "../simmesg.h"
#include "../simskin.h"
#include "../simsound.h"
#include "../simtools.h"
#include "../simware.h"
#include "../simwerkz.h"
#include "../simwin.h"
#include "../simworld.h"

#include "../bauer/brueckenbauer.h"
#include "../bauer/hausbauer.h"
#include "../bauer/tunnelbauer.h"
#include "../bauer/vehikelbauer.h"
#include "../bauer/warenbauer.h"
#include "../bauer/wegbauer.h"

#include "../besch/grund_besch.h"
#include "../besch/skin_besch.h"
#include "../besch/sound_besch.h"
#include "../besch/weg_besch.h"

#include "../boden/boden.h"
#include "../boden/grund.h"
#include "../boden/wege/schiene.h"
#include "../boden/wege/strasse.h"
#include "../boden/wege/weg.h"

#include "../dataobj/einstellungen.h"
#include "../dataobj/scenario.h"
#include "../dataobj/fahrplan.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/translator.h"
#include "../dataobj/umgebung.h"

#include "../dings/gebaeude.h"
#include "../dings/wayobj.h"
#include "../dings/zeiger.h"

#include "../gui/messagebox.h"
#include "../gui/money_frame.h"
#include "../gui/schedule_list.h"

#include "../sucher/bauplatz_sucher.h"

#include "../utils/simstring.h"

#include "../vehicle/simvehikel.h"

#include "simplay.h"

karte_t *spieler_t::welt = NULL;


spieler_t::spieler_t(karte_t *wl, uint8 nr) :
	simlinemgmt(wl)
{
	welt = wl;
	player_nr = nr;
	set_player_color( nr*8, nr*8+24 );

	konto = umgebung_t::starting_money;

	konto_ueberzogen = 0;
	automat = false;		// Start nicht als automatischer Spieler

	headquarter_pos = koord::invalid;
	headquarter_level = 0;

	/**
	 * initialize finance history array
	 * @author hsiegeln
	 */

	for (int year=0; year<MAX_PLAYER_HISTORY_YEARS; year++) {
		for (int cost_type=0; cost_type<MAX_PLAYER_COST; cost_type++) {
			finance_history_year[year][cost_type] = 0;
			if ((cost_type == COST_CASH) || (cost_type == COST_NETWEALTH)) {
				finance_history_year[year][cost_type] = umgebung_t::starting_money;
			}
		}
	}

	for (int month=0; month<MAX_PLAYER_HISTORY_MONTHS; month++) {
		for (int cost_type=0; cost_type<MAX_PLAYER_COST; cost_type++) {
			finance_history_month[month][cost_type] = 0;
			if ((cost_type == COST_CASH) || (cost_type == COST_NETWEALTH)) {
				finance_history_month[month][cost_type] = umgebung_t::starting_money;
			}
		}
	}

	haltcount = 0;

	maintenance = 0;

	last_message_index = 0;

	// we have different AI, try to find out our type:
	sprintf(spieler_name_buf,"player %i",player_nr-1);
}



spieler_t::~spieler_t()
{
	destroy_win( (long)this );
}



/* returns the name of the player; "player -1" sits in front of the screen
 * @author prissi
 */
const char* spieler_t::gib_name(void) const
{
	return translator::translate(spieler_name_buf);
}



/**
 * Zeigt Meldungen aus der Queue des Spielers auf dem Bildschirm an
 * @author Hj. Malthaner
 */
void spieler_t::display_messages()
{
	const sint16 raster = get_tile_raster_width();
	int last_displayed_message = -1;
	const sint16 yoffset = welt->gib_y_off()+((display_get_width()/raster)&1)*(raster/4);

	for(int n=0; n<=last_message_index; n++) {
		if(text_alter[n] >= -80) {
			const koord ij = text_pos[n]-welt->get_world_position()-welt->gib_ansicht_ij_offset();
			const sint16 x = (ij.x-ij.y)*(raster/2) + welt->gib_x_off();
			const sint16 y = (ij.x+ij.y)*(raster/4) + (text_alter[n] >> 4) - tile_raster_scale_y( welt->lookup_hgt(text_pos[n])*TILE_HEIGHT_STEP, raster) + yoffset;

			display_proportional_clip( x+1, y+1, texte[n], ALIGN_LEFT, COL_BLACK, true);
			display_proportional_clip( x, y, texte[n], ALIGN_LEFT, PLAYER_FLAG|(kennfarbe1+3), true);
			last_displayed_message = n;
		}
	}

	last_message_index = last_displayed_message;
}



/**
 * Age messages (move them upwards)
 * @author Hj. Malthaner
 */
void
spieler_t::age_messages(long /*delta_t*/)
{
	for(int n=0; n<=last_message_index; n++) {
		if(text_alter[n] >= -80) {
			text_alter[n] -= 5;//delta_t>>2;
		}
	}
}



void
spieler_t::add_message(koord k, int betrag)
{
	for(int n=0; n<50; n++) {
		if(text_alter[n] <= -80) {
			text_pos[n] = k;

			money_to_string(texte[n], betrag/100.0);
			text_alter[n] = 127;

			if(n > last_message_index) {
				last_message_index = n;
			}

			break;
		}
	}
}



void spieler_t::set_player_color(uint8 col1, uint8 col2)
{
	kennfarbe1 = col1;
	kennfarbe2 = col2;
	display_set_player_color_scheme( player_nr, col1, col2 );
}



/**
 * Wird von welt in kurzen abst�nden aufgerufen
 * @author Hj. Malthaner
 */
void spieler_t::step()
{
	// die haltestellen m�ssen die Fahrpl�ne rgelmaessig pruefen
	uint8 i = (uint8)(welt->gib_steps()+player_nr);
	slist_iterator_tpl <halthandle_t> iter( halt_list );
	while(iter.next()) {
		if( (i & 31) == 0 ) {
			iter.get_current()->step();
			INT_CHECK("simplay 156");
		}
		i++;
	}
}



/**
 * wird von welt nach jedem monat aufgerufen
 * @author Hj. Malthaner
 */
void spieler_t::neuer_monat()
{
	// since the messages must remain on the screen longer ...
	static char buf[256];


	// Wartungskosten abziehen
	calc_finance_history();
	roll_finance_history_month();

	if(welt->get_last_month()==0) {
		roll_finance_history_year();
	}

	// new month has started => recalculate vehicle value
	sint64 assets = 0;
	for(  vector_tpl<convoihandle_t>::const_iterator i = welt->convois_begin(), end = welt->convois_end();  i != end;  ++i  ) {
		convoihandle_t cnv = *i;
		if(cnv->gib_besitzer()==this) {
			assets += cnv->calc_restwert();
		}
	}
	finance_history_year[0][COST_ASSETS] = finance_history_month[0][COST_ASSETS] = assets;
	finance_history_year[0][COST_NETWEALTH] = finance_history_month[0][COST_NETWEALTH] = assets+konto;

	calc_finance_history();

	simlinemgmt.new_month();

	// subtract maintenance
	buche( -((sint64)maintenance) <<((sint64)welt->ticks_bits_per_tag-18ll), COST_MAINTENANCE);

	// enough money and scenario finished?
	if(konto > 0  &&  welt->get_scenario()->active()  &&  finance_history_year[0][COST_SCENARIO_COMPLETED]>=100) {
		destroy_all_win();
		sint32 time = welt->get_current_month()-(welt->gib_einstellungen()->gib_starting_year()*12);
		sprintf( buf, translator::translate("Congratulation\nScenario was complete in\n%i months %i years."), time%12, time/12 );
		create_win(280, 40, new news_img(buf), w_info, magic_none);
		// disable further messages
		welt->get_scenario()->init("",welt);
		return;
	}

	// Bankrott ?
	if(!umgebung_t::freeplay) {
		if(konto < 0) {
			konto_ueberzogen++;
			if(this == welt->gib_spieler(0)) {
				if(finance_history_year[0][COST_NETWEALTH]<0) {
					destroy_all_win();
					create_win(280, 40, new news_img("Bankrott:\n\nDu bist bankrott.\n"), w_info, magic_none);
					welt->beenden(false);
				}
				else {
					// tell the player
					sprintf(buf, translator::translate("On loan since %i month(s)"), konto_ueberzogen );
//					sprintf(buf,translator::translate("Verschuldet:\n\nDu hast %d Monate Zeit,\ndie Schulden zurueckzuzahlen.\n"), MAX_KONTO_VERZUG-konto_ueberzogen+1 );
					welt->get_message()->add_message(buf,koord::invalid,message_t::problems,player_nr,IMG_LEER);
				}
			}
			else if(automat  &&  this!=welt->gib_spieler(1)) {
				// for AI, we only declare bankrupt, if total assest are below zero
				if(finance_history_year[0][COST_NETWEALTH]<0) {
					ai_bankrupt();
				}
			}
		}
		else {
			konto_ueberzogen = 0;
		}
	}
}



/**
* we need to roll the finance history every year, so that
* the most recent year is at position 0, etc
* @author hsiegeln
*/
void spieler_t::roll_finance_history_month()
{
	int i;
	for (i=MAX_PLAYER_HISTORY_MONTHS-1; i>0; i--) {
		for (int cost_type = 0; cost_type<MAX_PLAYER_COST; cost_type++) {
			finance_history_month[i][cost_type] = finance_history_month[i-1][cost_type];
		}
	}
	for (int i=0;  i<MAX_PLAYER_COST;  i++) {
		finance_history_month[0][i] = 0;
	}
}



void spieler_t::roll_finance_history_year()
{
	int i;
	for (i=MAX_PLAYER_HISTORY_YEARS-1; i>0; i--) {
		for (int cost_type = 0; cost_type<MAX_PLAYER_COST; cost_type++) {
			finance_history_year[i][cost_type] = finance_history_year[i-1][cost_type];
		}
	}
	for (int i=0;  i<MAX_PLAYER_COST;  i++) {
		finance_history_year[0][i] = 0;
	}
}



void spieler_t::calc_finance_history()
{
	/**
	* copy finance data into historical finance data array
	* @author hsiegeln
	*/
	sint64 profit, mprofit;
	profit = mprofit = 0;
	for (int i=0; i<MAX_PLAYER_COST; i++) {
		// all costs < COST_ASSETS influence profit, so we must sum them up
		if(i<COST_ASSETS) {
			profit += finance_history_year[0][i];
			mprofit += finance_history_month[0][i];
		}
	}

	finance_history_year[0][COST_PROFIT] = profit;
	finance_history_month[0][COST_PROFIT] = mprofit;

	finance_history_year[0][COST_NETWEALTH] = finance_history_year[0][COST_ASSETS] + konto;
	finance_history_year[0][COST_CASH] = konto;
	finance_history_year[0][COST_OPERATING_PROFIT] = finance_history_year[0][COST_INCOME] + finance_history_year[0][COST_VEHICLE_RUN] + finance_history_year[0][COST_MAINTENANCE];
	sint64 margin_div = (finance_history_year[0][COST_VEHICLE_RUN] + finance_history_year[0][COST_MAINTENANCE]);
	if(margin_div<0) {
		margin_div = -margin_div;
	}
	finance_history_year[0][COST_MARGIN] = margin_div!= 0 ? (100*finance_history_year[0][COST_OPERATING_PROFIT]) / margin_div : 0;

	finance_history_month[0][COST_NETWEALTH] = finance_history_month[0][COST_ASSETS] + konto;
	finance_history_month[0][COST_CASH] = konto;
	finance_history_month[0][COST_OPERATING_PROFIT] = finance_history_month[0][COST_INCOME] + finance_history_month[0][COST_VEHICLE_RUN] + finance_history_month[0][COST_MAINTENANCE];
	margin_div = (finance_history_month[0][COST_VEHICLE_RUN] + finance_history_month[0][COST_MAINTENANCE]);
	if(margin_div<0) {
		margin_div = -margin_div;
	}
	finance_history_month[0][COST_MARGIN] = margin_div!=0 ? (100*finance_history_month[0][COST_OPERATING_PROFIT]) / margin_div : 0;
	finance_history_month[0][COST_SCENARIO_COMPLETED] = finance_history_year[0][COST_SCENARIO_COMPLETED] = welt->get_scenario()->completed(player_nr);
}



// add and amount, including the display of the message and some other things ...
void spieler_t::buche(const sint64 betrag, const koord pos, enum player_cost type)
{
	buche(betrag, type);

	if(betrag != 0) {
		add_message(pos, betrag);

		if(!(labs((sint32)betrag)<=10000)) {
			struct sound_info info;

			info.index = SFX_CASH;
			info.volume = 255;
			info.pri = 0;

			welt->play_sound_area_clipped(pos, info);
		}
	}
}



// add an amout to a subcategory
void spieler_t::buche(const sint64 betrag, enum player_cost type)
{
	assert(type < MAX_PLAYER_COST);

	finance_history_year[0][type] += betrag;
	finance_history_month[0][type] += betrag;

	if(type < COST_ASSETS) {
		konto += betrag;

		// fill year history
		finance_history_year[0][COST_PROFIT] += betrag;
		finance_history_year[0][COST_CASH] = konto;
		// fill month history
		finance_history_month[0][COST_PROFIT] += betrag;
		finance_history_month[0][COST_CASH] = konto;
		// the other will be updated only monthly or when a finance window is shown
	}
}



void spieler_t::accounting( spieler_t *sp, const sint64 amount, koord k, enum player_cost pc )
{
	if(sp!=NULL  &&  sp!=welt->gib_spieler(1)) {
		sp->buche( amount, k, pc );
	}
}




bool spieler_t::check_owner( const spieler_t *owner, const spieler_t *test )
{
	return owner == test  ||  owner == NULL  ||  test == NULL  ||  test == welt->gib_spieler(1);
}



/**
 * Erzeugt eine neue Haltestelle des Spielers an Position pos
 * @author Hj. Malthaner
 */
halthandle_t spieler_t::halt_add(koord pos)
{
	halthandle_t halt = haltestelle_t::create(welt, pos, this);
	halt_add(halt);
	return halt;
}



/**
 * Erzeugt eine neue Haltestelle des Spielers an Position pos
 * @author Hj. Malthaner
 */
void
spieler_t::halt_add(halthandle_t halt)
{
	if (!halt_list.contains(halt)) {
		halt_list.append(halt);
		haltcount ++;
	}
}



/**
 * Entfernt eine Haltestelle des Spielers aus der Liste
 * @author Hj. Malthaner
 */
void
spieler_t::halt_remove(halthandle_t halt)
{
	halt_list.remove(halt);
}



void spieler_t::ai_bankrupt()
{
	DBG_MESSAGE("spieler_t::ai_bankrupt()","Removing convois");

	for( int i = welt->get_convoi_count()-1;  i>=0;  i--  ) {
		const convoihandle_t cnv = welt->get_convoi(i);
		if(cnv->gib_besitzer()!=this) {
			continue;
		}

		linehandle_t line = cnv->get_line();

		cnv->self_destruct();
		cnv->step();	// to really get rid of it

		// last vehicle on that connection (no line => railroad)
		if(  !line.is_bound()  ||  line->count_convoys()==0  ) {
			simlinemgmt.delete_line( line );
		}
	}

	// remove headquarter pos
	headquarter_pos = koord::invalid;

	// remove all stops
	while(halt_list.count()>0) {
		halthandle_t h = halt_list.remove_first();
		haltestelle_t::destroy( h );
	}

	// next remove all ways, depot etc, that are not road or channels
	for( int y=0;  y<welt->gib_groesse_y();  y++  ) {
		for( int x=0;  x<welt->gib_groesse_x();  x++  ) {
			planquadrat_t *plan = welt->access(x,y);
			for(  int b=plan->gib_boden_count()-1;  b>=0;  b--  ) {
				grund_t *gr = plan->gib_boden_bei(b);
				if(  gr->gib_typ()==grund_t::brueckenboden  &&  gr->obj_bei(0)->gib_besitzer()==this  ) {
					brueckenbauer_t::remove( welt, this, gr->gib_pos(), (waytype_t)gr->gib_weg_nr(0)->gib_waytype() );
				}
				else if(  gr->gib_typ()==grund_t::tunnelboden  &&  gr->obj_bei(0)->gib_besitzer()==this  ) {
					tunnelbauer_t::remove( welt, this, gr->gib_pos(), gr->gib_weg_nr(0)->gib_waytype() );
				}
				else {
					for(  int i=gr->gib_top()-1;  i>=0;  i--  ) {
						ding_t *dt = gr->obj_bei(i);
						if(dt->gib_besitzer()==this) {
							switch(dt->gib_typ()) {
								case ding_t::airdepot:
								case ding_t::bahndepot:
								case ding_t::monoraildepot:
								case ding_t::tramdepot:
								case ding_t::strassendepot:
								case ding_t::schiffdepot:
								case ding_t::leitung:
								case ding_t::senke:
								case ding_t::pumpe:
								case ding_t::signal:
								case ding_t::wayobj:
								case ding_t::roadsign:
									dt->entferne(this);
									delete dt;
									break;
								case ding_t::gebaeude:
									hausbauer_t::remove( welt, this, (gebaeude_t *)dt );
									break;
								case ding_t::way:
								{
									weg_t *w=(weg_t *)dt;
									if(!gr->ist_karten_boden()  ||  w->gib_waytype()==road_wt  ||  w->gib_waytype()==water_wt) {
										add_maintenance( -w->gib_besch()->gib_wartung() );
										w->setze_besitzer( NULL );
									}
									else {
										gr->weg_entfernen( w->gib_waytype(), true );
									}
									break;
								}
								default:
									gr->obj_bei(i)->setze_besitzer( welt->gib_spieler(1) );
							}
						}
					}
				}
			}
		}
	}

	automat = false;
	char buf[256];
	sprintf(buf, translator::translate("%s\nwas liquidated."), gib_name() );
	welt->get_message()->add_message( buf, koord::invalid, message_t::ai, player_nr );
}



/**
 * Speichert Zustand des Spielers
 * @param file Datei, in die gespeichert wird
 * @author Hj. Malthaner
 */
void spieler_t::rdwr(loadsave_t *file)
{
	sint32 halt_count=0;

	file->rdwr_delim("Sp ");
	file->rdwr_longlong(konto, " ");
	file->rdwr_long(konto_ueberzogen, " ");

	if(file->get_version()<101000) {
		// ignore steps
		sint32 ldummy=0;
		file->rdwr_long(ldummy, " ");
	}

	if(file->get_version()<99009) {
		sint32 farbe;
		file->rdwr_long(farbe, " ");
		kennfarbe1 = (uint8)farbe*2;
		kennfarbe2 = kennfarbe1+24;
	}
	else {
		file->rdwr_byte(kennfarbe1, " ");
		file->rdwr_byte(kennfarbe2, " ");
	}
	if(file->get_version()<99008) {
		file->rdwr_long(halt_count, " ");
	}
	file->rdwr_long(haltcount, " ");

	if (file->get_version() < 84008) {
		// not so old save game
		for (int year = 0;year<MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<MAX_PLAYER_COST; cost_type++) {
				if (file->get_version() < 84007) {
					// a cost_type has has been added. For old savegames we only have 9 cost_types, now we have 10.
					// for old savegames only load 9 types and calculate the 10th; for new savegames load all 10 values
					if (cost_type < 9) {
						file->rdwr_longlong(finance_history_year[year][cost_type], " ");
					} else {
						sint64 tmp = finance_history_year[year][COST_VEHICLE_RUN] + finance_history_year[year][COST_MAINTENANCE];
						if(tmp<0) { tmp = -tmp; }
						finance_history_year[year][COST_MARGIN] = (tmp== 0) ? 0 : (finance_history_year[year][COST_OPERATING_PROFIT] * 100) / tmp;
					}
				} else {
					if (cost_type < 10) {
						file->rdwr_longlong(finance_history_year[year][cost_type], " ");
					} else {
						sint64 tmp = finance_history_year[year][COST_VEHICLE_RUN] + finance_history_year[year][COST_MAINTENANCE];
						if(tmp<0) { tmp = -tmp; }
						finance_history_year[year][COST_MARGIN] = (tmp==0) ? 0 : (finance_history_year[year][COST_OPERATING_PROFIT] * 100) / tmp;
					}
				}
			}
//DBG_MESSAGE("player_t::rdwr()", "finance_history[year=%d][cost_type=%d]=%ld", year, cost_type,finance_history_year[year][cost_type]);
		}
	}
	else if (file->get_version() < 86000) {
		for (int year = 0;year<MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<10; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type], " ");
			}
			sint64 tmp = finance_history_year[year][COST_VEHICLE_RUN] + finance_history_year[year][COST_MAINTENANCE];
			if(tmp<0) { tmp = -tmp; }
			finance_history_year[year][COST_MARGIN] = (tmp== 0) ? 0 : (finance_history_year[year][COST_OPERATING_PROFIT] * 100) / tmp;
		}
		// in 84008 monthly finance history was introduced
		for (int month = 0;month<MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<10; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type], " ");
			}
			sint64 tmp = finance_history_month[month][COST_VEHICLE_RUN] + finance_history_month[month][COST_MAINTENANCE];
			if(tmp<0) { tmp = -tmp; }
			finance_history_month[month][COST_MARGIN] = (tmp==0) ? 0 : (finance_history_month[month][COST_OPERATING_PROFIT] * 100) / tmp;
		}
	}
	else if (file->get_version() < 99011) {
		// powerline category missing
		for (int year = 0;year<MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<12; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type], " ");
			}
		}
		for (int month = 0;month<MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<12; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type], " ");
			}
		}
	}
	else if (file->get_version() < 99017) {
		// without detailed goo statistics
		for (int year = 0;year<MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<13; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type], " ");
			}
		}
		for (int month = 0;month<MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<13; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type], " ");
			}
		}
	}
	else {
		// most recent savegame version
		for (int year = 0;year<MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<MAX_PLAYER_COST; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type], " ");
			}
		}
		for (int month = 0;month<MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<MAX_PLAYER_COST; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type], " ");
			}
		}
	}
	// we have to pay maintenance at the beginning of a month
	if(file->get_version()<99018  &&  file->is_loading()) {
		buche( -finance_history_month[1][COST_MAINTENANCE], COST_MAINTENANCE );
	}

	file->rdwr_bool(automat, "\n");

	// state is not saved anymore
	if(file->get_version()<99014) {
		sint32 ldummy=0;
		file->rdwr_long(ldummy, " ");
		file->rdwr_long(ldummy, "\n");
	}

	// the AI stuff is now saved directly by the different AI
	if(  file->get_version()<101000) {
		sint32 ldummy = -1;
		file->rdwr_long(ldummy, " ");
		file->rdwr_long(ldummy, "\n");
		file->rdwr_long(ldummy, " ");
		file->rdwr_long(ldummy, "\n");
		koord k(-1,-1);
		k.rdwr( file );
		k.rdwr( file );
	}

	// Hajo: sanity checks
	if(halt_count < 0  ||  haltcount < 0) {
		dbg->fatal("spieler_t::rdwr()", "Halt count is out of bounds: %d -> corrupt savegame?", halt_count|haltcount);
	}

	if(file->is_loading()) {
		// first: financial sanity check
		for (int year = 0;year<MAX_PLAYER_HISTORY_YEARS;year++) {
			sint64 value=0;
			for (int cost_type = 0; cost_type<MAX_PLAYER_COST; cost_type++) {
				value += finance_history_year[year][cost_type];
			}
		}

		// halt_count will be zero for newer savegames
DBG_DEBUG("spieler_t::rdwr()","player %i: loading %i halts.",welt->sp2num( this ),halt_count);
		for(int i=0; i<halt_count; i++) {
			halthandle_t halt = haltestelle_t::create( welt, file );
			// it was possible to have stops without ground: do not load them
			if(halt.is_bound()) {
				halt_list.insert(halt);
				if(!halt->existiert_in_welt()) {
					dbg->warning("spieler_t::rdwr()","empty halt id %i qill be ignored", halt.get_id() );
				}
			}
		}
		last_message_index = 0;

		// empty undo buffer
		init_undo(road_wt,0);
	}

	// headquarter stuff
	if (file->get_version() < 86004)
	{
		headquarter_level = 0;
		headquarter_pos = koord::invalid;
	}
	else {
		file->rdwr_long(headquarter_level, " ");
		headquarter_pos.rdwr( file );
		if(file->is_loading()) {
			if(headquarter_level>(sint32)hausbauer_t::headquarter.get_count()) {
				headquarter_level = (sint32)hausbauer_t::headquarter.get_count();
			}
			if(headquarter_level<0) {
				headquarter_pos = koord::invalid;
				headquarter_level = 0;
			}
		}
	}

	// linemanagement
	if(file->get_version()>=88003) {
		simlinemgmt.rdwr(welt,file);
	}
}



/*
 * called after game is fully loaded;
 */
void spieler_t::laden_abschliessen()
{
	simlinemgmt.laden_abschliessen();
	display_set_player_color_scheme( player_nr, kennfarbe1, kennfarbe2 );
}



void spieler_t::rotate90( const sint16 y_size )
{
	simlinemgmt.rotate90( y_size );
	headquarter_pos.rotate90( y_size );
	for(int n=0; n<=last_message_index; n++) {
		text_pos[n].rotate90( y_size );
	}
}



/**
 * R�ckruf, um uns zu informieren, dass ein Vehikel ein Problem hat
 * @author Hansj�rg Malthaner
 * @date 26-Nov-2001
 */
void spieler_t::bescheid_vehikel_problem(convoihandle_t cnv,const koord3d ziel)
{
	switch(cnv->get_state()) {

		case convoi_t::NO_ROUTE:
DBG_MESSAGE("spieler_t::bescheid_vehikel_problem","Vehicle %s can't find a route to (%i,%i)!", cnv->gib_name(),ziel.x,ziel.y);
			if(this==welt->get_active_player()) {
				char buf[256];
				sprintf(buf,translator::translate("Vehicle %s can't find a route!"), cnv->gib_name());
				welt->get_message()->add_message(buf, cnv->gib_pos().gib_2d(),message_t::convoi,player_nr,cnv->gib_vehikel(0)->gib_basis_bild());
			}
			else if(this != welt->gib_spieler(0)) {
				cnv->self_destruct();
			}
			break;

		case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:
		case convoi_t::CAN_START_ONE_MONTH:
DBG_MESSAGE("spieler_t::bescheid_vehikel_problem","Vehicle %s stucked!", cnv->gib_name(),ziel.x,ziel.y);
			if(this==welt->get_active_player()) {
				char buf[256];
				sprintf(buf,translator::translate("Vehicle %s is stucked!"), cnv->gib_name());
				welt->get_message()->add_message(buf, cnv->gib_pos().gib_2d(),message_t::convoi,player_nr,cnv->gib_vehikel(0)->gib_basis_bild());
			}
			break;

		default:
DBG_MESSAGE("spieler_t::bescheid_vehikel_problem","Vehicle %s, state %i!", cnv->gib_name(), cnv->get_state());
	}
}



/* Here functions for UNDO
 * @date 7-Feb-2005
 * @author prissi
 */
void
spieler_t::init_undo( waytype_t wtype, unsigned short max )
{
	// only human player
	// prissi: allow for UNDO for real player
DBG_MESSAGE("spieler_t::int_undo()","undo tiles %i",max);
	last_built.clear();
	last_built.resize(max+1);
	if(max>0) {
		undo_type = wtype;
	}

}


void
spieler_t::add_undo(koord3d k)
{
	if(last_built.get_size()>0) {
//DBG_DEBUG("spieler_t::add_undo()","tile at (%i,%i)",k.x,k.y);
		last_built.push_back(k);
	}
}



bool
spieler_t::undo()
{
	if (last_built.empty()) {
		// nothing to UNDO
		return false;
	}
	// check, if we can still do undo
	for(unsigned short i=0;  i<last_built.get_count();  i++  ) {
		grund_t* gr = welt->lookup(last_built[i]);
		if(gr==NULL  ||  gr->gib_typ()!=grund_t::boden) {
			// well, something was built here ... so no undo
			last_built.clear();
			return false;
		}
		// we allow only leitung
		if(gr->obj_count()>0) {
			for( unsigned i=1;  i<gr->gib_top();  i++  ) {
				switch(gr->obj_bei(i)->gib_typ()) {
					// these are allowed
					case ding_t::way:
					case ding_t::verkehr:
					case ding_t::fussgaenger:
					case ding_t::leitung:
						break;
					// special case airplane
					// they can be everywhere, so we allow for everythign but runway undo
					case ding_t::aircraft:
						if(undo_type!=air_wt) {
							break;
						}
					// all other are forbidden => no undo any more
					default:
						last_built.clear();
						return false;
				}
			}
		}
	}

	// ok, now remove everything last built
	uint32 cost=0;
	for(unsigned short i=0;  i<last_built.get_count();  i++  ) {
		grund_t* gr = welt->lookup(last_built[i]);
		cost += gr->weg_entfernen(undo_type,true);
//DBG_DEBUG("spieler_t::add_undo()","undo tile %i at (%i,%i)",i,last_built.at(i).x,last_built.at(i).y);
	}
	last_built.clear();
	return cost!=0;
}


