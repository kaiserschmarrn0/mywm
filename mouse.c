#include "mouse.h"
#include "mywm.h"
#include "workspace.h"

#include "action.h"
#include "snap.h"
#include "margin.h"

static uint32_t x;
static uint32_t y;

static uint32_t east;
static uint32_t south;

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

static void mouse_move_motion(void *arg) {
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

static void mouse_move_motion_start(void *arg) {
	if (stack[curws].fwin->snap_index != SNAP_NONE) {
		snap_restore_state(stack[curws].fwin);
	}

	mouse_move_motion(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_move_motion;
}

void button_release(void *arg) {
	deactivate_margins();

	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
	state = DEFAULT;

	events[XCB_MOTION_NOTIFY] = NULL; //jic
	events[XCB_BUTTON_RELEASE] = NULL; //jic
}

static void grab_pointer_cursor(xcb_cursor_t cursor) {
	uint32_t mask = XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
			XCB_EVENT_MASK_POINTER_MOTION;
	uint32_t type = XCB_GRAB_MODE_ASYNC;
	xcb_grab_pointer(conn, 1, scr->root, mask, type, type, scr->root, cursor,
			XCB_CURRENT_TIME);
}

inline void grab_pointer(void) {
	grab_pointer_cursor(XCB_NONE);
}

void mouse_move(void *arg) {
	press_arg *info = (press_arg *)arg;

	xcb_get_geometry_reply_t *geom;
	
	if (!move_resize_helper(info->win, &geom)) {
		return;
	}

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_move_motion_start;
	events[XCB_BUTTON_RELEASE] = (void (*)(xcb_generic_event_t *))button_release;

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

static void resize_release(void *arg) {
	button_release(arg);

	events[XCB_ENTER_NOTIFY] = enter_notify;
	events[XCB_LEAVE_NOTIFY] = leave_notify;
}

static int mouse_resize(press_arg *info) {
	xcb_get_geometry_reply_t *geom;

	if (!move_resize_helper(info->win, &geom)) {
		return 0;
	}

	events[XCB_ENTER_NOTIFY] = NULL;
	events[XCB_LEAVE_NOTIFY] = NULL;
	
	events[XCB_BUTTON_RELEASE] = (void (*)(xcb_generic_event_t *))resize_release;

	/*x = stack[curws].fwin->geom[GEOM_W] - info->event_x;
	y = stack[curws].fwin->geom[GEOM_H] - info->event_y;*/

	state = RESIZE;
	
	return 1;
}

static void mouse_resize_set_x_east(press_arg *arg) {
	x = stack[curws].fwin->geom[GEOM_W] - arg->event_x;
}

static void mouse_resize_set_x_west(press_arg *arg) {
	x = arg->event_x - stack[curws].fwin->geom[GEOM_X];
}

static void mouse_resize_set_y_south(press_arg *arg) {
	y = stack[curws].fwin->geom[GEOM_H] - arg->event_y;
}

static void mouse_resize_set_y_north(press_arg *arg) {
	y = arg->event_y - stack[curws].fwin->geom[GEOM_Y];
}

static void mouse_resize_set_east(void) {
	east = stack[curws].fwin->geom[GEOM_X] + stack[curws].fwin->geom[GEOM_W];
}

static void mouse_resize_set_south(void) {
	south = stack[curws].fwin->geom[GEOM_Y] + stack[curws].fwin->geom[GEOM_H];
}

static void mouse_resize_motion_south_east(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();

	uint32_t vals[2];
	vals[0] = p->root_x + x;
	vals[1] = p->root_y + y;
	update_geometry(stack[curws].fwin, RESIZE_MASK, vals);
	
	free(p);
}

static void mouse_resize_motion_south_east_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_south_east(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_south_east;
}

void mouse_resize_south_east(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}
	
	grab_pointer_cursor(resize_cursors[4]);
	
	mouse_resize_set_x_east((press_arg *)arg);
	mouse_resize_set_y_south((press_arg *)arg);

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_south_east_start;
}

static void mouse_resize_motion_south(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();

	uint32_t vals[1];
	vals[0] = p->root_y + y;
	update_geometry(stack[curws].fwin, XCB_CONFIG_WINDOW_HEIGHT, vals);
	
	free(p);
}

static void mouse_resize_motion_south_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_south(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_south;
}

void mouse_resize_south(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}

	grab_pointer_cursor(resize_cursors[5]);
	
	mouse_resize_set_y_south((press_arg *)arg);

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_south_start;
}

static void mouse_resize_motion_north(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();

	uint32_t vals[2];
	vals[0] = p->root_y - y;
	vals[1] = south - p->root_y + y;
	update_geometry(stack[curws].fwin, XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT, vals);
	
	free(p);
}

static void mouse_resize_motion_north_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_north(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_north;
}

void mouse_resize_north(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}
	
	grab_pointer_cursor(resize_cursors[1]);
	
	mouse_resize_set_y_north((press_arg *)arg);
	mouse_resize_set_south();

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_north_start;
}

