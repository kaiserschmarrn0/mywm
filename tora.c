#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>

#define TOP    0
#define BOT    33
#define GAP    8
#define TITLE  32
#define BORDER 8
#define CORNER 24
#define LWIDTH 2
#define EDGE   8
#define BLEED  64
#define MOD    XCB_MOD_MASK_4
#define SHIFT  XCB_MOD_MASK_SHIFT

#define MOVE  0
#define UP    1
#define DOWN  2
#define LEFT  4
#define RIGHT 8

#define DEFAULT             0
#define INTERNAL_FULLSCREEN 1
#define EXTERNAL_FULLSCREEN 2

#define MIN(A, B) (((A) < (B)) ? (A) : (B))

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };
enum { NET_SUPPORTED, NET_FULLSCREEN, NET_WM_STATE, NET_COUNT };

typedef struct {
 uint16_t mod;
 xcb_keysym_t key;
 void (*function) (int arg);
 int arg;
} key;

typedef struct Frame {
 xcb_window_t p, c;
 struct Frame *n, *b;
 uint32_t x, y, w, h, rw, tx;
 xcb_rectangle_t geometry;
 xcb_size_hints_t hints;
 int max, snap;
 bool resize;
 XftDraw *draw;
 xcb_gcontext_t fgc, bgc;
 xcb_pixmap_t titlemap;
} Frame;

static void tora_close(int arg);
static void tora_cycle(int arg);
static void tora_snap_max(int arg);
static void tora_fullscreen(int arg);
static void tora_snap_left(int arg);
static void tora_snap_right(int arg);

static const key keys[] = {
// Modkey       Key       Function         Arg
 { MOD,         XK_q,     tora_close,      0                   },
 { MOD,         XK_Tab,   tora_cycle,      0                   },
 { MOD,         XK_f,     tora_snap_max,   0                   },
 { MOD | SHIFT, XK_f,     tora_fullscreen, INTERNAL_FULLSCREEN },
 { MOD,         XK_Left,  tora_snap_left,  0                   },
 { MOD,         XK_Right, tora_snap_right, 0                   },
};

static xcb_connection_t         *c;
static xcb_ewmh_connection_t    *ewmh;
static xcb_screen_t             *s;
static xcb_get_geometry_reply_t *g;
static xcb_colormap_t           colormap;
static xcb_atom_t               wm_atoms[WM_COUNT], net_atoms[NET_COUNT];
static Frame                    *dt = NULL;
static int                      state = 0, x, y;
static XftFont                  *title_font;
static XftColor                 title_color;
static Display                  *dpy;
static Visual                   *visual;

#define FIND(A, B) static Frame *A(xcb_window_t w) { \
 Frame *cur = dt; \
 while (cur) { \
  if (cur->B == w) return cur; \
  cur = cur->n; \
 } \
 return NULL; \
}

FIND(tora_wtf, p)
FIND(tora_ctf, c)

static void tora_insert(Frame *subject) {
 subject->n = dt;
 subject->b = NULL;
 if (dt) dt->b = subject;
 dt = subject;
}

static Frame *tora_excise(Frame *subject) {
 if (subject->n) subject->n->b = subject->b;
 if (subject->b) subject->b->n = subject->n;
 else dt = subject->n;
 return subject;
}

static void tora_get_atoms(const char **names, xcb_atom_t *atoms, unsigned int count) {
 int i = 0;
 xcb_intern_atom_cookie_t cookies[count];
 xcb_intern_atom_reply_t *reply;
 for (; i < count; i++) cookies[i] = xcb_intern_atom(c, 0, strlen(names[i]), names[i]);
 for (i = 0; i < count; i++) {
  reply = xcb_intern_atom_reply(c, cookies[i], NULL);
  if (reply) {
   atoms[i] = reply->atom;
   free(reply);
  }
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

static void tora_focus(Frame *f) {
 tora_insert(tora_excise(f));
 if (dt->n) xcb_grab_button(c, 1, dt->n->p, XCB_EVENT_MASK_BUTTON_PRESS, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_1, XCB_NONE); 
 xcb_ungrab_button(c, XCB_BUTTON_INDEX_1, dt->p, XCB_NONE);
 xcb_set_input_focus(c, XCB_INPUT_FOCUS_POINTER_ROOT, dt->c, XCB_CURRENT_TIME);
 xcb_configure_window(c, dt->p, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]){ XCB_STACK_MODE_ABOVE });
}

