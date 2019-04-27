#include <stdio.h>

#include <xcb/xcb_icccm.h>
#include <xcb/shape.h>

#include "window.h"
#include "mywm.h"
#include "workspace.h"
#include "rounded.h"

#define PARENT_EVENTS XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_BUTTON_PRESS | \
		XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_ENTER_WINDOW | \
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT

void release_events(window *subj) {
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = PARENT_EVENTS | XCB_EVENT_MASK_KEY_RELEASE;
	xcb_change_window_attributes(conn, subj->windows[WIN_PARENT], mask, &val); 
}

void normal_events(window *subj) {
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = PARENT_EVENTS;
	xcb_change_window_attributes(conn, subj->windows[WIN_PARENT], mask, &val); 
}

void stack_above(window *win) {
	uint32_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t val  = XCB_STACK_MODE_ABOVE;
	xcb_configure_window(conn, win->windows[WIN_PARENT], mask, &val);
}

void raise(window *win) {
	excise_from(curws, win);
	insert_into(curws, win);
}

void safe_raise(window *win) {
	if (win != stack[curws].lists[TYPE_ALL]) {
		raise(win);
	}
}

void unfocus(window *win) {
	uint32_t val = UNFOCUSCOL;
	xcb_change_window_attributes(conn, win->windows[WIN_PARENT], XCB_CW_BACK_PIXEL, &val);
	
	xcb_clear_area(conn, 0, win->windows[WIN_PARENT], 0, 0, win->geom[GEOM_W],
			win->geom[GEOM_H]);

	stack[curws].fwin = NULL;

	ewmh_state(win);
}

void focus(window *win) {
	if (stack[curws].fwin) {
		unfocus(stack[curws].fwin);
	}

	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL;
	uint32_t vals[2];
	vals[0] = FOCUSCOL;
	vals[1] = FOCUSCOL;
	xcb_change_window_attributes(conn, win->windows[WIN_PARENT], mask, vals);

	xcb_clear_area(conn, 0, win->windows[WIN_PARENT], 0, 0, win->geom[GEOM_W],
			win->geom[GEOM_H]);

	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, win->windows[WIN_CHILD],
			XCB_CURRENT_TIME);

	stack[curws].fwin = win;
	
	ewmh_state(win);
}

void show(window *win) {
	xcb_map_window(conn, win->windows[WIN_CHILD]);	
	xcb_map_window(conn, win->windows[WIN_PARENT]);	

	uint32_t vals[2];
	vals[0] = XCB_ICCCM_WM_STATE_NORMAL;
	vals[1] = XCB_NONE;
	
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win->windows[WIN_CHILD], wm_atoms[WM_STATE], 
			wm_atoms[WM_STATE], 32, 2, vals);

	win->ignore_unmap = 0;
	
	ewmh_state(win);
}

void hide(window *win) {
	xcb_unmap_window(conn, win->windows[WIN_CHILD]);
	xcb_unmap_window(conn, win->windows[WIN_PARENT]);
	
	uint32_t vals[2];
	vals[0] = XCB_ICCCM_WM_STATE_ICONIC;
	vals[1] = XCB_NONE;
	
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win->windows[WIN_CHILD], wm_atoms[WM_STATE], 
			wm_atoms[WM_STATE], 32, 2, vals);

	win->ignore_unmap = 1;

	ewmh_state(win);
}

void ewmh_state(window *win) {
	xcb_atom_t atoms[5];
	int i = 0;

	if (win == stack[curws].fwin) {
		atoms[i++] = ewmh__NET_WM_STATE_FOCUSED;
	}
	if (win->is_i_full || win->is_e_full) {
		atoms[i++] = ewmh->_NET_WM_STATE_FULLSCREEN;
	}
	if (win->ignore_unmap) {
		atoms[i++] = ewmh->_NET_WM_STATE_HIDDEN;
	}

	if (i) {
		xcb_ewmh_set_wm_state(ewmh, win->windows[WIN_CHILD], i, atoms);
	} else {
		xcb_delete_property(conn, win->windows[WIN_CHILD], ewmh->_NET_WM_STATE);
	}
}

void update_geometry(window *win, uint32_t mask, uint32_t *vals) {
	int p = 0;
	size_t c = 0;
	uint32_t child_mask = 0;

	int wider = 0;
	
	xcb_configure_window(conn, win->windows[WIN_PARENT], mask, vals);

	if (mask & XCB_CONFIG_WINDOW_X) {
		win->geom[0] = vals[p];
		p++;
		c++;
	}
	if (mask & XCB_CONFIG_WINDOW_Y) {
		win->geom[1] = vals[p];
		p++;
		c++;
	}
	if (mask & XCB_CONFIG_WINDOW_WIDTH) {
		wider = win->geom[GEOM_W] > vals[p];
		win->geom[2] = vals[p];
		child_mask |= XCB_CONFIG_WINDOW_WIDTH;
		p++;
	}
	if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
		win->geom[3] = vals[p];
		vals[p] -= TITLE;
		child_mask |= XCB_CONFIG_WINDOW_HEIGHT;
		p++;
	}

	xcb_configure_notify_event_t ev;
	ev.response_type = XCB_CONFIGURE_NOTIFY;
	ev.sequence = 0;
	ev.event = win->windows[WIN_CHILD];
	ev.window = win->windows[WIN_CHILD];
	ev.above_sibling = XCB_NONE;
	ev.x = win->geom[0];
	ev.y = win->geom[1] + TITLE;
	ev.width = win->geom[2];
	ev.height = win->geom[3] - TITLE;
	ev.border_width = 0;
	ev.override_redirect = 0;

	xcb_send_event(conn, 0, win->windows[WIN_CHILD], XCB_EVENT_MASK_NO_EVENT, (char *)&ev);

#ifdef ROUNDED
	if (p > c) {
		rounded_corners(win);	
	} else if (win->is_snap && GAP == 0) {
		xcb_shape_mask(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, win->windows[WIN_PARENT], 0, 0, XCB_NONE);
	}
#endif

	if (child_mask) {
		xcb_configure_window(conn, win->windows[WIN_CHILD], child_mask, vals + c);
	}
}
