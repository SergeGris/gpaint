#include <gtk/gtk.h>
#include <adwaita.h>

#include "color-swap-button.h"

typedef struct Tool Tool;

typedef struct
{
  gint x;
  gint y;
} Point;

typedef enum
{
  FILL_TRANSPARENT,
  FILL_PRIMARY,
  FILL_SECONDARY,
} FillType;

typedef struct
{
  GtkWidget *window;
  cairo_surface_t *main_surface;
  cairo_surface_t *preview_surface;
  cairo_surface_t *cursor_surface;
  GdkRGBA primary_color;
  GdkRGBA secondary_color;
  gboolean is_drawing;
  gdouble zoom_level;
  Tool *primary_tool;
  Tool *previous_tool;
  gboolean show_grid;
  Point start_point;
  Point last_point;
  GtkWidget *drawing_area;
  GtkWidget *color_btn;
  GtkWidget *zoom_label;
  GtkWidget *grid_toggle;
  GtkWidget *file_toolbar;
  GtkWidget *status_bar;
  GtkWidget *width_selector;
  GtkWidget *fill_selector;
  GtkWidget *eraser_size_selector;
  GSimpleActionGroup *action_group;
  gdouble width;
  FillType fill_type;
  gdouble eraser_size;
  gdouble brush_size;
} AppState;

typedef enum
{
  TOOL_FREEHAND,
  TOOL_BRUSH,
  TOOL_LINE,
  TOOL_RECTANGLE,
  TOOL_ELLIPSE,
  TOOL_BUCKET,
  TOOL_ERASER
} ToolType;

struct raw_bitmap
{
  gint height, width, stride;
  gboolean has_alpha;
  GdkColorspace colorspace;
  gint bits_per_sample;
  const guchar data[1024];
};

struct Tool
{
  ToolType type;
  const struct raw_bitmap *icon;
  const gchar *cursor;
  void (*draw_handler) (AppState *state, gint x0, gint y0, gint x1, gint y1);
  void (*motion_handler) (AppState *state, gint x, gint y);
  gboolean use_width;
  gboolean use_fill;
  void (*cursor_draw_handler) (AppState *state, gint x0, gint y0);
};

static void update_status_bar (AppState *state, gint x, gint y);

static cairo_t *
create_cairo (cairo_surface_t *surface, cairo_operator_t operator)
{
  cairo_t *cr = cairo_create (surface);
  cairo_set_operator (cr, operator);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  return cr;
}

static void
motion_handler (GtkEventControllerMotion *ctrl, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = user_data;

  gint px = (gint) (x / state->zoom_level);
  gint py = (gint) (y / state->zoom_level);

  update_status_bar (state, px, py);
  if (state->cursor_surface && state->primary_tool->cursor_draw_handler)
    {
      cairo_t *cr = create_cairo (state->cursor_surface, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);
      state->primary_tool->cursor_draw_handler (state, px, py);
      cairo_destroy (cr);

      gtk_widget_queue_draw (state->drawing_area);
    }

  if (!state->is_drawing)
    return;

  if (state->preview_surface)
    {
      cairo_t *cr = cairo_create (state->preview_surface);

      if (state->primary_tool->motion_handler == NULL && state->primary_tool->type != TOOL_BUCKET)
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);
      // Let the tool draw its preview into preview_surface:
      if (state->primary_tool->motion_handler == NULL)
        {
          if (state->primary_tool->type != TOOL_BUCKET) // TODO
            {
              state->primary_tool->draw_handler (state, state->start_point.x, state->start_point.y, px, py);
            }
        }
      else
        {
          cairo_set_operator (cr, CAIRO_OPERATOR_OVER); // Используем оператор наложения
          state->primary_tool->motion_handler (state, px, py);
        }
      cairo_destroy (cr);
    }

  gtk_widget_queue_draw (state->drawing_area);
}

static void
handle_pixel (AppState *state, gint x, gint y, const GdkRGBA *color)
{
  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cr, color);
  cairo_rectangle (cr, x, y, 1, 1);
  cairo_fill (cr);
  cairo_destroy (cr);
}

static void // TODO rename
draw_line_with_width_and_color (AppState *state, gint x0, gint y0, gint x1, gint y1, gdouble width, const GdkRGBA *color)
{
  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);
  cairo_set_line_width (cr, width);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
  cairo_move_to (cr, x0 + 0.5, y0 + 0.5);
  cairo_line_to (cr, x1 + 0.5, y1 + 0.5);
  gdk_cairo_set_source_rgba (cr, color);
  cairo_stroke (cr);
  cairo_destroy (cr);
}

static void
draw_freehand_line (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_line_with_width_and_color (state, x0, y0, x1, y1, 1.0, &state->primary_color);
}

static void
draw_brush_line (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_line_with_width_and_color (state, x0, y0, x1, y1, state->brush_size, &state->primary_color);
}

static void
draw_line (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_line_with_width_and_color (state, x0, y0, x1, y1, state->width, &state->primary_color);
}

static void
draw_rectangle (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // Calculate the rectangle bounds
  gint min_x = MIN (x0, x1);
  gint min_y = MIN (y0, y1);
  gint dx = abs (x1 - x0);
  gint dy = abs (y1 - y0);

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);

  // Draw the outer rectangle (border) with primary color
  cairo_rectangle (cr, min_x + 0.5, min_y + 0.5, dx, dy);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
  cairo_set_line_width (cr, state->width);
  gdk_cairo_set_source_rgba (cr, &state->primary_color);

  if (state->fill_type == FILL_TRANSPARENT || state->fill_type == FILL_SECONDARY)
    cairo_stroke (cr);
  else
    cairo_fill (cr);

  // If fill type is secondary, fill the inner rectangle.
  if (state->fill_type == FILL_SECONDARY)
    {
      // Compute the inset offset (half the line width on each side)
      gdouble half_width = state->width / 2.0;
      // The inner rectangle is inset by half_width on all sides.
      gdouble fill_x = min_x + half_width + 0.5;
      gdouble fill_y = min_y + half_width + 0.5;
      gdouble fill_w = dx - state->width;
      gdouble fill_h = dy - state->width;

      // Ensure the fill dimensions are positive.
      if (fill_w > 0 && fill_h > 0)
        {
          cairo_rectangle (cr, fill_x, fill_y, fill_w, fill_h);
          gdk_cairo_set_source_rgba (cr, &state->secondary_color);
          cairo_fill (cr);
        }
    }

  cairo_destroy (cr);
}

