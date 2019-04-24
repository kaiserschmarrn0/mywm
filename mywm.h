#ifndef MYWM_H
#define MYWM_H

#include <xcb/xcb.h>

typedef struct {
	uint16_t mod;
	xcb_keysym_t key;

	void (*function) (int arg);
	int arg;
} keybind;

typedef struct {
	uint16_t mod;
	uint32_t button;

	void (*function) (xcb_window_t win, uint32_t event_x, uint32_t event_y);
} button;

extern xcb_connection_t *conn;

void close(int arg);
void cycle(int arg);
void stick(int arg);

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

void mouse_roll_up(xcb_window_t win, uint32_t event_x, uint32_t event_y);
void mouse_roll_down(xcb_window_t win, uint32_t event_x, uint32_t event_y);

#endif
