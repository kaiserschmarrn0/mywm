#ifndef ACTION_H
#define ACTION_H

void stick(void *arg);
void close(void *arg);

void select_window_terminate(void);
void select_window(void *arg);

void snap_u(void *arg);
void snap_r(void *arg);
void snap_l(void *arg);

void int_full(void *arg);

void change_ws(void *arg);
void send_ws(void *arg);

void mywm_exit(void *arg);

void window_button_close(void *arg);
void window_button_snap_u(void *arg);
void region_abort();

void mouse_roll_up(void *arg);
void mouse_roll_down(void *arg);

#endif
