#ifndef ROUNDED_H
#define ROUNDED_H

#include <xcb/xcb.h>

#include "config.h"
#include "window.h"

extern xcb_rectangle_t rect_mask[];
extern int rect_count;
extern int corner_height;

void init_rounded_corners();
void rounded_corners(window *win);

#endif
