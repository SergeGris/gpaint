

#include "tools-internal.h"

static const struct raw_bitmap select_data;
static void draw_select_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_select_rectangle_tool = {
  .type = TOOL_SELECT_RECTANGLE,
  .icon = &select_data,
  .cursor_name = "crosshair",
  .draw_handler = draw_select_rectangle_handler,
  .motion_handler = NULL,
  .override_main_surface = false,
};

static void
draw_select_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // Calculate the rectangle bounds
  gint min_x = MIN (x0, x1);
  gint min_y = MIN (y0, y1);
  gint dx = abs (x1 - x0);
  gint dy = abs (y1 - y0);

  /* cairo_t *cr = create_cairo (state->select_surface, CAIRO_OPERATOR_SOURCE);
   */
  /* clear_canvas (state, state->select_surface); */
  /* cairo_rectangle (cr, min_x + 0.5, min_y + 0.5, dx, dy); */
  /* double dash[] = { 1.0, 2.0 };    // Длина штриха и пробела */
  /* cairo_set_dash (cr, dash, 2, 0); // Устанавливаем пунктирный стиль */

  /* cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE); */
  /* cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER); */
  /* cairo_set_line_width (cr, state->width); */
  /* gdk_cairo_set_source_rgba (cr, state->p_color); */

  GdkRectangle selected_rect = {
    .x = min_x,
    .y = min_y,
    .width = dx,
    .height = dy,
  };

  state->selected_rect = selected_rect;

  /* cairo_stroke (cr); */
  /* cairo_destroy (cr); */
}

// clang-format off
static const guchar select_bytes[] =
  {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0xF3, 0xFF, 0x61, 0x00, 0x00, 0x00, 0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x26, 0x49, 0x44, 0x41, 0x54, 0x38, 0xCB, 0x63, 0x60, 0x18, 0x16, 0xE0, 0x3F, 0x12, 0x4D, 0x2A, 0x1B, 0x95, 0x41, 0xA6, 0xC5, 0xD4, 0xF3, 0x02, 0x59, 0xFA, 0x98, 0x06, 0x55, 0x2C, 0x8C, 0x7A, 0x81, 0xF4, 0x94, 0x38, 0x48, 0x00, 0x00, 0x77, 0xA4, 0x19, 0xED, 0x81, 0x0F, 0x80, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
static const struct raw_bitmap select_data =
  {
    .hotspot_x = 0,
    .hotspot_y = 15,
    .size = sizeof (select_bytes),
    .data = select_bytes,
  };
// clang-format on
