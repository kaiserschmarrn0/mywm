#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/shape.h>
#include <X11/keysym.h>

#include "config.h"
#include "window.h"
#include "workspace.h"
#include "rounded.h"

#define LEN(A) sizeof(A)/sizeof(*A)

#define MOVE_MASK XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
#define RESIZE_MASK XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
#define MOVE_RESIZE_MASK MOVE_MASK | RESIZE_MASK

#define PARENT_EVENTS XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_BUTTON_PRESS | \
		XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_ENTER_WINDOW | \
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT

enum { DEFAULT, MOVE, RESIZE, CYCLE, };
enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_STATE, WM_COUNT, };
enum { NET_SUPPORTED, NET_FULLSCREEN, NET_WM_STATE, NET_COUNT, };

xcb_connection_t *conn;
static xcb_ewmh_connection_t *ewmh;
static xcb_screen_t *scr;

static xcb_atom_t wm_atoms[WM_COUNT];
static xcb_atom_t net_atoms[NET_COUNT];

static xcb_key_symbols_t *keysyms = NULL;

static unsigned int state = DEFAULT;

static window *marker = NULL;

static uint32_t x = 0;
static uint32_t y = 0;

static void ignore_unmap(window *subj) {
	if (subj->sticky || !subj->normal) {
		return;
	}

	xcb_unmap_window(conn, subj->windows[WIN_PARENT]);
	subj->ignore_unmap = 1;
}

static void map(window *subj) {
	if (subj->sticky || !subj->normal) {
		return;
	}

	xcb_map_window(conn, subj->windows[WIN_PARENT]);
}

static void release_events(window *subj) {
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = PARENT_EVENTS | XCB_EVENT_MASK_KEY_RELEASE;
	xcb_change_window_attributes(conn, subj->windows[WIN_PARENT], mask, &val); 
}

static void normal_events(window *subj) {
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = PARENT_EVENTS;
	xcb_change_window_attributes(conn, subj->windows[WIN_PARENT], mask, &val); 
}

static void update_geometry(window *win, uint32_t mask, uint32_t *vals) {
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

	if (p > c) {
		rounded_corners(win);	
	} else if (win->is_snap && GAP == 0) {
		xcb_shape_mask(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, win->windows[WIN_PARENT], 0, 0, XCB_NONE);
	}

	if (child_mask) {
		xcb_configure_window(conn, win->windows[WIN_CHILD], child_mask, vals + c);
	}
}

static xcb_get_geometry_reply_t *w_get_geometry(xcb_window_t win) {
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, win);
	return xcb_get_geometry_reply(conn, cookie, NULL);
}

static xcb_query_pointer_reply_t *w_query_pointer() {
	xcb_query_pointer_cookie_t cookie = xcb_query_pointer(conn, scr->root);
	return xcb_query_pointer_reply(conn, cookie, NULL);
}

static void get_atoms(const char **names, xcb_atom_t *atoms, unsigned int count) {
	xcb_intern_atom_cookie_t cookies[count];
	for (int i = 0; i < count; i++) {
		cookies[i] = xcb_intern_atom(conn, 0, strlen(names[i]), names[i]);
	}
	for (int i = 0; i < count; i++) {
		xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookies[i], NULL);
		if (reply) {
			atoms[i] = reply->atom;
			free(reply);
		}
	}
}

static void grab_keys() {
	xcb_key_symbols_free(keysyms);

	keysyms = xcb_key_symbols_alloc(conn);

	for (int i = 0; i < LEN(keys); i++) {
		xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keys[i].key);
		xcb_grab_key(conn, 0, scr->root, keys[i].mod, *keycode, XCB_GRAB_MODE_ASYNC,
				XCB_GRAB_MODE_ASYNC);
		free(keycode);
	}
}

static void unfocus(window *win) {
	uint32_t val = UNFOCUSCOL;
	xcb_change_window_attributes(conn, win->windows[WIN_PARENT], XCB_CW_BACK_PIXEL, &val);
	
	xcb_flush(conn);
	
	xcb_clear_area(conn, 0, win->windows[WIN_PARENT], 0, 0, win->geom[GEOM_W], win->geom[GEOM_H]);
}

