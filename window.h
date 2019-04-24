#ifndef WINDOW_H
#define WINDOW_H

#include "config.h"

enum { TYPE_ALL, TYPE_NORMAL, TYPE_ABOVE, TYPE_COUNT };
enum { WIN_CHILD, WIN_PARENT, WIN_COUNT };

typedef struct window {
	struct window *next[NUM_WS][TYPE_COUNT];
	struct window *prev[NUM_WS][TYPE_COUNT];

	int normal;
	int above;
	int sticky;

	xcb_window_t windows[WIN_COUNT];

	uint32_t geom[4];

	uint32_t before_roll;
	int is_roll;

	uint32_t before_snap[4];
	int is_snap;
	
	uint32_t before_full[4];	
	int is_i_full;
	int is_e_full;

	int ignore_unmap;
} window;

void stack_above(window *subj);
void raise(window *subj);

#endif
