
#include "tools-internal.h"

static void draw_ellipse (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void draw_ellipse_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_ellipse_tool = {
  .type = TOOL_ELLIPSE,
  .icon = &ellipse_data,
  .cursor = "crosshair",
  .draw_handler = draw_ellipse_handler,
  .motion_handler = NULL,
};

static void
draw_ellipse (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // Compute the bounding box and ellipse center
  gint min_x = MIN (x0, x1);
  gint min_y = MIN (y0, y1);
  gint dx = abs (x1 - x0);
  gint dy = abs (y1 - y0);
  gdouble cx = min_x + dx / 2.0;
  gdouble cy = min_y + dy / 2.0;
  gdouble rx = dx / 2.0;
  gdouble ry = dy / 2.0;

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);

  // Draw the outer ellipse (the stroke)
  cairo_save (cr);
  cairo_translate (cr, cx, cy);
  cairo_scale (cr, rx, ry);
  cairo_arc (cr, 0, 0, 1.0, 0, 2 * G_PI);
  cairo_restore (cr);
  cairo_set_line_width (cr, state->width);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
  gdk_cairo_set_source_rgba (cr, state->p_color);

  if (state->fill_type == FILL_TRANSPARENT || state->fill_type == FILL_SECONDARY)
    cairo_stroke (cr);
  else
    cairo_fill (cr);

  // Draw the inner ellipse if fill type is secondary
  if (state->fill_type == FILL_SECONDARY)
    {
      // Inset the inner ellipse by half the stroke width on all sides
      gdouble inset = state->width / 2.0;
      gdouble inner_rx = rx - inset;
      gdouble inner_ry = ry - inset;

      // Only draw if the inner ellipse is valid
      if (inner_rx > 0 && inner_ry > 0)
        {
          cairo_save (cr);
          cairo_translate (cr, cx, cy);
          cairo_scale (cr, inner_rx, inner_ry);
          cairo_arc (cr, 0, 0, 1.0, 0, 2 * G_PI);
          cairo_restore (cr);
          gdk_cairo_set_source_rgba (cr, state->s_color);
          cairo_fill (cr);
        }
    }

  cairo_destroy (cr);
}
static void
draw_ellipse_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_ellipse (state, x0, y0, x1, y1);
}
