/*
 * Copyright (c) 2008 Dwachs
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include "gui_numberinput.h"
#include "../../simwin.h"
#include "../../simgraph.h"
#include "../../macros.h"
#include "../../dataobj/translator.h"


char gui_numberinput_t::tooltip[256];


gui_numberinput_t::gui_numberinput_t()
{
	bt_left.set_typ(button_t::repeatarrowleft );
	bt_left.set_pos( koord(0,-1) );
	bt_left.set_groesse( koord(10,10) );
	bt_left.add_listener(this );

	textinp.set_alignment( ALIGN_RIGHT );
	textinp.set_color( COL_WHITE );
	textinp.add_listener( this );

	bt_right.set_typ(button_t::repeatarrowright );
	bt_right.set_groesse( koord(10,10) );
	bt_right.add_listener(this );

	set_limits(0, 9999);
	textbuffer[0] = 0;	// start with empty buffer
	textinp.set_text(textbuffer, 20);
	set_increment_mode( 1 );
	wrap_mode( true );
}



void gui_numberinput_t::set_groesse(koord groesse)
{
	// each button: width 10, margin 4
	// [<] [0124] [>]
	// 10 4  ??  4 10
	textinp.set_groesse(koord(groesse.x-2*10-2*4, groesse.y));
	textinp.set_pos( koord(14,-2) );
	bt_right.set_pos( koord(groesse.x-10,-1) );

	this->groesse = groesse;
}



void gui_numberinput_t::set_value(sint32 new_value)
{	// range check
	value = clamp( new_value, min_value, max_value );
	if(  !has_focus(&textinp)  ) {
		// final value should be correct, but during editing wrng values are allowed
		new_value = value;
	}
	// To preserve cursor position if text was edited, only set new text if changed (or empty before)
	if(  new_value != get_text_value()  ||  textbuffer[0]<=32  ) {
		sprintf(textbuffer, "%d", new_value);
		textinp.set_text(textbuffer, 20);
	}
	textinp.set_color( value == new_value ? COL_WHITE : COL_RED );
	value = new_value;
}


sint32 gui_numberinput_t::get_text_value()
{
	return (sint32)atol( textinp.get_text() );
}


sint32 gui_numberinput_t::get_value()
{
	return clamp( value, min_value, max_value );
}



bool gui_numberinput_t::check_value(sint32 _value)
{
	return (_value >= min_value)  &&  (_value <= max_value);
}


void gui_numberinput_t::set_limits(sint32 _min, sint32 _max)
{
	min_value = _min;
	max_value = _max;
}


bool gui_numberinput_t::action_triggered( gui_action_creator_t *komp, value_t /* */)
{
	if(  komp == &textinp  ) {
		// .. if enter / esc pressed
		set_value( get_text_value() );
		if(check_value(value)) {
			call_listeners(value_t(value));
		}
	}
	else if(  komp == &bt_left  ||  komp == &bt_right  ) {
		// value changed and feasible
		sint32 new_value = (komp == &bt_left) ? get_prev_value() : get_next_value();
		if(  new_value!=value  ) {
			set_value( new_value );
			if(check_value(new_value)) {
				// check for valid change - call listeners
				call_listeners(value_t(value));
			}
		}
	}
	return false;
}



sint8 gui_numberinput_t::percent[7] = { 0, 1, 5, 10, 20, 50, 100 };

sint32 gui_numberinput_t::get_next_value()
{
	if(  value>=max_value  ) {
		// turn over
		return (wrapping  &&  value==max_value) ? min_value : max_value;
	}

	switch( step_mode ) {
		// automatic linear
		case AUTOLINEAR:
			return clamp( value+max(1,(max_value-min_value)/100), min_value, max_value );
		// power of 2
		case POWER2:
		{
			sint32 new_value=1;
			for( int i=0;  i<32;  i++  ) {
				if(  value<(new_value<<i)  ) {
					return clamp( (new_value<<i), min_value, max_value );
				}
			}
			return max_value;
		}
		// pregressive (used for loading bars
		case PROGRESS:
		{
			sint64 diff = max_value-min_value;
			for( int i=0;  i<7;  i++  ) {
				if(  value-min_value < ((diff*(sint64)percent[i])/100l)  ) {
					return min_value+(sint32)((diff*percent[i])/100l);
				}
			}
			return max_value;
		}
		// default value is step size
		default:
			return clamp( ((value+step_mode)/step_mode)*step_mode, min_value, max_value );
	}
}



