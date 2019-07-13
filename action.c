#include <xcb/shape.h>

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

static void start_cycle(void);
static void cycle_iterate(void);
static void cycle_iterate_restart(void);
static void cycle_iterate_subsequent(void);

static void (*cycle_handler)(void) = start_cycle;
static void (*cycle_iterate_helper)(void) = cycle_iterate_subsequent;

static void cycle_raise(window *cur) {
	window *last = stack[curws].lists[TYPE_NORMAL];
	for (; cur != last;) {
		window *temp = cur->prev[curws][TYPE_NORMAL];
		mywm_raise(cur); 
		cur = temp;
	}
}

static void cycle_iterate_subsequent_helper(window *temp) {
	mywm_raise(temp);
	center_pointer(temp);
	focus(temp);
}

static void cycle_iterate_subsequent(void) {
	window *temp = marker;
	marker = marker->next[curws][TYPE_NORMAL];

	cycle_raise(temp);

	cycle_iterate_subsequent_helper(temp);	
}

static void cycle_iterate_restart(void) {
	mywm_lower(stack[curws].lists[TYPE_NORMAL]);
	cycle_iterate_subsequent();
	cycle_iterate_helper = cycle_iterate_subsequent;	
}

static void cycle_iterate_last_helper(void) {
	window *temp = marker;
	marker = stack[curws].lists[TYPE_NORMAL];

	mywm_raise(temp);
	center_pointer(temp);
	focus(temp);

	cycle_iterate_helper = cycle_iterate_restart;
}

static void cycle_iterate(void) {
	if (marker->next[curws][TYPE_NORMAL]) {
		cycle_iterate_helper();
	} else {
		cycle_raise(marker);
		
		cycle_iterate_last_helper();
	}
}

static void start_cycle() {
	events[XCB_ENTER_NOTIFY] = NULL;
	
	state = CYCLE;
	cycle_handler = cycle_iterate;
	traverse(curws, TYPE_NORMAL, release_events);

	if (stack[curws].fwin == stack[curws].lists[TYPE_NORMAL]) {
		marker = stack[curws].fwin->next[curws][TYPE_NORMAL];
	} else {
		marker = stack[curws].fwin;
	}

	if (marker->next[curws][TYPE_NORMAL]) {
		window *temp = marker;
		marker = marker->next[curws][TYPE_NORMAL];
	
		cycle_iterate_subsequent_helper(temp);	
	} else {
		cycle_iterate_last_helper();
	}
}

void stop_cycle() {
	state = DEFAULT;
	events[XCB_ENTER_NOTIFY] = enter_notify;
	traverse(curws, TYPE_NORMAL, reset_events); 
	cycle_handler = start_cycle;
	cycle_iterate_helper = cycle_iterate_subsequent;
}

void cycle(void *arg) {
	if (!stack[curws].lists[TYPE_NORMAL] ||
			!stack[curws].lists[TYPE_NORMAL]->next[curws][TYPE_NORMAL]) {
		return;
	}

	cycle_handler();
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

	refocus(curws);
}

static uint32_t snap_regions[7][4];

