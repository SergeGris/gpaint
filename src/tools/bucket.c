
#include "tools-internal.h"

static const struct raw_bitmap bucket_data;
static void draw_bucket_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);

// Do nothing on motion.
static void
motion_handler (AppState *state, gint x, gint y)
{
  return;
}

const Tool global_bucket_tool = {
  .type = TOOL_BUCKET,
  .icon = &bucket_data,
  .cursor_name = NULL,
  .draw_handler = draw_bucket_handler,
  .motion_handler = motion_handler,
  .override_main_surface = true,
  .is_drawing = TRUE,
};

static void
draw_bucket_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{ // TODO... Ensure it is the most optimal solution
  gint width = cairo_image_surface_get_width (state->preview_surface);
  gint height = cairo_image_surface_get_height (state->preview_surface);

  if (x1 < 0 || x1 >= width || y1 < 0 || y1 >= height)
    return;

  // TODO
  /* copy_surface (state->preview_surface, state->main_surface); */

  /* const guchar *main_data = cairo_image_surface_get_data
   * (state->main_surface); */
  guchar *preview_data = cairo_image_surface_get_data (state->preview_surface);
  gint stride = cairo_image_surface_get_stride (state->preview_surface);

  const guint32 target_color = get_pix_clr (preview_data, x1, y1, stride);

  guint32 color = gdk_rgba_to_clr (state->p_color);

  // TODO: if do not check it goes to infinite cycle
  if (target_color == color)
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

      guint32 current_color = get_pix_clr (preview_data, x, y, stride);

      if (current_color != target_color)
        continue;

      set_pix_clr (preview_data, x, y, stride, color);

      const Point neighbors[] = {
        { x - 1, y     },
        { x + 1, y     },
        { x,     y - 1 },
        { x,     y + 1 }
      };

      for (size_t i = 0; i < G_N_ELEMENTS (neighbors); i++)
        {
          gint nx = neighbors[i].x;
          gint ny = neighbors[i].y;

          if (nx >= 0 && nx < width && ny >= 0 && ny < height)
            {
              const guint32 neighbor_color = get_pix_clr (preview_data, nx, ny, stride);

              if (neighbor_color != target_color)
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

// clang-format off
static const guchar bucket_bytes[] =
  {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0xF3, 0xFF, 0x61, 0x00, 0x00, 0x00, 0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x8D, 0x49, 0x44, 0x41, 0x54, 0x38, 0xCB, 0xD5, 0x92, 0xC1, 0x0D, 0x80, 0x20, 0x0C, 0x45, 0x7F, 0x99, 0x83, 0x93, 0x26, 0x6E, 0xE0, 0x76, 0x1E, 0xDD, 0xCE, 0x0D, 0x4C, 0xF4, 0xE4, 0x1E, 0xDF, 0x8B, 0x10, 0xC0, 0x42, 0x89, 0x37, 0x9B, 0x70, 0x29, 0x7D, 0xF4, 0x01, 0x05, 0xFE, 0x10, 0x7C, 0x96, 0x1A, 0xCE, 0x82, 0x49, 0x36, 0x0B, 0xA4, 0xE8, 0x94, 0xE6, 0x33, 0x58, 0x44, 0x34, 0x26, 0x1A, 0xC4, 0xCA, 0xF3, 0xD8, 0xA1, 0xC1, 0x24, 0xF1, 0xE4, 0x58, 0x1A, 0x64, 0xF0, 0x30, 0x4E, 0x98, 0xD7, 0x4B, 0xD5, 0xDD, 0x16, 0xFF, 0xB2, 0x77, 0x1F, 0xE1, 0x68, 0xE2, 0x6A, 0x85, 0x29, 0xA0, 0xC0, 0xA1, 0x11, 0x45, 0x79, 0x3C, 0x00, 0x60, 0x30, 0x69, 0xC0, 0xD8, 0x16, 0xDF, 0xFE, 0x46, 0x0B, 0x06, 0x20, 0x35, 0x03, 0x68, 0xC3, 0x53, 0xC2, 0xD6, 0x20, 0x89, 0x05, 0x87, 0x03, 0xC4, 0x3A, 0xA4, 0x06, 0x77, 0xCF, 0x7B, 0xC7, 0xBE, 0x7E, 0xE7, 0x9E, 0xB8, 0x01, 0x11, 0xB0, 0x4A, 0xC0, 0x9E, 0x08, 0xA7, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
static const struct raw_bitmap bucket_data =
  {
    .hotspot_x = 0,
    .hotspot_y = 15,
    .size = sizeof (bucket_bytes),
    .data = bucket_bytes,
  };
// clang-format on
