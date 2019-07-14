#include "margin.h"

#include <xcb/shape.h>

#include "config.h"
#include "workspace.h"
#include "snap.h"

margin margins[] = {
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

void activate_margins() {
	raise_margins();
	traverse_margins(activate_margin);
}

void deactivate_margins() {
	traverse_margins(deactivate_margin);
}

static void margin_enter_helper(int i) {
	snap(margins[i].arg);
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

