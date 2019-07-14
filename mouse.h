#ifndef MOUSE_H
#define MOUSE_H

void grab_pointer();
void button_release(void *arg);

void mouse_move(void *arg);

void mouse_resize_south(void *arg);
void mouse_resize_north(void *arg);
void mouse_resize_south_east(void *arg);
void mouse_resize_west(void *arg);
void mouse_resize_east(void *arg);
void mouse_resize_north_west(void *arg);
void mouse_resize_north_east(void *arg);
void mouse_resize_south_west(void *arg);

#endif