static void mouse_resize_motion_west(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();

	uint32_t vals[2];
	vals[0] = p->root_x - x;
	vals[1] = east - p->root_x + x;	

	update_geometry(stack[curws].fwin, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH, vals);
	free(p);
}

static void mouse_resize_motion_west_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_west(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_west;
}

void mouse_resize_west(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}
	
	grab_pointer_cursor(resize_cursors[7]);
	
	mouse_resize_set_x_west((press_arg *)arg);
	mouse_resize_set_east();

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_west_start;
}

static void mouse_resize_motion_east(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();

	uint32_t vals[1];
	vals[0] = p->root_x + x;

	update_geometry(stack[curws].fwin, XCB_CONFIG_WINDOW_WIDTH, vals);
	free(p);
}

static void mouse_resize_motion_east_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_east(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_east;
}

void mouse_resize_east(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}
	
	grab_pointer_cursor(resize_cursors[3]);
	
	mouse_resize_set_x_east((press_arg *)arg);

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_east_start;
}

static void mouse_resize_motion_north_west(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();
	uint32_t vals[4];
	vals[0] = p->root_x - x;
	vals[1] = p->root_y - y;
	vals[2] = east - p->root_x + x;	
	vals[3] = south - p->root_y + y;
	free(p);

	update_geometry(stack[curws].fwin, MOVE_RESIZE_MASK, vals);
}

static void mouse_resize_motion_north_west_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_west(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_north_west;
}

void mouse_resize_north_west(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}
	
	grab_pointer_cursor(resize_cursors[0]);
	
	mouse_resize_set_x_west((press_arg *)arg);
	mouse_resize_set_y_north((press_arg *)arg);
	mouse_resize_set_east();
	mouse_resize_set_south();

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_north_west_start;
}

static void mouse_resize_motion_north_east(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();
	uint32_t vals[3];
	vals[0] = p->root_y - y;
	vals[1] = p->root_x + x;
	vals[2] = south - p->root_y + y;
	free(p);

	update_geometry(stack[curws].fwin, XCB_CONFIG_WINDOW_Y | RESIZE_MASK, vals);
}

static void mouse_resize_motion_north_east_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_north_east(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_north_east;
}

void mouse_resize_north_east(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}
	
	grab_pointer_cursor(resize_cursors[2]);
	
	mouse_resize_set_x_east((press_arg *)arg);
	mouse_resize_set_y_north((press_arg *)arg);
	mouse_resize_set_south();

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_north_east_start;
}

static void mouse_resize_motion_south_west(void *arg) {
	xcb_query_pointer_reply_t *p = w_query_pointer();
	uint32_t vals[3];
	vals[0] = p->root_x - x;
	vals[1] = east - p->root_x + x;
	vals[2] = p->root_y + y;
	free(p);

	update_geometry(stack[curws].fwin, XCB_CONFIG_WINDOW_X | RESIZE_MASK, vals);
}

static void mouse_resize_motion_south_west_start(void *arg) {
	stack[curws].fwin->snap_index = SNAP_NONE;

	mouse_resize_motion_south_west(NULL);
	
	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_south_west;
}

void mouse_resize_south_west(void *arg) {
	if (!mouse_resize((press_arg *)arg)) {
		return;
	}
	
	grab_pointer_cursor(resize_cursors[6]);
	
	mouse_resize_set_x_west((press_arg *)arg);
	mouse_resize_set_y_south((press_arg *)arg);
	mouse_resize_set_east();

	events[XCB_MOTION_NOTIFY] = (void (*)(xcb_generic_event_t *))mouse_resize_motion_south_west_start;
}