static void
draw_ellipse (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // Compute the bounding box and ellipse center
  gint min_x = MIN (x0, x1);
  gint min_y = MIN (y0, y1);
  gint dx = abs (x1 - x0);
  gint dy = abs (y1 - y0);
  gdouble cx = min_x + dx / 2.0;
  gdouble cy = min_y + dy / 2.0;
  gdouble rx = dx / 2.0;
  gdouble ry = dy / 2.0;

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);

  // Draw the outer ellipse (the stroke)
  cairo_save (cr);
  cairo_translate (cr, cx, cy);
  cairo_scale (cr, rx, ry);
  cairo_arc (cr, 0, 0, 1.0, 0, 2 * G_PI);
  cairo_restore (cr);
  cairo_set_line_width (cr, state->width);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
  gdk_cairo_set_source_rgba (cr, &state->primary_color);

  if (state->fill_type == FILL_TRANSPARENT || state->fill_type == FILL_SECONDARY)
    cairo_stroke (cr);
  else
    cairo_fill (cr);

  // Draw the inner ellipse if fill type is secondary
  if (state->fill_type == FILL_SECONDARY)
    {
      // Inset the inner ellipse by half the stroke width on all sides
      gdouble inset = state->width / 2.0;
      gdouble inner_rx = rx - inset;
      gdouble inner_ry = ry - inset;

      // Only draw if the inner ellipse is valid
      if (inner_rx > 0 && inner_ry > 0)
        {
          cairo_save (cr);
          cairo_translate (cr, cx, cy);
          cairo_scale (cr, inner_rx, inner_ry);
          cairo_arc (cr, 0, 0, 1.0, 0, 2 * G_PI);
          cairo_restore (cr);
          gdk_cairo_set_source_rgba (cr, &state->secondary_color);
          cairo_fill (cr);
        }
    }

  cairo_destroy (cr);
}

static void
draw_freehand_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  /* For freehand, simply plot the pixel at the new coordinate */
  handle_pixel (state, x1, y1, &state->primary_color);
}
static void
motion_freehand_handler (AppState *state, gint x, gint y)
{
  draw_freehand_line (state, state->last_point.x, state->last_point.y, x, y);
  state->last_point.x = x;
  state->last_point.y = y;
}
static void
draw_brush_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  // TODO
  /* For brush, simply plot the pixel at the new coordinate */
  //handle_pixel (state, x1, y1, &state->primary_color);

  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cr, &state->primary_color);
  gdouble size = state->brush_size;
  cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size, size);
  cairo_fill (cr);
  cairo_destroy (cr);
}

static void
draw_brush_cursor (AppState *state, gint x0, gint y0)
{
  cairo_t *cr = create_cairo (state->cursor_surface, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cr, &state->primary_color);
  gdouble size = state->brush_size;
  cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size, size);
  cairo_fill (cr);
  cairo_destroy (cr);
}

static void
motion_brush_handler (AppState *state, gint x, gint y)
{
  draw_brush_line (state, state->last_point.x, state->last_point.y, x, y);
  state->last_point.x = x;
  state->last_point.y = y;
}
static void
draw_eraser_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  cairo_t *cr = create_cairo (state->preview_surface, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cr, &state->secondary_color);
  gdouble size = state->eraser_size;
  cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size, size);
  //cairo_rectangle (cr, x1, y1, state->eraser_size, state->eraser_size);
  cairo_fill (cr);
  cairo_destroy (cr);
  //handle_pixel (state, x1, y1, &state->secondary_color);
}
static void
motion_eraser_handler (AppState *state, gint x, gint y)
{
  draw_line_with_width_and_color (state, state->last_point.x, state->last_point.y, x, y, state->eraser_size, &state->secondary_color);
  state->last_point.x = x;
  state->last_point.y = y;
}
static void
draw_line_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_line (state, x0, y0, x1, y1);
}
static void
draw_rectangle_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_rectangle (state, x0, y0, x1, y1);
}
static void
draw_ellipse_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
  draw_ellipse (state, x0, y0, x1, y1);
}

static GdkRGBA
get_pixel_color (const guchar *data, gint x, gint y, gint stride)
{
  /* CAIRO_FORMAT_ARGB32 stores pixels in BGRA order on little-endian systems */
  gint idx = y * stride + x * 4;
  GdkRGBA color =
    {
      .red = (gfloat) (data[idx + 2] / 255.0f),
      .green = (gfloat) (data[idx + 1] / 255.0f),
      .blue = (gfloat) (data[idx + 0] / 255.0f),
      .alpha = (gfloat) (data[idx + 3] / 255.0f),
    };
  return color;
}

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

  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), &state->primary_color);
}

static void
set_pixel_color (guchar *data, gint x, gint y, int stride, GdkRGBA *color)
{
  int idx = y * stride + x * 4;
  data[idx + 0] = (guchar) (color->blue * 255.0f);
  data[idx + 1] = (guchar) (color->green * 255.0f);
  data[idx + 2] = (guchar) (color->red * 255.0f);
  data[idx + 3] = (guchar) (color->alpha * 255.0f);
}

static gboolean
rgba_equal (const GdkRGBA *rgba1, const GdkRGBA *rgba2)
{
  guchar r1 = (guchar) (rgba1->red * 255.0f);
  guchar g1 = (guchar) (rgba1->green * 255.0f);
  guchar b1 = (guchar) (rgba1->blue * 255.0f);
  guchar a1 = (guchar) (rgba1->alpha * 255.0f);

  guchar r2 = (guchar) (rgba2->red * 255.0f);
  guchar g2 = (guchar) (rgba2->green * 255.0f);
  guchar b2 = (guchar) (rgba2->blue * 255.0f);
  guchar a2 = (guchar) (rgba2->alpha * 255.0f);

  return r1 == r2 && g1 == g2 && b1 == b2 && a1 == a2;
}

