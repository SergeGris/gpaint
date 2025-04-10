
#include "tools-internal.h"

static void draw_bucket_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void
motion_handler (AppState *state, gint x, gint y)
{
} // Do nothing on motion.

const Tool global_bucket_tool = {
  .type = TOOL_BUCKET,
  .icon = &bucket_data,
  .cursor = NULL,
  .draw_handler = draw_bucket_handler,
  .motion_handler = motion_handler,
  .override_main_surface = true,
  .is_drawing = TRUE,
};

static gboolean
rgba_equal (const GdkRGBA *rgba1, const GdkRGBA *rgba2)
{
  guint8 r1 = (guint8) (rgba1->red * 255.0f);
  guint8 g1 = (guint8) (rgba1->green * 255.0f);
  guint8 b1 = (guint8) (rgba1->blue * 255.0f);
  guint8 a1 = (guint8) (rgba1->alpha * 255.0f);

  guint8 r2 = (guint8) (rgba2->red * 255.0f);
  guint8 g2 = (guint8) (rgba2->green * 255.0f);
  guint8 b2 = (guint8) (rgba2->blue * 255.0f);
  guint8 a2 = (guint8) (rgba2->alpha * 255.0f);

  return r1 == r2 && g1 == g2 && b1 == b2 && a1 == a2;
}

static void
draw_bucket_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{ // TODO... Ensure it is the most optimal solution
  gint width = cairo_image_surface_get_width (state->preview_surface);
  gint height = cairo_image_surface_get_height (state->preview_surface);

  if (x1 < 0 || x1 >= width || y1 < 0 || y1 >= height)
    return;

  // TODO
  /* copy_surface (state->preview_surface, state->main_surface); */

  /* const guchar *main_data = cairo_image_surface_get_data (state->main_surface); */
  guchar *preview_data = cairo_image_surface_get_data (state->preview_surface);
  gint stride = cairo_image_surface_get_stride (state->preview_surface);

  const GdkRGBA target_color = get_pixel_color (preview_data, x1, y1, stride);

  // TODO: if do not check it goes to infinite cycle
  if (rgba_equal (&target_color, state->p_color))
    return;

  GQueue *queue = g_queue_new ();
  Point *start = g_new (Point, 1);
  *start = (Point) { x1, y1 };
  g_queue_push_tail (queue, start);

  while (!g_queue_is_empty (queue))
    {
      Point *current = (Point *) g_queue_pop_head (queue);
      gint x = current->x;
      gint y = current->y;
      g_free (current);

      if (x < 0 || x >= width || y < 0 || y >= height)
        continue;

      GdkRGBA current_color = get_pixel_color (preview_data, x, y, stride);

      if (!rgba_equal (&current_color, &target_color))
        continue;

      set_pixel_color (preview_data, x, y, stride, state->p_color);

      const Point neighbors[] = { { x - 1, y }, { x + 1, y }, { x, y - 1 }, { x, y + 1 } };

      for (size_t i = 0; i < G_N_ELEMENTS (neighbors); i++)
        {
          gint nx = neighbors[i].x;
          gint ny = neighbors[i].y;

          if (nx >= 0 && nx < width && ny >= 0 && ny < height)
            {
              const GdkRGBA neighbor_color = get_pixel_color (preview_data, nx, ny, stride);

              if (!rgba_equal (&neighbor_color, &target_color))
                continue;

              Point *np = g_new (Point, 1);
              *np = (Point) { nx, ny };
              g_queue_push_tail (queue, np);
            }
        }
    }

  g_queue_free (queue);
  cairo_surface_mark_dirty (state->preview_surface);
  gtk_widget_queue_draw (state->drawing_area);
}