static void tora_close(int arg) {
 if (!dt) return;
 int tick = 0;
 xcb_icccm_get_wm_protocols_reply_t pro;
 if (xcb_icccm_get_wm_protocols_reply(c, xcb_icccm_get_wm_protocols_unchecked(c, dt->c, ewmh->WM_PROTOCOLS), &pro, NULL))
  for (int i = 0; i < pro.atoms_len; i++)
   if (wm_atoms[WM_DELETE_WINDOW] == pro.atoms[i]) {
    xcb_client_message_event_t ev = { XCB_CLIENT_MESSAGE, 32, 0, dt->c, wm_atoms[0] };
    ev.data.data32[0] = wm_atoms[WM_DELETE_WINDOW];
    ev.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(c, 0, dt->c, XCB_EVENT_MASK_NO_EVENT, (char *)&ev);
    tick = 1;
   }
 if (!tick) xcb_kill_client(c, dt->c);
 xcb_icccm_get_wm_protocols_reply_wipe(&pro);
 xcb_unmap_window(c, dt->p);
}

static void tora_update_title(Frame *subject) {
 xcb_ewmh_get_utf8_strings_reply_t ewmh_txt_prop;
 char title[255];
 int len = 0;
 if (!xcb_ewmh_get_wm_name_reply(ewmh, xcb_ewmh_get_wm_name(ewmh, subject->c), &ewmh_txt_prop, NULL) || !ewmh_txt_prop.strings || !ewmh_txt_prop.strings_len) return;
 len = MIN(ewmh_txt_prop.strings_len, 255);
 strncpy(title, ewmh_txt_prop.strings, len);
 xcb_free_pixmap(c, subject->titlemap);
 subject->titlemap = xcb_generate_id(c);
 XGlyphInfo ret;
 XftTextExtentsUtf8(dpy, title_font, (XftChar8 *)title, len, &ret);
 xcb_create_pixmap(c, s->root_depth, subject->titlemap, subject->p, ret.width, title_font->height);
 xcb_poly_fill_rectangle(c, subject->titlemap, subject->bgc, 1, (xcb_rectangle_t[]){ { 0, 0, ret.width, title_font->height } });
 if (subject->draw) XftDrawDestroy(subject->draw);
 subject->draw = XftDrawCreate(dpy, subject->titlemap, DefaultVisual(dpy, 0), s->default_colormap);
 XftDrawStringUtf8(subject->draw, &title_color, title_font, ret.x, title_font->ascent, (XftChar8 *)title, len);
 subject->tx = ret.width;
}

static void tora_draw_title(Frame *subject) {
 xcb_copy_area(c, subject->titlemap, subject->p, subject->fgc, 0, 0, (subject->rw - subject->tx) / 2, 0, subject->tx, title_font->height);
 xcb_poly_fill_rectangle(c, subject->p, subject->fgc, 2, (xcb_rectangle_t[]){ { BORDER + LWIDTH / 2 - 1, BORDER + LWIDTH / 2, TITLE / 2 - LWIDTH / 2, TITLE / 2 - LWIDTH / 2 }, { 2 * BORDER + LWIDTH + TITLE / 2 - 1, BORDER + LWIDTH / 2, TITLE / 2 - LWIDTH / 2, TITLE / 2 - LWIDTH / 2 } });
}

