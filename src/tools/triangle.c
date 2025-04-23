
#include "tools-internal.h"

static const struct raw_bitmap triangle_data;
static void draw_triangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_triangle_tool = {
  .type = TOOL_TRIANGLE,
  .icon = &triangle_data,
  .cursor_name = "crosshair",
  .draw_handler = draw_triangle_handler,
  .motion_handler = NULL,
  .is_drawing = TRUE,
};

/* static void */
/* draw_triangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1) */
/* { */
/*   // Calculate the difference between the two given points. */
/*   double dx = x1 - x0; */
/*   double dy = y1 - y0; */

/*   // Compute the side length (distance between (x0,y0) and (x1,y1)). */
/*   double side = sqrt (dx * dx + dy * dy); */

/*   if (side == 0) */
/*     return; // Avoid division by zero if the points are identical. */

/*   // Compute the altitude of the equilateral triangle. */
/*   double altitude = (sqrt (3.0) / 2.0) * side; */

/*   // Find the midpoint of the line segment (base). */
/*   double mid_x = (x0 + x1) / 2.0; */
/*   double mid_y = (y0 + y1) / 2.0; */

/*   // Compute a normalized perpendicular vector. */
/*   // For (dx, dy), a perpendicular vector is (-dy, dx). */
/*   double nx = -dy / side; // normalized x component */
/*   double ny = dx / side;  // normalized y component */

/*   // Compute the third vertex using the altitude along the perpendicular direction. */
/*   double x2 = mid_x + nx * altitude; */
/*   double y2 = mid_y + ny * altitude; */

/*   // Use cairo to draw the triangle. */
/*   cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE, state->antialiasing); */

/*   // Begin a new path. */
/*   cairo_new_path (cr); */

/*   // Move to the first vertex. */
/*   cairo_move_to (cr, x0, y0); */

/*   // Draw line to the second vertex. */
/*   cairo_line_to (cr, x1, y1); */

/*   // Draw line to the third vertex. */
/*   cairo_line_to (cr, x2, y2); */

/*   // Close the path to connect back to the first vertex. */
/*   cairo_close_path (cr); */

/*   // Optionally, set a stroke color and line width. */
/*   gdk_cairo_set_source_rgba (cr, state->p_color); */
/*   cairo_set_line_width (cr, state->width); */

/*   // Stroke the path. */
/*   cairo_stroke (cr); */
/* } */

#include <math.h>

static void
draw_triangle_handler (AppState *state,
                       gint x0,
                       gint y0,
                       gint x1,
                       gint y1)
{
  // 1) Compute base vector and its length
  double dx = x1 - x0;
  double dy = y1 - y0;
  double side = hypot (dx, dy); // sqrt(dx*dx + dy*dy) :contentReference[oaicite:0]{index=0}
  if (side < 1e-6)
    return;

  // 2) Normalize base direction
  double ux = dx / side;
  double uy = dy / side;

  // 3) Rotate by +60° to get the third vertex direction
  const double COS60 = 0.5;
  const double SIN60 = 1.7320508075688772935274463415058723669428052538104 * 0.5; // √3/2
  double vx = ux * COS60 - uy * SIN60;
  double vy = ux * SIN60 + uy * COS60;

  // 4) Compute the third vertex at the same distance
  double x2 = x0 + vx * side;
  double y2 = y0 + vy * side;

  // 5) Draw in Cairo
  cairo_t *cr = create_cairo (state->preview_surface,
                              CAIRO_OPERATOR_SOURCE,
                              state->antialiasing);

  // Set stroke style
  gdk_cairo_set_source_rgba (cr, state->p_color);
  cairo_set_line_width (cr, state->width);

  // Move to first point and draw the triangle edges
  cairo_move_to (cr, x0, y0);
  cairo_line_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_line_to (cr, x0, y0);

  // Render the stroke and clear the path in one go
  cairo_stroke (cr);
}

// clang-format off
static const guchar triangle_bytes[] =
  {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0xF3, 0xFF, 0x61, 0x00, 0x00, 0x00, 0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x47, 0x49, 0x44, 0x41, 0x54, 0x38, 0xCB, 0xBD, 0x90, 0x41, 0x0E, 0x00, 0x20, 0x08, 0xC3, 0xEA, 0xE2, 0xFF, 0xBF, 0x8C, 0x67, 0xA3, 0x20, 0x91, 0x04, 0x8E, 0x6C, 0xD0, 0x01, 0xC4, 0x65, 0x0F, 0x9D, 0x91, 0x1C, 0x76, 0x7D, 0x2A, 0x00, 0x52, 0x0B, 0xBE, 0xCB, 0xB2, 0xBF, 0x50, 0x07, 0x3D, 0xEC, 0xAB, 0x8B, 0xEE, 0xEA, 0xEA, 0xA4, 0x5F, 0x7D, 0xEA, 0xA6, 0x1F, 0xFE, 0x72, 0x02, 0x7D, 0xD0, 0xB7, 0x14, 0xB3, 0x70, 0x06, 0x00, 0x0B, 0xC2, 0x61, 0x0F, 0x09, 0x5B, 0x92, 0xD4, 0xF3, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
static const struct raw_bitmap triangle_data =
  {
    .hotspot_x = 0,
    .hotspot_y = 15,
    .size = sizeof (triangle_bytes),
    .data = triangle_bytes,
  };
// clang-format on