static void focus(window *subj) {
	if (stack[curws].fwin) {
		unfocus(stack[curws].fwin);
	}
	
	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL;
	uint32_t vals[2];
	vals[0] = FOCUSCOL;
	vals[1] = FOCUSCOL;
	xcb_change_window_attributes(conn, subj->windows[WIN_PARENT], mask, vals);

	xcb_flush(conn);

	xcb_clear_area(conn, 0, subj->windows[WIN_PARENT], 0, 0, subj->geom[GEOM_W], subj->geom[GEOM_H]);

	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, subj->windows[WIN_CHILD], XCB_CURRENT_TIME);
	stack[curws].fwin = subj;
}

static void center_pointer(window *subj) {
	xcb_get_geometry_reply_t *temp = w_get_geometry(subj->windows[WIN_PARENT]);
	uint32_t x = (temp->width + 2 * BORDER)/2;
	uint32_t y = (temp->height + 2 * BORDER)/2;
	xcb_warp_pointer(conn, XCB_NONE, subj->windows[WIN_CHILD], 0, 0, 0, 0, x, y);
	free(temp);
}

static void button_release(xcb_generic_event_t *ev) {
	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
	state = DEFAULT;
}

static void forget_client(window *subj, int ws) {
	printf("forgetting client: %d\n", subj->windows[WIN_PARENT]);

	xcb_reparent_window(conn, subj->windows[WIN_CHILD], scr->root, 0, 0);
	xcb_destroy_window(conn, subj->windows[WIN_PARENT]);

	if ((state == MOVE || state == RESIZE) && subj == stack[curws].fwin) {
		button_release(NULL);
	}

	if (subj->sticky) {
		excise_from_all_but(ws, subj);
	}

	excise_from(ws, subj);
	free(subj);
	
	if (ws != curws || stack[curws].fwin != subj) {
		return;
	}
		
	stack[curws].fwin = NULL;

	if (stack[curws].lists[TYPE_NORMAL]) {
		focus(stack[curws].lists[TYPE_NORMAL]);
		return;
	}
}

static void stick_helper(window *win) {
	if (win->sticky) {
		win->sticky = 0;
		excise_from_all_but(curws, win);
		return;
	}

	win->sticky = 1;
	insert_into_all_but(curws, win);
}

void stick(int arg) {
	if (stack[curws].fwin) {
		stick_helper(stack[curws].fwin);
	}
}

static void kill(xcb_window_t win) {
	xcb_icccm_get_wm_protocols_reply_t pro;
	xcb_get_property_cookie_t cookie;
	cookie = xcb_icccm_get_wm_protocols_unchecked(conn, win, ewmh->WM_PROTOCOLS);
	if (!xcb_icccm_get_wm_protocols_reply(conn, cookie, &pro, NULL)) {
		xcb_kill_client(conn, win);
		return;
	}

	for (int i = 0; i < pro.atoms_len; i++) {
		if (pro.atoms[i] == wm_atoms[WM_DELETE_WINDOW]) {
			xcb_icccm_get_wm_protocols_reply_wipe(&pro);
			goto a;
		}
	}
	xcb_icccm_get_wm_protocols_reply_wipe(&pro);
	xcb_kill_client(conn, win);
	return;

	a:;
	
	xcb_client_message_event_t ev;
	ev.response_type = XCB_CLIENT_MESSAGE;
	ev.format = 32;
	ev.sequence = 0;
	ev.window = win;
	ev.type = wm_atoms[WM_PROTOCOLS];
	ev.data.data32[0] = wm_atoms[WM_DELETE_WINDOW];
	ev.data.data32[1] = XCB_CURRENT_TIME;
	uint32_t mask = XCB_EVENT_MASK_NO_EVENT;
	xcb_send_event(conn, 0, win, mask, (char *)&ev);
}

