#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
//#include <xcb/xcb_keysyms.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TITLE  32
#define BORDER 8
#define CORNER 24
#define LWIDTH 2

#define MOVE  0
#define UP    1
#define DOWN  2
#define LEFT  4
#define RIGHT 8

enum { PARENT, CHILD };
enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };

typedef struct Frame {
  xcb_window_t p, c;
  struct Frame *n;
} Frame;

static xcb_connection_t	        *c;
static xcb_ewmh_connection_t    *ewmh;
static xcb_screen_t             *s;
static xcb_get_geometry_reply_t *g;
static xcb_atom_t               wm_atoms[WM_COUNT];
static Frame                    *fwin = NULL;
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

static Frame * tora_wtf(xcb_window_t w, int mode) {
 Frame *cur = dt;
 while (cur) {
  if (cur->p == w) return cur;
 	cur = cur->n;
 }
 return NULL;
}

static Frame * tora_ctf(xcb_window_t w) {
 Frame *cur = dt;
 while (cur) {
  if (cur->c == w) return cur;
 	cur = cur->n;
 }
 return NULL;
}

static void tora_get_atoms(const char **names, xcb_atom_t *atoms, unsigned int count) {
 int i = 0;
 xcb_intern_atom_cookie_t cookies[count];
 xcb_intern_atom_reply_t *reply;
	for (; i < count; i++) cookies[i] = xcb_intern_atom(c, 0, strlen(names[i]), names[i]);
	for (i = 0; i < count; i++) {
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
 if (fwin) xcb_grab_button(c, 1, fwin->c, XCB_EVENT_MASK_BUTTON_PRESS, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_1, XCB_NONE);
 xcb_ungrab_button(c, XCB_BUTTON_INDEX_1, f->c, XCB_NONE);
 fwin = f;
 xcb_set_input_focus(c, XCB_INPUT_FOCUS_POINTER_ROOT, f->c, XCB_CURRENT_TIME);
 xcb_configure_window(c, f->p, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]){ XCB_STACK_MODE_ABOVE });
}

static void tora_close(Frame *f) {
 int tick = 0;
 xcb_window_t temp = fwin->c;
 xcb_icccm_get_wm_protocols_reply_t pro;
 if (xcb_icccm_get_wm_protocols_reply(c, xcb_icccm_get_wm_protocols_unchecked(c, temp, ewmh->WM_PROTOCOLS), &pro, NULL))
  for (int i = 0; i < pro.atoms_len; i++)
   if (wm_atoms[WM_DELETE_WINDOW] == pro.atoms[i]) {
    xcb_client_message_event_t ev = { XCB_CLIENT_MESSAGE, 32, 0, temp, wm_atoms[0] };
		  ev.data.data32[0] = wm_atoms[WM_DELETE_WINDOW];
		  ev.data.data32[1] = XCB_CURRENT_TIME;
		  xcb_send_event(c, 0, temp, XCB_EVENT_MASK_NO_EVENT, (char *)&ev);
    tick = 1;
   }
 if (!tick) xcb_kill_client(c, temp);
 xcb_icccm_get_wm_protocols_reply_wipe(&pro);
 xcb_unmap_window(c, fwin->p);
 tora_remove_frame(fwin->p);
}

static void tora_moveresize(uint32_t mask, uint32_t* values) {
 int tick = 0;
 if (mask & XCB_CONFIG_WINDOW_X) tick++;
 if (mask & XCB_CONFIG_WINDOW_Y) tick++;
 xcb_configure_window(c, fwin->p, mask, values);
 if (mask & XCB_CONFIG_WINDOW_WIDTH) {
  uint32_t vw[] = { *(values + tick) - 2 * BORDER };
  tick++;
  xcb_configure_window(c, fwin->c, XCB_CONFIG_WINDOW_WIDTH, vw);
 }
 if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
  uint32_t vh[] = { *(values + tick) - TITLE - BORDER };
  xcb_configure_window(c, fwin->c, XCB_CONFIG_WINDOW_HEIGHT, vh);
 }
}

