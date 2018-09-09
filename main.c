#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TITLE   32
#define BORDER  8
#define CORNER  24

#define MOVE  0
#define UP    1
#define DOWN  2
#define LEFT  4
#define RIGHT 8

typedef struct Frame {
  xcb_window_t p, c;
  struct Frame *n;
} Frame;

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };

static xcb_connection_t	        *c;
static xcb_ewmh_connection_t    *ewmh;
static xcb_screen_t             *s;
static xcb_get_geometry_reply_t *g;
static xcb_atom_t               wm_atoms[WM_COUNT];
static Frame                    *fwin;
static Frame                    *dt;
static int                      state = 0, x, y;


static void tora_remove_frame(xcb_window_t w) {
  Frame *cur = dt, *prev = NULL;
  while (cur && cur->p != w) {
    prev = cur;
    cur = cur->n;
  }
  if (!cur) return;
  else if (cur->n && prev) prev->n = cur->n;
  else if (prev) prev->n = NULL;
  else if (cur->n) dt = cur->n;
  else dt = NULL;
  free(cur);
}

static Frame *
tora_wtf(xcb_window_t w)
{
  Frame *cur = dt;
  while (cur) {
  	if (cur->p == w) return cur;
  	cur = cur->n;
  }
  return NULL;
}

/*static void
tora_print(void)
{
  Frame *cur = dt;
  while (cur) {
    printf("[%d, %d] ", cur->p, cur->c);
    cur = cur->n;
  }
  printf("\n");
}*/

static void tora_get_atoms(const char **names, xcb_atom_t *atoms, unsigned int count) {
  xcb_intern_atom_cookie_t cookies[count];
  xcb_intern_atom_reply_t *reply;
	for (int i = 0; i < count; i++) cookies[i] = xcb_intern_atom(c, 0, strlen(names[i]), names[i]);
	for (int i = 0; i < count; i++) {
	  reply = xcb_intern_atom_reply(c, cookies[i], NULL);
	  if (reply) atoms[i] = reply->atom;
    free(reply);
  }	
}

static int tora_check_managed(xcb_window_t win) {
  xcb_ewmh_get_atoms_reply_t type;
  if (!xcb_ewmh_get_wm_window_type_reply(ewmh, xcb_ewmh_get_wm_window_type(ewmh, win), &type, NULL)) return 1;
  for (unsigned int i = 0; i < type.atoms_len; i++)
	  if (type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_DOCK || type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR || type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP) {
	    xcb_ewmh_get_atoms_reply_wipe(&type);
	    return 0;
    }
  xcb_ewmh_get_atoms_reply_wipe(&type);
  return 1;
}

static int tora_focus(Frame *f) {
  fwin = f;
  xcb_set_input_focus(c, XCB_INPUT_FOCUS_POINTER_ROOT, f->c, XCB_CURRENT_TIME);
  const uint32_t v[] = { XCB_STACK_MODE_ABOVE };
  xcb_configure_window(c, f->p, XCB_CONFIG_WINDOW_STACK_MODE, v);
}

static void
tora_close(Frame *f)
{
  xcb_window_t temp = fwin->c;
  xcb_icccm_get_wm_protocols_reply_t pro;
  if (xcb_icccm_get_wm_protocols_reply(c,
      xcb_icccm_get_wm_protocols_unchecked(c, temp, ewmh->WM_PROTOCOLS),
      &pro,
      NULL)) {
    for (int i = 0; i < pro.atoms_len; i++) {
      if (wm_atoms[WM_DELETE_WINDOW] == pro.atoms[i]) {
        xcb_client_message_event_t ev;
        ev.response_type = XCB_CLIENT_MESSAGE;
		    ev.window = temp;
		    ev.format = 32;
		    ev.sequence = 0;
		    ev.type = wm_atoms[0];
		    ev.data.data32[0] = wm_atoms[WM_DELETE_WINDOW];
		    ev.data.data32[1] = XCB_CURRENT_TIME;
		    xcb_send_event(c,
			      0,
			      temp,
		        XCB_EVENT_MASK_NO_EVENT,
		        (char *)&ev);
		    xcb_icccm_get_wm_protocols_reply_wipe(&pro);
        xcb_unmap_window(c, fwin->p);
        tora_remove_frame(fwin->p);
        return;
      }
    }
  }
  xcb_icccm_get_wm_protocols_reply_wipe(&pro);
  xcb_kill_client(c, temp);

  xcb_unmap_window(c, fwin->p);
  tora_remove_frame(fwin->p);
}

static void tora_map_notify(xcb_generic_event_t *ev) {
  xcb_map_notify_event_t *e = (xcb_map_notify_event_t *)ev;
  if (e->override_redirect || !tora_check_managed(e->window) || tora_wtf(e->window)) return;
  Frame *temp = dt;
  dt = malloc(sizeof(Frame));
  dt->p = xcb_generate_id(c);
  dt->c = e->window;
  uint32_t v[2] = { s->white_pixel, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_EXPOSURE };
  xcb_create_window(c, XCB_COPY_FROM_PARENT, dt->p, s->root, 0, 0, 640, 480, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, v);
  xcb_reparent_window(c, dt->c, dt->p, BORDER, TITLE);
  v[0] = 640 - 2 * BORDER;
  v[1] = 480 - TITLE - BORDER;
  xcb_configure_window(c, dt->c, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, v);
  dt->n = temp;
  xcb_map_window(c, dt->p);
  xcb_map_window(c, dt->c);
  tora_focus(dt);
  
  /*xcb_flush(c);

  fc = xcb_generate_id(c);
  uint32_t m = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  v[0] = s->black_pixel;
  v[1] = 0;
  xcb_create_gc(c, fc, dt->p, m, v);

  xcb_flush(c);
  
  xcb_rectangle_t r[] = { { BORDER, BORDER, TITLE / 2, TITLE / 2 } };

  xcb_poly_rectangle(c, dt->p, fc, 1, r);
  
  xcb_flush(c);*/
}