void close(int arg) {
	if (!stack[curws].fwin) {
		return;
	}

	xcb_unmap_window(conn, stack[curws].fwin->windows[WIN_PARENT]);
	xcb_unmap_window(conn, stack[curws].fwin->windows[WIN_CHILD]);

	kill(stack[curws].fwin->windows[WIN_CHILD]);
	printf("kill %d\n", stack[curws].fwin->windows[WIN_CHILD]);

	forget_client(stack[curws].fwin, curws);
}

static void cycle_raise(window *cur) {
	for (; cur != stack[curws].fwin;) {
		window *temp = cur->prev[curws][TYPE_NORMAL];
		raise(cur); 
		cur = temp;
	}
}

static void stop_cycle() {
	state = DEFAULT;
	traverse(curws, TYPE_NORMAL, normal_events); 
}

void cycle(int arg) {
	if (!stack[curws].lists[TYPE_NORMAL] || !stack[curws].lists[TYPE_NORMAL]->next[curws][TYPE_NORMAL]) {
		return;
	}

	if (state != CYCLE) {
		traverse(curws, TYPE_NORMAL, release_events);
		marker = stack[curws].fwin;
		state = CYCLE;
	}

	if (marker->next[curws][TYPE_NORMAL]) {
		cycle_raise(marker);
		marker = stack[curws].fwin;
		center_pointer(marker->next[curws][TYPE_NORMAL]);
		raise(marker->next[curws][TYPE_NORMAL]);
	} else {
		cycle_raise(marker);
		center_pointer(stack[curws].lists[TYPE_NORMAL]);
		marker = stack[curws].fwin;
	}
}

void change_ws(int arg) {
	if (arg == curws) {
		return;
	}

	if (stack[curws].fwin) {
		unfocus(stack[curws].fwin);
	}
	
	traverse(arg, TYPE_NORMAL, map);
	traverse(curws, TYPE_NORMAL, ignore_unmap); 
	
	curws = arg;

	if (stack[arg].fwin) {
		focus(stack[arg].fwin);
	} else if (stack[arg].lists[TYPE_NORMAL]) {
		focus(stack[arg].lists[TYPE_NORMAL]);
	}
}

void send_ws(int arg) {
	if (!stack[curws].fwin || arg == curws || stack[curws].fwin->sticky) {
		return;
	}

	stack[curws].fwin->ignore_unmap = 1;
	xcb_unmap_window(conn, stack[curws].fwin->windows[WIN_PARENT]);

	excise_from(curws, stack[curws].fwin);
	insert_into(arg, stack[curws].fwin);
	stack[curws].fwin = NULL;
}

static void save_state(window *win, uint32_t *state) {
	for (int i = 0; i < 4; i++) {
		state[i] = win->geom[i];
	}
}

static void snap_save_state(window *win) {
	save_state(win, win->before_snap);
	
	win->is_snap = 1;
}

static void snap_restore_state(window *win) {
	win->is_snap = 0;
	
	update_geometry(win, MOVE_RESIZE_MASK, win->before_snap);
}

#define SNAP_TEMPLATE(A, B, C, D, E) void A(int arg) {                                            \
	if (!stack[curws].fwin || stack[curws].fwin->is_e_full || stack[curws].fwin->is_i_full) { \
		return;                                                                           \
	}                                                                                         \
	                                                                                          \
	if (!stack[curws].fwin->is_snap) {                                                        \
		snap_save_state(stack[curws].fwin);                                               \
	}                                                                                         \
	                                                                                          \
	uint32_t vals[4];                                                                         \
	vals[0] = B;                                                                              \
	vals[1] = C;                                                                              \
	vals[2] = D;                                                                              \
	vals[3] = E;                                                                              \
	update_geometry(stack[curws].fwin, MOVE_RESIZE_MASK, vals);                               \
	                                                                                          \
	if (state == MOVE) {                                                                      \
		return;                                                                           \
	}                                                                                         \
	                                                                                          \
	center_pointer(stack[curws].fwin);                                                        \
	raise(stack[curws].fwin);                                                                 \
}

