#ifndef MYWM_H
#define MYWM_H

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#define MOVE_MASK XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
#define RESIZE_MASK XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
#define MOVE_RESIZE_MASK MOVE_MASK | RESIZE_MASK

enum { DEFAULT, MOVE, RESIZE, CYCLE, };

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_STATE, WM_COUNT, };

typedef struct {
	uint16_t mod;
	xcb_keysym_t key;

	void (*function) (int arg);
	int arg;
} keybind;

typedef struct {
	uint16_t mod;
	uint32_t button;

	void (*press) (xcb_window_t win, uint32_t event_x, uint32_t event_y);
	void (*motion) (xcb_generic_event_t *ev);
	void (*release) (xcb_generic_event_t *ev);
} button;

extern xcb_connection_t *conn;
extern xcb_screen_t *scr;
extern xcb_ewmh_connection_t *ewmh;

extern unsigned int state;

extern xcb_atom_t wm_atoms[];
extern xcb_atom_t ewmh__NET_WM_STATE_FOCUSED;
	
extern void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);

xcb_query_pointer_reply_t *w_query_pointer(); //util
/* static */ void stop_cycle(); //util

#endif
