#include "color.h"

static uint32_t xcb_color(uint32_t hex) {
	rgba col;
	col.v = hex;

	if (!col.a) {
		return 0U;
	}

	col.r = (col.r * col.a) / 255;
	col.g = (col.g * col.a) / 255;
	col.b = (col.b * col.a) / 255;

	return col.v;
}

XRenderColor hex_to_rgb(uint32_t hex) {
	rgba col;
	col.v = xcb_color(hex);

	XRenderColor ret;
	ret.red = col.r * 65535 / 255;
	ret.green = col.g * 65535 / 255;
	ret.blue = col.b * 65535 / 255;
	ret.alpha = col.a * 65535 / 255;

	return ret;
}

XftColor xft_color(uint32_t hex) {
	XRenderColor rc = hex_to_rgb(hex);

	XftColor ret;
	XftColorAllocValue(dpy, vis_ptr, cm, &rc, &ret);

	return ret;
}