#ifndef SNAP_MAX_SMART
SNAP_TEMPLATE(snap_max,
	GAP,
	GAP + TOP,
	scr->width_in_pixels - 2 * GAP - 2 * BORDER,
	scr->height_in_pixels - 2 * GAP - 2 * BORDER - TOP - BOT)
#else
SNAP_TEMPLATE(snap_max,
	0,
	TOP,
	scr->width_in_pixels - 2 * BORDER,
	scr->height_in_pixels - 2 * BORDER - TOP - BOT)
#endif

SNAP_TEMPLATE(snap_l,
	GAP,
	GAP + TOP,
	scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2,
	scr->height_in_pixels - 2 * GAP - 2 * BORDER - TOP - BOT)

SNAP_TEMPLATE(snap_lu,
	GAP,
	GAP + TOP,
	scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2,
	(scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER)

SNAP_TEMPLATE(snap_ld,
	GAP,
	(scr->height_in_pixels - TOP - BOT) / 2 + GAP / 2 + TOP,
	scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2,
	(scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER)

SNAP_TEMPLATE(snap_r,
	scr->width_in_pixels / 2 + GAP / 2,
	GAP + TOP,
	scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2,
	scr->height_in_pixels - 2 * GAP - 2 * BORDER - TOP - BOT)

SNAP_TEMPLATE(snap_ru,
	scr->width_in_pixels / 2 + GAP / 2,
	GAP + TOP,
	scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2,
	(scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER)

SNAP_TEMPLATE(snap_rd,
	scr->width_in_pixels / 2 + GAP / 2,
	(scr->height_in_pixels - TOP - BOT) / 2 + GAP / 2 + TOP,
	scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2,
	(scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER)

static void full_save_state(window *win) {
	raise(win);

	save_state(win, win->before_full);
}

static void full_restore_state(window *win) {
	update_geometry(win, MOVE_RESIZE_MASK, win->before_full);
}

static void full(window *win) {
	uint32_t vals[4];
	vals[0] = 0;
	vals[1] = - TITLE;
	vals[2] = scr->width_in_pixels;
	vals[3] = scr->height_in_pixels + TITLE;
	update_geometry(win, MOVE_RESIZE_MASK, vals);
}

void int_full(int arg) {
	if (!stack[curws].fwin) {
		return;
	}

	stack[curws].fwin->is_i_full = !stack[curws].fwin->is_i_full;

	if (stack[curws].fwin->is_e_full) {
		return;
	}

	if (!stack[curws].fwin->is_i_full) {
		full_restore_state(stack[curws].fwin); 
		return;
	}
	
	full_save_state(stack[curws].fwin);

	full(stack[curws].fwin);
}

static void ext_full(window *subj) {
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

static void frame_extents(xcb_window_t win) {
	uint32_t vals[4];
	vals[0] = 0;     //left
	vals[1] = 0;     //right
	vals[2] = TITLE; //top
	vals[3] = 0;     //bot
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, ewmh->_NET_FRAME_EXTENTS,
			XCB_ATOM_CARDINAL, 32, 4, &vals);
}

static void map_request(xcb_generic_event_t *ev) {
	xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
	if (search_all(NULL, TYPE_ALL, WIN_CHILD, e->window)) {
		return;
	}
	
	window *win = malloc(sizeof(window));
	win->windows[WIN_PARENT] = e->window;
	win->windows[WIN_CHILD] = e->window;
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

	xcb_get_property_cookie_t cookie = xcb_ewmh_get_wm_window_type(ewmh, e->window);
	xcb_ewmh_get_atoms_reply_t type;
	if (xcb_ewmh_get_wm_window_type_reply(ewmh, cookie, &type, NULL)) {
		for (unsigned int i = 0; i < type.atoms_len; i++) {		
			if (type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_DOCK 
					|| type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR
					|| type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP) {
				win->normal = 0;

				mask = XCB_CW_EVENT_MASK;
				vals[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
				xcb_configure_window(conn, win->windows[WIN_PARENT], mask, vals);
				
				xcb_map_window(conn, e->window);
			}
		}
		xcb_ewmh_get_atoms_reply_wipe(&type);
	}

	cookie = xcb_ewmh_get_wm_state(ewmh, e->window);
	if (xcb_ewmh_get_wm_state_reply(ewmh, cookie, &type, NULL)) {
		for (unsigned int i = 0; i < type.atoms_len; i++) {		
			if (type.atoms[i] == ewmh->_NET_WM_STATE_STICKY) {
				stick_helper(win);
			}
			if (type.atoms[i] == ewmh->_NET_WM_STATE_ABOVE) {
				win->above = 1;
			}
		}
		xcb_ewmh_get_atoms_reply_wipe(&type);
	}
	
	if (win->normal) {
		win->windows[WIN_PARENT] = xcb_generate_id(conn);
	
		mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
		vals[0] = 0;
		xcb_configure_window(conn, win->windows[WIN_CHILD], mask, vals);

		/* not needed rn tbh
		frame_extents(win->windows[WIN_CHILD]); */
	
		xcb_get_geometry_reply_t *init_geom = w_get_geometry(win->windows[WIN_CHILD]);
		xcb_query_pointer_reply_t *ptr = w_query_pointer();
		uint32_t w = size_helper(init_geom->width, scr->width_in_pixels);
		uint32_t h = size_helper(init_geom->height + TITLE, scr->height_in_pixels);
		uint32_t x = place_helper(ptr->root_x, w, scr->width_in_pixels);
		uint32_t y = place_helper(ptr->root_y, h, scr->height_in_pixels);
		free(ptr);
		free(init_geom);
	
		mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
		vals[0] = 1;
		vals[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
				XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
		xcb_create_window(conn, scr->root_depth, win->windows[WIN_PARENT], scr->root, x, y, w, h, 0,
				XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, mask, vals);
	
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
		xcb_map_window(conn, win->windows[WIN_CHILD]);

		if (!state) {
			focus(win);
		} else {
			unfocus(win);
		}
	}
	
	insert_into(curws, win);

	xcb_map_window(conn, win->windows[WIN_PARENT]);

	vals[0] = XCB_ICCCM_WM_STATE_NORMAL;
	vals[1] = XCB_NONE;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win->windows[WIN_CHILD], wm_atoms[WM_STATE],
	wm_atoms[WM_STATE], 32, 2, vals);
}

static void enter_notify(xcb_generic_event_t *ev) {
	xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, e->event);
	if (found && found->normal) {
		focus(found);
	}
}

static int move_resize_helper(xcb_window_t win, xcb_get_geometry_reply_t **geom) {
	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, win);
	if (!found || !found->normal || found != stack[curws].fwin) {
		return 0;
	}

	raise(stack[curws].fwin);
	
	if (stack[curws].fwin->is_e_full || stack[curws].fwin->is_i_full) {
		return 0;
	}

	*geom = w_get_geometry(stack[curws].fwin->windows[WIN_PARENT]);

	return 1;
}

static void grab_pointer() {
	uint32_t mask = XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
			XCB_EVENT_MASK_POINTER_MOTION;
	uint32_t type = XCB_GRAB_MODE_ASYNC;
	xcb_grab_pointer(conn, 0, scr->root, mask, type, type, scr->root, XCB_NONE,
			XCB_CURRENT_TIME);
}

void mouse_move(xcb_window_t win, uint32_t event_x, uint32_t event_y) {
	xcb_get_geometry_reply_t *geom;
	
	if (!move_resize_helper(win, &geom)) {
		return;
	}

	if (stack[curws].fwin->is_snap) {
		x = stack[curws].fwin->before_snap[2] * (event_x - geom->x) / geom->width;
		y = stack[curws].fwin->before_snap[3] * (event_y - geom->y) / geom->height; 
	} else {
		x = event_x - geom->x;
		y = event_y - geom->y;
	}
	
	free(geom);
	
	state = MOVE;

	grab_pointer();
}

void mouse_resize(xcb_window_t win, uint32_t event_x, uint32_t event_y) {
	xcb_get_geometry_reply_t *geom;

	if (!move_resize_helper(win, &geom)) {
		return;
	}

	stack[curws].fwin->is_snap = 0;
	x = geom->width - event_x;
	y = geom->height - event_y;

	state = RESIZE;
	
	free(geom);
	
	grab_pointer();
}

void mouse_roll_up(xcb_window_t win, uint32_t event_x, uint32_t event_y) {
	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, win);
	if (!found || !found->normal || found != stack[curws].fwin || found->is_roll) {
		return;
	}
	
	found->before_roll = found->geom[GEOM_H];

	uint32_t val = TITLE;
	update_geometry(found, XCB_CONFIG_WINDOW_HEIGHT, &val);
	
	found->is_roll = 1;
}

void mouse_roll_down(xcb_window_t win, uint32_t event_x, uint32_t event_y) {
	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, win);
	if (!found || !found->normal || found != stack[curws].fwin || !found->is_roll) {
		return;
	}

	update_geometry(found, XCB_CONFIG_WINDOW_HEIGHT, &found->before_roll);
	
	found->is_roll = 0;
}

static void button_press(xcb_generic_event_t *ev) {
	xcb_button_press_event_t *e = (xcb_button_press_event_t *)ev;

	for (int i = 0; i < LEN(parent_buttons); i++) {
		if (e->detail == parent_buttons[i].button && parent_buttons[i].mod == e->state) {
			parent_buttons[i].function(e->event, e->root_x, e->root_y);
			return;
		}
	}

	for (int i = 0; i < LEN(grab_buttons); i++) {
		if (e->detail == grab_buttons[i].button && grab_buttons[i].mod == e->state) {
			grab_buttons[i].function(e->child, e->event_x, e->event_y);
			return;
		}
	}
}

static void mouse_snap(uint32_t ptr_pos, uint32_t tolerance, void (*snap_x)(int arg),
		void (*snap_y)(int arg), void (*snap_z)(int arg)) {
	if (ptr_pos < SNAP_CORNER) {
		snap_x(0);
	} else if (ptr_pos > tolerance - SNAP_CORNER) {
		snap_y(0);
	} else {
		snap_z(0);
	}
}

static void motion_notify(xcb_generic_event_t *ev) {
	xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)ev;

	xcb_query_pointer_reply_t *p = w_query_pointer();
	if (!p) {
		return;
	}

	if (state == MOVE) {
		if (p->root_x < SNAP_MARGIN) {
			mouse_snap(p->root_y, scr->height_in_pixels, snap_lu, snap_ld, snap_l);
		} else if (p->root_y < SNAP_MARGIN) {
			mouse_snap(p->root_x, scr->width_in_pixels, snap_lu, snap_ru, snap_max);
		} else if (p->root_x > scr->width_in_pixels - SNAP_MARGIN) {
			mouse_snap(p->root_y, scr->height_in_pixels, snap_ru, snap_rd, snap_r);
		} else if (p->root_y > scr->height_in_pixels - SNAP_MARGIN) {
			mouse_snap(p->root_x, scr->width_in_pixels, snap_ld, snap_rd, snap_max);
		} else {
			if (stack[curws].fwin->is_snap) {
				snap_restore_state(stack[curws].fwin);
			}

			uint32_t vals[2];
			vals[0] = p->root_x - x;
			vals[1] = p->root_y - y;
			update_geometry(stack[curws].fwin, MOVE_MASK, vals);
		}
	} else if (state == RESIZE) {
		uint32_t vals[2];
		vals[0] = p->root_x + x;
		vals[1] = p->root_y + y;
		update_geometry(stack[curws].fwin, RESIZE_MASK, vals);
	}

	free(p);
}