static void
tora_button_press(xcb_generic_event_t *ev)
{
  xcb_button_press_event_t *e = (xcb_button_press_event_t *)ev;
  if (e->detail != 1) return;
  Frame *found = tora_wtf(e->event);
  if (!found) return;
  tora_focus(found);
  if (e->event_x < BORDER + TITLE / 2 && e->event_x > BORDER && e->event_y < BORDER + TITLE / 2 && e->event_y > BORDER)
    tora_close(found);
  state = MOVE;
  g = xcb_get_geometry_reply(c, xcb_get_geometry(c, e->event), NULL);
  if (e->event_y < BORDER) state = UP;
  else if (e->event_y > g->height - BORDER) state = state | DOWN;
  if (e->event_x < BORDER) state = state | LEFT;
  else if (e->event_x > g->width - BORDER) state = state | RIGHT;
  x = e->event_x;
  y = e->event_y;
  xcb_grab_pointer(c, 0, s->root, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, s->root, XCB_NONE, XCB_CURRENT_TIME);
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
        fwin->p,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
        v);
  } else {
    if (state & UP) {
      v[0] = p->root_y - y;
      v[1] = g->y + g->height - p->root_y + y;
      xcb_configure_window(c,
          fwin->p,
          XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT,
          v);
      v[0] = v[1] - TITLE - BORDER;
      xcb_configure_window(c,
          fwin->c,
          XCB_CONFIG_WINDOW_HEIGHT,
          v);
    } else if (state & DOWN) {
      v[0] = p->root_y - g->y;
      xcb_configure_window(c,
          fwin->p,
          XCB_CONFIG_WINDOW_HEIGHT,
          v);
      v[0] = v[0] - TITLE - BORDER;
      xcb_configure_window(c,
          fwin->c,
          XCB_CONFIG_WINDOW_HEIGHT,
          v);
    }
    if (state & LEFT) {
      v[0] = p->root_x - x;
      v[1] = g->x + g->width - p->root_x + x;
      xcb_configure_window(c,
          fwin->p,
          XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH,
          v);
      v[0] = v[1] - 2 * BORDER;
      xcb_configure_window(c,
          fwin->c,
          XCB_CONFIG_WINDOW_WIDTH,
          v);
    } else if (state & RIGHT) {
      v[0] = p->root_x - g->x;
      xcb_configure_window(c,
          fwin->p,
          XCB_CONFIG_WINDOW_WIDTH,
          v);
      v[0] = v[0] - 2 * BORDER;
      xcb_configure_window(c,
          fwin->c,
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
  if (g) {
    free(g);
    g = NULL;
  }
  state = MOVE;
}

static void
tora_expose_notify(xcb_generic_event_t *ev)
{
  xcb_expose_event_t *e = (xcb_expose_event_t *)ev;
  xcb_gcontext_t fc = xcb_generate_id(c);
  uint32_t v[] = { s->black_pixel, s->black_pixel, XCB_FILL_STYLE_SOLID, 0 };
  xcb_create_gc(c, fc, e->window, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FILL_STYLE | XCB_GC_GRAPHICS_EXPOSURES, v);

  
  xcb_rectangle_t r[] = { { BORDER, BORDER, TITLE / 2, TITLE / 2 },
                          { BORDER * 2 + TITLE / 2, BORDER, TITLE / 2, TITLE / 2 }
                        };

  xcb_poly_rectangle(c, e->window, fc, 2, r);
}

int
main(void)
{
  printf("Tora: >:)\n");

  uint32_t v[] = {
      XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
      XCB_EVENT_MASK_BUTTON_PRESS |
      XCB_EVENT_MASK_KEY_PRESS
  };
  c = xcb_connect(NULL, NULL);
  s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
  xcb_change_window_attributes_checked(c,
      s->root,
      XCB_CW_EVENT_MASK,
      v);
  
  ewmh = calloc(1, sizeof(xcb_ewmh_connection_t));
  xcb_ewmh_init_atoms_replies(ewmh,
      xcb_ewmh_init_atoms(c, ewmh),
	    (void *)0);
  
  const char *WM_ATOM_NAME[] = {
      "WM_PROTOCOLS",
      "WM_DELETE_WINDOW",
  };
  tora_get_atoms(WM_ATOM_NAME, wm_atoms, WM_COUNT);

  static void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);
  
  /*v[0] = s->black_pixel;
  xcb_window_t ch = xcb_generate_id(c);
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

  tora_wrap(ch); */

  for (int i = 0; i < XCB_NO_OPERATION; i++) events[i] = NULL;
  events[XCB_BUTTON_PRESS]    = tora_button_press;
  events[XCB_MOTION_NOTIFY]	  = tora_motion_notify;
  events[XCB_BUTTON_RELEASE]  = tora_button_release;
  events[XCB_MAP_NOTIFY]	    = tora_map_notify;
  events[XCB_EXPOSE]          = tora_expose_notify;

  /*events[XCB_CLIENT_MESSAGE]	    = arai_client_message;
  events[XCB_CONFIGURE_NOTIFY]    = arai_configure_notify;
  events[XCB_KEY_PRESS]	    = arai_key_press;
  events[XCB_MAP_NOTIFY]	    = arai_map_notify;
  events[XCB_UNMAP_NOTIFY]	    = arai_unmap_notify;
  events[XCB_ENTER_NOTIFY]	    = arai_enter_notify;*/

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
