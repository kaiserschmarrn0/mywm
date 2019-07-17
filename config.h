#ifndef CONFIG_H
#define CONFIG_H

#include <X11/keysym.h>

#include "mywm.h"
#include "mouse.h"
#include "action.h"

#define NUM_WS 4

//manage pre-existing windows
#define PICKUP

#define TOP 0
#define BOT 0
#define GAP 0
#define BORDER 0

#define PAD_N 32
#define PAD 1

#define RESIZE_REGION_WIDTH 8
#define RESIZE_REGION_CORNER 32

#define FOCUSCOL 0xff3d3a3c
#define UNFOCUSCOL 0xff302e2f

#define CURSOR_FG 0xFFd7d7d7
#define CURSOR_BG 0xff242223

#define MARGIN 5

//disregard gaps when maximized
#define SNAP_MAX_SMART

//rounded corners
//#define ROUNDED
#define RAD 2

#define MOD XCB_MOD_MASK_4
#define SHIFT XCB_MOD_MASK_SHIFT

static const char *fonts[] = {
	"Font Awesome:size=12:autohint=true:antialias=true"
};

static const button grab_buttons[] = {
	{ MOD, XCB_BUTTON_INDEX_1, mouse_move,   },
	{ MOD, XCB_BUTTON_INDEX_3, mouse_resize_south_east, },
};

static const button parent_buttons[] = {
	{ 0, XCB_BUTTON_INDEX_1, mouse_move,   },
	{ 0, XCB_BUTTON_INDEX_3, mouse_resize_south_east, },
};

static const button close_buttons[] = {
	{ 0, XCB_BUTTON_INDEX_1, window_button_close },
};

static const button max_buttons[] = {
	{ 0, XCB_BUTTON_INDEX_1, window_button_snap_u },
};

#define WHITE 0xffd7d7d7
#define GREEN 0xff28cd41
#define RED 0xffff3b30

static const control controls[] = {
	{
		{ 0, 0, PAD_N, PAD_N }, XCB_GRAVITY_NORTH_WEST, "\uf00d", NULL, close_buttons, 1, 0,
		{
			WHITE, FOCUSCOL, //PM_FOCUS
			RED, FOCUSCOL, //PM_HOVER
			FOCUSCOL, RED, //PM_PRESS
			WHITE, UNFOCUSCOL, //PM_UNFOCUS
		},
	},
	{
		{ PAD_N, 0, PAD_N, PAD_N }, XCB_GRAVITY_NORTH_WEST, "\uf077", NULL, max_buttons, 1, 0,
		{		
			WHITE, FOCUSCOL, //PM_FOCUS
			GREEN, FOCUSCOL, //PM_HOVER
			FOCUSCOL, GREEN, //PM_PRESS
			WHITE, UNFOCUSCOL, //PM_UNFOCUS
		},
	},
};

extern xcb_pixmap_t pixmaps[LEN(controls)][PM_COUNT]; //ree

static const keybind keys[] = {
	{ MOD,         XK_q,     close,     NULL      },
	{ MOD,         XK_s,     stick,     NULL      },
	{ MOD,         XK_Tab,   select_window,     NULL      },
	{ MOD,         XK_Left,  snap_l,    NULL      },
	{ MOD,         XK_Right, snap_r,    NULL      },
	{ MOD,         XK_f,     snap_u,    NULL      },
	{ MOD | SHIFT, XK_f,     int_full,  NULL      },
	{ MOD | SHIFT, XK_q,     mywm_exit,  NULL      },

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