static void key_press(xcb_generic_event_t *ev) {
	xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, e->detail, 0);

	if (keysym != XK_Tab && state == CYCLE) {
		stop_cycle();
	}

	for (int i = 0; i < LEN(keys); i++) {
		if (keysym == keys[i].key && keys[i].mod == e->state) {
			keys[i].function(keys[i].arg);
			break;
		}
	}
}

static void key_release(xcb_generic_event_t *ev) {
	xcb_key_release_event_t *e = (xcb_key_release_event_t *)ev;
	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, e->detail, 0);

	if (keysym == XK_Super_L && state == CYCLE) {
		stop_cycle();
	}
}

static void unmap_notify(xcb_generic_event_t *ev) {
	xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;

	window *found = search_ws(curws, TYPE_ALL, WIN_CHILD, e->window);
	if (!found) {
		return;
	}
	
	printf("unmap %d\n", found->windows[WIN_CHILD]);

	if (found->ignore_unmap) {
		printf("unmap ignored\n");
		found->ignore_unmap = 0;
		return;
	}

	forget_client(found, curws);
}

static void destroy_notify(xcb_generic_event_t *ev) {
	xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;

	int ws;
	window *found = search_all(&ws, TYPE_ALL, WIN_CHILD, e->window);
	
	if (found) {
		printf("destroy %d on ws %d\n", found->windows[WIN_CHILD], ws);
		forget_client(found, ws);
	}
}

