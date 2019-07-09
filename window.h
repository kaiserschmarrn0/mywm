#ifndef WINDOW_H
#define WINDOW_H

#include "config.h"

#define GEOM_X 0
#define GEOM_Y 1
#define GEOM_W 2
#define GEOM_H 3

#define MOVE_MASK XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
#define RESIZE_MASK XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
#define MOVE_RESIZE_MASK MOVE_MASK | RESIZE_MASK

enum { TYPE_ALL, TYPE_NORMAL, TYPE_ABOVE, TYPE_COUNT };
enum { WIN_CHILD, WIN_PARENT, WIN_COUNT };

typedef struct window {
	struct window *next[NUM_WS][TYPE_COUNT];
	struct window *prev[NUM_WS][TYPE_COUNT];

	int normal;
	int above;
	int sticky;

	xcb_window_t windows[WIN_COUNT];
	xcb_window_t controls[LEN(controls)];

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

void release_events(window *subj);
void normal_events(window *subj);

void center_pointer(window *win);

void stack_above(window *subj);
void mywm_raise(window *subj);
void safe_raise(window *subj);

void focus(window *subj);
void unfocus(window *win);

void show_state(window *win);
void show(window *win);
void hide(window *win);

void ewmh_state(window *win);

void stick_helper(window *win);

void save_state(window *win, uint32_t *state);

void full_save_state(window *win);
void full_restore_state(window *win);
void full(window *win);
void ext_full(window *win);

void free_client(window *subj, int ws);
void forget_client(window *subj, int ws);

void update_geometry(window *win, uint32_t mask, uint32_t *vals);

void make_win_normal(window *win);
window *new_win(xcb_window_t child);

#endif
