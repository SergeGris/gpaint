
#pragma once

#include <cairo.h>

static inline cairo_t *
create_cairo (cairo_surface_t *surface, cairo_operator_t op)
{
  cairo_t *cr = cairo_create (surface);
  cairo_set_operator (cr, op);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  return cr;
}

static inline cairo_surface_t *
create_surface (gint height, gint width, bool transparent)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, height, width);
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE);
  gdouble color = transparent ? 0.0 : 1.0;
  cairo_set_source_rgba (cr, color, color, color, color);
  cairo_paint (cr);
  cairo_destroy (cr);
  return surface;
}

static inline void
clear_canvas (AppState *state, cairo_surface_t *surface)
{
  /* cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE); */
  /* cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0); */
  /* cairo_paint (cr); */
  /* cairo_destroy (cr); */

  cairo_t *cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_destroy (cr);
  gtk_widget_queue_draw (state->drawing_area);
}

static inline void
set_pixel_color (guint8 *data, gint x, gint y, gint stride, const GdkRGBA *color)
{
  gint idx = y * stride + x * 4;
  data[idx + 0] = (guint8) (color->blue * 255.0f);
  data[idx + 1] = (guint8) (color->green * 255.0f);
  data[idx + 2] = (guint8) (color->red * 255.0f);
  data[idx + 3] = (guint8) (color->alpha * 255.0f);
}

static inline void
copy_surface (cairo_surface_t *dst, cairo_surface_t *src)
{
  cairo_t *cr = cairo_create (dst);
  cairo_set_source_surface (cr, src, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}
