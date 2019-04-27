#ifndef ACTION_H
#define ACTION_H

void stick(int arg);
void close(int arg);
void cycle(int arg);
void stop_cycle();

void snap_l(int arg);
void snap_lu(int arg);
void snap_ld(int arg);
void snap_r(int arg);
void snap_ru(int arg);
void snap_rd(int arg);
void snap_max(int arg);

void int_full(int arg);

void change_ws(int arg);
void send_ws(int arg);

void mouse_move(xcb_window_t win, uint32_t event_x, uint32_t event_y);
void mouse_resize(xcb_window_t win, uint32_t event_x, uint32_t event_y);

void mouse_move_motion(xcb_generic_event_t *ev);
void mouse_resize_motion(xcb_generic_event_t *ev);

void button_release(xcb_generic_event_t *ev);

void mouse_roll_up(xcb_window_t win, uint32_t event_x, uint32_t event_y);
void mouse_roll_down(xcb_window_t win, uint32_t event_x, uint32_t event_y);

#endif