static void client_message(xcb_generic_event_t *ev) {
	xcb_client_message_event_t *e = (xcb_client_message_event_t *)ev;

	window *found = search_all(NULL, TYPE_ALL, WIN_CHILD, e->window);

	if (!found || !found->normal || e->type != ewmh->_NET_WM_STATE) {
		return;
	}	

	for (int i = 1; i < 3; i++) {
		xcb_atom_t atom = (xcb_atom_t)e->data.data32[i];
		if (atom == ewmh->_NET_WM_STATE_FULLSCREEN) {
			switch (2 * e->data.data32[0] + found->is_e_full) {
				case 2 * XCB_EWMH_WM_STATE_ADD:
					ext_full(found);
					break;
				case 2 * XCB_EWMH_WM_STATE_REMOVE + 1:
					ext_full(found);
					break;
				case 2 * XCB_EWMH_WM_STATE_TOGGLE:
					/* fallthrough */
				case 2 * XCB_EWMH_WM_STATE_TOGGLE + 1:
					ext_full(found);
			}
		}
	}
}

static void mapping_notify(xcb_generic_event_t *ev) {
	xcb_mapping_notify_event_t *e = (xcb_mapping_notify_event_t *)ev;
	if (e->request != XCB_MAPPING_MODIFIER && e->request != XCB_MAPPING_KEYBOARD) {
		return;
	}

	xcb_ungrab_key(conn, XCB_GRAB_ANY, scr->root, XCB_MOD_MASK_ANY);
	grab_keys();
}

