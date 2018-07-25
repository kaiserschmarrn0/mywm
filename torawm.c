#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TITLE 64
#define GRAB  8

#define MOVE  0
#define UP    1
#define DOWN  2
#define LEFT  4
#define RIGHT 8

typedef struct Frame {
  xcb_window_t p, c;
  struct Frame *n;
} Frame;

static xcb_connection_t	*c;
static xcb_screen_t     *s;

static xcb_window_t             fwin;
static Frame                    *dt;
static int                      state = 0, x, y;
static xcb_get_geometry_reply_t *g;

static void tora_wrap(xcb_window_t ch) {
  uint32_t v[2] = { s->white_pixel, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE };
  Frame f;
  f.c = ch;
  f.p = xcb_generate_id(c);
  xcb_create_window(c,
			XCB_COPY_FROM_PARENT,
			f.p,
      s->root,
			0, 0,
			640, 480,
			0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT,
			s->root_visual,
			XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
			v);
  xcb_reparent_window(c, f.c, f.p, 0, TITLE);
  xcb_map_window(c, f.p);
  xcb_map_window(c, f.c);
}

static void
tora_button_press(xcb_generic_event_t *ev)
{
  xcb_button_press_event_t *e = (xcb_button_press_event_t *)ev;
  if (e->detail == XCB_BUTTON_INDEX_1) {
    fwin = e->event;
    state = MOVE;
    g = xcb_get_geometry_reply(c, xcb_get_geometry(c, e->event), NULL);
    if (e->event_y < 8) {
      state = UP;
    } else if (e->event_y > g->height - 8) state = state | DOWN;
    if (e->event_x < 8) {
      state = state | LEFT;
    } else if (e->event_x > g->width - 8) state = state | RIGHT;
    x = e->event_x;
    y = e->event_y;
    xcb_grab_pointer(c,
        0,
        s->root,
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_BUTTON_MOTION |
        XCB_EVENT_MASK_POINTER_MOTION_HINT,
        XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC,
        s->root,
        XCB_NONE,
        XCB_CURRENT_TIME);
  }
}

static void
tora_motion_notify(xcb_generic_event_t *ev)
{
  uint32_t v[2];
  xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)ev;
  xcb_query_pointer_reply_t *p = xcb_query_pointer_reply(c,
	    xcb_query_pointer(c, s->root),
	    0);
  if (state == MOVE) {
    v[0] = p->root_x - x;
    v[1] = p->root_y - y;
    xcb_configure_window(c,
        fwin,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
        v);
  } else {
    if (state & UP) {
      v[0] = p->root_y - y;
      v[1] = g->y + g->height - p->root_y;
      xcb_configure_window(c,
          fwin,
          XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_Y,
          v);
    } else if (state & DOWN) {
      v[0] = p->root_y - g->y;
      xcb_configure_window(c,
          fwin,
          XCB_CONFIG_WINDOW_HEIGHT,
          v);
    }
    if (state & LEFT) {
        v[0] = p->root_x - x;
        v[1] = g->x + g->width - p->root_x;
      xcb_configure_window(c,
            fwin,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_X,
            v);
    } else if (state & RIGHT) {
      v[0] = p->root_x - g->x;
      xcb_configure_window(c,
          fwin,
          XCB_CONFIG_WINDOW_WIDTH,
          v);
    }
  }
  free(p);
}

static void
tora_button_release(xcb_generic_event_t *ev)
{
  xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
  free(g);
  state = MOVE;
}

int
main(void) {
  c = xcb_connect(NULL, NULL);
  s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
  static void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);
  
  xcb_window_t ch = xcb_generate_id(c);
 
  uint32_t v[2] = { s->black_pixel };

  xcb_create_window(c,
				XCB_COPY_FROM_PARENT,
				ch,
        s->root,
				0, 0,
				1920, 1080,
				0,
				XCB_WINDOW_CLASS_INPUT_OUTPUT,
				s->root_visual,
				XCB_CW_BACK_PIXEL,
				v);

  tora_wrap(ch); 

  for (int i = 0; i < XCB_NO_OPERATION; i++) events[i] = NULL;
  events[XCB_BUTTON_PRESS]  = tora_button_press;
  events[XCB_MOTION_NOTIFY]	= tora_motion_notify;
  events[XCB_BUTTON_RELEASE]  = tora_button_release;

  xcb_generic_event_t *ev;
  for (;;) {
    xcb_flush(c);
    if (xcb_connection_has_error(c)) printf("tora: YEET SKRRT DAB\n");
    ev = xcb_wait_for_event(c);
    if (events[ev->response_type & ~0x80]) events[ev->response_type & ~0x80](ev);
    free(ev);
  }

  xcb_disconnect(c);

  return 0;
}
