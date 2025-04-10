

#include "tools-internal.h"

static void draw_picker_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void motion_handler (AppState *state, gint x, gint y);

const Tool global_picker_tool = {
  .type = TOOL_PICKER,
  .icon = &picker_data,
  .cursor_icon = &picker_cursor_data,
  .cursor = NULL,
  .draw_handler = draw_picker_handler,
  .motion_handler = motion_handler,
  .is_drawing = FALSE,
};

static void
handle (AppState *state, gint x, gint y)
{
  if (cairo_surface_get_type (state->main_surface) != CAIRO_SURFACE_TYPE_IMAGE)
    return;

  gint width = cairo_image_surface_get_width (state->main_surface);
  gint height = cairo_image_surface_get_height (state->main_surface);

  if (x < 0 || x >= width || y < 0 || y >= height)
    return;

  gint stride = cairo_image_surface_get_stride (state->main_surface);
  const guchar *data = cairo_image_surface_get_data (state->main_surface);
  *state->p_color = get_pixel_color (data, x, y, stride);
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), state->p_color);
}

static void
draw_picker_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  handle (state, x1, y1);
}

static void
motion_handler (AppState *state, gint x, gint y)
{
  handle (state, x, y);
}
