
#include "tools-internal.h"

static void draw_line_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_line_tool = {
  .type = TOOL_LINE,
  .icon = &line_data,
  .cursor = "crosshair",
  .draw_handler = draw_line_handler,
  .motion_handler = NULL,
  .is_drawing = TRUE,
};

static void
draw_line_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_line_with_width_and_color (state->preview_surface, x0, y0, x1, y1,
                                  state->width, state->p_color, state->antialiasing);
}