#define check_mask(a, b, c, d, e) \
	if (d & e) {              \
		a[b++] = c;       \
	}

static int mask_to_geo(xcb_configure_request_event_t *e, uint32_t *vals) {
	int i = 0;

	check_mask(vals, i, e->x, e->value_mask, XCB_CONFIG_WINDOW_X)
	check_mask(vals, i, e->y, e->value_mask, XCB_CONFIG_WINDOW_Y)
	check_mask(vals, i, e->width, e->value_mask, XCB_CONFIG_WINDOW_WIDTH)
	check_mask(vals, i, e->height, e->value_mask, XCB_CONFIG_WINDOW_HEIGHT)

	return i;
}

static void configure_request(xcb_generic_event_t *ev) {
	xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
	window *found = search_all(NULL, TYPE_ALL, WIN_CHILD, e->window);
	
	uint32_t vals[6];

	if (!found || !found->normal) {
		int i = mask_to_geo(e, vals);

		check_mask(vals, i, e->sibling, e->value_mask, XCB_CONFIG_WINDOW_SIBLING)
		check_mask(vals, i, e->stack_mode, e->value_mask, XCB_CONFIG_WINDOW_STACK_MODE)

		xcb_configure_window(conn, e->window, e->value_mask, vals);
	} else if (!found->is_i_full && !found->is_e_full) {
		/*mask_to_geo(e, vals);
		uint32_t ignore = XCB_CONFIG_WINDOW_STACK_MODE | XCB_CONFIG_WINDOW_SIBLING;
		xcb_configure_window(conn, found->windows[WIN_CHILD], e->value_mask & ~ignore, vals);*/
	}
}

