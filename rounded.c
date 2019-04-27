#include <math.h>

#include <xcb/shape.h>

#include "rounded.h"

xcb_rectangle_t rect_mask[2 * RAD];
int rect_count = -1;
int corner_height = 0;

void init_rounded_corners() {
	int last_res;
	int yoff = 0;
	int res = -1;
	for (float y = 0; y < 2 * RAD; y++) {
		last_res = res;
		float center_y = RAD - 1 - y + .5;
		res = round(sqrt(RAD * RAD - center_y * center_y));
	
		if (RAD - res == 0) {
			yoff = y + 1;
		}

		if (res == last_res) {
			rect_mask[rect_count].height++;
		} else {
			rect_count++;
			rect_mask[rect_count].height = 1;
			rect_mask[rect_count].x = RAD - res;
			rect_mask[rect_count].y = y - yoff;
		} 
	}

	rect_count++;

	corner_height = yoff - rect_mask[rect_count / 2].height;
}

void rounded_corners(window *win) {
	int w = win->geom[GEOM_W];
	int h = win->geom[GEOM_H];

	xcb_rectangle_t win_rects[2 * RAD + 1];

	int half_rect_count = rect_count / 2;

	int i = 0;
	for (; i < half_rect_count; i++) {
		win_rects[i].x = rect_mask[i].x;
		win_rects[i].y = rect_mask[i].y;
		win_rects[i].width = w - 2 * rect_mask[i].x;
		win_rects[i].height = rect_mask[i].height;
	}

	win_rects[i].x = 0;
	win_rects[i].y = corner_height;
	win_rects[i].width = w;
	win_rects[i].height = h - 2 * corner_height;
	
	i++;

	for (; i < rect_count; i++) {
		win_rects[i].x = rect_mask[i].x;
		win_rects[i].y = h - corner_height + rect_mask[i].y;
		win_rects[i].width = w - 2 * rect_mask[i].x;
		win_rects[i].height = rect_mask[i].height;
	}

	xcb_shape_rectangles(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
			XCB_CLIP_ORDERING_Y_SORTED, win->windows[WIN_PARENT], 0, 0, rect_count,
			win_rects);
}

