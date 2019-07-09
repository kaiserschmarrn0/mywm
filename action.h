#ifndef ACTION_H
#define ACTION_H

void stick(void *arg);
void close(void *arg);
void cycle(void *arg);
void stop_cycle();

void snap_l(void *arg);
void snap_lu(void *arg);
void snap_ld(void *arg);
void snap_r(void *arg);
void snap_ru(void *arg);
void snap_rd(void *arg);
void snap_max(void *arg);

void int_full(void *arg);

void change_ws(void *arg);
void send_ws(void *arg);

void mouse_move(void *arg);
void mouse_resize(void *arg);

void mouse_move_motion(void *arg);
void mouse_resize_motion(void *arg);

void button_release(void *arg);

void mouse_roll_up(void *arg);
void mouse_roll_down(void *arg);

#endif
