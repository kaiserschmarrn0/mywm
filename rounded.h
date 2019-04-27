#ifndef ROUNDED_H
#define ROUNDED_H

#include <xcb/xcb.h>

#include "config.h"
#include "window.h"

void init_rounded_corners();
void rounded_corners(window *win);

#endif
