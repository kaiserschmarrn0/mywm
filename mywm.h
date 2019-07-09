#ifndef MYWM_H
#define MYWM_H

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#define LEN(A) sizeof(A)/sizeof(*A)

enum { DEFAULT, MOVE, RESIZE, CYCLE, };


enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_STATE, WM_COUNT, };

typedef struct {
	uint16_t mod;
	xcb_keysym_t key;

	void (*function) (void *arg);
	void *arg;
} keybind;

typedef struct {
	xcb_window_t win;
	uint32_t event_x;
	uint32_t event_y;
} press_arg;

typedef struct {
	uint16_t mod;
	uint32_t button;

	void (*press) (void *arg);
	void (*motion) (void *arg);
	void (*release) (void *arg);
} button;

enum {
	PM_FOCUS_DEFAULT,
	PM_FOCUS_HOVER,
	PM_FOCUS_PRESS,
	PM_UNFOCUS_DEFAULT,
	PM_UNFOCUS_HOVER,
	PM_UNFOCUS_PRESS,
	PM_COUNT
};

#define PM_FG(A) 2 * A
#define PM_BG(A) 2 * A + 1

enum {
	COL_FOCUS_FG,
	COL_FOCUS_BG,
	COL_FOCUS_FG_HOVER,
	COL_FOCUS_BG_HOVER,
	COL_FOCUS_FG_PRESS,
	COL_FOCUS_BG_PRESS,
	COL_UNFOCUS_FG,
	COL_UNFOCUS_BG,
	COL_UNFOCUS_FG_HOVER,
	COL_UNFOCUS_BG_HOVER,
	COL_UNFOCUS_FG_PRESS,
	COL_UNFOCUS_BG_PRESS,
	COL_COUNT
};

typedef struct {
	xcb_rectangle_t geom;

	char *shape_fg;
	char *shape_bg;

	const button *buttons;
	int buttons_len;
	
	uint32_t colors[12];
} control;

extern xcb_connection_t *conn;
extern xcb_screen_t *scr;
extern xcb_ewmh_connection_t *ewmh;

extern unsigned int state;

extern uint32_t depth;
extern xcb_visualid_t vis;
extern xcb_colormap_t cm;

extern xcb_atom_t wm_atoms[];
extern xcb_atom_t ewmh__NET_WM_STATE_FOCUSED;
	
extern void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);

//shared utilities
xcb_query_pointer_reply_t *w_query_pointer();
void close_helper(xcb_window_t win);

#endif
