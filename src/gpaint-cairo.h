
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
create_surface (gint height, gint width)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, height, width);
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr);
  cairo_destroy (cr);
  return surface;
}

static inline void
clear_canvas (cairo_surface_t *surface)
{
  /* cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE); */
  /* cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0); */
  /* cairo_paint (cr); */
  /* cairo_destroy (cr); */

  cairo_t *cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static inline void
draw_transparent_square (cairo_t *cr, double px, double py, double pw, double ph, double scale)
{
  cairo_save (cr);
  GdkRectangle v =
    {
      .x = (gint) (px / scale),
      .y = (gint) (py / scale),
      .width = (gint) (pw / scale),
      .height = (gint) (ph / scale),
    };

  // TODO cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_save (cr);
  cairo_scale (cr, scale, scale);

  gdouble c = 0.3;
  cairo_set_source_rgb (cr, c, c, c);
  cairo_rectangle (cr, v.x, v.y, v.width, v.height);
  cairo_fill (cr);
  cairo_restore (cr);

  /* Draw checkerboard background */
  cairo_save (cr);
  gint k = 2; // TODO make it adaptive for zoom.
  cairo_scale (cr, scale / k, scale / k);

  gint grid_width = (v.x + v.width) * k;
  gint grid_height = (v.y + v.height) * k;

  cairo_new_path (cr);
  const gdouble dash[] = { 1.0, 1.0 };
  cairo_set_dash (cr, dash, 1, 0);
  cairo_set_line_width (cr, 1.0);

  for (gint y = v.y * k; y < grid_height; y++)
    {
      gdouble x0, x1, y0, y1;

      x0 = v.x * k + (y & 1);
      x1 = grid_width;
      y0 = y1 = y + 0.5;

      cairo_move_to (cr, x0, y0);
      cairo_line_to (cr, x1, y1);
    }

  double cc = 0.7;
  cairo_set_source_rgb (cr, cc, cc, cc);
  cairo_stroke (cr);
  cairo_restore (cr);

  cairo_restore (cr);
}

// TODO FIX THIS SHIT
static inline void
draw_colored_square (cairo_t *cr, const GdkRGBA *color, gdouble px, gdouble py, gdouble pw, gdouble ph, gdouble scale)
{
  if (color->alpha == 1.0)
    {
      cairo_save (cr);
      gdk_cairo_set_source_rgba (cr, color);
      cairo_rectangle (cr, px, py, pw, ph);
      cairo_fill (cr);
      cairo_restore (cr);
    }
  else
    draw_transparent_square (cr, px, py, pw, ph, scale);
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
