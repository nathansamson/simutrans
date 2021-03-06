/*
 * system-independant event handling routines
 * Hj. Malthaner, Jan. 2001
 */

#include "tpl/debug_helper.h"

#include "simevent.h"
#include "simsys.h"


static int cx = -1; // coordinates of last mouse click event
static int cy = -1; // initialised to "nowhere"
static int control_shift_state = 0;	// none pressed


int event_get_last_control_shift(void)
{
#if 0
	MESSAGE("event_get_last_control_shift()", "state %i", control_shift_state);
#endif
	return control_shift_state & 0x03;
}


/**
 * each drag event contains the origin of the first click.
 * if the window is being dragged, it is convenient to change this
 * so the origin keeps pointing to the window top bar.
 *  Mainly to prevent copied, double code.
 */
void change_drag_start(int x, int y)
{
	cx += x;
	cy += y;
}


static void fill_event(struct event_t *ev)
{
	// for autorepeat buttons we track button state, press time and a repeat time
	// code by Niels Roest and Hj. Maltahner

	static int  pressed_buttons = 0; // assume: at startup no button pressed (nneded for some backends)
	static unsigned long lb_time = 0;
	static long repeat_time = 500;

	ev->ev_class = EVENT_NONE;

	ev->mx = sys_event.mx;
	ev->my = sys_event.my;
	ev->cx = cx;
	ev->cy = cy;

	// always put key mod code into event
	ev->ev_key_mod = sys_event.key_mod;
	control_shift_state = sys_event.key_mod;

	switch (sys_event.type) {
		case SIM_KEYBOARD:
			ev->ev_class = EVENT_KEYBOARD;
			ev->ev_code  = sys_event.code;
			break;

		case SIM_MOUSE_BUTTONS:
			// press only acknowledged when no buttons are pressed
			pressed_buttons = sys_event.mb;
			switch (sys_event.code) {
				case SIM_MOUSE_LEFTBUTTON:
					ev->ev_class = EVENT_CLICK;
					pressed_buttons |= MOUSE_LEFTBUTTON;
					ev->ev_code = MOUSE_LEFTBUTTON;
					ev->cx = cx = sys_event.mx;
					ev->cy = cy = sys_event.my;
					break;

				case SIM_MOUSE_RIGHTBUTTON:
					ev->ev_class = EVENT_CLICK;
					pressed_buttons |= MOUSE_RIGHTBUTTON;
					ev->ev_code = MOUSE_RIGHTBUTTON;
					ev->cx = cx = sys_event.mx;
					ev->cy = cy = sys_event.my;
					break;

				case SIM_MOUSE_MIDBUTTON:
					ev->ev_class = EVENT_CLICK;
					pressed_buttons |= MOUSE_MIDBUTTON;
					ev->ev_code = MOUSE_MIDBUTTON;
					ev->cx = cx = sys_event.mx;
					ev->cy = cy = sys_event.my;
					break;

				case SIM_MOUSE_WHEELUP:
					ev->ev_class = EVENT_CLICK;
					ev->ev_code = MOUSE_WHEELUP;
					ev->cx = cx = sys_event.mx;
					ev->cy = cy = sys_event.my;
					break;

				case SIM_MOUSE_WHEELDOWN:
					ev->ev_class = EVENT_CLICK;
					ev->ev_code = MOUSE_WHEELDOWN;
					ev->cx = cx = sys_event.mx;
					ev->cy = cy = sys_event.my;
					break;

				case SIM_MOUSE_LEFTUP:
					ev->ev_class = EVENT_RELEASE;
					ev->ev_code = MOUSE_LEFTBUTTON;
					pressed_buttons &= ~MOUSE_LEFTBUTTON;
					break;

				case SIM_MOUSE_RIGHTUP:
					ev->ev_class = EVENT_RELEASE;
					ev->ev_code = MOUSE_RIGHTBUTTON;
					pressed_buttons &= ~MOUSE_RIGHTBUTTON;
					break;

				case SIM_MOUSE_MIDUP:
					ev->ev_class = EVENT_RELEASE;
					ev->ev_code = MOUSE_MIDBUTTON;
					pressed_buttons &= ~MOUSE_MIDBUTTON;
					break;
			}
			break;

		case SIM_MOUSE_MOVE:
			if (sys_event.mb) { // drag
				ev->ev_class = EVENT_DRAG;
				ev->ev_code  = sys_event.mb;
			} else { // move
				ev->ev_class = EVENT_MOVE;
				ev->ev_code  = 0;
			}
			break;

		case SIM_SYSTEM:
			ev->ev_class = EVENT_SYSTEM;
			ev->ev_code  = sys_event.code;
			break;
	}

	if (IS_LEFTCLICK(ev)) {
		// remember button press
		lb_time = dr_time();
		repeat_time = 400;
	} else if (pressed_buttons == 0) {
		lb_time = 0;
	} else { // the else is to prevent race conditions
		/* Hajo: this would transform non-left button presses always
		 * to repeat events. I need right button clicks.
		 * I have no idea how thiscan be done cleanly, currently just
		 * disabling the repeat feature for non-left buttons
		 */
		if (pressed_buttons == MOUSE_LEFTBUTTON) {
			unsigned long now = dr_time();

			if (now > lb_time + repeat_time) {
				repeat_time = 100;
				lb_time = now;
				ev->ev_class = EVENT_REPEAT;
				ev->ev_code = pressed_buttons;
			}
		}
	}

	ev->button_state = pressed_buttons;
}


/**
 * Holt ein Event ohne zu warten
 * @author Hj. Malthaner
 */
void display_poll_event(struct event_t *ev)
{
	GetEventsNoWait();
	fill_event(ev);
	// prepare for next event
	sys_event.type = SIM_NOEVENT;
	sys_event.code = 0;
}


/**
 * Holt ein Event mit warten
 * @author Hj. Malthaner
 */
void display_get_event(struct event_t *ev)
{
	GetEvents();
	fill_event(ev);
	// prepare for next event
	sys_event.type = SIM_NOEVENT;
	sys_event.code = 0;
}
