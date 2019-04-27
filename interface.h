#ifndef INTERFACE_H
#define INTERFACE_H

#include "window.h"

/*
 * this file serves as an interface for actions to use
 * utilities originally intended only for use in mywm.c
 */

/* static */ void save_state(window *win, uint32_t *state);

/* static */ void full_save_state(window *win);
/* static */ void full_restore_state(window *win);
/* static */ void full(window *win);

/* static */ void kill(xcb_window_t win);

/* static */ void center_pointer(window *win);

void forget_client(window *subj, int ws);

#endif