static void
draw_bucket_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{ // TODO... Ensure it is the most optimal solution
  gint width = cairo_image_surface_get_width (state->preview_surface);
  gint height = cairo_image_surface_get_height (state->preview_surface);

  if (x1 < 0 || x1 >= width || y1 < 0 || y1 >= height)
    return;

void copy_surface(cairo_surface_t *src, cairo_surface_t *dst) {
    // Create a context for the destination surface.
    cairo_t *cr = cairo_create(dst);

    // Set the source surface as the source pattern.
    // The x and y offsets determine where the source is painted on the destination.
    cairo_set_source_surface(cr, src, 0, 0);

    // Paint copies the current source everywhere in the destination.
    cairo_paint(cr);

    // Clean up the context to free resources.
    cairo_destroy(cr);
}

 copy_surface(state->main_surface, state->preview_surface);

  /* const guchar *main_data = cairo_image_surface_get_data (state->main_surface); */
  guchar *preview_data = cairo_image_surface_get_data (state->preview_surface);
  gint stride = cairo_image_surface_get_stride (state->preview_surface);

  const GdkRGBA target_color = get_pixel_color (preview_data, x1, y1, stride);

  // TODO: if do not check it goes to infinite cycle
  if (rgba_equal (&target_color, &state->primary_color))
    return;

  GQueue *queue = g_queue_new ();
  Point *start = g_new (Point, 1);
  *start = (Point) { x1, y1 };
  g_queue_push_tail (queue, start);

  while (!g_queue_is_empty (queue))
    {
      Point *current = g_queue_pop_head (queue);
      gint x = current->x;
      gint y = current->y;
      g_free (current);

      if (x < 0 || x >= width || y < 0 || y >= height)
        continue;

      GdkRGBA current_color = get_pixel_color (preview_data, x, y, stride);

      if (!rgba_equal (&current_color, &target_color))
        continue;

      set_pixel_color (preview_data, x, y, stride, &state->primary_color);

      // Check and add neighbors
      const Point neighbors[] = { { x - 1, y }, { x + 1, y }, { x, y - 1 }, { x, y + 1 } };

      for (size_t i = 0; i < G_N_ELEMENTS (neighbors); i++)
        {
          gint nx = neighbors[i].x;
          gint ny = neighbors[i].y;

          if (nx >= 0 && nx < width && ny >= 0 && ny < height)
            {
              GdkRGBA neighbor_color = get_pixel_color (preview_data, nx, ny, stride);

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

static void
draw_colored_square (AppState *state, gint x0, gint y0)
{
  cairo_t *cr = create_cairo (state->cursor_surface, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cr, &state->secondary_color);
  gdouble size = state->eraser_size;
  cairo_rectangle (cr, x0 + 0.5 - size / 2, y0 + 0.5 - size / 2, size, size);
  cairo_fill (cr);
  cairo_destroy (cr);
}

#include "tools-icons.c"

static Tool global_freehand_tool = {
  .type = TOOL_FREEHAND,
  .icon = &freehand_data,
  .cursor = NULL,
  .draw_handler = draw_freehand_handler,
  .motion_handler = motion_freehand_handler,
  .use_width = false,
  .use_fill = false,
};
static Tool global_brush_tool = {
  .type = TOOL_BRUSH,
  .icon = &brush_data,
  .cursor = NULL,
  .draw_handler = draw_brush_handler,
  .motion_handler = motion_brush_handler,
  .use_width = false,
  .use_fill = false,
  .cursor_draw_handler = draw_brush_cursor,
};
static Tool global_line_tool = {
  .type = TOOL_LINE,
  .icon = &line_data,
  .cursor = "crosshair",
  .draw_handler = draw_line_handler,
  .motion_handler = NULL,
  .use_width = false,
  .use_fill = false,
};
static Tool global_rectangle_tool = {
  .type = TOOL_RECTANGLE,
  .icon = &rectangle_data,
  .cursor = "crosshair",
  .draw_handler = draw_rectangle_handler,
  .motion_handler = NULL,
  .use_width = false,
  .use_fill = false,
};
static Tool global_ellipse_tool = {
  .type = TOOL_ELLIPSE,
  .icon = &ellipse_data,
  .cursor = "crosshair",
  .draw_handler = draw_ellipse_handler,
  .motion_handler = NULL,
  .use_width = false,
  .use_fill = false,
};
static Tool global_eraser_tool = {
  .type = TOOL_ERASER,
  .icon = &eraser_data,
  .cursor = NULL,
  .draw_handler = draw_eraser_handler,
  .motion_handler = motion_eraser_handler,
  .use_width = false,
  .use_fill = false,
  .cursor_draw_handler = draw_colored_square,
};
static Tool global_picker_tool = {
  .type = TOOL_ERASER,
  .icon = &picker_data,
  .cursor = NULL,
  .draw_handler = draw_picker_handler,
  .motion_handler = NULL,
  .use_width = false,
  .use_fill = false,
};
static Tool global_bucket_tool = {
  .type = TOOL_BUCKET,
  .icon = &bucket_data,
  .cursor = NULL,
  .draw_handler = draw_bucket_handler,
  .motion_handler = NULL,
  .use_width = false,
  .use_fill = false,
};

static void on_new_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_open_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_save_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data);

typedef struct
{
  const gchar *label;
  const gchar *key;
  const gchar *short_key;
  const gchar *accel[2];
  GCallback callback;
} AppAction;

static const AppAction file_actions[] = {
  { "New", "app.new", "new", { "<Primary>n", NULL }, G_CALLBACK (on_new_file) },
  { "Open", "app.open", "open", { "<Primary>o", NULL }, G_CALLBACK (on_open_file) },
  { "Save", "app.save", "save", { "<Primary>s", NULL }, G_CALLBACK (on_save_file) },
  //{ "Save as", "app.saveas", "saveas", { "<Primary><Shift>s", NULL }, G_CALLBACK (on_save_as_file) },
  { "Quit", "app.quit", "quit", { "<Primary>q", NULL }, G_CALLBACK (on_quit) },
};

static cairo_surface_t *
create_surface (gint height, gint width)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, height, width);
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cr, &(GdkRGBA) { 0.0, 0.0, 0.0, 0.0 });
  cairo_paint (cr);
  cairo_destroy (cr);
  return surface;
}

static void
clear_canvas (AppState *state, cairo_surface_t *surface)
{
  cairo_t *cr = create_cairo (surface, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  gtk_widget_queue_draw (state->drawing_area);
}

static void
update_status_bar (AppState *state, gint x, gint y)
{
  cairo_surface_t *surface = state->main_surface;
  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_height (surface);

  // TODO undocumanted?
  cairo_format_t format = cairo_image_surface_get_format (surface);
  int color_depth = 0;

  switch (format)
    {
    case CAIRO_FORMAT_ARGB32:
      color_depth = 32; // 8 bits for each of 4 channels (Alpha, Red, Green, Blue)
      break;
    case CAIRO_FORMAT_RGB24:
      color_depth = 24; // 8 bits for each of 3 channels (Red, Green, Blue)
      break;
    case CAIRO_FORMAT_A8:
      color_depth = 8; // 8 bits for Alpha only
      break;
    case CAIRO_FORMAT_RGB16_565:
      color_depth = 16; // 5 bits for Red, 6 bits for Green, 5 bits for Blue
      break;
    case CAIRO_FORMAT_RGB30:
      color_depth = 30; // 10 bits for each of 3 channels (Red, Green, Blue)
      break;
    case CAIRO_FORMAT_A1:
      color_depth = 1;
      break;
    case CAIRO_FORMAT_RGB96F:   // TODO
    case CAIRO_FORMAT_RGBA128F: // TODO
    case CAIRO_FORMAT_INVALID:
    default:
      color_depth = 0;
      break;
    }

  gchar status_text[256];
  snprintf (status_text, sizeof (status_text), "%d×%d×%d [%d, %d]", width, height, color_depth, MIN (x, width - 1), MIN (y, height - 1));
  if (x < 0 || x >= width || y < 0 || y >= height)
    {
      // TODO
      clear_canvas (state, state->cursor_surface);
    }
  gtk_label_set_text (GTK_LABEL (state->status_bar), status_text);
}

static void
draw_callback (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  const gdouble pixel_size = state->zoom_level;
  gint surface_width = cairo_image_surface_get_width (state->main_surface);
  gint surface_height = cairo_image_surface_get_height (state->main_surface);
  cairo_pattern_t *pattern;

  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_save (cr);
  cairo_scale (cr, pixel_size, pixel_size);

  // Set a single color
  gdouble color = 0.9; // Light gray
  cairo_set_source_rgb (cr, color, color, color);

  // Fill the entire rectangle at once
  cairo_rectangle (cr, 0, 0, surface_width, surface_height);
  cairo_fill (cr);

  cairo_restore (cr);

  // VERY IMPORTANT! IT DRAWS EVERY PIXEL WITH HARD EDGES
  // Draw main content with nearest-neighbor filtering (hard-edged pixels)
  cairo_save (cr);
  cairo_scale (cr, pixel_size, pixel_size);
  pattern = cairo_pattern_create_for_surface (state->main_surface);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);
  cairo_set_source (cr, pattern);
  cairo_paint (cr);
  cairo_pattern_destroy (pattern);
  cairo_restore (cr);

  // TODO
  // Overlay preview layer if available:
  if (state->preview_surface)
    {
      cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
      cairo_save (cr);
      cairo_scale (cr, pixel_size, pixel_size);
      pattern = cairo_pattern_create_for_surface (state->preview_surface);
      cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);
      cairo_set_source (cr, pattern);
      // cairo_set_source_surface(cr, state->preview_surface, 0, 0);
      cairo_paint (cr);
      cairo_pattern_destroy (pattern);
      cairo_restore (cr);
    }

  // TODO
  // Overlay cursor layer if available:
  if (state->cursor_surface)
    {
      cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
      cairo_save (cr);
      cairo_scale (cr, pixel_size, pixel_size);
      pattern = cairo_pattern_create_for_surface (state->cursor_surface);
      cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);
      cairo_set_source (cr, pattern);
      // cairo_set_source_surface(cr, state->preview_surface, 0, 0);
      cairo_paint (cr);
      cairo_pattern_destroy (pattern);
      cairo_restore (cr);
    }

  if (state->show_grid && pixel_size >= 8.0)
    {
      cairo_save (cr);

      // Set the color and line width for the grid
      cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 1); // Light gray color
      cairo_set_line_width (cr, 1.0);               // Increased line width for visibility

      cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
      cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);

      // Draw vertical grid lines
      for (int x = 0; x <= surface_width; x++)
        {
          cairo_move_to (cr, x * pixel_size, 0);
          cairo_line_to (cr, x * pixel_size, surface_height * pixel_size);
        }

      // Draw horizontal grid lines
      for (int y = 0; y <= surface_height; y++)
        {
          cairo_move_to (cr, 0, y * pixel_size);
          cairo_line_to (cr, surface_width * pixel_size, y * pixel_size);
        }

      // Stroke the lines to render them
      cairo_stroke (cr);
      cairo_restore (cr);
    }
}