sint32 gui_numberinput_t::get_prev_value()
{
	if(  value<=min_value  ) {
		// turn over
		return (wrapping  &&  value==min_value) ? max_value : min_value;
	}

	switch( step_mode ) {
		// automatic linear
		case AUTOLINEAR:
			return clamp( value-max(1,(uint32)(max_value-min_value)/100u), min_value, max_value );
		// power of 2
		case POWER2:
		{
			sint32 new_value=1;
			for( int i=30;  i>=0;  i--  ) {
				if(  value>(new_value<<i)  ) {
					return clamp( (new_value<<i), min_value, max_value );
				}
			}
			return min_value;
		}
		// pregressive (used for loading bars
		case PROGRESS:
		{
			sint64 diff = max_value-min_value;
			for( int i=6;  i>=0;  i--  ) {
				if(  value-min_value > ((diff*percent[i])/100l)  ) {
					return min_value+(sint32)((diff*percent[i])/100l);
				}
			}
			return min_value;
		}
		// default value is step size
		default:
			return clamp( value-step_mode, min_value, max_value );
	}
}




void gui_numberinput_t::infowin_event(const event_t *ev)
{
	// buttons pressed
	if(  bt_left.getroffen(ev->cx, ev->cy)  &&  ev->ev_code == MOUSE_LEFTBUTTON  ) {
		event_t ev2 = *ev;
		translate_event(&ev2, -bt_left.get_pos().x, -bt_left.get_pos().y);
		bt_left.infowin_event(&ev2);
		request_focus( &textinp );
	}
	else if(  bt_right.getroffen(ev->cx, ev->cy)  &&  ev->ev_code == MOUSE_LEFTBUTTON  ) {
		event_t ev2 = *ev;
		translate_event(&ev2, -bt_right.get_pos().x, -bt_right.get_pos().y);
		bt_right.infowin_event(&ev2);
		request_focus( &textinp );
	}
	else {
		// since button have different callback ...
		sint32 new_value = value;
		// mouse wheel -> fast increase / decrease
		if(IS_WHEELUP(ev)){
			new_value = get_next_value();
			request_focus( &textinp );
		}
		else if(IS_WHEELDOWN(ev)){
			new_value = get_prev_value();
			request_focus( &textinp );
		}

		// catch non-number keys
		if(  ev->ev_class == EVENT_KEYBOARD  ||  value==new_value  ) {
			// assume false input
			bool call_textinp = ev->ev_class != EVENT_KEYBOARD;
			// editing keys, arrows, hom/end
			switch (ev->ev_code) {
				case '-':
					call_textinp = min_value <0;
					break;
				case 8:
				case 13:
				case 27:
				case 127:
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case SIM_KEY_LEFT:
				case SIM_KEY_RIGHT:
				case SIM_KEY_HOME:
				case SIM_KEY_END:
					call_textinp = true;
			}
			if(  call_textinp  ) {
				event_t ev2 = *ev;
				translate_event(&ev2, -textinp.get_pos().x, -textinp.get_pos().y);
				textinp.infowin_event(&ev2);
				new_value = get_text_value();
			}
		}

		// value changed and feasible
		if(  new_value!=value  ) {
			set_value( new_value );
			if(check_value(new_value)) {
				// check for valid change - call listeners
				call_listeners(value_t(value));
			}
		}
	}
}



/**
 * Zeichnet die Komponente
 * @author Dwachs
 */
void gui_numberinput_t::zeichnen(koord offset)
{
	koord new_offset = pos+offset;
	bt_left.zeichnen(new_offset);
	textinp.zeichnen(new_offset);
	bt_right.zeichnen(new_offset);

	if(getroffen( get_maus_x()-offset.x, get_maus_y()-offset.y )) {
		sprintf( tooltip, translator::translate("enter a value between %i and %i"), min_value, max_value );
		win_set_tooltip(get_maus_x() + 16, get_maus_y() - 16, tooltip );
	}
}