static void tora_moveresize(Frame *subject, uint32_t mask, uint32_t* values) {
 int xflag = 0, yflag = 0, wflag = 0, hflag = 0;
 if (mask & XCB_CONFIG_WINDOW_X) xflag = 1;
 if (mask & XCB_CONFIG_WINDOW_Y) yflag = 1;
 if (mask & XCB_CONFIG_WINDOW_WIDTH) wflag = 1;
 if (mask & XCB_CONFIG_WINDOW_HEIGHT) hflag = 1;
 int tick = xflag + yflag;
 xcb_size_hints_t *hints = &subject->hints;
 if ((hints->flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) && (hints->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE)) {
  xcb_size_hints_t *hints = &subject->hints;
  if (wflag) {
   uint32_t *v = values + tick;
   *v -= (*v - 2 * BORDER - hints->base_width) % hints->width_inc;
  }
  if (hflag) {
   uint32_t *v = values + tick + xflag;
   *v -= (*v - BORDER - TITLE - hints->base_height) % hints->height_inc;
  }
 }
 xcb_configure_window(c, subject->p, mask, values);
 if (wflag) {
  uint32_t vw[] = { *(values + tick) - 2 * BORDER };
  dt->rw = *(values + tick);
  xcb_configure_window(c, subject->c, XCB_CONFIG_WINDOW_WIDTH, vw);
 }
 if (hflag) {
  uint32_t vh[] = { *(values + tick + wflag) - TITLE - BORDER };
  xcb_configure_window(c, subject->c, XCB_CONFIG_WINDOW_HEIGHT, vh);
 }
}

static void tora_cycle(int arg) {
 if (dt == NULL || dt->n == NULL) return;
 Frame *cur = dt;
 while (cur->n) cur = cur->n;
 tora_focus(cur);
}

static void tora_save_state() {
 xcb_get_geometry_reply_t *temp = xcb_get_geometry_reply(c, xcb_get_geometry(c, dt->p), NULL);
 dt->x = temp->x;
 dt->y = temp->y;
 dt->w = temp->width;
 dt->h = temp->height;
 free(temp);
 dt->snap = 1;}

static void tora_restore_state() {
 dt->snap = 0;
 tora_moveresize(dt, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ dt->x, dt->y, dt->w, dt->h });
}

static void tora_fullscreen(int arg) {
 if (!dt || dt->max == EXTERNAL_FULLSCREEN && arg == INTERNAL_FULLSCREEN) return;
 if (dt->max == INTERNAL_FULLSCREEN && arg == EXTERNAL_FULLSCREEN) {
  dt->max = INTERNAL_FULLSCREEN;
  return;
 } else if (dt->max) {
  dt->max = DEFAULT;
  tora_restore_state();
 } else {
  dt->max = arg;
  tora_save_state();
  tora_moveresize(dt, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ - BORDER, - TITLE, s->width_in_pixels + 2 * BORDER, s->height_in_pixels + TITLE + BORDER });
 }
}

#define SNAP(A, B, C, D, E) static void A(int arg) { \
 if (!dt || dt->max) return; \
 if (!dt->snap) tora_save_state(); \
 tora_moveresize(dt, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ B, C, D, E });\
}

SNAP(tora_snap_max, GAP, GAP + TOP, s->width_in_pixels - GAP * 2, (s->height_in_pixels - TOP - BOT) - GAP * 2)
SNAP(tora_snap_left, GAP, GAP + TOP, s->width_in_pixels / 2 - BORDER * 1.5, s->height_in_pixels - GAP * 2 - TOP - BOT)
SNAP(tora_snap_uleft, GAP, GAP + TOP, s->width_in_pixels / 2 - BORDER * 1.5, (s->height_in_pixels - TOP - BOT) / 2 - GAP * 1.5)
SNAP(tora_snap_dleft, GAP, (s->height_in_pixels - TOP - BOT) / 2 + BORDER / 2 + TOP, s->width_in_pixels / 2 - BORDER * 1.5, (s->height_in_pixels - TOP - BOT) / 2 - GAP * 1.5)
SNAP(tora_snap_right, s->width_in_pixels / 2 + GAP / 2, GAP + TOP, s->width_in_pixels / 2 - BORDER * 1.5, s->height_in_pixels - GAP * 2 - TOP - BOT)
SNAP(tora_snap_uright, s->width_in_pixels / 2 + GAP / 2, GAP + TOP, s->width_in_pixels / 2 - BORDER * 1.5, (s->height_in_pixels - TOP - BOT) / 2 - GAP * 1.5)
SNAP(tora_snap_dright, s->width_in_pixels / 2 + GAP / 2, (s->height_in_pixels - TOP - BOT) / 2 + BORDER / 2 + TOP, s->width_in_pixels / 2 - BORDER * 1.5, (s->height_in_pixels - TOP - BOT) / 2 - GAP * 1.5)