static GdkTexture *
create_texture_from_raw_data (gint height, gint width, const guchar *raw_data)
{
  gint rowstride = width * 4;
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data ((guchar *) raw_data, GDK_COLORSPACE_RGB, TRUE, 8, width, height, rowstride, NULL, NULL);
  if (!pixbuf)
    return NULL;
  GdkTexture *texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);
  return texture;
}

/* Updates cursor based on current tool – update per GTK4 docs if needed */
static void
update_cursor (AppState *state)
{
  g_autoptr (GdkCursor) cursor;

  if (state->primary_tool->cursor_draw_handler != NULL)
    {
      cursor = gdk_cursor_new_from_name ("crosshair", NULL);
    }
  else if (state->primary_tool->cursor != NULL)
    {
      cursor = gdk_cursor_new_from_name (state->primary_tool->cursor, NULL);

      if (!cursor)
        {
          g_warning ("Failed to create GdkCursor from name.");
          return;
        }
    }
  else
    {
      /* g_autoptr (GdkTexture) texture = gdk_texture_new_from_filename (state->primary_tool->icon, NULL); */

      /* if (!texture) */
      /*   { */
      /*     g_warning ("Failed to create GdkTexture from filename: %s", state->primary_tool->icon); */
      /*     return; */
      /*   } */

      GdkTexture *texture = create_texture_from_raw_data (state->primary_tool->icon->height, state->primary_tool->icon->width, state->primary_tool->icon->data);

      cursor = gdk_cursor_new_from_texture (texture, 0, 15, NULL);

      if (!cursor)
        {
          g_warning ("Failed to create GdkCursor from texture.");
          return;
        }
    }

  gtk_widget_set_cursor (state->drawing_area, cursor);
}

static void
on_click_pressed (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = user_data;
  gint px = (gint) (x / state->zoom_level);
  gint py = (gint) (y / state->zoom_level);

  state->is_drawing = TRUE;
  state->start_point.x = px;
  state->start_point.y = py;
  state->last_point = state->start_point;

  if (state->preview_surface)
    cairo_surface_destroy (state->preview_surface);

  state->preview_surface = create_surface (cairo_image_surface_get_width (state->main_surface), cairo_image_surface_get_height (state->main_surface));
  if (state->primary_tool->type == TOOL_FREEHAND || state->primary_tool->type == TOOL_ERASER || state->primary_tool->type == TOOL_BRUSH || state->primary_tool->type == TOOL_BUCKET) // TODO
    state->primary_tool->draw_handler (state, px, py, px, py);
}

static void
on_click_released (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = user_data;

  if (!state->is_drawing)
    return;

  // TODO
  if (state->preview_surface)
    {
      cairo_t *cr = create_cairo (state->main_surface, CAIRO_OPERATOR_OVER);
      cairo_set_source_surface (cr, state->preview_surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      cairo_surface_destroy (state->preview_surface);
      state->preview_surface = NULL;
    }

  state->is_drawing = FALSE;
  gtk_widget_queue_draw (state->drawing_area);
}

// TODO
static void
on_secondary (GtkGestureDrag *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = user_data;

  if (state->is_drawing && state->preview_surface)
    {
      cairo_surface_destroy (state->preview_surface);
      state->preview_surface = NULL;
      state->is_drawing = FALSE;
      gtk_widget_queue_draw (state->drawing_area);
    }
}

/* Scroll event handler for zooming */
static void
on_scroll (GtkEventControllerScroll *ctrl, gdouble dx, gdouble dy, AppState *state)
{
  GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (ctrl));
  GdkModifierType modifiers = gdk_event_get_modifier_state (event);
  if (modifiers & GDK_CONTROL_MASK)
    {
      gdouble factor = (dy < 0) ? 1.1 : 0.9;
      state->zoom_level = CLAMP (state->zoom_level * factor, 1.0, 64.0);
      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
      gchar label[1024];
      snprintf (label, sizeof (label), "Zoom: %.0f%%", state->zoom_level * 100.0);
      gtk_label_set_text (GTK_LABEL (state->zoom_label), label);
      gtk_widget_queue_draw (state->drawing_area);
    }

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_widget_queue_resize (state->drawing_area);
}

static void
on_color_changed (GtkColorDialogButton *btn, GParamSpec *pspec, gpointer user_data)
{
  AppState *state = user_data;
  state->primary_color = *gtk_color_dialog_button_get_rgba (btn);
}

