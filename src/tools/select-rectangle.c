

#include "tools-internal.h"

static void draw_select_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_select_rectangle_tool = {
  .type = TOOL_SELECT_RECTANGLE,
  .icon = &select_data,
  .cursor = "crosshair",
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

  /* cairo_t *cr = create_cairo (state->select_surface, CAIRO_OPERATOR_SOURCE); */
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