static void tora_update_frame_hints(Frame *subject) {
 xcb_size_hints_t *hints = &subject->hints;
 if (xcb_icccm_get_wm_normal_hints_reply(c, xcb_icccm_get_wm_normal_hints_unchecked(c, subject->c), hints, NULL)) {
  if (!(hints->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) && (hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
   hints->base_width = hints->min_width;
   hints->base_height = hints->min_height;
   hints->flags |= XCB_ICCCM_SIZE_HINT_BASE_SIZE;
  } else if ((hints->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) && !(hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
   hints->min_width = hints->base_width;
   hints->min_height = hints->base_height;
   hints->flags |= XCB_ICCCM_SIZE_HINT_BASE_SIZE;
  }
  if (!(hints->flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)) hints->width_inc = hints->height_inc = 1;
  if ((hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) && (hints->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) && (hints->max_height == hints->min_height) && (hints->max_width == hints->min_width)) subject->resize = 0;
 }
}

static void tora_map_request(xcb_generic_event_t *ev) {
 xcb_map_notify_event_t *e = (xcb_map_notify_event_t *)ev;
 if (e->override_redirect || tora_wtf(e->window) || tora_wtf(e->event)) return;

 int tick = 0;
 bool resize = 1;
 xcb_ewmh_get_atoms_reply_t type;
 if (xcb_ewmh_get_wm_window_type_reply(ewmh, xcb_ewmh_get_wm_window_type(ewmh, e->window), &type, NULL)) {
  for (unsigned int i = 0; i < type.atoms_len; i++)
   if (type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_DOCK || type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR || type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP) {
    xcb_map_window(c, e->window); 
    tick = 1;
   } else if (type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_SPLASH || type.atoms[i] == ewmh->_NET_WM_WINDOW_TYPE_DIALOG) resize = 0;
  xcb_ewmh_get_atoms_reply_wipe(&type);
 }
 if (tick) return;

 xcb_get_geometry_reply_t *init_geom = xcb_get_geometry_reply(c, xcb_get_geometry_unchecked(c, e->window), NULL);
 if (!init_geom) return;
 
 printf("map notify: new win\n");
 xcb_rectangle_t temp_geom = { (s->width_in_pixels - init_geom->width) / 2 - BORDER, (s->height_in_pixels - init_geom->height - TITLE) / 2 , init_geom->width + BORDER * 2, init_geom->height + BORDER + TITLE };
 free(init_geom);

 xcb_change_save_set(c, XCB_SET_MODE_INSERT, e->window);

 Frame *new = malloc(sizeof(Frame));
 new->p = xcb_generate_id(c);
 new->c = e->window;
 new->max = 0;
 new->snap = 0;
 new->rw = temp_geom.width;
 new->draw = NULL;
 new->geometry = temp_geom;
 new->resize = resize;

 tora_update_frame_hints(new);

 xcb_create_window(c, s->root_depth, new->p, s->root, new->geometry.x, new->geometry.y, new->geometry.width, new->geometry.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, (uint32_t[]){ s->white_pixel, 1,  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT });
 xcb_configure_window(c, new->c, XCB_CONFIG_WINDOW_BORDER_WIDTH, (uint32_t[]){ 0 }); 
 
 xcb_reparent_window(c, new->c, new->p, BORDER, TITLE);
 tora_insert(new);
 xcb_map_window(c, new->p);
 xcb_map_window(c, new->c);
 tora_focus(new);
    
 xcb_change_window_attributes(c, new->c, XCB_CW_EVENT_MASK, (uint32_t[]){ XCB_EVENT_MASK_PROPERTY_CHANGE });

 new->fgc = xcb_generate_id(c);
 new->bgc = xcb_generate_id(c);
 xcb_create_gc(c, new->fgc, new->p, XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH | XCB_GC_GRAPHICS_EXPOSURES, (uint32_t[]){ 0x051519, LWIDTH, 0 });
 xcb_create_gc(c, new->bgc, new->p, XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES, (uint32_t[]){ s->white_pixel, 0 });
 
 tora_update_title(new);
}

static void tora_button_press(xcb_generic_event_t *ev) {
 xcb_button_press_event_t *e = (xcb_button_press_event_t *)ev;
 if (e->detail != 1) return;
 Frame *found = tora_wtf(e->event);
 if (!found) return;

 printf("button press\n");
 if (found != dt) tora_focus(found);
 if (e->child == found->c || found->max) return;

 if (e->event_x < BORDER + TITLE / 2 && e->event_x > BORDER && e->event_y < BORDER + TITLE / 2 && e->event_y > BORDER) {
  tora_close(0);
  return;
 }
 if (e->event_x < BORDER * 2 + TITLE && e->event_x > 2 * BORDER + TITLE / 2 && e->event_y < BORDER + TITLE / 2 && e->event_y > BORDER) {
  tora_snap_max(0);
  return;
 }

 state = MOVE;
 g = xcb_get_geometry_reply(c, xcb_get_geometry(c, e->event), NULL);
 if (e->event_y < BORDER) state = UP;
 else if (e->event_y > g->height - BORDER) state = state | DOWN;
 if (e->event_x < BORDER) state = state | LEFT;
 else if (e->event_x > g->width - BORDER) state = state | RIGHT;
 
 if (state && !found->resize) return;
 if (state) dt->snap = 0;
 if (state == MOVE && dt->snap) x = dt->w * e->event_x / g->width;
 else x = e->event_x;
 y = e->event_y;
 xcb_grab_pointer(c, 0, s->root, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, s->root, XCB_NONE, XCB_CURRENT_TIME);
}

static void tora_motion_notify(xcb_generic_event_t *ev) {
 printf("motion notify\n");
 xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)ev;
 xcb_query_pointer_reply_t *p = xcb_query_pointer_reply(c, xcb_query_pointer(c, s->root), 0);
 if (!p) return;
 
 if (state == MOVE) {
  if (dt->snap) tora_restore_state();
  
  int mx = s->width_in_pixels - BLEED, my = s->height_in_pixels - BLEED, tick = 0;
  if (p->root_x < EDGE) {
   if (p->root_y < BLEED) tora_snap_uleft(0);
   else if (p->root_y > my) tora_snap_dleft(0);
   else tora_snap_left(0);
   tick++;
  } else if (p->root_x > s->width_in_pixels - EDGE) {
   if (p->root_y < BLEED) tora_snap_uright(0);
   else if (p->root_y > my) tora_snap_dright(0);
   else tora_snap_right(0);
   tick++;
  } else if (p->root_y < EDGE) {
   if (p->root_x < BLEED) tora_snap_uleft(0);
   else if (p->root_x > mx) tora_snap_uright(0);
   else tora_snap_max(0);
   tick++;
  } else if (p->root_y > s->height_in_pixels - EDGE) {
   if (p->root_x < BLEED) { tora_snap_dleft(0); tick++; }
   else if (p->root_x > mx) { tora_snap_dright(0); tick++; };
  }
  
  if (tick) xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
  else tora_moveresize(dt, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (uint32_t[]){ p->root_x - x, p->root_y - y });
 } else {
  if (state & UP) tora_moveresize(dt, XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){ p->root_y - y, g->y + g->height - p->root_y + y });
  else if (state & DOWN) tora_moveresize(dt, XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){ p->root_y - g->y + g->height - y });
  if (state & LEFT) tora_moveresize(dt, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ p->root_x - x, g->x + g->width - p->root_x + x });
  else if (state & RIGHT) tora_moveresize(dt, XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ p->root_x - g->x + g->width - x });
 }
 free(p);
}