static void
export_image (AppState *state, const gchar *filename)
{
  if (!filename)
    return;

  const gchar *ext = strrchr (filename, '.');

  if (ext && g_ascii_strcasecmp (ext, ".png") == 0)
    {
      cairo_status_t status = cairo_surface_write_to_png (state->main_surface, filename);
      if (status != CAIRO_STATUS_SUCCESS)
        g_warning ("Failed to save PNG image: %s", cairo_status_to_string (status));
      return;
    }

  /* For non-PNG formats, grab a GdkPixbuf from the surface and save it.
     This allows formats such as JPG, BMP, or GIF. */
  g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new_from_data (cairo_image_surface_get_data (state->main_surface),
                                                           GDK_COLORSPACE_RGB, // TODO colospace
                                                           TRUE,               // TODO has alpha
                                                           8,                  // TODO rowstride
                                                           cairo_image_surface_get_width (state->main_surface), cairo_image_surface_get_height (state->main_surface), cairo_image_surface_get_stride (state->main_surface), NULL, NULL);

  if (pixbuf)
    {
      GError *error = NULL;
      if (!gdk_pixbuf_save (pixbuf, filename, ext ? ext + 1 : "png", &error, NULL))
        {
          g_warning ("Failed to save image: %s", error->message);
          g_error_free (error);
        }
    }
}

static void
on_save_response (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  AppState *state = user_data;
  g_autoptr (GFile) file = gtk_file_dialog_save_finish (dialog, res, NULL);
  if (!file)
    return;

  g_autofree gchar *path = g_file_get_path (file);
  export_image (state, path);
  // TODO gtk_window_destroy (GTK_WINDOW (dialog));
}

// Change the on_save_file function to use the modern GTK4 file dialog:
static void
on_save_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = user_data;
  GtkFileDialog *dialog = GTK_FILE_DIALOG (gtk_file_dialog_new ());
  gtk_file_dialog_set_title (dialog, "Save image");
  gtk_file_dialog_set_modal (GTK_FILE_DIALOG (dialog), TRUE);
  gtk_file_dialog_set_initial_name (dialog, "image.png");
  gtk_file_dialog_save (dialog, GTK_WINDOW (state->window), NULL, on_save_response, state);
}

static void
on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = user_data;
  gtk_window_destroy (GTK_WINDOW (state->window));
}

static void
on_open_response (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  g_autoptr (GFile) file = gtk_file_dialog_open_finish (dialog, res, NULL);

  if (file == NULL)
    return;

  g_autofree gchar *path = g_file_get_path (file);

  /* Load the image into a new Cairo surface */
  cairo_surface_t *new_surface = cairo_image_surface_create_from_png (path);
  cairo_status_t status = cairo_surface_status (new_surface);

  if (status != CAIRO_STATUS_SUCCESS)
    {
      /* Handle the error (e.g., show a message to the user) */
      g_printerr ("Failed to load image: %s\n", cairo_status_to_string (status));
      cairo_surface_destroy (new_surface);
    }

  /* Get image dimensions */
  gint width = cairo_image_surface_get_width (new_surface);
  gint height = cairo_image_surface_get_height (new_surface);

  /* Replace the existing main_surface with the new image surface */
  if (state->main_surface)
    cairo_surface_destroy (state->main_surface);

  state->main_surface = new_surface;

  /* Update the drawing area size */
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (gint) (width * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (gint) (height * state->zoom_level));

  /* Request a redraw */
  gtk_widget_queue_draw (state->drawing_area);
}

// Change the on_open_file function to use the modern GTK4 file dialog:
static void
on_open_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = user_data;
  GtkFileDialog *dialog = GTK_FILE_DIALOG (gtk_file_dialog_new ());
  gtk_file_dialog_set_title (dialog, "Open image");
  gtk_file_dialog_set_modal (GTK_FILE_DIALOG (dialog), TRUE);
  gtk_file_dialog_open (dialog, GTK_WINDOW (state->window), NULL, on_open_response, state);
}

/* static void */
/* update_container_child (AppState *state, GtkWidget *new_child) */
/* { */
/*   GtkWidget *child = gtk_widget_get_first_child (state->info_widget); */
/*   while (child != NULL) */
/*     { */
/*       GtkWidget *next = gtk_widget_get_next_sibling (child); */
/*       gtk_widget_unparent (child); */
/*       child = next; */
/*     } */
/*   gtk_box_append (GTK_BOX (state->info_widget), new_child); */
/* } */

static void
tool_toggled (GtkToggleButton *btn, gpointer user_data)
{
  /* Only process if this button is activated */
  if (!gtk_toggle_button_get_active (btn))
    return;

  /* Get parent container (toolbar) and iterate its children */
  GtkWidget *toolbar = gtk_widget_get_parent (GTK_WIDGET (btn));

  for (GtkWidget *child = gtk_widget_get_first_child (toolbar); child != NULL; child = gtk_widget_get_next_sibling (child))
    {
      if (child != GTK_WIDGET (btn))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child), FALSE);
    }

  /* Update application state based on the activated button */
  AppState *state = user_data;
  state->primary_tool = g_object_get_data (G_OBJECT (btn), "tool");
  clear_canvas(state, state->cursor_surface);
  update_cursor (state);

  // TODO
  /* if (state->primary_tool == &global_eraser_tool) */
  /*   update_container_child (state, create_color_filled_widget (&state->primary_color)); */
}

static void on_resize_clicked (GtkButton *btn, AppState *state);

/* static void on_list_box_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) { */
/*   AppState *state = (AppState *) user_data; */
/*   /\* state->width = gtk_list_box_row_get_index (row) + 1; *\/ */
/*     int *val = g_object_get_data(G_OBJECT(row), "value"); */
/*     if (val) */
/*       state->width = GPOINTER_TO_INT(val); */

/* } */

/* static GtkWidget *width_widget (AppState *state) */
/* { */
/*   GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); */
/*   gtk_box_set_spacing(GTK_BOX(vbox), 8); // Ensure spacing is set */
/*   gtk_widget_set_margin_top(vbox, 2); */
/*   gtk_widget_set_margin_bottom(vbox, 2); */
/*   gtk_widget_set_margin_start(vbox, 8); */
/*   gtk_widget_set_margin_end(vbox, 8); */
/*   GtkWidget *list_box = gtk_list_box_new(); */
/*   gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_SINGLE); */

/*   /\* Connect the "row-selected" signal to detect changes *\/ */
/*   g_signal_connect(list_box, "row-selected", G_CALLBACK(on_list_box_row_selected), state); */

/*   /\* Programmatically select the first row *\/ */
/*   gtk_list_box_select_row(GTK_LIST_BOX(list_box), gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box), 0)); */

/*   for (int i = 0; i < 5; i++) { */
/*     GtkWidget *row = gtk_list_box_row_new(); */
/*     char buffer[32]; */
/*     snprintf(buffer, sizeof(buffer), "line%d.png", i + 1); */
/*     GtkWidget *image = gtk_image_new_from_file(buffer); */
/*     /\* Add spacing by setting bottom margin for each row *\/ */
/*     gtk_widget_set_margin_bottom(row, 4); */
/*     g_object_set_data_full(G_OBJECT(row), "value", GINT_TO_POINTER(i + 1), NULL); */
/*     gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), image); */
/*     gtk_list_box_append(GTK_LIST_BOX(list_box), row); */
/*   } */
/*   gtk_box_append(GTK_BOX(vbox), list_box); */
/*   return vbox; */
/* } */

