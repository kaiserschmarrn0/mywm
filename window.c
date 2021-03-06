#include <xcb/shape.h>
#include <string.h>

#include "window.h"
#include "mywm.h"
#include "workspace.h"
#include "rounded.h"
#include "mouse.h"
#include "color.h"

#define PARENT_EVENTS XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_BUTTON_PRESS | \
		XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_ENTER_WINDOW | \
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT

void release_events(window *subj) {
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(conn, subj->windows[WIN_CHILD], mask, &val); 
}

void reset_events(window *subj) {
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(conn, subj->windows[WIN_CHILD], mask, &val);
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

void stack_above_helper(xcb_window_t win) {
	uint32_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t val  = XCB_STACK_MODE_ABOVE;
	xcb_configure_window(conn, win, mask, &val);
}

void stack_below_helper(xcb_window_t win) {
	uint32_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t val  = XCB_STACK_MODE_BELOW;
	xcb_configure_window(conn, win, mask, &val);
}

void stack_below_abnormal(window *win) {
	stack_below_helper(win->windows[WIN_CHILD]);
}

void stack_below(window *win) {
	stack_below_helper(win->windows[WIN_PARENT]);
}

void stack_above_abnormal(window *win) {
	stack_above_helper(win->windows[WIN_CHILD]);
}

void stack_above(window *win) {
	stack_above_helper(win->windows[WIN_PARENT]);
}

void mywm_raise(window *win) {
	excise_from(curws, win);
	insert_into(curws, win);
}

void mywm_lower(window *win) {
	excise_from(curws, win);
	append_to(curws, win);
}

void safe_raise(window *win) {
	if (win != stack[curws].lists[TYPE_ALL].first) {
		mywm_raise(win);
	}
}

static void draw_regions(window *win, int pm_index) {
	for (int i = WIN_COUNT; i < REGION_COUNT; i++) {
		draw_region(win, i, pm_index);
	}
}

void unfocus(window *win) {
	uint32_t val = UNFOCUSCOL;
	xcb_change_window_attributes(conn, win->windows[WIN_PARENT], XCB_CW_BACK_PIXEL, &val);
	
	xcb_clear_area(conn, 0, win->windows[WIN_PARENT], 0, 0, win->geom[GEOM_W],
			win->geom[GEOM_H]);

	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, win->windows[WIN_CHILD],
			XCB_CURRENT_TIME);

	draw_regions(win, PM_UNFOCUS);
	
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
	
	draw_regions(win, PM_FOCUS);

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
	if (win->sticky) {
		return;
	}

	xcb_map_window(conn, win->windows[WIN_CHILD]);	
	xcb_map_window(conn, win->windows[WIN_PARENT]);	

	show_state(win);
}

void hide(window *win) {
	if (win->sticky) {
		return;
	}

	xcb_unmap_window(conn, win->windows[WIN_CHILD]);
	xcb_unmap_window(conn, win->windows[WIN_PARENT]);
	
	uint32_t vals[2];
	vals[0] = XCB_ICCCM_WM_STATE_ICONIC;
	vals[1] = XCB_NONE;
	
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win->windows[WIN_CHILD], wm_atoms[WM_STATE], 
			wm_atoms[WM_STATE], 32, 2, vals);

	ewmh_state(win);
	
	win->ignore_unmap = 1;
}

