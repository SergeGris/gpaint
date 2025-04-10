
#include "tools-internal.h"

static void draw_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_rectangle_tool = {
  .type = TOOL_RECTANGLE,
  .icon = &rectangle_data,
  .cursor = "crosshair",
  .draw_handler = draw_rectangle_handler,
  .motion_handler = NULL,
};

static void
draw_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // Calculate the rectangle bounds
  gint min_x = MIN (x0, x1);
  gint min_y = MIN (y0, y1);
  gint dx = abs (x1 - x0);
  gint dy = abs (y1 - y0);

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);

  // Draw the outer rectangle (border) with primary color
  cairo_rectangle (cr, min_x + 0.5, min_y + 0.5, dx, dy);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
  cairo_set_line_width (cr, state->width);
  gdk_cairo_set_source_rgba (cr, state->p_color);

  if (state->fill_type == FILL_TRANSPARENT || state->fill_type == FILL_SECONDARY)
    cairo_stroke (cr);
  else
    cairo_fill (cr);

  // If fill type is secondary, fill the inner rectangle.
  if (state->fill_type == FILL_SECONDARY)
    {
      // Compute the inset offset (half the line width on each side)
      gdouble half_width = state->width / 2.0;
      // The inner rectangle is inset by half_width on all sides.
      gdouble fill_x = min_x + half_width + 0.5;
      gdouble fill_y = min_y + half_width + 0.5;
      gdouble fill_w = dx - state->width;
      gdouble fill_h = dy - state->width;

      // Ensure the fill dimensions are positive.
      if (fill_w > 0.0 && fill_h > 0.0)
        {
          cairo_rectangle (cr, fill_x, fill_y, fill_w, fill_h);
          gdk_cairo_set_source_rgba (cr, state->s_color);
          cairo_fill (cr);
        }
    }

  cairo_destroy (cr);
}
