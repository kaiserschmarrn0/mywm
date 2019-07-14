#ifndef SNAP_H
#define SNAP_H

#include "window.h"

void create_snap_regions();

void snap(int index);

void snap_restore_state(window *win);

#endif
