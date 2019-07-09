#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <stdio.h>

#include "window.h"
#include "config.h"

typedef struct workspace {
	window *lists[TYPE_COUNT];

	window *fwin;
} workspace;

typedef struct {
	window *win;
	int index;
} search_data;

extern workspace stack[NUM_WS];
extern int curws;

void print_stack(int ws, int type);
void print_all_stacks(int ws);

void traverse(int ws, int type, void (*func)(window *));
void safe_traverse(int ws, int type, void (*func)(window *));

void insert_into(int ws, window *win);
void excise_from(int ws, window *win);

void insert_into_all_but(int ws, window *win);
void excise_from_all_but(int ws, window *win);

search_data search_range(int ws, int type, int start_index, int end_index, xcb_window_t id);

window *search_ws(int ws, int type, int window_index, xcb_window_t id);
window *search_all(int *ws, int type, int window_index, xcb_window_t id);

void refocus(int ws);

#endif
