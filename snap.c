#include "snap.h"
#include "workspace.h"

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
	snap_regions[3][2] = scr->width_in_pixels;
	snap_regions[3][3] = scr->height_in_pixels - TOP - BOT;
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

void snap_restore_state(window *win) {
	win->snap_index = SNAP_NONE;
	
	update_geometry(win, MOVE_RESIZE_MASK, win->before_snap);
}

void snap(int index) {
	if (!stack[curws].fwin || stack[curws].fwin->is_e_full || stack[curws].fwin->is_i_full) {
		return;
	}

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