static void tora_map_notify(xcb_generic_event_t *ev) {
 xcb_map_notify_event_t *e = (xcb_map_notify_event_t *)ev;
 if (e->override_redirect || !tora_check_managed(e->window) || tora_wtf(e->window, PARENT)) return;
 if (tora_wtf(e->event, PARENT)) return;
 if (tora_ctf(e->window)) {
  printf("you already exist\n");
  return;
 } 
 Frame *temp = dt;
 dt = malloc(sizeof(Frame));
 dt->p = xcb_generate_id(c);
 dt->c = e->window;
 xcb_create_window(c, XCB_COPY_FROM_PARENT, dt->p, s->root, 0, 0, 640, 480, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, (uint32_t[]){ s->white_pixel, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY });
 xcb_configure_window(c, dt->c, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){ 640 - 2 * BORDER, 480 - TITLE - BORDER});
 xcb_reparent_window(c, dt->c, dt->p, BORDER, TITLE);
 dt->n = temp;
 xcb_map_window(c, dt->p);
 xcb_map_window(c, dt->c);
 tora_focus(dt);
}

static void tora_button_press(xcb_generic_event_t *ev) {
 xcb_button_press_event_t *e = (xcb_button_press_event_t *)ev;
 if (e->detail != 1) return;
 Frame *found = tora_ctf(e->event);
 if (found) {
  tora_focus(found);
  return;
 }
 found = tora_wtf(e->event, PARENT);
 if (!found) return;
 tora_focus(found);
 if (e->event == found->c) return;
 if (e->event_x < BORDER + TITLE / 2 && e->event_x > BORDER && e->event_y < BORDER + TITLE / 2 && e->event_y > BORDER) tora_close(found);
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

static void tora_motion_notify(xcb_generic_event_t *ev) {
 xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)ev;
 if (e->event == fwin->c) return;
 xcb_query_pointer_reply_t *p = xcb_query_pointer_reply(c, xcb_query_pointer(c, s->root), 0);
 if (state == MOVE) tora_moveresize(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (uint32_t[]){ p->root_x - x, p->root_y - y });
 else
  if (state & UP) tora_moveresize(XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){ p->root_y - y, g->y + g->height - p->root_y + y });
  else if (state & DOWN) tora_moveresize(XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){ p->root_y - g->y });
  if (state & LEFT) tora_moveresize(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ p->root_x - x, g->x + g->width - p->root_x + x });
  else if (state & RIGHT) tora_moveresize(XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ p->root_x - g->x });
 free(p);
}

static void tora_button_release(xcb_generic_event_t *ev) {
 xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
 if (g) {
  free(g);
  g = NULL;
 }
 state = MOVE;}

static void tora_expose_notify(xcb_generic_event_t *ev) {
 xcb_expose_event_t *e = (xcb_expose_event_t *)ev;
 xcb_gcontext_t fc = xcb_generate_id(c);
 xcb_create_gc(c, fc, e->window, XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH | XCB_GC_GRAPHICS_EXPOSURES, (uint32_t[]){ s->black_pixel, LWIDTH, 0 }); 
 xcb_rectangle_t r[] = { { BORDER + LWIDTH / 2, BORDER + LWIDTH / 2, TITLE / 2 - LWIDTH / 2, TITLE / 2 - LWIDTH / 2 } };
 xcb_poly_rectangle(c, e->window, fc, 1, r);
}

static void tora_unmap_notify(xcb_generic_event_t *ev) {
 xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
 if (e->event == s->root) return;
 Frame *found = tora_ctf(e->window);
 if (!found) return;
 printf("pt 2 of 2\n");
 xcb_destroy_window(c, found->p);
 tora_remove_frame(e->window);
}

int
main(void)
{
 printf("Tora: >:)\n");

 c = xcb_connect(NULL, NULL);
 s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
 xcb_change_window_attributes_checked(c, s->root, XCB_CW_EVENT_MASK, (uint32_t[]){ XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY });
 
 ewmh = calloc(1, sizeof(xcb_ewmh_connection_t));
 xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(c, ewmh), (void *)0);

 const char *WM_ATOM_NAME[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW", };
 tora_get_atoms(WM_ATOM_NAME, wm_atoms, WM_COUNT);

 static void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);
 for (int i = 0; i < XCB_NO_OPERATION; i++) events[i] = NULL;
 events[XCB_BUTTON_PRESS]   = tora_button_press;
 events[XCB_MOTION_NOTIFY]	 = tora_motion_notify;
 events[XCB_BUTTON_RELEASE] = tora_button_release;
 events[XCB_MAP_NOTIFY]	    = tora_map_notify;
 events[XCB_EXPOSE]         = tora_expose_notify;
 events[XCB_UNMAP_NOTIFY]	  = tora_unmap_notify;

 /*events[XCB_CLIENT_MESSAGE]	    = arai_client_message;
 events[XCB_CONFIGURE_NOTIFY]    = arai_configure_notify;
 events[XCB_KEY_PRESS]	    = arai_key_press;
 events[XCB_MAP_NOTIFY]	    = arai_map_notify;
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
