#include <xcb/shape.h>

#include "action.h"
#include "mywm.h"
#include "workspace.h"
#include "mouse.h"
#include "snap.h"

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

static void select_window_init(void);

static void (*select_window_handler)(void) = select_window_init;
static window **select_window_list = NULL;
static unsigned int select_window_index = 0;

static void select_window_iterate(void) {
	for (int i = select_window_index; i >= 0; i--) {
		mywm_raise(select_window_list[i]);
	}

	unsigned int new_index = select_window_index + 1;
	if (new_index < stack[curws].lists[TYPE_NORMAL].count) {
		select_window_index = new_index;
	} else {
		select_window_index = 0;
	}

	window *win = select_window_list[select_window_index];
	mywm_raise(win);
	center_pointer(win);
	focus(win);
}

static void select_window_init(void) {
	state = SELECT_WINDOW;
	events[XCB_ENTER_NOTIFY] = NULL;
	events[XCB_LEAVE_NOTIFY] = NULL;
	traverse(curws, TYPE_NORMAL, release_events);

	int count = stack[curws].lists[TYPE_NORMAL].count;
	select_window_list = malloc(count * sizeof(window *));

	window *cur = stack[curws].lists[TYPE_NORMAL].first;
	for (int i = 0; i < count && cur; i++) {
		if (cur == stack[curws].fwin) {
			select_window_index = i;
		}

		select_window_list[i] = cur;
		cur = cur->next[curws][TYPE_NORMAL];
	}

	select_window_handler = select_window_iterate;
	select_window_iterate();
}

void select_window_terminate(void) {
	state = DEFAULT;
	events[XCB_ENTER_NOTIFY] = enter_notify;
	events[XCB_LEAVE_NOTIFY] = leave_notify;
	traverse(curws, TYPE_NORMAL, reset_events);
	select_window_handler = select_window_init;
	free(select_window_list);
}

void select_window(void *arg) {
	window *stack_curws = stack[curws].lists[TYPE_NORMAL].first;
	if (!stack_curws || !stack_curws->next[curws][TYPE_NORMAL]) {
		return;
	}

	select_window_handler();
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

#define snap_macro(A, B) void A(void *arg) { \
	snap(B);          \
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

static void region_press(void *arg) {
	state = PRESS;
	grab_pointer();
}

void region_release_helper(void (*action)(void *), void *arg) {
	region_abort();	

	action(arg);
}

void region_close(void *arg) {
	region_release_helper(close, arg);
}

void region_snap_u(void *arg) {
	region_release_helper(snap_u, arg);
}

void window_button_close(void *arg) {
	region_press(NULL);
	events[XCB_BUTTON_RELEASE] = (void (*)(xcb_generic_event_t *))region_close;
}

void window_button_snap_u(void *arg) {
	region_press(NULL);
	events[XCB_BUTTON_RELEASE] = (void (*)(xcb_generic_event_t *))region_snap_u;
}

void region_abort() {
	state = DEFAULT;

	events[XCB_BUTTON_RELEASE] = NULL;

	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
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