static void cleanup(window *win) {
	kill(win->windows[WIN_CHILD]);
	free(win);
}

static void die() {
	for (int i = 0; i < NUM_WS; i++) {
		safe_traverse(i, TYPE_ALL, cleanup);
	}

	xcb_ungrab_key(conn, XCB_GRAB_ANY, scr->root, XCB_MOD_MASK_ANY);
	xcb_key_symbols_free(keysyms);

	xcb_disconnect(conn);
}

int main(void) {
	conn = xcb_connect(NULL, NULL);
	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;

	xcb_change_window_attributes(conn, scr->root, mask, &val);
	
	atexit(die);
	
	const xcb_query_extension_reply_t *shape_reply = xcb_get_extension_data(conn, &xcb_shape_id);
	if (!shape_reply->present) {
		fprintf(stderr, "x server doesn't have xshape\n");
		return 1;
	}

	ewmh = calloc(1, sizeof(xcb_ewmh_connection_t));
	if (!ewmh) {
		fprintf(stderr, "couldn't allocate ewmh connection\n");
		return 1;
	}
	xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh), (void *)0);

	const char *wm_atom_name[3]; 
	wm_atom_name[0] = "WM_PROTOCOLS";
	wm_atom_name[1] = "WM_DELETE_WINDOW";
	wm_atom_name[2] = "WM_STATE";
	get_atoms(wm_atom_name, wm_atoms, WM_COUNT);

	xcb_atom_t supported_atoms[] = {
		ewmh->_NET_SUPPORTED,
		ewmh->_NET_WM_STATE,
		ewmh->_NET_WM_STATE_FULLSCREEN,
		ewmh->_NET_WM_STATE_STICKY,
		ewmh->_NET_WM_STATE_ABOVE,
		ewmh->_NET_WM_WINDOW_TYPE,
		ewmh->_NET_WM_WINDOW_TYPE_DOCK,
		ewmh->_NET_FRAME_EXTENTS,
		wm_atoms[WM_PROTOCOLS],
		wm_atoms[WM_DELETE_WINDOW],
		wm_atoms[WM_STATE],
	};

	xcb_ewmh_set_supported(ewmh, 0, LEN(supported_atoms), supported_atoms);
	
	mask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;
	for (int i = 0; i < LEN(grab_buttons); i++) {
		xcb_grab_button(conn, 0, scr->root, mask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
				scr->root, XCB_NONE, grab_buttons[i].button, grab_buttons[i].mod);
	}

	grab_keys();
	
	void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);

	for (int i = 0; i < XCB_NO_OPERATION; i++) events[i] = NULL;

	events[XCB_BUTTON_PRESS]      = button_press;
	events[XCB_BUTTON_RELEASE]    = button_release;
	events[XCB_MOTION_NOTIFY]     = motion_notify;
	events[XCB_CLIENT_MESSAGE]    = client_message;
	events[XCB_CONFIGURE_REQUEST] = configure_request;
	events[XCB_KEY_PRESS]         = key_press;
	events[XCB_KEY_RELEASE]       = key_release;
	events[XCB_MAP_REQUEST]       = map_request;
	events[XCB_UNMAP_NOTIFY]      = unmap_notify;
	events[XCB_DESTROY_NOTIFY]    = destroy_notify;
	events[XCB_ENTER_NOTIFY]      = enter_notify;
	events[XCB_MAPPING_NOTIFY]    = mapping_notify;

	keysyms = xcb_key_symbols_alloc(conn);

	init_rounded_corners();
	
	xcb_generic_event_t *ev;
	for (; !xcb_connection_has_error(conn);) {
		xcb_flush(conn);

		ev = xcb_wait_for_event(conn);
		if (events[ev->response_type & ~0x80]) {
			events[ev->response_type & ~0x80](ev);
		}
		free(ev);
	}

	return 0;
}
