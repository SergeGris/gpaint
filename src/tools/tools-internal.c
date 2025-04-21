#include "tools-internal.h"

void
handle_pixel (cairo_surface_t *surface, gint x, gint y, const GdkRGBA *color, cairo_antialias_t antialiasing)
{
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE, antialiasing);
  gdk_cairo_set_source_rgba (cr, color);
  cairo_rectangle (cr, x, y, 1, 1);
  cairo_fill (cr);
  cairo_destroy (cr);
}

void // TODO rename
draw_line_with_width_and_color (cairo_surface_t *surface, gint x0, gint y0, gint x1, gint y1, gdouble width, const GdkRGBA *color, cairo_antialias_t antialiasing)
{
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE, antialiasing);
  cairo_set_line_width (cr, width);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_move_to (cr, x0 + 0.5, y0 + 0.5);
  cairo_line_to (cr, x1 + 0.5, y1 + 0.5);
  gdk_cairo_set_source_rgba (cr, color);
  // cairo_set_antialias(cr, CAIRO_ANTIALIAS_BILINEAR); // Enable bilinear
  // anti-aliasing
  cairo_stroke (cr);
  cairo_destroy (cr);

  /* cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE); */
  /* cairo_set_line_width (cr, width); */
  /* cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE); */
  /* cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER); */
  /* cairo_move_to (cr, x0 + 0.5, y0 + 0.5); */
  /* cairo_line_to (cr, x1 + 0.5, y1 + 0.5); */
  /* gdk_cairo_set_source_rgba (cr, color); */
  /* cairo_stroke (cr); */
  /* cairo_destroy (cr); */
}

GdkRGBA
get_pixel_color (const guchar *data, gint x, gint y, gint stride)
{
  /* CAIRO_FORMAT_ARGB32 stores pixels in BGRA order on little-endian systems
   */
  gint idx = y * stride + x * 4;
  GdkRGBA color = {
    .red = (gfloat) data[idx + 2] / 255.0f,
    .green = (gfloat) data[idx + 1] / 255.0f,
    .blue = (gfloat) data[idx + 0] / 255.0f,
    .alpha = (gfloat) data[idx + 3] / 255.0f,
  };
  return color;
}