void frame_extents(xcb_window_t win) { //unused aorn
	uint32_t vals[4];
	vals[0] = 0;     //left
	vals[1] = 0;     //right
	vals[2] = PAD_N; //top
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
	vals[0] = - PAD;
	vals[1] = - PAD_N;
	vals[2] = scr->width_in_pixels + 2 * PAD;
	vals[3] = scr->height_in_pixels + PAD_N + PAD;
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

void forget_client(window *win, int ws) {
	if (win->windows[WIN_PARENT] != XCB_WINDOW_NONE) {
		xcb_generic_error_t *error = NULL;
		error = xcb_request_check(conn, xcb_reparent_window_checked(conn,
				win->windows[WIN_CHILD], scr->root, win->geom[GEOM_X] + PAD,
				win->geom[GEOM_Y] + PAD_N));

		if (!error) {
			xcb_change_save_set(conn, XCB_SET_MODE_DELETE, win->windows[WIN_CHILD]);
		} else {
			free(error);
		}

		xcb_destroy_window(conn, win->windows[WIN_PARENT]);
	
		xcb_free_gc(conn, win->gc);
	}

	if ((state == MOVE || state == RESIZE) && win == stack[curws].fwin) {
		button_release(NULL);
	}

	if (win->sticky) {
		excise_from_all_but(ws, win);
	}
	
	excise_from(ws, win);

	free(win);
	
	if (ws != curws || stack[curws].fwin != win) {
		return;
	}
	
	stack[ws].fwin = NULL;

	refocus(ws);
}

void update_geometry(window *win, uint32_t mask, const uint32_t *true_vals) {
	int p = 0;
	size_t c = 0;
	uint32_t child_mask = 0;
	uint32_t vals[4] = { 0 };

	if (mask & XCB_CONFIG_WINDOW_X) {
		win->geom[0] = true_vals[p];
		vals[p] = true_vals[p];
		p++;
		c++;
	}

	if (mask & XCB_CONFIG_WINDOW_Y) {
		win->geom[1] = true_vals[p];
		vals[p] = true_vals[p];
		p++;
		c++;
	}

	if (mask & XCB_CONFIG_WINDOW_WIDTH) {
		win->geom[2] = true_vals[p];
		vals[p] = true_vals[p] - 2 * PAD;
		child_mask |= XCB_CONFIG_WINDOW_WIDTH;

		uint32_t new_w = true_vals[p] - 2 * RESIZE_REGION_CORNER;
		xcb_configure_window(conn, win->resize_regions[1], XCB_CONFIG_WINDOW_WIDTH, &new_w);
		xcb_configure_window(conn, win->resize_regions[5], XCB_CONFIG_WINDOW_WIDTH, &new_w);
		
		p++;
	}

	if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
		win->geom[3] = true_vals[p];
		vals[p] = true_vals[p] - PAD_N - PAD;
		child_mask |= XCB_CONFIG_WINDOW_HEIGHT;

		uint32_t new_h = true_vals[p] - 2 * RESIZE_REGION_CORNER;
		xcb_configure_window(conn, win->resize_regions[3], XCB_CONFIG_WINDOW_HEIGHT, &new_h);
		xcb_configure_window(conn, win->resize_regions[7], XCB_CONFIG_WINDOW_HEIGHT, &new_h);

		p++;
	}

	xcb_configure_notify_event_t ev;
	ev.response_type = XCB_CONFIGURE_NOTIFY;
	ev.sequence = 0;
	ev.event = win->windows[WIN_CHILD];
	ev.window = win->windows[WIN_CHILD];
	ev.above_sibling = XCB_NONE;
	ev.x = win->geom[0];
	ev.y = win->geom[1] + PAD_N;
	ev.width = win->geom[2];
	ev.height = win->geom[3] - PAD_N;
	ev.border_width = 0;
	ev.override_redirect = 0;

	xcb_send_event(conn, 0, win->windows[WIN_CHILD], XCB_EVENT_MASK_NO_EVENT, (char *)&ev);

#ifdef ROUNDED
	/*if (p > c) {
		rounded_corners(win);	
	}*/
#ifdef SNAP_MAX_SMART
	if ((win->snap_index != SNAP_NONE && GAP == 0)|| win->is_i_full || win->is_e_full || win->snap_index == SNAP_U) {
#else
	if ((win->snap_index != SNAP_NONE && GAP == 0)|| win->is_i_full || win->is_e_full) {
#endif
		xcb_shape_mask(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, win->windows[WIN_PARENT], 0, 0, XCB_NONE);
	} else if (p > c) {
		rounded_corners(win);	
	}
#endif

	if (child_mask) {
		xcb_configure_window(conn, win->windows[WIN_CHILD], child_mask, vals + c);
	}
	
	xcb_configure_window(conn, win->windows[WIN_PARENT], mask, true_vals);
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
	xcb_change_window_attributes(conn, win->windows[WIN_CHILD], mask, &val);

	xcb_map_window(conn, win->windows[WIN_CHILD]);
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

void draw_region(window *win, int window_index, int pm_index) {
	int region_index = window_index - WIN_COUNT;
	xcb_copy_area_checked(conn, pixmaps[region_index][pm_index], win->windows[window_index],
			win->gc, 0, 0, 0, 0, controls[region_index].geom.width,
			controls[region_index].geom.height);

	win->last_pm[region_index] = pm_index;
}

typedef struct {
	uint32_t gravity;
	uint32_t cursor_index;
	xcb_rectangle_t geom;
} resize_region;

typedef struct {
	uint32_t cursor_index;
	xcb_cursor_t cursor;
} mywm_cursor;

int search_resize_regions(window *win, xcb_window_t id) {
	for (unsigned int i = 0; i < 8; i++) {
		if (win->resize_regions[i] == id) {
			return i;
		}
	}

	return -1;
}

#define CURSOR_DEFAULT 68

const uint16_t resize_cursor_indicies[8] = { 144, 140, 148, 98, 78, 18, 76, 72 };

xcb_cursor_t default_cursor;
xcb_cursor_t resize_cursors[8];

static xcb_cursor_t cursor_create(xcb_font_t font, XRenderColor fg, XRenderColor bg, uint16_t index) {
	xcb_cursor_t cursor = xcb_generate_id(conn);
	xcb_create_glyph_cursor(conn, cursor, font, font, index, index + 1, fg.red, fg.green, fg.blue,
			bg.red, bg.green, bg.blue);
	return cursor;
}

void cursor_init(void) {
	xcb_font_t cursor_font = xcb_generate_id(conn);
	xcb_open_font(conn, cursor_font, strlen("cursor"), "cursor");

	XRenderColor cursor_fg = hex_to_rgb(CURSOR_FG);
	XRenderColor cursor_bg = hex_to_rgb(CURSOR_BG);

	default_cursor = cursor_create(cursor_font, cursor_fg, cursor_bg, CURSOR_DEFAULT);
	
	for (int i = 0; i < 8; i++) {
		resize_cursors[i] = cursor_create(cursor_font, cursor_fg, cursor_bg, resize_cursor_indicies[i]);
	}

	xcb_close_font_checked(conn, cursor_font);

	xcb_change_window_attributes(conn, scr->root, XCB_CW_CURSOR, &default_cursor);
}

void cursor_clean(void) {
	xcb_free_cursor(conn, default_cursor);

	for (int i = 0; i < 8; i++) {
		xcb_free_cursor(conn, resize_cursors[i]);
	}
}

void install_normal_hints(window *win) {
	if (!xcb_icccm_get_wm_normal_hints_reply(conn, xcb_icccm_get_wm_normal_hints_unchecked(conn,
			win->windows[WIN_CHILD]), &win->hints, NULL)) {
		memset(&win->hints, 0, sizeof(xcb_size_hints_t));
		return;	
	}

#define MIN_WIDTH 160
#define MIN_HEIGHT 90

	if ((win->hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) &&
			!(win->hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
		win->hints.min_width = win->hints.base_width;
		win->hints.min_height = win->hints.base_height;
	} else {
		win->hints.min_width = MIN_WIDTH;
		win->hints.min_height = MIN_HEIGHT;
	}

	if (!(win->hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)) {
		win->hints.width_inc = 1;
		win->hints.height_inc = 1;
	}
}

static window *allocate_window(xcb_window_t child) {
	window *win = malloc(sizeof(window));
	win->windows[WIN_PARENT] = XCB_WINDOW_NONE;
	win->windows[WIN_CHILD] = child;
	win->ignore_unmap = 0;
	win->is_roll = 0;
	win->snap_index = SNAP_NONE;
	win->is_e_full = 0;
	win->is_i_full = 0;
	win->sticky = 0;
	win->above = 0;
	win->normal = 1;

	window_property_helper(xcb_ewmh_get_wm_window_type(ewmh, child), win, test_window_type);
	window_property_helper(xcb_ewmh_get_wm_state(ewmh, child), win, test_window_state);

	install_normal_hints(win);

	return win;
}

static window *create_window(window *win, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	if (!win->normal) {
		return win;
	}

	xcb_change_save_set(conn, XCB_SET_MODE_INSERT, win->windows[WIN_CHILD]);

	win->windows[WIN_PARENT] = xcb_generate_id(conn);
	
	uint32_t mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t vals[6];
	vals[0] = 0;
	xcb_configure_window(conn, win->windows[WIN_CHILD], mask, vals);
	
	mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK |
			XCB_CW_COLORMAP;
	vals[0] = 0xffffffff;
	vals[1] = 0xffffffff;
	vals[2] = 0;
	vals[3] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_FOCUS_CHANGE;
	vals[4] = cm;
	xcb_create_window(conn, depth, win->windows[WIN_PARENT], scr->root, x, y, w, h, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, vis, mask, vals);

	win->gc = xcb_generate_id(conn);
	vals[0] = 0xffffff00;
	vals[1] = 0;
	xcb_create_gc(conn, win->gc, win->windows[WIN_PARENT], XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES, vals);

	for (int i = WIN_COUNT, j = 0; i < REGION_COUNT; i++, j++) {
		win->windows[i] = xcb_generate_id(conn);

		mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_WIN_GRAVITY | 
				XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
		vals[0] = controls[i - WIN_COUNT].colors[PM_BG(PM_FOCUS)];
		vals[1] = 0xff000000;
		vals[2] = controls[j].gravity;
		vals[3] = 0;
		vals[4] = XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE |
				XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW;
		vals[5] = cm;
		xcb_create_window(conn, depth, win->windows[i], win->windows[WIN_PARENT],
				controls[j].geom.x, controls[j].geom.y, controls[j].geom.width, 
				controls[j].geom.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
				vis, mask, vals);

		xcb_map_window(conn, win->windows[i]);

		xcb_flush(conn);

		draw_region(win, i, PM_FOCUS);
	}

	xcb_reparent_window(conn, win->windows[WIN_CHILD], win->windows[WIN_PARENT], PAD, PAD_N);

	resize_region resize_regions[8] = {
		{ XCB_GRAVITY_NORTH_WEST, 144, { 0, 0, RESIZE_REGION_CORNER, RESIZE_REGION_CORNER } },
		{ XCB_GRAVITY_NORTH_WEST, 140, { RESIZE_REGION_CORNER, 0, w - 2 * RESIZE_REGION_CORNER, RESIZE_REGION_WIDTH } },
		{ XCB_GRAVITY_NORTH_EAST, 148, { w - RESIZE_REGION_CORNER, 0, RESIZE_REGION_CORNER, RESIZE_REGION_CORNER } },
		{ XCB_GRAVITY_NORTH_EAST, 98, { w - RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER, RESIZE_REGION_WIDTH, h - 2 * RESIZE_REGION_CORNER } },
		{ XCB_GRAVITY_SOUTH_EAST, 78, { w - RESIZE_REGION_CORNER, h - RESIZE_REGION_CORNER, RESIZE_REGION_CORNER, RESIZE_REGION_CORNER } },
		{ XCB_GRAVITY_SOUTH_WEST, 18, { RESIZE_REGION_CORNER, h - RESIZE_REGION_WIDTH, w - 2 * RESIZE_REGION_CORNER, RESIZE_REGION_WIDTH } },
		{ XCB_GRAVITY_SOUTH_WEST, 76, { 0, h - RESIZE_REGION_CORNER, RESIZE_REGION_CORNER, RESIZE_REGION_CORNER } },
		{ XCB_GRAVITY_NORTH_WEST, 72, { 0, RESIZE_REGION_CORNER, RESIZE_REGION_WIDTH, h - RESIZE_REGION_CORNER } },
	};

	for (int i = 0; i < 8; i++) {
		win->resize_regions[i] = xcb_generate_id(conn);
		mask = XCB_CW_WIN_GRAVITY | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
		vals[0] = resize_regions[i].gravity;
		vals[1] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_ENTER_WINDOW;
		vals[2] = resize_cursors[i];
		xcb_create_window(conn, XCB_COPY_FROM_PARENT, win->resize_regions[i],
			win->windows[WIN_PARENT], resize_regions[i].geom.x, resize_regions[i].geom.y,
			resize_regions[i].geom.width, resize_regions[i].geom.height, 0,
			XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, mask, vals);
	}
	
	xcb_rectangle_t resize_region_rects[4][2] = {
		{ { 0, 0, RESIZE_REGION_CORNER, RESIZE_REGION_WIDTH }, { 0, RESIZE_REGION_WIDTH, RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH } },
		{ { 0, 0, RESIZE_REGION_CORNER, RESIZE_REGION_WIDTH }, { RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH, RESIZE_REGION_WIDTH, RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH } },
		{ { RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH, 0, RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER }, { 0, RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER, RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH } },
		{ { 0, 0, RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER }, { RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH, RESIZE_REGION_CORNER - RESIZE_REGION_WIDTH, RESIZE_REGION_WIDTH } }, 
	};

	for (int i = 0; i < 4; i++) {
		xcb_shape_rectangles(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
			XCB_CLIP_ORDERING_UNSORTED, win->resize_regions[2 * i], 0, 0, 2,
			resize_region_rects[i]);
	}

	for (int i = 0; i < 8; i++) {
		xcb_map_window(conn, win->resize_regions[i]);
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
	
	show(win);

	if (state != MOVE && state != RESIZE) {
		focus(win);
	} else {
		unfocus(win);
	}

	return win;
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

void create_window_new(xcb_window_t child) {
	window *win = allocate_window(child);

	uint32_t w;
	uint32_t h;
	uint32_t x;
	uint32_t y;
	xcb_get_geometry_reply_t *geom = w_get_geometry(child);
	if (win->hints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION) {
		w = geom->width;
		h = geom->height;
		x = geom->x;
		y = geom->y;
	} else {
		xcb_query_pointer_reply_t *ptr = w_query_pointer();
		w = size_helper(geom->width + 2 * PAD, scr->width_in_pixels);
		h = size_helper(geom->height + PAD_N + PAD, scr->height_in_pixels - TOP - BOT);
		x = place_helper(ptr->root_x, w, scr->width_in_pixels);
		y = TOP + place_helper(ptr->root_y - TOP, h, scr->height_in_pixels - TOP - BOT);
		free(ptr);
	}
	free(geom);

	create_window(win, x, y, w, h);
	
	insert_into(curws, win);
}

static uint32_t create_window_existing_place_helper(int start, uint32_t win_sze, uint32_t scr_sze) {
	if (start < 0) {
		return 0;
	} else if (start + win_sze > scr_sze) {
		return scr_sze - win_sze;
	}

	return start;
}

void create_window_existing(xcb_window_t child) {
	window *win = allocate_window(child);

	xcb_get_geometry_reply_t *geom = w_get_geometry(child);
	uint32_t w = size_helper(geom->width + 2 * PAD, scr->width_in_pixels);
	uint32_t h = size_helper(geom->height + PAD_N + PAD, scr->height_in_pixels - TOP - BOT);
	uint32_t x = create_window_existing_place_helper(geom->x - PAD, w, scr->width_in_pixels);
	uint32_t y = TOP + create_window_existing_place_helper(geom->y - TOP - PAD_N, h, scr->height_in_pixels - TOP - BOT);
	free(geom);

	create_window(win, x, y, w, h);
	win->ignore_unmap = 1;
	
	insert_into(curws, win);
}
