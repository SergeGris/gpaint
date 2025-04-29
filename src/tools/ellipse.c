
#include "tools-internal.h"

static const struct raw_bitmap ellipse_data;
static void draw_ellipse (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void draw_ellipse_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_ellipse_tool = {
  .type = TOOL_ELLIPSE,
  .icon = &ellipse_data,
  .cursor_name = "crosshair",
  .draw_handler = draw_ellipse_handler,
  .motion_handler = NULL,
  .is_drawing = TRUE,
};

static void
draw_ellipse (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // Compute the bounding box and ellipse center
  gint min_x = min_int (x0, x1);
  gint min_y = min_int (y0, y1);
  gint dx = abs (x1 - x0) + 1;
  gint dy = abs (y1 - y0) + 1;
  gdouble cx = min_x + dx / 2.0;
  gdouble cy = min_y + dy / 2.0;
  gdouble rx = dx / 2.0;
  gdouble ry = dy / 2.0;

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE, state->antialiasing);

  // Draw the outer ellipse (the stroke)
  cairo_save (cr);
  cairo_translate (cr, cx, cy);
  cairo_scale (cr, rx, ry);
  cairo_arc (cr, 0, 0, 1.0, 0, 2 * G_PI);
  cairo_restore (cr);
  cairo_set_line_width (cr, state->width);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
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

// clang-format off
static const guchar ellipse_bytes[] =
  {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0xF3, 0xFF, 0x61, 0x00, 0x00, 0x00, 0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x4A, 0x49, 0x44, 0x41, 0x54, 0x38, 0xCB, 0x63, 0x60, 0x18, 0xF2, 0x80, 0x11, 0x8B, 0xD8, 0x7F, 0x52, 0xF4, 0x30, 0xA2, 0x6B, 0xFC, 0xFF, 0x1F, 0xBF, 0x7E, 0x46, 0x46, 0x46, 0x14, 0xBD, 0x30, 0xDE, 0xFF, 0xFF, 0xFF, 0xFF, 0x33, 0xDC, 0xBF, 0x77, 0x9B, 0x28, 0x67, 0x2B, 0x2A, 0xA9, 0xC2, 0x0C, 0x62, 0x64, 0x24, 0x55, 0x33, 0xBA, 0x21, 0x4C, 0x94, 0x06, 0xE2, 0xC0, 0x1B, 0x40, 0x95, 0x40, 0xA4, 0x4A, 0x34, 0x92, 0x9D, 0x90, 0x86, 0x01, 0x00, 0x00, 0xFB, 0x8C, 0x1F, 0x0D, 0x93, 0xBD, 0x77, 0x6F, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
static const struct raw_bitmap ellipse_data =
  {
    .hotspot_x = 0,
    .hotspot_y = 15,
    .size = sizeof (ellipse_bytes),
    .data = ellipse_bytes,
  };
// clang-format on
