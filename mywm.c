#include <signal.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/shape.h>

#include <X11/keysym.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xft/Xft.h>

#include "config.h"
#include "window.h"
#include "workspace.h"
#include "rounded.h"
#include "action.h"

enum { NET_SUPPORTED, NET_FULLSCREEN, NET_WM_STATE, NET_COUNT, };

unsigned int sigcode = 0;

static Display *dpy;
static Visual *vis_ptr;
uint32_t depth;

static XftFont *xfts[LEN(fonts)];

xcb_connection_t *conn;
xcb_ewmh_connection_t *ewmh;
xcb_screen_t *scr;
xcb_visualid_t vis;
xcb_colormap_t cm;

xcb_pixmap_t pixmaps[LEN(controls)][PM_COUNT];

xcb_atom_t wm_atoms[WM_COUNT];
xcb_atom_t ewmh__NET_WM_STATE_FOCUSED; //not in ewmh
static xcb_atom_t net_atoms[NET_COUNT];
	
void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);

static xcb_key_symbols_t *keysyms = NULL;

unsigned int state = DEFAULT;

static void ignore_unmap(window *subj) {
	if (subj->sticky || !subj->normal) {
		return;
	}

	xcb_unmap_window(conn, subj->windows[WIN_PARENT]);
	subj->ignore_unmap = 1;
}

xcb_query_pointer_reply_t *w_query_pointer() {
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

void close_helper(xcb_window_t win) {
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

static void map_request(xcb_generic_event_t *ev) {
	xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
	if (search_all(NULL, TYPE_ALL, WIN_CHILD, e->window)) {
		return;
	}

	new_win(e->window);
}

void enter_notify(xcb_generic_event_t *ev) {
	xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;

	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, e->event);
	if (state != RESIZE && found && found->normal && found != stack[curws].fwin && state != MOVE) {
		focus(found);
		return;
	}
	
	search_data data = search_range(curws, TYPE_NORMAL, WIN_COUNT, REGION_COUNT, e->event);
	if (data.win) {
		draw_region(data.win, data.index, PM_HOVER);
		return;
	}

	margin_enter_handler(ev);
}

void leave_notify(xcb_generic_event_t *ev) {
	xcb_leave_notify_event_t *e = (xcb_leave_notify_event_t *)ev;

	//printf("left %d, mode: %d, detail: %d, state: %d\n", e->event, e->mode, e->detail, e->state);

	search_data data = search_range(curws, TYPE_NORMAL, WIN_COUNT, REGION_COUNT, e->event);
	if (data.win && e->mode == XCB_NOTIFY_MODE_NORMAL) {
		if (state == PRESS) {
			region_abort();
		}

		draw_region(data.win, data.index, PM_FOCUS);

		return;
	}

	margin_leave_handler(ev);
}

static int button_press_helper(xcb_button_press_event_t *e, int len, const button list[],
		xcb_window_t win, uint32_t x, uint32_t y) {
	for (int i = 0; i < len; i++) {
		if (e->detail == list[i].button && list[i].mod == e->state) {
			if (list[i].press) {
				list[i].press(&(press_arg){ win, x, y });
			}
			events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))list[i].motion;
			events[XCB_BUTTON_RELEASE] = (void (*)(xcb_generic_event_t *))list[i].release;
			return 1;
		}
	}

	return 0;
}

static void button_press(xcb_generic_event_t *ev) {
	xcb_button_press_event_t *e = (xcb_button_press_event_t *)ev;

	for (int i = WIN_COUNT; i < REGION_COUNT; i++) {
		if (stack[curws].fwin->windows[i] == e->child) {
			draw_region(stack[curws].fwin, i, PM_PRESS);
			button_press_helper(e, controls[i - WIN_COUNT].buttons_len,
					controls[i - WIN_COUNT].buttons, e->event,
					e->event_x, e->event_y);
			return;
		}
	}

	if (button_press_helper(e, LEN(parent_buttons), parent_buttons, e->event, e->root_x,
			e->root_y)) {
		return;
	}
	if (button_press_helper(e, LEN(grab_buttons), grab_buttons, e->child, e->event_x,
			e->event_y)) {
		return;
	}
}