void create_snap_regions() {
	/* ld */

	snap_regions[0][0] = GAP;
	snap_regions[0][1] = (scr->height_in_pixels - TOP - BOT) / 2 + GAP / 2 + TOP;
	snap_regions[0][2] = scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2;
	snap_regions[0][3] = (scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER;
	
	/* l */

	snap_regions[1][0] = GAP;
	snap_regions[1][1] = GAP + TOP;
	snap_regions[1][2] = scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2;
	snap_regions[1][3] = scr->height_in_pixels - 2 * GAP - 2 * BORDER - TOP - BOT;

	/* lu */

	snap_regions[2][0] = GAP;
	snap_regions[2][1] = GAP + TOP;
	snap_regions[2][2] = scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2;
	snap_regions[2][3] = (scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER;

	/* max */

#ifndef SNAP_MAX_SMART
	snap_regions[3][0] = GAP;
	snap_regions[3][1] = GAP + TOP;
	snap_regions[3][2] = scr->width_in_pixels - 2 * GAP - 2 * BORDER;
	snap_regions[3][3] = scr->height_in_pixels - 2 * GAP - 2 * BORDER - TOP - BOT;
#else
	snap_regions[3][0] = 0;
	snap_regions[3][1] = TOP;
	snap_regions[3][2] = scr->width_in_pixels - 2 * GAP - 2 * BORDER;
	snap_regions[3][3] = scr->height_in_pixels - 2 * GAP - 2 * BORDER - TOP - BOT;
#endif

	/* ru */

	snap_regions[4][0] = scr->width_in_pixels / 2 + GAP / 2;
	snap_regions[4][1] = GAP + TOP;
	snap_regions[4][2] = scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2;
	snap_regions[4][3] = (scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER;
	
	/* r */

	snap_regions[5][0] = scr->width_in_pixels / 2 + GAP / 2;
	snap_regions[5][1] = GAP + TOP;
	snap_regions[5][2] = scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2;
	snap_regions[5][3] = scr->height_in_pixels - 2 * GAP - 2 * BORDER - TOP - BOT;
	
	/* rd */
	
	snap_regions[6][0] = scr->width_in_pixels / 2 + GAP / 2;
	snap_regions[6][1] = (scr->height_in_pixels - TOP - BOT) / 2 + GAP / 2 + TOP;
	snap_regions[6][2] = scr->width_in_pixels / 2 - 1.5 * GAP - BORDER * 2;
	snap_regions[6][3] = (scr->height_in_pixels - TOP - BOT) / 2 - 1.5 * GAP - 2 * BORDER;
}

static void snap_save_state(window *win, int index) {
	save_state(win, win->before_snap);
	
	win->snap_index = index;
}

static void snap_restore_state(window *win) {
	win->snap_index = SNAP_NONE;
	
	update_geometry(win, MOVE_RESIZE_MASK, win->before_snap);
}

void snap(void *arg) {
	if (!stack[curws].fwin || stack[curws].fwin->is_e_full || stack[curws].fwin->is_i_full) {
		return;
	}

	int index = *(int *)arg;
	if (index == stack[curws].fwin->snap_index) {
		snap_restore_state(stack[curws].fwin);
		return;
	}

	if (stack[curws].fwin->snap_index == SNAP_NONE) {
		snap_save_state(stack[curws].fwin, index);
	} else {
		stack[curws].fwin->snap_index = index;
	}

	update_geometry(stack[curws].fwin, MOVE_RESIZE_MASK, snap_regions[index]);

	if (state == MOVE) {
		return;
	}

	safe_raise(stack[curws].fwin);
}

#define snap_macro(A, B) void A(void *arg) { \
	snap((void *)&(int){ B } );          \
}

snap_macro(snap_ld, SNAP_LD);
snap_macro(snap_l, SNAP_L);
snap_macro(snap_lu, SNAP_LU);
snap_macro(snap_u, SNAP_U);
snap_macro(snap_ru, SNAP_RU);
snap_macro(snap_r, SNAP_R);
snap_macro(snap_rd, SNAP_RD);

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
	xcb_grab_pointer(conn, 1, scr->root, mask, type, type, scr->root, XCB_NONE,
			XCB_CURRENT_TIME);
}

void region_press(void *arg) {
	state = PRESS;
	grab_pointer();
}

void region_abort() {
	state = DEFAULT;

	events[XCB_BUTTON_RELEASE] = NULL;

	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
}

void region_release_helper(void (*action)(void *), void *arg) {
	region_abort();	

	action(arg);
}

void region_leave_handler(xcb_leave_notify_event_t *ev) {
	
}

void region_close(void *arg) {
	region_release_helper(close, arg);
}

void region_snap_u(void *arg) {
	region_release_helper(snap_u, arg);
}

/* 
 * mouse snapping would be better served by a map,
 * but because of the way X works, we recieve
 * unwanted enter events when dragging, and must
 * iterate through all of the regions anyway.
 */

#define MARGIN 5

typedef struct {
	xcb_rectangle_t rects[2];
	int rect_count;
	
	xcb_window_t win;

	int arg;
} margin;

static margin margins[] = {
	{ { }, 2, XCB_WINDOW_NONE, SNAP_LD  },
	{ { }, 1, XCB_WINDOW_NONE, SNAP_L   },
	{ { }, 2, XCB_WINDOW_NONE, SNAP_LU  },
	{ { }, 1, XCB_WINDOW_NONE, SNAP_U   },
	{ { }, 2, XCB_WINDOW_NONE, SNAP_RU  },
	{ { }, 1, XCB_WINDOW_NONE, SNAP_R   },
	{ { }, 2, XCB_WINDOW_NONE, SNAP_RD  },
};

static void traverse_margins(void (*func)(int)) {
	for (int i = 0; i < LEN(margins); i++) {
		func(i);
	}
}

static void shape_margin(int i) {
	xcb_shape_rectangles(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
			XCB_CLIP_ORDERING_UNSORTED, margins[i].win, 0, 0, margins[i].rect_count,
			margins[i].rects);
}

static void create_margin_window(int i) {
	margins[i].win = xcb_generate_id(conn);
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t val = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW;
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, margins[i].win, scr->root, 0, 0,
			scr->width_in_pixels, scr->height_in_pixels, 0,
			XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, mask, &val);
}

