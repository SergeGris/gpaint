
#include "tools-internal.h"

static void draw_freehand_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void motion_freehand_handler (AppState *state, gint x, gint y);

const Tool global_freehand_tool = {
  .type = TOOL_FREEHAND,
  .icon = &freehand_data,
  .cursor = NULL,
  .draw_handler = draw_freehand_handler,
  .motion_handler = motion_freehand_handler,
  .is_drawing = TRUE,
};

static void
draw_freehand_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  handle_pixel (state->preview_surface, x1, y1, state->p_color);
}

static void
motion_freehand_handler (AppState *state, gint x, gint y)
{
  draw_line_with_width_and_color (state->preview_surface, state->last_point.x, state->last_point.y, x, y, 1.0, state->p_color);
  state->last_point.x = x;
  state->last_point.y = y;
}
