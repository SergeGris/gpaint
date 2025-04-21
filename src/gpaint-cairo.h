
#pragma once

#include <cairo.h>

static inline cairo_t *
create_cairo (cairo_surface_t *surface, cairo_operator_t op, cairo_antialias_t antialiasing)
{
  cairo_t *cr = cairo_create (surface);
  cairo_set_operator (cr, op);
  cairo_set_antialias (cr, antialiasing);
  return cr;
}

static inline cairo_surface_t *
create_surface (gint height, gint width)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, height, width);
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE, CAIRO_ANTIALIAS_NONE);
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
  GdkRectangle v = {
    .x = (gint) (px / scale),
    .y = (gint) (py / scale),
    .width = (gint) (pw / scale),
    .height = (gint) (ph / scale),
  };

  scale = 16.0; // TODO
  // TODO cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_save (cr);
  cairo_scale (cr, scale, scale);

  gdouble bg[] = { 0x54 / 255.0, 0xA8 / 255.0 };

  cairo_set_source_rgb (cr, bg[0], bg[0], bg[0]);
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

  cairo_set_source_rgb (cr, bg[1], bg[1], bg[1]);
  cairo_stroke (cr);
  cairo_restore (cr);

  cairo_restore (cr);
}

// TODO FIX THIS SHIT
static inline void
draw_colored_square (cairo_t *cr, const GdkRGBA *color, gdouble px, gdouble py, gdouble pw, gdouble ph, gdouble scale)
{
  if ((int) roundf (255.0f * color->alpha) == 255)
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
  data[idx + 0] = (guint8) (int) roundf (color->blue * 255.0f);
  data[idx + 1] = (guint8) (int) roundf (color->green * 255.0f);
  data[idx + 2] = (guint8) (int) roundf (color->red * 255.0f);
  data[idx + 3] = (guint8) (int) roundf (color->alpha * 255.0f);
}

static inline gboolean
rgba_equal (const GdkRGBA *rgba1, const GdkRGBA *rgba2)
{
  int r1 = (int) roundf (rgba1->red * 255.0f);
  int r2 = (int) roundf (rgba2->red * 255.0f);

  if (r1 != r2)
    return FALSE;

  int g1 = (int) roundf (rgba1->green * 255.0f);
  int g2 = (int) roundf (rgba2->green * 255.0f);

  if (g1 != g2)
    return FALSE;

  int b1 = (int) roundf (rgba1->blue * 255.0f);
  int b2 = (int) roundf (rgba2->blue * 255.0f);

  if (b1 != b2)
    return FALSE;

  int a1 = (int) roundf (rgba1->alpha * 255.0f);
  int a2 = (int) roundf (rgba2->alpha * 255.0f);

  return a1 == a2;
}

static inline void
copy_surface (cairo_surface_t *dst, cairo_surface_t *src)
{
  cairo_t *cr = cairo_create (dst);
  cairo_set_source_surface (cr, src, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static inline int
gpaint_cairo_get_color_depth (cairo_surface_t *surface)
{
  // TODO undocumanted?
  const cairo_format_t format = cairo_image_surface_get_format (surface);
  int color_depth = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

  // Handle only supported formats... TODO
  switch (format)
    {
    case CAIRO_FORMAT_ARGB32:
      color_depth = 32; // 8 bits for each of 4 channels (Alpha, Red, Green, Blue)
      break;
    case CAIRO_FORMAT_RGB24:
      color_depth = 24; // 8 bits for each of 3 channels (Red, Green, Blue)
      break;
    case CAIRO_FORMAT_A8:
      color_depth = 8; // 8 bits for Alpha only
      break;
      // TODO
    /* case CAIRO_FORMAT_RGB16_565: */
    /*   color_depth = 16; // 5 bits for Red, 6 bits for Green, 5 bits for Blue
     */
    /*   break; */
    /* case CAIRO_FORMAT_RGB30: */
    /*   color_depth = 30; // 10 bits for each of 3 channels (Red, Green, Blue)
     */
    /*   break; */
    case CAIRO_FORMAT_A1:
      color_depth = 1;
      break;
    case CAIRO_FORMAT_INVALID:
    default:
      color_depth = 0;
      break;
    }

#pragma GCC diagnostic pop

  return color_depth;
}

static inline int
gpaint_cairo_get_bytes_per_pixel (cairo_surface_t *surface)
{
  // TODO undocumanted?
  const cairo_format_t format = cairo_image_surface_get_format (surface);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

  // Handle only supported formats... TODO
  switch (format)
    {
    case CAIRO_FORMAT_ARGB32:
      return 4; // 8 bits for each of 4 channels (Alpha, Red, Green, Blue)
    case CAIRO_FORMAT_RGB24:
      return 4; // 8 bits for each of 3 channels (Red, Green, Blue)
    case CAIRO_FORMAT_A8:
      return 1; // 8 bits for Alpha only
      // TODO
      /* case CAIRO_FORMAT_RGB16_565: */
      /*   color_depth = 16; // 5 bits for Red, 6 bits for Green, 5 bits for
       * Blue */
      /*   break; */
      /* case CAIRO_FORMAT_RGB30: */
      /*   color_depth = 30; // 10 bits for each of 3 channels (Red, Green,
       * Blue) */
      /*   break; */
      /* case CAIRO_FORMAT_A1: */
      /*   color_depth = 1; */
      /*   break; */
    case CAIRO_FORMAT_INVALID:
    default:
      abort ();
    }

#pragma GCC diagnostic pop
}