static void tora_button_release(xcb_generic_event_t *ev) {
 printf("button release\n");
 xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
 if (g) {
  free(g);
  g = NULL;
 }
 state = MOVE;
}

static void tora_expose_notify(xcb_generic_event_t *ev) {
 xcb_expose_event_t *e = (xcb_expose_event_t *)ev;
 Frame *found = tora_wtf(e->window);
 if (!found) return;
 printf("expose notify\n");
 tora_draw_title(found);
}

static void tora_forget_frame(Frame *subject) {
 xcb_generic_error_t *error = NULL;
 error = xcb_request_check(c, xcb_reparent_window_checked(c, subject->c, s->root, 0, 0));
 xcb_destroy_window(c, subject->p);
 if (!error || error->error_code != XCB_WINDOW) xcb_change_save_set(c, XCB_SET_MODE_DELETE, subject->c);
 if (error) free(error);
 free(tora_excise(subject));
 tora_focus(subject);
 xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
}

static void tora_unmap_notify(xcb_generic_event_t *ev) {
 xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
 Frame *subject = tora_wtf(e->event);
 if (subject) tora_forget_frame(subject);
}

static void tora_destroy_notify(xcb_generic_event_t *ev) {
 xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
 Frame *subject = tora_wtf(e->event);
 if (subject) tora_forget_frame(subject);
}

