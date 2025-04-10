

#include "tools-internal.h"

static void draw_picker_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

const Tool global_picker_tool = {
  .type = TOOL_ERASER,
  .icon = &picker_data,
  .cursor_icon = &picker_cursor_data,
  .cursor = NULL,
  .draw_handler = draw_picker_handler,
  .motion_handler = NULL,
};

static void
draw_picker_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  if (cairo_surface_get_type (state->main_surface) != CAIRO_SURFACE_TYPE_IMAGE)
    return;

  gint width = cairo_image_surface_get_width (state->main_surface);
  gint height = cairo_image_surface_get_height (state->main_surface);

  if (x1 < 0 || x1 >= width || y1 < 0 || y1 >= height)
    return;

  gint stride = cairo_image_surface_get_stride (state->main_surface);
  const guchar *data = cairo_image_surface_get_data (state->main_surface);
  state->primary_color = get_pixel_color (data, x1, y1, stride);

  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), state->p_color);
}
