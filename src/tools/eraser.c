
#include "tools-internal.h"

static void draw_eraser_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void motion_eraser_handler (AppState *state, gint x, gint y);
static void draw_eraser_cursor (AppState *state, cairo_t *cr);

const Tool global_eraser_tool = {
  .type = TOOL_ERASER,
  .icon = &eraser_data,
  .cursor_name = NULL,
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
  draw_line_with_width_and_color (state->preview_surface, state->last_point.x, state->last_point.y, x, y, state->eraser_size, &state->secondary_color, state->antialiasing);
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
  /* gdouble x = floor (state->cursor_x / pixel_size) * pixel_size + 0.5 - size
   * / 2; */
  /* gdouble y = floor (state->cursor_y / pixel_size) * pixel_size + 0.5 - size
   * / 2; */

  // TODO
  // TODO draw transparent differently
  // TODO
  double x = floor (state->cursor_x / pixel_size) * pixel_size;
  double y = floor (state->cursor_y / pixel_size) * pixel_size;
  cairo_save (cr);
  draw_colored_square (cr, &state->secondary_color, x + 0.5 - state->eraser_size / 2, y + 0.5 - state->eraser_size / 2, state->eraser_size, state->eraser_size, pixel_size);
  cairo_restore (cr);

  /* gdk_cairo_set_source_rgba (cr, &state->secondary_color); */
  /* cairo_rectangle (cr, x, y, size, size); */
  /* cairo_fill (cr); */

  cairo_save (cr);
  cairo_set_line_width (cr, 0.9);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_rectangle (cr, x + 0.5 - size / 2, y + 0.5 - size / 2, size, size);
  cairo_stroke (cr);
  cairo_restore (cr);

  // draw_eraser (state->cursor_surface, &state->secondary_color, x0, y0,
  // state->eraser_size);

  /* cairo_t *cr = create_cairo (state->cursor_surface, CAIRO_OPERATOR_SOURCE);
   */
  /*   /\* Set anti-aliasing to subpixel for smooth rendering of thin lines *\/
   */
  /* cairo_save(cr); */
  /*   cairo_set_antialias(cr, CAIRO_ANTIALIAS_SUBPIXEL); */

  /*   /\* Set the line width to 0.5 (i.e. half a pixel) *\/ */
  /*   cairo_set_line_width(cr, 0.5); */

  /*   /\* Set drawing color to black *\/ */
  /*   cairo_set_source_rgb(cr, 0, 0.0, 0); */

  /*   const gdouble pixel_size = state->zoom_level; */
  /*   const gdouble size = state->eraser_size; */

  /*   /\* Draw a rectangle at (20,20) with width and height of 0.5 pixels each
   * *\/ */
  /*   cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size,
   * size); */
  /*   //cairo_rectangle(cr, 20, 20, 1.5 * pixel_size, 1.5 *
   * state->pixel_size);
   */
  /*   cairo_stroke(cr); */
}

// clang-format off
static const guchar eraser_bytes[] =
  {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0xF3, 0xFF, 0x61, 0x00, 0x00, 0x00, 0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x5D, 0x49, 0x44, 0x41, 0x54, 0x38, 0xCB, 0x63, 0x60, 0x18, 0x05, 0x14, 0x03, 0x46, 0x02, 0xF2, 0xFF, 0x09, 0xE9, 0xC3, 0x67, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0x43, 0x95, 0xA0, 0xD2, 0x8C, 0x8C, 0x08, 0xBD, 0x4C, 0x64, 0x6A, 0x86, 0x03, 0x26, 0x52, 0x35, 0xFF, 0xFF, 0x8F, 0xDF, 0x00, 0x92, 0x34, 0xA3, 0x87, 0x01, 0x51, 0x9A, 0x91, 0xFD, 0x8F, 0xCC, 0x20, 0x4B, 0x33, 0x8C, 0xF3, 0xFF, 0xDE, 0xDD, 0x5B, 0x58, 0x43, 0x52, 0x49, 0x59, 0x0D, 0xAF, 0x66, 0x7C, 0xB1, 0x80, 0xEA, 0x4F, 0x46, 0xDC, 0x51, 0xCE, 0x44, 0x89, 0x66, 0x94, 0x30, 0x20, 0x37, 0xB5, 0x02, 0x00, 0xB3, 0xE3, 0x34, 0x10, 0xE2, 0x65, 0x05, 0xC5, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
const struct raw_bitmap eraser_data =
  {
    .hotspot_x = 0,
    .hotspot_y = 15,
    .size = sizeof (eraser_bytes),
    .data = eraser_bytes,
  };
// clang-format on