static void tora_key_press(xcb_generic_event_t *ev) {
 printf("key press\n");
 xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
 xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(c);
 xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, e->detail, 0);
 xcb_key_symbols_free(keysyms);
 for (int i = 0; i < sizeof(keys)/sizeof(*keys); i++)
  if (keysym == keys[i].key && keys[i].mod == e->state) {
   keys[i].function(keys[i].arg);
   break;
  }
}

static void tora_client_message(xcb_generic_event_t *ev) {
 printf("client message\n");
 xcb_client_message_event_t *e = (xcb_client_message_event_t *)ev;
 if (e->type == net_atoms[NET_WM_STATE] && ((unsigned)e->data.data32[2] == net_atoms[NET_FULLSCREEN] || (unsigned)e->data.data32[1] == net_atoms[NET_FULLSCREEN])) tora_fullscreen(EXTERNAL_FULLSCREEN);
}

static void tora_property_notify(xcb_generic_event_t *ev) {
 printf("property notify\n");
 xcb_property_notify_event_t *e = (xcb_property_notify_event_t *)ev;
 Frame *found = tora_ctf(e->window);
 if (!found) return;
 if ((e->atom == ewmh->_NET_WM_NAME || e->atom == XCB_ATOM_WM_NAME) && found) {
  tora_update_title(found);
  xcb_clear_area(c, 0, found->p, 0, 0, found->rw, TITLE);
  tora_draw_title(found);
 }
}

static void tora_configure_request(xcb_generic_event_t *ev) {
 xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
 Frame *subject = tora_wtf(e->parent);
 if (!subject) return;
 printf("configure request\n");
 if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) tora_moveresize(subject, XCB_CONFIG_WINDOW_WIDTH, (uint32_t[]){ e->width });
 if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) tora_moveresize(subject, XCB_CONFIG_WINDOW_HEIGHT, (uint32_t[]){ e->height });
}

static void tora_cleanup(void) {
 Frame *cur = dt, *temp;
 while (cur) {
  XftDrawDestroy(cur->draw);
  xcb_free_pixmap(c, cur->titlemap);
  xcb_free_gc(c, cur->fgc);
  xcb_free_gc(c, cur->bgc);

  xcb_ungrab_button(c, XCB_BUTTON_INDEX_1, cur->p, XCB_NONE);
  temp = cur;
  cur = cur->n;
  free(temp);
 }

 XftFontClose(dpy, title_font);
 XftColorFree(dpy, visual, colormap, &title_color);

 if (g) free(g);
 xcb_ewmh_connection_wipe(ewmh);
 xcb_flush(c);
 free(ewmh);
 xcb_disconnect(c);
}