#include "value-selector.c"

static ValueItem line_widths[] = {
  {
      "line1.png",
      1,
  },
  {
      "line2.png",
      2,
  },
  {
      "line3.png",
      3,
  },
  {
      "line4.png",
      4,
  },
  {
      "line5.png",
      5,
  },
};

static ValueItem fills[] = {
  {
      "fill1.png",
      FILL_TRANSPARENT,
  },
  {
      "fill2.png",
      FILL_SECONDARY,
  },
  {
      "fill3.png",
      FILL_PRIMARY,
  },
};

static ValueItem eraser_sizes[] = {
  {
      "fill1.png",
      1,
  },
  {
      "fill2.png",
      2,
  },
  {
      "fill3.png",
      3,
  },
};

static void
on_width_selected (gpointer user_data, int width)
{
  AppState *state = (AppState *) user_data;
  state->width = (gdouble) width;
}

static void
on_fill_selected (gpointer user_data, int fill_type)
{
  AppState *state = (AppState *) user_data;
  state->fill_type = (FillType) fill_type;
}

static void
on_eraser_size_selected (gpointer user_data, int eraser_size)
{
  AppState *state = (AppState *) user_data;
  state->eraser_size = eraser_size;
}

static const GdkRGBA *
get_primary_color (gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  return &state->primary_color;
}

static const GdkRGBA *
get_secondary_color (gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  return &state->secondary_color;
}

static void
swap_colors (gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  GdkRGBA t = state->primary_color;
  state->primary_color = state->secondary_color;
  state->secondary_color = t;
}

static GtkWidget *
create_toolbar_grid (AppState *state)
{
  const struct
  {
    const gchar *label;
    Tool *tool;
  } tools[] = {
    { "Freehand", &global_freehand_tool },
    { "Brush", &global_brush_tool },
    { "Line", &global_line_tool },
    { "Rect", &global_rectangle_tool },
    { "Ellipse", &global_ellipse_tool },
    /* { "Bucket", &global_filler_tool }, */
    { "Eraser", &global_eraser_tool },
    { "Picker", &global_picker_tool },
    { "Bucket", &global_bucket_tool },
  };

  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *grid = gtk_grid_new ();

  gtk_widget_set_margin_top (grid, 2);
  gtk_widget_set_margin_bottom (grid, 2);
  gtk_widget_set_margin_start (grid, 8);
  gtk_widget_set_margin_end (grid, 8);

  for (gint i = 0; i < (gint) G_N_ELEMENTS (tools); i++)
    {
      GtkWidget *btn = gtk_toggle_button_new ();
      // Create an icon image using the tool's icon name.

      GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (tools[i].tool->icon->data, tools[i].tool->icon->colorspace, tools[i].tool->icon->has_alpha, tools[i].tool->icon->bits_per_sample, tools[i].tool->icon->height, tools[i].tool->icon->width,
                                                    tools[i].tool->icon->width * 4, // TODO
                                                    NULL, NULL);
      GtkWidget *icon_image = gtk_image_new_from_pixbuf (pixbuf);
      g_object_unref (pixbuf);

      /* GtkWidget *icon_image = gtk_image_new_from_file (tools[i].tool->icon); */
      // Set the icon as the button's child.
      gtk_button_set_child (GTK_BUTTON (btn), icon_image);
      // Add tooltip with the tool name.
      gtk_widget_set_tooltip_text (btn, tools[i].label);
      g_object_set_data (G_OBJECT (btn), "tool", tools[i].tool);
      g_signal_connect (btn, "toggled", G_CALLBACK (tool_toggled), state);
      // Place the button in a 2-column grid.
      int col = i % 2;
      int row = i / 2;
      gtk_grid_attach (GTK_GRID (grid), btn, col, row, 1, 1);
      if (i == 0)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (btn), TRUE);
    }

  /*
    USE STACK INSTEAD OF BOX
GtkWidget *stack = gtk_stack_new();

// Create two children
GtkWidget *child1 = gtk_label_new("Child 1");
GtkWidget *child2 = gtk_label_new("Child 2");

// Add children to the stack
gtk_stack_add_child(GTK_STACK(stack), child1, "child1");
gtk_stack_add_child(GTK_STACK(stack), child2, "child2");

// Set the visible child
gtk_stack_set_visible_child(GTK_STACK(stack), "child1");
   */

  state->width_selector = value_selector_new (line_widths, G_N_ELEMENTS (line_widths), on_width_selected, state);
  state->fill_selector = value_selector_new (fills, G_N_ELEMENTS (fills), on_fill_selected, state);
  state->eraser_size_selector = value_selector_new (eraser_sizes, G_N_ELEMENTS (eraser_sizes), on_eraser_size_selected, state);
  gtk_box_append (GTK_BOX (vbox), grid);
  gtk_box_append (GTK_BOX (vbox), state->width_selector);
  gtk_box_append (GTK_BOX (vbox), state->fill_selector);
  gtk_box_append (GTK_BOX (vbox), state->eraser_size_selector);

  GtkWidget *button = color_swap_button_new (get_primary_color, get_secondary_color, swap_colors, state);
  gtk_box_append (GTK_BOX (vbox), button);

  /* update_container_child (state, color_widget (&state->primary_color)); */
  return vbox;
}

static void
on_new_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  clear_canvas (state, state->main_surface);
}

/* Creates the file toolbar (New, Open, Save, Clear) */
static GtkWidget *
create_file_toolbar (GtkApplication *app, AppState *state)
{
  for (size_t i = 0; i < G_N_ELEMENTS (file_actions); i++)
    {
      GSimpleAction *action = g_simple_action_new (file_actions[i].short_key, NULL);
      g_signal_connect (action, "activate", file_actions[i].callback, state);
      g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (action));
    }

  GMenu *file_menu = g_menu_new ();

  for (size_t i = 0; i < G_N_ELEMENTS (file_actions); i++)
    g_menu_append (file_menu, file_actions[i].label, file_actions[i].key);

  GtkWidget *file_menu_btn = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (file_menu_btn), G_MENU_MODEL (file_menu));
  gtk_menu_button_set_label (GTK_MENU_BUTTON (file_menu_btn), "File");

  for (size_t i = 0; i < G_N_ELEMENTS (file_actions); i++)
    gtk_application_set_accels_for_action (app, file_actions[i].key, file_actions[i].accel);

  return file_menu_btn;
}

static void
update_zoom_label (AppState *state)
{
  gchar label[1024];
  snprintf (label, sizeof (label), "Zoom: %.0f%%", state->zoom_level * 100.0);
  gtk_label_set_text (GTK_LABEL (state->zoom_label), label);
}

static void
zoom_in (GtkButton *btn, AppState *state)
{
  state->zoom_level = fmin (64.0, state->zoom_level * 1.2);
  gtk_widget_queue_draw (state->drawing_area);
  update_zoom_label (state);
}

static void
zoom_out (GtkButton *btn, AppState *state)
{
  state->zoom_level = fmax (1.0, state->zoom_level / 1.2);
  gtk_widget_queue_draw (state->drawing_area);
  update_zoom_label (state);
}

