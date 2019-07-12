#ifndef ACTION_H
#define ACTION_H

void stick(void *arg);
void close(void *arg);
void cycle(void *arg);
void stop_cycle();

void create_snap_regions();
void snap(void *arg);
void snap_u(void *arg);
void snap_r(void *arg);
void snap_l(void *arg);

void int_full(void *arg);

void change_ws(void *arg);
void send_ws(void *arg);

void create_margins();
void margin_leave_handler(xcb_generic_event_t *ev);
void margin_enter_handler(xcb_generic_event_t *ev);

void mouse_move(void *arg);
void mouse_resize(void *arg);

void mouse_move_motion(void *arg);
void mouse_move_motion_start(void *arg);
void mouse_resize_motion(void *arg);

void button_release(void *arg);
void resize_release(void *arg);

void region_press(void *arg);
void region_close(void *arg);
void region_snap_u(void *arg);
void region_abort();

void mouse_roll_up(void *arg);
void mouse_roll_down(void *arg);

#endif