void create_margins() {
	traverse_margins(create_margin_window);

	uint32_t corner_width = scr->width_in_pixels / 4;
	uint32_t corner_height = scr->height_in_pixels / 4;

	uint32_t width = scr->width_in_pixels - 2 * corner_width; //odd_numbers
	uint32_t height = scr->height_in_pixels - 2 * corner_height; //odd_numbers

	/* dl */

	margins[0].rects[0].x = 0;
	margins[0].rects[0].y = scr->height_in_pixels - corner_height;
	margins[0].rects[0].width = MARGIN;
	margins[0].rects[0].height = corner_height;

	margins[0].rects[1].x = 0;
	margins[0].rects[1].y = scr->height_in_pixels - MARGIN;
	margins[0].rects[1].width = corner_width;
	margins[0].rects[1].height = MARGIN;
	
	/* l */

	margins[1].rects[0].x = 0;
	margins[1].rects[0].y = corner_height;
	margins[1].rects[0].width = MARGIN;
	margins[1].rects[0].height = height;

	/* ul */

	margins[2].rects[0].x = 0;
	margins[2].rects[0].y = 0;
	margins[2].rects[0].width = MARGIN;
	margins[2].rects[0].height = corner_height;

	margins[2].rects[1].x = 0;
	margins[2].rects[1].y = 0;
	margins[2].rects[1].width = corner_width;
	margins[2].rects[1].height = MARGIN;

	/* t */

	margins[3].rects[0].x = corner_width;
	margins[3].rects[0].y = 0;
	margins[3].rects[0].width = width;
	margins[3].rects[0].height = MARGIN;

	/* ur */

	margins[4].rects[0].x = scr->width_in_pixels - corner_width;
	margins[4].rects[0].y = 0;
	margins[4].rects[0].width = corner_width;
	margins[4].rects[0].height = MARGIN;

	margins[4].rects[1].x = scr->width_in_pixels - MARGIN;
	margins[4].rects[1].y = 0;
	margins[4].rects[1].width = MARGIN;
	margins[4].rects[1].height = corner_height;

	/* r */

	margins[5].rects[0].x = scr->width_in_pixels - MARGIN;
	margins[5].rects[0].y = corner_height;
	margins[5].rects[0].width = MARGIN;
	margins[5].rects[0].height = height;

	/* dr */

	margins[6].rects[0].x = scr->width_in_pixels - MARGIN;
	margins[6].rects[0].y = scr->height_in_pixels - corner_height;
	margins[6].rects[0].width = MARGIN;
	margins[6].rects[0].height = corner_height;

	margins[6].rects[1].x = scr->width_in_pixels - corner_width;
	margins[6].rects[1].y = scr->height_in_pixels - MARGIN;
	margins[6].rects[1].width = corner_width;
	margins[6].rects[1].height = MARGIN;
	
	traverse_margins(shape_margin);
}

