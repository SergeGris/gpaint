
#include "tools-internal.h"

static const struct raw_bitmap rectangle_data;
static void draw_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_rectangle_tool = {
  .type = TOOL_RECTANGLE,
  .icon = &rectangle_data,
  .cursor_name = "crosshair",
  .draw_handler = draw_rectangle_handler,
  .motion_handler = NULL,
  .is_drawing = TRUE,
};

static void
draw_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // Calculate the rectangle bounds
  gint min_x = MIN (x0, x1);
  gint min_y = MIN (y0, y1);
  gint dx = abs (x1 - x0);
  gint dy = abs (y1 - y0);

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE, state->antialiasing);

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

// clang-format off
static const guchar rectangle_bytes[] =
  {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0xF3, 0xFF, 0x61, 0x00, 0x00, 0x00, 0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x47, 0x49, 0x44, 0x41, 0x54, 0x38, 0xCB, 0xED, 0xD2, 0xB1, 0x0D, 0x80, 0x30, 0x0C, 0x45, 0xC1, 0x73, 0xC4, 0x22, 0x20, 0xB1, 0xFF, 0x40, 0x48, 0x30, 0x8A, 0x29, 0x32, 0x01, 0x71, 0x11, 0x8A, 0xFC, 0xDE, 0x57, 0x58, 0x8F, 0xD9, 0x0B, 0x64, 0x05, 0xD8, 0x20, 0x73, 0xCC, 0x88, 0x88, 0x0E, 0xC0, 0x73, 0x5F, 0x9F, 0x8E, 0xF7, 0xE3, 0x04, 0xAD, 0xFA, 0x83, 0x05, 0xFC, 0x01, 0x88, 0xDE, 0xD1, 0x78, 0x48, 0xE5, 0x94, 0xE7, 0xEF, 0x05, 0x44, 0xB5, 0x0C, 0x17, 0x97, 0x93, 0xA8, 0x55, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
static const struct raw_bitmap rectangle_data =
  {
    .hotspot_x = 0,
    .hotspot_y = 15,
    .size = sizeof (rectangle_bytes),
    .data = rectangle_bytes,
  };
// clang-format on
