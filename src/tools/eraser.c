
#include "tools-internal.h"

static void draw_eraser_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void motion_eraser_handler (AppState *state, gint x, gint y);
static void draw_eraser_cursor (AppState *state, cairo_t *cr);

const Tool global_eraser_tool = {
  .type = TOOL_ERASER,
  .icon = &eraser_data,
  .cursor = NULL,
  .draw_handler = draw_eraser_handler,
  .motion_handler = motion_eraser_handler,
  .draw_cursor_handler = draw_eraser_cursor,
  .override_main_surface = true,
  .is_drawing = TRUE,
};

static void
draw_eraser (cairo_surface_t *surface, const GdkRGBA *color, gint x, gint y, gdouble size, cairo_antialias_t antialiasing)
{
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE, antialiasing);
  gdk_cairo_set_source_rgba (cr, color);
  cairo_rectangle (cr, x + 0.5 - size / 2, y + 0.5 - size / 2, size, size);
  cairo_fill (cr);
  cairo_destroy (cr);
}

static void
draw_eraser_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_eraser (state->preview_surface, &state->secondary_color, x0, y0, state->eraser_size, state->antialiasing);
}

static void
motion_eraser_handler (AppState *state, gint x, gint y)
{
  draw_line_with_width_and_color (state->preview_surface, state->last_point.x, state->last_point.y,
                                  x, y, state->eraser_size, &state->secondary_color, state->antialiasing);
  draw_eraser (state->preview_surface, &state->secondary_color, x, y, state->eraser_size, state->antialiasing);
  state->last_point.x = x;
  state->last_point.y = y;
}

static void
draw_eraser_cursor (AppState *state, cairo_t *cr)
{
  const gdouble pixel_size = state->zoom_level;
  // TODO cairo_set_antialias (cr, CAIRO_ANTIALIAS_SUBPIXEL);
  // cairo_set_line_width (cr, 0.5);
  const gdouble size = state->eraser_size * pixel_size;
  /* gdouble x = floor (state->cursor_x / pixel_size) * pixel_size + 0.5 - size / 2; */
  /* gdouble y = floor (state->cursor_y / pixel_size) * pixel_size + 0.5 - size / 2; */

  // TODO draw transparent differently
  // TODO
  double x = floor (state->cursor_x / pixel_size) * pixel_size;
  double y = floor (state->cursor_y / pixel_size) * pixel_size;
  draw_colored_square (cr, &state->secondary_color,
                       x + 0.5 - size / 2,
                       y + 0.5 - size / 2,
                       size, size, pixel_size);

  /* gdk_cairo_set_source_rgba (cr, &state->secondary_color); */
  /* cairo_rectangle (cr, x, y, size, size); */
  /* cairo_fill (cr); */

  cairo_set_line_width (cr, 0.9);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_rectangle (cr, x + 0.5 - size / 2, y + 0.5 - size / 2, size, size);
  cairo_stroke (cr);

  // draw_eraser (state->cursor_surface, &state->secondary_color, x0, y0, state->eraser_size);

  /* cairo_t *cr = create_cairo (state->cursor_surface, CAIRO_OPERATOR_SOURCE); */
  /*   /\* Set anti-aliasing to subpixel for smooth rendering of thin lines *\/ */
  /* cairo_save(cr); */
  /*   cairo_set_antialias(cr, CAIRO_ANTIALIAS_SUBPIXEL); */

  /*   /\* Set the line width to 0.5 (i.e. half a pixel) *\/ */
  /*   cairo_set_line_width(cr, 0.5); */

  /*   /\* Set drawing color to black *\/ */
  /*   cairo_set_source_rgb(cr, 0, 0.0, 0); */

  /*   const gdouble pixel_size = state->zoom_level; */
  /*   const gdouble size = state->eraser_size; */

  /*   /\* Draw a rectangle at (20,20) with width and height of 0.5 pixels each *\/ */
  /*   cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size, size); */
  /*   //cairo_rectangle(cr, 20, 20, 1.5 * pixel_size, 1.5 * state->pixel_size); */
  /*   cairo_stroke(cr); */
}
