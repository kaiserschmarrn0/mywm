#ifndef MARGIN_H
#define MARGIN_H

#include <xcb/xcb.h>

typedef struct {
	xcb_rectangle_t rects[2];
	int rect_count;
	
	xcb_window_t win;

	int arg;
} margin;

void create_margins();

void activate_margins();
void deactivate_margins();

void margin_leave_handler(xcb_generic_event_t *ev);
void margin_enter_handler(xcb_generic_event_t *ev);

extern margin margins[7];

#endif