static void activate_margin(int i) {
	xcb_map_window(conn, margins[i].win);
}

static void deactivate_margin(int i) {
	xcb_unmap_window(conn, margins[i].win);
}

static void raise_margin(int i) {
	stack_above_helper(margins[i].win);
}

static void raise_margins() {
	traverse_margins(raise_margin);	
}

static void activate_margins() {
	raise_margins();
	traverse_margins(activate_margin);
}

static void deactivate_margins() {
	traverse_margins(deactivate_margin);
}

static void margin_enter_helper(int i) {
	snap((void *)&margins[i].arg);
}

static void margin_leave_helper(int i) {
	if (stack[curws].fwin->snap_index != SNAP_NONE) {
		snap_restore_state(stack[curws].fwin);
	}
}

void margin_event_helper(xcb_generic_event_t *ev, void (*func)(int)) {
	if (state != MOVE) {
		return;
	}
	
	xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;

	for (int i = 0; i < LEN(margins); i++) {
		if (e->event == margins[i].win) {
			func(i);
			return;
		}
	}
}

void margin_enter_handler(xcb_generic_event_t *ev) {
	margin_event_helper(ev, margin_enter_helper);
}

void margin_leave_handler(xcb_generic_event_t *ev) {
	margin_event_helper(ev, margin_leave_helper);
}

void mouse_move(void *arg) {
	press_arg *info = (press_arg *)arg;

	xcb_get_geometry_reply_t *geom;
	
	if (!move_resize_helper(info->win, &geom)) {
		return;
	}

	if (stack[curws].fwin->snap_index != SNAP_NONE) {
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
	
	activate_margins();

	grab_pointer();
}

void mouse_resize(void *arg) {
	press_arg *info = (press_arg *)arg;

	xcb_get_geometry_reply_t *geom;

	if (!move_resize_helper(info->win, &geom)) {
		return;
	}

	events[XCB_ENTER_NOTIFY] = NULL;
	events[XCB_LEAVE_NOTIFY] = NULL;

	stack[curws].fwin->snap_index = SNAP_NONE;
	x = stack[curws].fwin->geom[GEOM_W] - info->event_x;
	y = stack[curws].fwin->geom[GEOM_H] - info->event_y;

	state = RESIZE;
	
	grab_pointer();
}

void mouse_move_motion(void *arg) {
	if (stack[curws].fwin->snap_index != SNAP_NONE) {
		return;
	}

	xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)arg;
	xcb_query_pointer_reply_t *p = w_query_pointer();
	
	uint32_t vals[2];
	vals[0] = p->root_x - x;
	vals[1] = p->root_y - y;
	update_geometry(stack[curws].fwin, MOVE_MASK, vals);

	free(p);
}

void mouse_move_motion_start(void *arg) {
	if (stack[curws].fwin->snap_index != SNAP_NONE) {
		snap_restore_state(stack[curws].fwin);
	}

	mouse_move_motion(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_move_motion;
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
	deactivate_margins();

	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
	state = DEFAULT;

	events[XCB_MOTION_NOTIFY] = NULL; //jic
	events[XCB_BUTTON_RELEASE] = NULL; //jic
}

void resize_release(void *arg) {
	button_release(arg);

	events[XCB_ENTER_NOTIFY] = enter_notify;
	events[XCB_LEAVE_NOTIFY] = leave_notify;
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
