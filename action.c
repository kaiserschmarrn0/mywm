#include "mywm.h"
#include "workspace.h"
#include "action.h"

//used for mouse data
static uint32_t x = 0;
static uint32_t y = 0;

//used for cycling
static window *marker = NULL;

void stick(void *arg) {
	if (stack[curws].fwin) {
		stick_helper(stack[curws].fwin);
	}
}

void close(void *arg) {
	if (!stack[curws].fwin) {
		return;
	}

	xcb_unmap_window(conn, stack[curws].fwin->windows[WIN_PARENT]);
	xcb_unmap_window(conn, stack[curws].fwin->windows[WIN_CHILD]);

	close_helper(stack[curws].fwin->windows[WIN_CHILD]);

	forget_client(stack[curws].fwin, curws);
}

static void cycle_raise(window *cur) {
	for (; cur != stack[curws].fwin;) {
		window *temp = cur->prev[curws][TYPE_NORMAL];
		mywm_raise(cur); 
		cur = temp;
	}
}

void stop_cycle() {
	state = DEFAULT;
	traverse(curws, TYPE_NORMAL, normal_events); 
}

void cycle(void *arg) {
	if (!stack[curws].lists[TYPE_NORMAL] ||
			!stack[curws].lists[TYPE_NORMAL]->next[curws][TYPE_NORMAL]) {
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
		mywm_raise(marker->next[curws][TYPE_NORMAL]);
	} else {
		cycle_raise(marker);
		center_pointer(stack[curws].lists[TYPE_NORMAL]);
		marker = stack[curws].fwin;
	}
}

void change_ws(void *arg) {
	int new_ws = *(int *)arg;

	if (new_ws == curws) {
		return;
	}

	traverse(new_ws, TYPE_NORMAL, show);
	traverse(curws, TYPE_NORMAL, hide); 
	
	curws = new_ws;

	refocus(new_ws);
}

void send_ws(void *arg) {
	int new_ws = *(int *)arg;

	if (!stack[curws].fwin || new_ws == curws || stack[curws].fwin->sticky) {
		return;
	}

	stack[curws].fwin->ignore_unmap = 1;
	xcb_unmap_window(conn, stack[curws].fwin->windows[WIN_PARENT]);

	excise_from(curws, stack[curws].fwin);
	insert_into(new_ws, stack[curws].fwin);
	stack[curws].fwin = NULL;
}

static void snap_save_state(window *win) {
	save_state(win, win->before_snap);
	
	win->is_snap = 1;
}

static void snap_restore_state(window *win) {
	win->is_snap = 0;
	
	update_geometry(win, MOVE_RESIZE_MASK, win->before_snap);
}

#define SNAP_TEMPLATE(A, B, C, D, E) void A(void *arg) {                                          \
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
	safe_raise(stack[curws].fwin);                                                            \
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

void int_full(void *arg) {
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

static int move_resize_helper(xcb_window_t win, xcb_get_geometry_reply_t **geom) {
	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, win);
	if (!found || !found->normal || found != stack[curws].fwin) {
		return 0;
	}

	safe_raise(stack[curws].fwin);
	
	if (stack[curws].fwin->is_e_full || stack[curws].fwin->is_i_full) {
		return 0;
	}

	return 1;
}

static void grab_pointer() {
	uint32_t mask = XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
			XCB_EVENT_MASK_POINTER_MOTION;
	uint32_t type = XCB_GRAB_MODE_ASYNC;
	xcb_grab_pointer(conn, 0, scr->root, mask, type, type, scr->root, XCB_NONE,
			XCB_CURRENT_TIME);
}

void mouse_move(void *arg) {
	press_arg *info = (press_arg *)arg;

	xcb_get_geometry_reply_t *geom;
	
	if (!move_resize_helper(info->win, &geom)) {
		return;
	}

	if (stack[curws].fwin->is_snap) {
		x = stack[curws].fwin->before_snap[GEOM_W] *
				(info->event_x - stack[curws].fwin->geom[GEOM_X]) /
				stack[curws].fwin->geom[GEOM_W];
		y = stack[curws].fwin->before_snap[GEOM_H] *
				(info->event_y - stack[curws].fwin->geom[GEOM_Y]) /
				stack[curws].fwin->geom[GEOM_H];
	} else {
		x = info->event_x - stack[curws].fwin->geom[GEOM_X];
		y = info->event_y - stack[curws].fwin->geom[GEOM_Y];
	}
	
	state = MOVE;

	grab_pointer();
}

void mouse_resize(void *arg) {
	press_arg *info = (press_arg *)arg;

	xcb_get_geometry_reply_t *geom;

	if (!move_resize_helper(info->win, &geom)) {
		return;
	}

	stack[curws].fwin->is_snap = 0;
	x = stack[curws].fwin->geom[GEOM_W] - info->event_x;
	y = stack[curws].fwin->geom[GEOM_H] - info->event_y;

	state = RESIZE;
	
	grab_pointer();
}

static void mouse_snap(uint32_t ptr_pos, uint32_t tolerance, void (*snap_x)(void *arg),
		void (*snap_y)(void *arg), void (*snap_z)(void *arg)) {
	if (ptr_pos < SNAP_CORNER) {
		snap_x(NULL);
	} else if (ptr_pos > tolerance - SNAP_CORNER) {
		snap_y(NULL);
	} else {
		snap_z(NULL);
	}
}

void mouse_move_motion(void *arg) {
	xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)arg;
	xcb_query_pointer_reply_t *p = w_query_pointer();

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

	free(p);
}

void mouse_resize_motion(void *arg) {
	xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)arg;
	xcb_query_pointer_reply_t *p = w_query_pointer();

	uint32_t vals[2];
	vals[0] = p->root_x + x;
	vals[1] = p->root_y + y;
	update_geometry(stack[curws].fwin, RESIZE_MASK, vals);
	
	free(p);
}

void button_release(void *arg) {
	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
	state = DEFAULT;

	events[XCB_MOTION_NOTIFY] = NULL; //jic
	events[XCB_BUTTON_RELEASE] = NULL; //jic
}

void mouse_roll_up(void *arg) {
	press_arg *info = (press_arg *)arg;

	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, info->win);
	if (!found || !found->normal || found != stack[curws].fwin || found->is_roll) {
		return;
	}
	
	found->before_roll = found->geom[GEOM_H];

	uint32_t val = TITLE;
	update_geometry(found, XCB_CONFIG_WINDOW_HEIGHT, &val);
	
	found->is_roll = 1;
}

void mouse_roll_down(void *arg) {
	press_arg *info = (press_arg *)arg;

	window *found = search_ws(curws, TYPE_NORMAL, WIN_PARENT, info->win);
	if (!found || !found->normal || found != stack[curws].fwin || !found->is_roll) {
		return;
	}

	update_geometry(found, XCB_CONFIG_WINDOW_HEIGHT, &found->before_roll);
	
	found->is_roll = 0;
}
