
#include "tools-internal.h"

static void draw_symmetric_freehand_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void motion_symmetric_freehand_handler (AppState *state, gint x, gint y);

const Tool global_symmetric_freehand_tool = {
  .type = TOOL_SYMMETRIC_FREEHAND,
  .icon = &freehand_data,
  .cursor = NULL,
  .draw_handler = draw_symmetric_freehand_handler,
  .motion_handler = motion_symmetric_freehand_handler,
  .is_drawing = TRUE,
};

static void
draw_symmetric_freehand_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  handle_pixel (state->preview_surface, x1, y1, state->p_color);

  int width = cairo_image_surface_get_width(state->main_surface);
  int height = cairo_image_surface_get_height(state->main_surface);

  handle_pixel (state->preview_surface, width - x1 - 1, height - y1 - 1, state->p_color);
}

static void
motion_symmetric_freehand_handler (AppState *state, gint x, gint y)
{
  draw_line_with_width_and_color (state->preview_surface, state->last_point.x, state->last_point.y, x, y, 1.0, state->p_color);

  int width = cairo_image_surface_get_width(state->main_surface);
  int height = cairo_image_surface_get_height(state->main_surface);

  draw_line_with_width_and_color (state->preview_surface, width - state->last_point.x - 1, height - state->last_point.y - 1, width - x - 1, height - y - 1, 1.0, state->p_color);

  state->last_point.x = x;
  state->last_point.y = y;
}