static void key_press(xcb_generic_event_t *ev) {
	xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, e->detail, 0);

	printf("press: %d %d\n", keysym, e->state);
	if (keysym != XK_Tab && state == SELECT_WINDOW) {
		printf("stopping select\n");
		select_window_terminate();
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
	
	printf("releass: %d %d\n", keysym, e->state);

	if (keysym == XK_Super_L && state == SELECT_WINDOW) {
		printf("stopping select\n");
		select_window_terminate();
	}
}

static void unmap_notify(xcb_generic_event_t *ev) {
	xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;

	window *found = search_ws(curws, TYPE_ALL, WIN_CHILD, e->window);
	if (!found) {
		return;
	}
	
	if (found->ignore_unmap) {
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
		if ((xcb_atom_t)e->data.data32[i] == ewmh->_NET_WM_STATE_FULLSCREEN) {
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

#define CHECK_MASK(a, b, c, d, e) \
	if (d & e) {              \
		a[b++] = c;       \
	}

static int mask_to_geo(xcb_configure_request_event_t *e, uint32_t *vals) {
	int i = 0;

	CHECK_MASK(vals, i, e->x, e->value_mask, XCB_CONFIG_WINDOW_X)
	CHECK_MASK(vals, i, e->y, e->value_mask, XCB_CONFIG_WINDOW_Y)
	CHECK_MASK(vals, i, e->width, e->value_mask, XCB_CONFIG_WINDOW_WIDTH)
	CHECK_MASK(vals, i, e->height, e->value_mask, XCB_CONFIG_WINDOW_HEIGHT)

	return i;
}

static void configure_request(xcb_generic_event_t *ev) {
	xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
	window *found = search_all(NULL, TYPE_ALL, WIN_CHILD, e->window);
	
	uint32_t vals[6];

	if (!found || !found->normal) {
		int i = mask_to_geo(e, vals);

		CHECK_MASK(vals, i, e->sibling, e->value_mask, XCB_CONFIG_WINDOW_SIBLING)
		CHECK_MASK(vals, i, e->stack_mode, e->value_mask, XCB_CONFIG_WINDOW_STACK_MODE)

		xcb_configure_window(conn, e->window, e->value_mask, vals);

		return;
	}
	
	if (!found->is_i_full && !found->is_e_full) {
		mask_to_geo(e, vals);
		uint32_t ignore = XCB_CONFIG_WINDOW_STACK_MODE | XCB_CONFIG_WINDOW_SIBLING;

		update_geometry(found, e->value_mask & ~ignore, vals);
	}
		
	if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
		switch (e->stack_mode) {
			case XCB_STACK_MODE_ABOVE: mywm_raise(found); break;
			case XCB_STACK_MODE_BELOW: /* dumbass */ break;
		}
	}
}

static void die() {
	//no need to preserve stack, could refactor forget_client
	for (int i = 0; i < NUM_WS; i++) {
		for (int j = 0; j < stack[i].lists[TYPE_ALL].count; j++) {
			forget_client(stack[i].lists[TYPE_ALL].first, i);
		}
	}

	xcb_ungrab_key(conn, XCB_GRAB_ANY, scr->root, XCB_MOD_MASK_ANY);
	xcb_key_symbols_free(keysyms);

	for (int i = 0; i < LEN(controls); i++) {
		for (int j = 0; j < PM_COUNT; j++) {
			xcb_free_pixmap(conn, pixmaps[i][j]);
		}
	}
	
	for (int i = 0; i < LEN(fonts); i++) {
		XftFontClose(dpy, xfts[i]);
	}
	
	xcb_flush(conn);

	xcb_disconnect(conn);
}

static void catch(int sig) {
	sigcode = sig;
}

static void expose(xcb_generic_event_t *ev) {
	xcb_expose_event_t *e = (xcb_expose_event_t *)ev;

	search_data data = search_range(curws, TYPE_NORMAL, WIN_COUNT, REGION_COUNT, e->window);
	if (!data.win) {
		return;
	}

	draw_region(data.win, data.index, data.win->last_pm[data.index - WIN_COUNT]);
}

typedef union {
	struct {
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a;
	};
	uint32_t v;
} rgba;

static uint32_t xcb_color(uint32_t hex) {
	rgba col;
	col.v = hex;

	if (!col.a) {
		return 0U;
	}

	col.r = (col.r * col.a) / 255;
	col.g = (col.g * col.a) / 255;
	col.b = (col.b * col.a) / 255;

	return col.v;
}

XftColor xft_color(uint32_t hex) {
	rgba col;
	col.v = xcb_color(hex);

	XRenderColor rc;
	rc.red = col.r * 65535 / 255;
	rc.green = col.g * 65535 / 255;
	rc.blue = col.b * 65535 / 255;
	rc.alpha = col.a * 65535 / 255;

	XftColor ret;
	XftColorAllocValue(dpy, vis_ptr, cm, &rc, &ret);

	return ret;
}

int main(void) {
	signal(SIGINT, catch);
	signal(SIGTERM, catch);

	dpy = XOpenDisplay(0);
	conn = XGetXCBConnection(dpy);
	XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	XVisualInfo xv, *res = NULL;
	xv.depth = 32;
	int flag = 0;
	res = XGetVisualInfo(dpy, VisualDepthMask, &xv, &flag);
	if (flag > 0) {
		vis_ptr = res->visual;
		vis = res->visualid;
	} else {
		vis_ptr = DefaultVisual(dpy, 0);
		vis = scr->root_visual;
	}

	cm = xcb_generate_id(conn);
	xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, cm, scr->root, vis);

	depth = (vis == scr->root_visual) ? XCB_COPY_FROM_PARENT : 32;

	for (int i = 0; i < LEN(fonts); i++) {
		xfts[i] = XftFontOpenName(dpy, 0, fonts[i]);
	}

	for (int i = 0; i < LEN(controls); i++) {
		for (int j = 0; j < PM_COUNT; j++) {
			pixmaps[i][j] = xcb_generate_id(conn);
			xcb_create_pixmap(conn, depth, pixmaps[i][j], scr->root,
					controls[i].geom.width, controls[i].geom.height);

			int w = controls[i].geom.width;
			int h = controls[i].geom.height;

			xcb_gcontext_t gc = xcb_generate_id(conn);
			uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
			uint32_t vals[2];
			vals[0] = controls[i].colors[PM_BG(j)];
			vals[1] = 0;
			xcb_create_gc(conn, gc, pixmaps[i][j], mask, vals);

			xcb_rectangle_t rect = { 0, 0, w, h };
			xcb_poly_fill_rectangle_checked(conn, pixmaps[i][j], gc, 1, &rect);

			xcb_free_gc(conn, gc);

			XftFont *font = xfts[controls[i].font_index];
			char *icon = controls[i].shape_fg;
			int len = strlen(icon);

			XGlyphInfo ret;
			XftTextExtentsUtf8(dpy, font, (XftChar8 *)icon, len, &ret);
			int x = (w - ret.width) / 2;
			int y = (controls[i].geom.height + font->ascent - font->descent) / 2;

			XftDraw *draw = XftDrawCreate(dpy, pixmaps[i][j], vis_ptr, cm);
			XftColor fg = xft_color(controls[i].colors[PM_FG(j)]);

			XftDrawStringUtf8(draw, &fg, font, x, y, (XftChar8 *)icon, len);
		}
	}

	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;

	xcb_change_window_attributes(conn, scr->root, mask, &val);
	xcb_flush(conn);
	
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

	const char *net_atom_name[1];
	net_atom_name[0] = "_NET_WM_STATE_FOCUSED";
	get_atoms(net_atom_name, &ewmh__NET_WM_STATE_FOCUSED, 0);

	xcb_atom_t supported_atoms[] = {
		ewmh->_NET_SUPPORTED,
		ewmh->_NET_WM_STATE,
		ewmh->_NET_WM_STATE_FULLSCREEN,
		ewmh->_NET_WM_STATE_STICKY,
		ewmh->_NET_WM_STATE_ABOVE,
		ewmh->_NET_WM_STATE_HIDDEN,
		ewmh->_NET_WM_WINDOW_TYPE,
		ewmh->_NET_WM_WINDOW_TYPE_DOCK,
		ewmh->_NET_FRAME_EXTENTS,

		ewmh__NET_WM_STATE_FOCUSED,

		wm_atoms[WM_PROTOCOLS],
		wm_atoms[WM_DELETE_WINDOW],
		wm_atoms[WM_STATE],
	};

	xcb_ewmh_set_supported(ewmh, 0, LEN(supported_atoms), supported_atoms);
	xcb_ewmh_set_wm_name(ewmh, scr->root, 4, "mywm");
	
	mask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;
	for (int i = 0; i < LEN(grab_buttons); i++) {
		xcb_grab_button(conn, 0, scr->root, mask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
				scr->root, XCB_NONE, grab_buttons[i].button, grab_buttons[i].mod);
	}

	grab_keys();

	for (int i = 0; i < XCB_NO_OPERATION; i++) events[i] = NULL;

	events[XCB_BUTTON_PRESS]      = button_press;
	events[XCB_CLIENT_MESSAGE]    = client_message;
	events[XCB_CONFIGURE_REQUEST] = configure_request;
	events[XCB_KEY_PRESS]         = key_press;
	events[XCB_KEY_RELEASE]       = key_release;
	events[XCB_MAP_REQUEST]       = map_request;
	events[XCB_UNMAP_NOTIFY]      = unmap_notify;
	events[XCB_DESTROY_NOTIFY]    = destroy_notify;
	events[XCB_ENTER_NOTIFY]      = enter_notify;
	events[XCB_LEAVE_NOTIFY]      = leave_notify;
	events[XCB_MAPPING_NOTIFY]    = mapping_notify;
	events[XCB_EXPOSE]            = expose;

	keysyms = xcb_key_symbols_alloc(conn);

#ifdef ROUNDED
	init_rounded_corners();
#endif

	xcb_query_tree_reply_t *tr_reply = xcb_query_tree_reply(conn,
			xcb_query_tree(conn, scr->root), 0);

	int ch_len = xcb_query_tree_children_length(tr_reply);
	xcb_window_t *children = xcb_query_tree_children(tr_reply);

	for (int i = 0; i < ch_len; i++) {
		xcb_get_window_attributes_reply_t *attr = xcb_get_window_attributes_reply(conn,
				xcb_get_window_attributes_unchecked(conn, children[i]), NULL);

		if (!attr || attr->override_redirect || attr->map_state != XCB_MAP_STATE_VIEWABLE) {
			continue;
		}

		window *cur = new_win(children[i]);

		cur->ignore_unmap = 1;
	}

	free(tr_reply);

	create_snap_regions();
	create_margins();

	struct pollfd fd;
	
	fd.fd = xcb_get_file_descriptor(conn);
	fd.events = POLLIN;

	xcb_generic_event_t *ev;
	for (; !xcb_connection_has_error(conn) && sigcode == 0;) {
		xcb_flush(conn);

		if (poll(&fd, 1, -1) == -1) {
			break;
		}

		while(ev = xcb_poll_for_event(conn)) {
			if (ev->response_type == 0) {
				xcb_generic_error_t *error = (xcb_generic_error_t *) ev;
				printf("mywm: error:\n\terror_code: %d\n\tmajor_code: "
						"%d\n\tminor_code: %d\n", error->error_code,
						error->major_code, error->minor_code);
			}

			if (events[ev->response_type & ~0x80]) {
				events[ev->response_type & ~0x80](ev);
			}
			free(ev);
		}
	}

	die();

	return 0;
}
