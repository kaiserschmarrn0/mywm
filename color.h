#ifndef MYWM_COLOR_H
#define MYWM_COLOR_H

#include <stdint.h>
#include <X11/Xft/Xft.h>

#include "mywm.h"

typedef union {
	struct {
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a;
	};
	uint32_t v;
} rgba;

XRenderColor hex_to_rgb(uint32_t hex);
XftColor xft_color(uint32_t hex);

#endif
