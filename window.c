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

void center_pointer(window *win) {
	uint32_t x = win->geom[GEOM_W] / 2;
	uint32_t y = win->geom[GEOM_H] / 2;
	xcb_warp_pointer(conn, XCB_NONE, win->windows[WIN_CHILD], 0, 0, 0, 0, x, y);
}

void stack_above(window *win) {
	uint32_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t val  = XCB_STACK_MODE_ABOVE;
	xcb_configure_window(conn, win->windows[WIN_PARENT], mask, &val);
}

void mywm_raise(window *win) {
	excise_from(curws, win);
	insert_into(curws, win);
}

void safe_raise(window *win) {
	if (win != stack[curws].lists[TYPE_ALL]) {
		mywm_raise(win);
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

void show_state(window *win) {
	uint32_t vals[2];
	vals[0] = XCB_ICCCM_WM_STATE_NORMAL;
	vals[1] = XCB_NONE;
	
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win->windows[WIN_CHILD], wm_atoms[WM_STATE], 
			wm_atoms[WM_STATE], 32, 2, vals);

	win->ignore_unmap = 0;

	ewmh_state(win);
}

void show(window *win) {
	xcb_map_window(conn, win->windows[WIN_CHILD]);	
	xcb_map_window(conn, win->windows[WIN_PARENT]);	

	show_state(win);
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

void frame_extents(xcb_window_t win) { //unused aorn
	uint32_t vals[4];
	vals[0] = 0;     //left
	vals[1] = 0;     //right
	vals[2] = TITLE; //top
	vals[3] = 0;     //bot
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, ewmh->_NET_FRAME_EXTENTS,
			XCB_ATOM_CARDINAL, 32, 4, &vals);
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

void stick_helper(window *win) {
	if (win->sticky) {
		win->sticky = 0;
		excise_from_all_but(curws, win);
		return;
	}

	win->sticky = 1;
	insert_into_all_but(curws, win);
}

void save_state(window *win, uint32_t *state) {
	for (int i = 0; i < 4; i++) {
		state[i] = win->geom[i];
	}
}

void full_save_state(window *win) {
	safe_raise(win);

	save_state(win, win->before_full);
}

void full_restore_state(window *win) {
	update_geometry(win, MOVE_RESIZE_MASK, win->before_full);

	safe_traverse(curws, TYPE_ABOVE, mywm_raise);
}

void full(window *win) {
	uint32_t vals[4];
	vals[0] = 0;
	vals[1] = - TITLE;
	vals[2] = scr->width_in_pixels;
	vals[3] = scr->height_in_pixels + TITLE;
	update_geometry(win, MOVE_RESIZE_MASK, vals);

	ewmh_state(win);
}

void ext_full(window *subj) {
	subj->is_e_full = !subj->is_e_full;

	if (!subj->is_e_full) {
		if (!subj->is_i_full) {
			full_restore_state(subj);
		}

		return;
	}
		
	if (!subj->is_i_full) {
		full_save_state(subj);
	}

	full(stack[curws].fwin);	
}

void free_client(window *subj, int ws) {
	if (subj->sticky) {
		excise_from_all_but(ws, subj);
	}

	excise_from(ws, subj);
	free(subj);
	
	if (ws != curws || stack[curws].fwin != subj) {
		return;
	}
		
	stack[ws].fwin = NULL;

	refocus(ws);
}

void forget_client(window *win, int ws) {
	xcb_change_save_set(conn, XCB_SET_MODE_DELETE, win->windows[WIN_CHILD]);

	xcb_reparent_window(conn, win->windows[WIN_CHILD], scr->root, 0, 0);
	xcb_map_window(conn, win->windows[WIN_CHILD]);
	xcb_destroy_window(conn, win->windows[WIN_PARENT]);

	if ((state == MOVE || state == RESIZE) && win == stack[curws].fwin) {
		button_release(NULL);
	}

	free_client(win, ws);	
}

void update_geometry(window *win, uint32_t mask, uint32_t *vals) {
	int p = 0;
	size_t c = 0;
	uint32_t child_mask = 0;

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
	}

	if (win->is_snap || win->is_i_full || win->is_e_full) {
		xcb_shape_mask(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, win->windows[WIN_PARENT], 0, 0, XCB_NONE);
	}
#endif

	if (child_mask) {
		xcb_configure_window(conn, win->windows[WIN_CHILD], child_mask, vals + c);
	}
}

static uint32_t size_helper(uint32_t win_sze, uint32_t scr_sze) {
	return win_sze > scr_sze ? scr_sze : win_sze;
}

static uint32_t place_helper(uint32_t ptr_pos, uint32_t win_sze, uint32_t scr_sze) {
	if (ptr_pos < win_sze / 2 + BORDER) {
		return 0;
	} else if (ptr_pos + win_sze / 2 + BORDER > scr_sze) {
		return scr_sze - win_sze - 2 * BORDER;
	} else {
		return ptr_pos - win_sze / 2 - BORDER;
	}
}

static void test_window_type(window *win, xcb_ewmh_get_atoms_reply_t type, int i) {
	if (type.atoms[i] != ewmh->_NET_WM_WINDOW_TYPE_DOCK 
			&& type.atoms[i] != ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR
			&& type.atoms[i] != ewmh->_NET_WM_WINDOW_TYPE_DESKTOP) {
		return;
	}

	win->normal = 0;

	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	xcb_configure_window(conn, win->windows[WIN_PARENT], mask, &val);

	xcb_map_window(conn, win->windows[WIN_PARENT]);
}

static void test_window_state(window *win, xcb_ewmh_get_atoms_reply_t type, int i) {
	if (type.atoms[i] == ewmh->_NET_WM_STATE_STICKY) {
		stick_helper(win);
	}
	if (type.atoms[i] == ewmh->_NET_WM_STATE_ABOVE) {
		win->above = 1;
	}
}

static void window_property_helper(xcb_get_property_cookie_t cookie, window *win, 
		void (*func)(window *, xcb_ewmh_get_atoms_reply_t, int)) {
	xcb_ewmh_get_atoms_reply_t type;
	if (!xcb_ewmh_get_wm_window_type_reply(ewmh, cookie, &type, NULL)) {
		return;
	}
	
	for (int i = 0; i < type.atoms_len; i++) {
		func(win, type, i);
	}
		
	xcb_ewmh_get_atoms_reply_wipe(&type);
}

static xcb_get_geometry_reply_t *w_get_geometry(xcb_window_t win) {
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, win);
	return xcb_get_geometry_reply(conn, cookie, NULL);
}