static void
zoom_reset (GtkButton *btn, AppState *state)
{
  state->zoom_level = 1.0;
  gtk_widget_queue_draw (state->drawing_area);
  gtk_label_set_text (GTK_LABEL (state->zoom_label), "Zoom: 100%");
}

/* Grid toggle handler */
static void
on_grid_toggle (GtkToggleButton *btn, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  state->show_grid = gtk_toggle_button_get_active (btn);
  gtk_widget_queue_draw (state->drawing_area);
}

static void
create_menus (GtkApplication *app, GtkWindow *window, GtkWidget *header_bar, AppState *state)
{
  GtkWidget *file_menu = create_file_toolbar (app, state);

  /* Append menus to the header bar */
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), file_menu);

  GtkWidget *resize_btn = gtk_button_new_with_label ("Resize");
  g_signal_connect (resize_btn, "clicked", G_CALLBACK (on_resize_clicked), state);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), resize_btn);
}

/* Function to resize drawable area and preserve data */
static void
resize_drawable_area (AppState *state, int new_width, int new_height)
{
  cairo_surface_t *old_surface = state->main_surface;

  state->main_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, new_width, new_height);
  clear_canvas (state, state->main_surface);

  cairo_t *cr = cairo_create (state->main_surface);
  cairo_set_source_surface (cr, old_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_destroy (old_surface);
}

static void on_entry_changed (GtkEditable *editable, gpointer user_data);

typedef struct
{
  AppState *state;
  GtkWidget *width_entry;
  GtkWidget *height_entry;
} ResizeData;

static void
on_ok_clicked (GtkButton *btn, gpointer user_data)
{
  ResizeData *rd = user_data;
  const gchar *width_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->width_entry)));
  const gchar *height_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->height_entry)));

  int new_width = atoi (width_text);
  int new_height = atoi (height_text);
  resize_drawable_area (rd->state, new_width, new_height);
  gtk_window_destroy (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn))));
  g_free (rd);
}

static void
on_cancel_clicked (GtkButton *btn, gpointer user_data)
{
  ResizeData *rd = user_data;
  gtk_window_destroy (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn))));
  g_free (rd);
}

static void
on_resize_clicked (GtkButton *btn, AppState *state)
{
  GtkWidget *window = gtk_window_new ();
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn))));
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_title (GTK_WINDOW (window), "Resize Drawable Area");

  GtkWidget *grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 8);

  GtkWidget *content_area = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_window_set_child (GTK_WINDOW (window), content_area);
  gtk_box_append (GTK_BOX (content_area), grid);

  GtkWidget *keep_ratio_check = gtk_check_button_new_with_label ("Keep ratio");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (keep_ratio_check), FALSE);

  gchar buffer[64];
  gint n;

  struct Dimension
  {
    gint current;
    const char *label;
    GtkWidget *entry;
  };

  struct Dimension h = {
    .current = cairo_image_surface_get_height (state->main_surface),
    .label = "Height",
    .entry = NULL,
  };

  struct Dimension w = {
    .current = cairo_image_surface_get_width (state->main_surface),
    .label = "Width",
    .entry = NULL,
  };

  struct Dimension *d[] = { &w, &h };

  for (gint i = 0; i < (gint) G_N_ELEMENTS (d); i++)
    {
      struct Dimension *t = d[i];
      t->entry = gtk_entry_new ();

      n = snprintf (buffer, sizeof (buffer), "%d", t->current);
      gtk_entry_set_buffer (GTK_ENTRY (t->entry), gtk_entry_buffer_new (buffer, n));

      g_object_set_data (G_OBJECT (t->entry), "keep_ratio_check", keep_ratio_check);
      g_object_set_data (G_OBJECT (t->entry), "value", GINT_TO_POINTER (t->current));

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new (t->label), 0, i, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), t->entry, 1, i, 1, 1);
    }

  g_signal_connect (w.entry, "changed", G_CALLBACK (on_entry_changed), h.entry);
  g_signal_connect (h.entry, "changed", G_CALLBACK (on_entry_changed), w.entry);

  gtk_grid_attach (GTK_GRID (grid), keep_ratio_check, 0, 2, 2, 1);

  ResizeData *rd = g_malloc (sizeof (ResizeData));
  rd->state = state;
  rd->width_entry = w.entry;
  rd->height_entry = h.entry;

  GtkWidget *ok_button = gtk_button_new_with_label ("Ok");
  g_signal_connect (ok_button, "clicked", G_CALLBACK (on_ok_clicked), rd);

  GtkWidget *cancel_button = gtk_button_new_with_label ("Cancel");
  g_signal_connect (cancel_button, "clicked", G_CALLBACK (on_cancel_clicked), rd);

  gtk_grid_attach (GTK_GRID (grid), ok_button, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), cancel_button, 1, 3, 1, 1);

  gtk_window_present (GTK_WINDOW (window));
}

// TODO
void
print_widget_type (GtkWidget *widget)
{
  // Get the GType of the widget
  GType widget_type = G_OBJECT_TYPE (widget);

  // Get the name of the type
  const gchar *type_name = g_type_name (widget_type);

  // Print the type name
  g_print ("Widget type: %s\n", type_name);
}

/* Label change handlers to maintain aspect ratio */
static void
on_entry_changed (GtkEditable *editable, gpointer user_data)
{
  GtkWidget *other_entry = GTK_WIDGET (user_data);
  GtkWidget *keep_ratio_check = g_object_get_data (G_OBJECT (other_entry), "keep_ratio_check");

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (keep_ratio_check)))
    {
      gint original_this = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (editable), "value"));
      gint original_other = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (other_entry), "value"));
      gint new_this = atoi (gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (editable))));
      gint new_other = (new_this * original_other) / original_this;
      g_object_set_data (G_OBJECT (other_entry), "value", GINT_TO_POINTER (new_other));
      g_object_set_data (G_OBJECT (editable), "value", GINT_TO_POINTER (new_this));

      gchar buffer[64];
      gint n = snprintf (buffer, sizeof (buffer), "%d", new_other);
      GtkEntryBuffer *other = gtk_entry_buffer_new (buffer, n);
      gtk_entry_set_buffer (GTK_ENTRY (other_entry), other);
    }
}