int main(void) {
 printf("Tora: >:)\n");

 dpy = XOpenDisplay(0);
 c = XGetXCBConnection(dpy);
 XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
 s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

 xcb_change_window_attributes_checked(c, s->root, XCB_CW_EVENT_MASK, (uint32_t[]){ XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT });
 
 ewmh = calloc(1, sizeof(xcb_ewmh_connection_t));
 xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(c, ewmh), (void *)0);

 /*xcb_atom_t net[] = { ewmh->_NET_SUPPORTED, ewmh->_NET_WM_NAME };
 xcb_ewmh_set_supported(ewmh, 0, sizeof(net)/sizeof(*net), net);*/
 const char *WM_ATOM_NAME[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW", };
 tora_get_atoms(WM_ATOM_NAME, wm_atoms, WM_COUNT);
 const char *NET_ATOM_NAME[] = { "_NET_SUPPORTED", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE" };
 tora_get_atoms(NET_ATOM_NAME, net_atoms, NET_COUNT);
 xcb_change_property(c, XCB_PROP_MODE_REPLACE, s->root, net_atoms[NET_SUPPORTED], XCB_ATOM_ATOM, 32, NET_COUNT, net_atoms);

 xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(c);
 for (int i = 0; i < sizeof(keys)/sizeof(*keys); i++) xcb_grab_key(c, 0, s->root, keys[i].mod, *xcb_key_symbols_get_keycode(keysyms, keys[i].key), XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
 xcb_key_symbols_free(keysyms);

 title_font = XftFontOpenName(dpy, 0, "IBM Plex Sans:autohint=true:antialias=true:size=16");

 colormap = xcb_generate_id(c);
 XVisualInfo xv;
 xv.depth = 32;
 int result = 0;
 XVisualInfo *result_ptr = NULL;
 result_ptr = XGetVisualInfo(dpy, VisualDepthMask, &xv, &result);
 visual = result_ptr->visual;
 xcb_create_colormap(c, XCB_COLORMAP_ALLOC_NONE, colormap, s->root, result_ptr->visualid);
 XRenderColor rc = { 5 / 255 * 65535, 21 / 255 * 65535, 25 / 255 * 65535, 65535 };
 XftColorAllocValue(dpy, visual, colormap, &rc, &title_color);
 
 static void (*events[XCB_NO_OPERATION])(xcb_generic_event_t *event);
 for (int i = 0; i < XCB_NO_OPERATION; i++) events[i] = NULL;
 events[XCB_BUTTON_PRESS]      = tora_button_press;
 events[XCB_MOTION_NOTIFY]	    = tora_motion_notify;
 events[XCB_BUTTON_RELEASE]    = tora_button_release;
 events[XCB_MAP_REQUEST]	      = tora_map_request;
 events[XCB_EXPOSE]            = tora_expose_notify;
 events[XCB_DESTROY_NOTIFY]	   = tora_destroy_notify;
 events[XCB_KEY_PRESS]	        = tora_key_press;
 events[XCB_CLIENT_MESSAGE]	   = tora_client_message;
 events[XCB_PROPERTY_NOTIFY]	  = tora_property_notify;
 events[XCB_CONFIGURE_REQUEST] = tora_configure_request;
 
 atexit(tora_cleanup);

 xcb_generic_event_t *ev;
 for (;;) {
  xcb_flush(c);
  if (xcb_connection_has_error(c)) printf("tora: YEET SKRRT DAB\n");
  ev = xcb_wait_for_event(c);
  if (events[ev->response_type & ~0x80]) events[ev->response_type & ~0x80](ev);
  free(ev);
 }

 return 0;
}