void make_win_normal(window *win) {
	xcb_change_save_set(conn, XCB_SET_MODE_INSERT, win->windows[WIN_CHILD]);

	win->windows[WIN_PARENT] = xcb_generate_id(conn);
	
	uint32_t mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t vals[4];
	vals[0] = 0;
	xcb_configure_window(conn, win->windows[WIN_CHILD], mask, vals);

	xcb_get_geometry_reply_t *init_geom = w_get_geometry(win->windows[WIN_CHILD]);
	xcb_query_pointer_reply_t *ptr = w_query_pointer();
	uint32_t w = size_helper(init_geom->width, scr->width_in_pixels);
	uint32_t h = size_helper(init_geom->height + TITLE, scr->height_in_pixels - TOP - BOT);
	uint32_t x = place_helper(ptr->root_x, w, scr->width_in_pixels);
	uint32_t y = TOP + place_helper(ptr->root_y - TOP, h, scr->height_in_pixels - TOP - BOT);
	free(ptr);
	free(init_geom);

	mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
	vals[0] = 1;
	vals[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
			XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
	xcb_create_window(conn, scr->root_depth, win->windows[WIN_PARENT], scr->root, x, y, w, h, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, mask, vals);

	for (int i = 0; i < LEN(controls); i++) {
		win->controls[i] = xcb_generate_id(conn);

		mask =  XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
		vals[0] = scr->white_pixel;
		vals[1] = 1;
		vals[2] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		xcb_create_window(conn, scr->root_depth, win->controls[i], win->windows[WIN_PARENT],
				controls[i].geom.x, controls[i].geom.y, controls[i].geom.width,
				controls[i].geom.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
				XCB_COPY_FROM_PARENT, mask, vals);

		xcb_map_window(conn, win->controls[i]);
	}

	vals[0] = x;
	vals[1] = y;
	vals[2] = w;
	vals[3] = h;
	update_geometry(win, MOVE_RESIZE_MASK, vals);

	mask = XCB_CW_EVENT_MASK;
	vals[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(conn, win->windows[WIN_CHILD], mask, vals);

	normal_events(win);
	
	xcb_reparent_window(conn, win->windows[WIN_CHILD], win->windows[WIN_PARENT], 0, TITLE);

	if (!state) {
		focus(win);
	} else {
		unfocus(win);
	}
}

window *new_win(xcb_window_t child) {
	window *win = malloc(sizeof(window));
	win->windows[WIN_PARENT] = child;
	win->windows[WIN_CHILD] = child;
	win->ignore_unmap = 0;
	win->is_roll = 0;
	win->is_snap = 0;
	win->is_e_full = 0;
	win->is_i_full = 0;
	win->sticky = 0;
	win->above = 0;
	win->normal = 1;

	uint32_t vals[4];
	uint32_t mask;

	window_property_helper(xcb_ewmh_get_wm_window_type(ewmh, child), win, test_window_type);
	window_property_helper(xcb_ewmh_get_wm_state(ewmh, child), win, test_window_state);
	
	if (win->normal) {
		make_win_normal(win);
	}
	
	insert_into(curws, win);

	return win;
}