static void
activate (GtkApplication *app, AppState *state)
{
  GtkWidget *window = gtk_application_window_new (app);
  state->window = window;
  gtk_window_set_title (GTK_WINDOW (window), "Paint");
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  GtkWidget *header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);
  GtkColorDialog *dialog = gtk_color_dialog_new ();
  gtk_color_dialog_set_with_alpha (dialog, TRUE);
  state->color_btn = gtk_color_dialog_button_new (dialog);
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), &state->primary_color);
  g_signal_connect (state->color_btn, "notify::rgba", G_CALLBACK (on_color_changed), state);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), state->color_btn);
  GtkWidget *zoom_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *zin = gtk_button_new_from_icon_name ("zoom-in-symbolic");
  GtkWidget *zout = gtk_button_new_from_icon_name ("zoom-out-symbolic");
  GtkWidget *zreset = gtk_button_new_from_icon_name ("zoom-fit-best-symbolic");
  state->zoom_label = gtk_label_new ("Zoom: 100%");
  g_signal_connect (zin, "clicked", G_CALLBACK (zoom_in), state);
  g_signal_connect (zout, "clicked", G_CALLBACK (zoom_out), state);
  g_signal_connect (zreset, "clicked", G_CALLBACK (zoom_reset), state);
  gtk_box_append (GTK_BOX (zoom_box), zin);
  gtk_box_append (GTK_BOX (zoom_box), zout);
  gtk_box_append (GTK_BOX (zoom_box), zreset);
  gtk_box_append (GTK_BOX (zoom_box), state->zoom_label);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), zoom_box);
  state->grid_toggle = gtk_toggle_button_new ();
  gtk_button_set_icon_name (GTK_BUTTON (state->grid_toggle), "grid-symbolic");
  gtk_widget_set_tooltip_text (state->grid_toggle, "Toggle Grid");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->grid_toggle), state->show_grid);
  g_signal_connect (state->grid_toggle, "toggled", G_CALLBACK (on_grid_toggle), state);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), state->grid_toggle);

  state->drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (state->drawing_area), draw_callback, state, NULL);

  GtkWidget *scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scrolled, TRUE); // Allow vertical expansion
  gtk_widget_set_hexpand (scrolled, TRUE); // Allow horizontal expansion
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), state->drawing_area);

  state->status_bar = gtk_label_new ("");

  /* Create left-side toolbar as a 2-column grid */
  GtkWidget *toolbar_grid = create_toolbar_grid (state);

  /* Create the central content area: left toolbar and drawing area */
  GtkWidget *content_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (content_hbox), toolbar_grid);
  gtk_box_append (GTK_BOX (content_hbox), scrolled);

  /* Create a main vertical container to arrange header, content, and status bar */
  GtkWidget *main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (main_vbox), content_hbox);
  gtk_box_append (GTK_BOX (main_vbox), state->status_bar);

  /* Set the main container as the child of the window */
  gtk_window_set_child (GTK_WINDOW (window), main_vbox);

  GtkEventController *motion = gtk_event_controller_motion_new ();
  g_signal_connect (motion, "motion", G_CALLBACK (motion_handler), state);
  gtk_widget_add_controller (state->drawing_area, motion);
  /* GtkGesture *click = gtk_gesture_click_new (); */
  /* g_signal_connect (click, "pressed", G_CALLBACK (on_click_pressed), state); */
  /* g_signal_connect (click, "released", G_CALLBACK (on_click_released), state); */
  /* gtk_widget_add_controller (state->drawing_area, GTK_EVENT_CONTROLLER (click)); */

  // TODO
  GtkGesture *drag = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), GDK_BUTTON_PRIMARY);
  gtk_widget_add_controller (state->drawing_area, GTK_EVENT_CONTROLLER (drag));
  g_signal_connect (drag, "drag-begin", G_CALLBACK (on_click_pressed), state);
  /* g_signal_connect (drag, "drag-update", G_CALLBACK (on_click_update), state); */
  g_signal_connect (drag, "drag-end", G_CALLBACK (on_click_released), state);

  GtkGesture *press = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (press), GDK_BUTTON_SECONDARY);
  gtk_widget_add_controller (state->drawing_area, GTK_EVENT_CONTROLLER (press));
  g_signal_connect (press, "drag-begin", G_CALLBACK (on_secondary), state);
  /* g_signal_connect (press, "drag-end", G_CALLBACK (on_secondary), state); */

  GtkEventController *scroll_controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  g_signal_connect (scroll_controller, "scroll", G_CALLBACK (on_scroll), state);
  gtk_widget_add_controller (state->drawing_area, scroll_controller);
  create_menus (app, GTK_WINDOW (window), header, state);
  update_cursor (state);
  gtk_window_present (GTK_WINDOW (window));
  update_status_bar (state, 0, 0);
}

int
main (int argc, char **argv)
{
  /*  void dump_texture_as_c_code(const char *icon) { */
  /*       g_autoptr (GdkTexture) texture = gdk_texture_new_from_filename (icon, NULL); */

  /*       if (!texture) */
  /*         { */
  /*           g_warning ("Failed to create GdkTexture from filename: %s", icon); */
  /*           return; */
  /*         } */

  /*     // Convert GdkTexture to a GdkPixbuf (available in recent GTK4 versions) */
  /*     g_autoptr(GdkPixbuf) pixbuf = gdk_pixbuf_get_from_texture(texture); */
  /*     if (!pixbuf) { */
  /*         g_printerr("Failed to convert texture to pixbuf.\n"); */
  /*         return; */
  /*     } */
  /*     int width = gdk_pixbuf_get_width(pixbuf); */
  /*     int height = gdk_pixbuf_get_height(pixbuf); */
  /*     int rowstride = gdk_pixbuf_get_rowstride(pixbuf); */
  /*     guchar *data = gdk_pixbuf_get_pixels(pixbuf); */

  /*     printf("static const unsigned char %s_data[%d] = {\n", icon, height * rowstride); */
  /*     for (int y = 0; y < height; y++) { */
  /*         printf("    "); */
  /*         for (int x = 0; x < rowstride; x++) { */
  /*             printf("0x%02X, ", data[y * rowstride + x]); */
  /*         } */
  /*         printf("\n"); */
  /*     } */
  /*     printf("};\n"); */
  /* } */
  /* Tool *tools[] = { */
  /*     {  &global_freehand_tool }, */
  /*     {  &global_brush_tool }, */
  /*     {  &global_line_tool }, */
  /*     {  &global_rectangle_tool }, */
  /*     {  &global_ellipse_tool }, */
  /*     /\* { "Bucket", &global_filler_tool }, *\/ */
  /*     {  &global_eraser_tool }, */
  /*     {  &global_picker_tool }, */
  /*     {  &global_bucket_tool }, */
  /*   }; */

  /*  for (int i = 0; i < G_N_ELEMENTS (tools); i++) */
  /*    dump_texture_as_c_code (tools[i]->icon); */

  AppState state[1] = { 0 };
  state->main_surface = create_surface (32, 32);
  state->cursor_surface = create_surface (cairo_image_surface_get_width (state->main_surface), cairo_image_surface_get_height (state->main_surface));
  state->primary_color = (GdkRGBA) { 0.0, 0.0, 0.0, 1.0 };
  state->secondary_color = (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
  state->zoom_level = 1.0;
  state->show_grid = FALSE;
  state->width = 1.0;
  state->brush_size = 3.0;
  state->fill_type = FILL_TRANSPARENT;
  state->primary_tool = &global_freehand_tool;

  //g_autoptr (GtkApplication) app = gtk_application_new ("org.gnu.paint", G_APPLICATION_DEFAULT_FLAGS);
  g_autoptr (AdwApplication) app = adw_application_new ("org.gnu.paint", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), state);
  return g_application_run (G_APPLICATION (app), argc, argv);
}
