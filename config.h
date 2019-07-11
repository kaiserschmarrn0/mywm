#ifndef CONFIG_H
#define CONFIG_H

#include <X11/keysym.h>

#include "mywm.h"
#include "action.h"

#define NUM_WS 4

#define TOP 0
#define BOT 28
#define GAP 0
#define BORDER 0

#define TITLE 32

#define FOCUSCOL 0xff4d484a
#define UNFOCUSCOL 0xff302e2f

#define SNAP_MARGIN 5
#define SNAP_CORNER 256

#define SNAP_MAX_SMART

/* rounded corners */

//uncomment for rounded corners
//#define ROUNDED

#define RAD 6

/* keyboard modifiers */

#define MOD XCB_MOD_MASK_4
#define SHIFT XCB_MOD_MASK_SHIFT

static const char *fonts[] = {
	"Font Awesome:size=13:autohint=true:antialias=true"
};

/* mouse controls */

static const button grab_buttons[] = {
	{ MOD, XCB_BUTTON_INDEX_1, mouse_move,   mouse_move_motion_start,   button_release },
	{ MOD, XCB_BUTTON_INDEX_3, mouse_resize, mouse_resize_motion, button_release },
};

static const button parent_buttons[] = {
	{ 0, XCB_BUTTON_INDEX_1, mouse_move,   mouse_move_motion_start,   button_release },
	{ 0, XCB_BUTTON_INDEX_3, mouse_resize, mouse_resize_motion, button_release },
};

static const button close_buttons[] = {
	{ 0, XCB_BUTTON_INDEX_1, NULL, NULL, close },
};

static const button max_buttons[] = {
	{ 0, XCB_BUTTON_INDEX_1, NULL, NULL, snap_u },
};

static const control controls[] = {
	{
		{ 0, 0, TITLE, TITLE }, XCB_GRAVITY_NORTH_WEST, "\uf00d", NULL, close_buttons, 1, 0,
		{
			0xffd7d7d7, 0xff4d484a, //PM_FOCUS
			0xffffff00, 0xff4d484a, //PM_HOVER
			0xffd7d7d7, 0xff4d484a, //PM_PRESS
			0xffd7d7d7, 0xff302e2f, //PM_UNFOCUS
		},
	},
	{
		{ TITLE, 0, TITLE, TITLE }, XCB_GRAVITY_NORTH_WEST, "\uf077", NULL, max_buttons, 1, 0,
		{		
			0xffd7d7d7, 0xff4d484a, //PM_FOCUS
			0xffd7d7d7, 0xff4d484a, //PM_HOVER
			0xffd7d7d7, 0xff4d484a, //PM_PRESS
			0xffd7d7d7, 0xff302e2f, //PM_UNFOCUS
		},
	},
};

extern xcb_pixmap_t pixmaps[LEN(controls)][PM_COUNT]; //ree

/* keyboard controls */

static const keybind keys[] = {
	{ MOD,         XK_q,     close,     NULL      },
	{ MOD,         XK_s,     stick,     NULL      },
	{ MOD,         XK_Tab,   cycle,     NULL      },
	{ MOD,         XK_Left,  snap_l,    NULL      },
	{ MOD,         XK_Right, snap_r,    NULL      },
	{ MOD,         XK_f,     snap_u,    NULL      },
	{ MOD | SHIFT, XK_f,     int_full,  NULL      },

	{ MOD,         XK_1,     change_ws, &(int){0} },
	{ MOD,         XK_2,     change_ws, &(int){1} },
	{ MOD,         XK_3,     change_ws, &(int){2} },
	{ MOD,         XK_4,     change_ws, &(int){3} },

	{ MOD | SHIFT, XK_1,     send_ws,   &(int){0} },
	{ MOD | SHIFT, XK_2,     send_ws,   &(int){1} },
	{ MOD | SHIFT, XK_3,     send_ws,   &(int){2} },
	{ MOD | SHIFT, XK_4,     send_ws,   &(int){3} },
};

#endif
