
#include "tools-internal.h"

static void draw_brush_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void draw_brush_cursor (AppState *state, gint x0, gint y0);
static void motion_brush_handler (AppState *state, gint x, gint y);

const Tool global_brush_tool = {
  .type = TOOL_BRUSH,
  .icon = &brush_data,
  .cursor = NULL,
  .draw_handler = draw_brush_handler,
  .motion_handler = motion_brush_handler,
  .is_drawing = TRUE,

  /* TODO .draw_cursor_handler = draw_brush_cursor, */
};

static void
draw_brush_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // TODO
  /* For brush, simply plot the pixel at the new coordinate */
  // handle_pixel (state, x1, y1, state->color);

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE, state->antialiasing);
  gdk_cairo_set_source_rgba (cr, state->p_color);
  gdouble size = state->brush_size;
  cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size, size);
  cairo_fill (cr);
  cairo_destroy (cr);
}

/* static void */
/* draw_brush_cursor (AppState *state, gint x0, gint y0) */
/* { */
/*   cairo_t *cr = create_cairo (state->cursor_surface, CAIRO_OPERATOR_SOURCE); */
/*   gdk_cairo_set_source_rgba (cr, state->color); */
/*   gdouble size = state->brush_size; */
/*   cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size, size); */
/*   cairo_fill (cr); */
/*   cairo_destroy (cr); */
/* } */

static void
motion_brush_handler (AppState *state, gint x, gint y)
{
  draw_line_with_width_and_color (state->preview_surface, state->last_point.x, state->last_point.y, x, y, state->brush_size, state->p_color, state->antialiasing);
  state->last_point.x = x;
  state->last_point.y = y;
}
