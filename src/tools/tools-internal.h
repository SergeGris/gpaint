
#pragma once

#include "tools-icons.h"
#include "tools.h"

extern void handle_pixel (cairo_surface_t *surface, gint x, gint y, const GdkRGBA *color, cairo_antialias_t antialiasing);
// TODO rename
extern void draw_line_with_width_and_color (cairo_surface_t *surface, gint x0, gint y0, gint x1, gint y1, gdouble width, const GdkRGBA *color, cairo_antialias_t antialiasing);
extern GdkRGBA get_pixel_color (const guchar *data, gint x, gint y, gint stride);
