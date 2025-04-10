
#include "config.h"

#include <glib/gi18n.h>

#if HAS_LIBADWAITA
# include <adwaita.h>
#endif

#include <gtk/gtk.h>

#include "color-swap-button.h"
#include "drag-square.h"
#include "gpaint.h"
#include "tools/tools.h"
#include "number-entry.c"

/* #include "layers.c" */
#include "zoom.c"

static void update_cursor_position (AppState *state, gdouble x, gdouble y);

static void
motion_handler (GtkEventControllerMotion *ctrl, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  gint px = (gint) (x / state->zoom_level);
  gint py = (gint) (y / state->zoom_level);

  update_cursor_position (state, x, y);

  /* if (state->cursor_surface && state->primary_tool->draw_cursor_handler) */
  /*   { */
  /*     cairo_t *cr = create_cairo (state->cursor_surface, CAIRO_OPERATOR_SOURCE); */
  /*     cairo_set_source_rgba (cr, 0, 0, 0, 0); */
  /*     cairo_paint (cr); */
  /*     state->primary_tool->draw_cursor_handler (state, px, py); */
  /*     cairo_destroy (cr); */

  /*     gtk_widget_queue_draw (state->drawing_area); */
  /*   } */

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
            state->primary_tool->draw_handler (state, state->start_point.x, state->start_point.y, px, py);
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
on_leave (GtkEventControllerMotion *ctrl, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (!state->is_drawing)
    {
      update_cursor_position (state, -1, -1);
      gtk_widget_queue_draw (state->drawing_area); // TODO REDRAW TO CLEAR CURSOR
    }
}

static void on_new_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_open_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_save_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
on_undo (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  move_backward (&state->backup_manager, state);
  gtk_widget_queue_draw (state->drawing_area);
}
static void
on_redo (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  move_forward (&state->backup_manager, state);
  gtk_widget_queue_draw (state->drawing_area);
}
static void on_resize (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void on_toggle_show_grid(GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  AppState *state = (AppState *) user_data;
  GVariant *current = g_action_get_state(G_ACTION(action));
  gboolean value = g_variant_get_boolean(current);
  g_simple_action_set_state(action, g_variant_new_boolean(!value));
  g_variant_unref(current);
  gtk_widget_queue_draw (state->drawing_area);
}

/* typedef struct */
/* { */
/*   const gchar *label; */
/*   const gchar *key; */
/*   const gchar *short_key; */
/*   const gchar *accel[2]; */
/*   GCallback callback; */
/*   GSimpleAction **action; */
/* } AppAction; */

/* static AppAction file_actions[] = { */
/*   { "New",  "app.new",  "new",  { "<Primary>n", NULL }, G_CALLBACK (on_new_file) }, */
/*   { "Open", "app.open", "open", { "<Primary>o", NULL }, G_CALLBACK (on_open_file) }, */
/*   { "Save", "app.save", "save", { "<Primary>s", NULL }, G_CALLBACK (on_save_file) }, */
/*   { "Quit", "app.quit", "quit", { "<Primary>q", NULL }, G_CALLBACK (on_quit) }, */
/* }; */

/* static AppAction edit_actions[] = { */
/*   { "Undo", "app.undo", "undo", { "<Primary>z", NULL }, G_CALLBACK (on_undo) }, */
/*   { "Redo", "app.redo", "redo", { "<Primary>y", NULL }, G_CALLBACK (on_redo) }, */
/*   { "Resize", "app.resize", "resize", { NULL, NULL }, G_CALLBACK (on_resize) }, */
/* }; */

////
static const GActionEntry file_actions[] = {
    { "new",  on_new_file, NULL, NULL, NULL },
    { "open", on_open_file, NULL, NULL, NULL },
    { "save", on_save_file, NULL, NULL, NULL },
    { "quit", on_quit, NULL, NULL, NULL }
};

static const GActionEntry edit_actions[] = {
    { "undo", on_undo, NULL, NULL, NULL },
    { "redo", on_redo, NULL, NULL, NULL },
    { "resize", on_resize, NULL, NULL, NULL },
};

static GActionEntry view_actions[] = {
    { "showgrid", on_toggle_show_grid, NULL, "false", NULL }
};

/* Accelerators configuration */
static const struct {
    const gchar *action;
    const gchar *accels[2];
} app_accels[] =
  {
    { "app.new",  { "<Primary>n", NULL } },
    { "app.open", { "<Primary>o", NULL } },
    { "app.save", { "<Primary>s", NULL } },
    { "app.quit", { "<Primary>q", NULL } },
    { "app.undo", { "<Primary>z", NULL } },
    { "app.redo", { "<Primary>y", NULL } }
  };

static void
update_cursor_position (AppState *state, gdouble x, gdouble y)
{
  cairo_surface_t *surface = state->main_surface;
  const gint width = cairo_image_surface_get_width (surface);
  const gint height = cairo_image_surface_get_height (surface);

  // TODO undocumanted?
  const cairo_format_t format = cairo_image_surface_get_format (surface);
  gint color_depth = 0;

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
      case CAIRO_FORMAT_A1: color_depth = 1; break;
      case CAIRO_FORMAT_RGB96F: // TODO
        color_depth = 96;
        break;
      case CAIRO_FORMAT_RGBA128F: // TODO
        color_depth = 128;
        break;
      case CAIRO_FORMAT_INVALID:
      default:
        color_depth = 0;
        break;
    }

  gchar image_info[256];

  if (color_depth != 0)
    snprintf (image_info, sizeof (image_info), "%d×%d×%d", width, height, color_depth);
  else
    snprintf (image_info, sizeof (image_info), "%d×%d", width, height);

  gtk_label_set_text (GTK_LABEL (state->image_info), image_info);

  gint px = (gint) (x / state->zoom_level);
  gint py = (gint) (y / state->zoom_level);

  state->cursor_x = x;
  state->cursor_y = y;

  if (x < 0 || px >= width || y < 0 || py >= height)
    {
      // TODO
      gtk_label_set_text (GTK_LABEL (state->current_position), "");
    }
  else
    {
      gchar position[256];
      snprintf (position, sizeof (position), "[%d, %d]", MIN (px, width - 1), MIN (py, height - 1));
      gtk_label_set_text (GTK_LABEL (state->current_position), position);
    }

  gtk_widget_queue_draw (state->drawing_area);
}

///

/*
 * Returns the intersection (in child widget coordinates) of the child's allocation
 * and the viewport's allocation (which represents the visible area).
 * Assumes that `child` is a descendant of a GtkViewport.
 */
void
my_get_visible_rect(GtkWidget *child, GtkWidget *scrolled,
                    gdouble *x, gdouble *y, gdouble *width, gdouble *height)
{
  GdkRectangle rect;
  GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW (scrolled));
  GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW (scrolled));
  double x_offset = gtk_adjustment_get_value(hadj);
  double y_offset = gtk_adjustment_get_value(vadj);
  double page_width = gtk_adjustment_get_page_size(hadj);
  double page_height = gtk_adjustment_get_page_size(vadj);

  /* The visible rectangle in child's coordinate system */
  *x = x_offset;
  *y = y_offset;
  *width = page_width;
  *height = page_height;
}
///

static void
draw_callback (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  const gdouble pixel_size = state->zoom_level;
  const gint surface_width = cairo_image_surface_get_width (state->main_surface);
  const gint surface_height = cairo_image_surface_get_height (state->main_surface);
  cairo_pattern_t *pattern;
  const gdouble bg[2] = { 0.8, 0.9 };
  const gdouble line_color = 0.7; // TODO

  gdouble s_x, s_y, s_width, s_height;
  my_get_visible_rect(GTK_WIDGET (area), state->scrolled, &s_x, &s_y, &s_width, &s_height);
  /* g_print("Visible rect: x=%lf, y=%lf, w=%lf, h=%lf\n", vis.x / pixel_size, vis.y / pixel_size, vis.width / pixel_size, vis.height / pixel_size); */

  const GdkRectangle v =
    {
      .x = (gint) (s_x / pixel_size),
      .y = (gint) (s_y / pixel_size),
      .width = (gint) (s_width / pixel_size),
      .height = (gint) (s_height / pixel_size),
    };

  /* g_print("Visible rect: x=%d, y=%d, w=%d, h=%d\n", v.x, v.y, v.width, v.height); */

  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_save (cr);
  cairo_scale (cr, pixel_size, pixel_size);

  gdouble color = bg[1];
  cairo_set_source_rgb (cr, color, color, color);
  cairo_rectangle (cr, v.x - 1, v.y - 1, v.width + 3, v.height + 3);
  cairo_fill (cr);
  cairo_restore (cr);

  if (pixel_size > 4.0)
    {
      /* Draw checkerboard background */
      cairo_save (cr);
      gint k = 2; // TODO make it adaptive for zoom.
      cairo_scale (cr, pixel_size / k, pixel_size / k);

      gint grid_width = (v.x + v.width) * k;
      gint grid_height = (v.y + v.height) * k;

      cairo_new_path (cr);
      const gdouble dash[] = { 1.0, 1.0 };
      cairo_set_dash(cr, dash, 1, 0);
      cairo_set_line_width (cr, 1.0);

      for (gint y = v.y; y < grid_height + k + 1; y++)
        {
          gdouble x0, x1, y0, y1;

          x0 = (v.x - 1) * k + (y & 1);
          x1 = grid_width + k;
          y0 = y1 = y + 0.5;

          cairo_move_to (cr, x0, y0);
          cairo_line_to (cr, x1, y1);
        }

      cairo_set_source_rgb (cr, bg[0], bg[0], bg[0]);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

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

  if (g_variant_get_boolean(g_action_get_state(state->show_grid_action)) && pixel_size >= 4.0)
    {
      cairo_save (cr);

      gdouble t = line_color;
      cairo_set_source_rgba (cr, t, t, t, 1.0);
      cairo_set_line_width (cr, 1.0);

      cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
      cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);

      // Draw vertical grid lines
      for (gint x = 0; x <= surface_width; x++)
        {
          cairo_move_to (cr, x * pixel_size, 0);
          cairo_line_to (cr, x * pixel_size, surface_height * pixel_size);
        }

      // Draw horizontal grid lines
      for (gint y = 0; y <= surface_height; y++)
        {
          cairo_move_to (cr, 0, y * pixel_size);
          cairo_line_to (cr, surface_width * pixel_size, y * pixel_size);
        }

      // Stroke the lines to render them
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  // TODO
  // Overlay cursor layer if available:
  if (state->primary_tool->draw_cursor_handler && state->cursor_x >= 0.0 && state->cursor_y >= 0.0)
    {
      /* cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE); */
      /* cairo_save (cr); */
      /* cairo_scale (cr, pixel_size, pixel_size); */
      /* pattern = cairo_pattern_create_for_surface (state->cursor_surface); */
      /* cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST); */
      /* cairo_set_source (cr, pattern); */
      /* // cairo_set_source_surface(cr, state->preview_surface, 0, 0); */
      /* cairo_paint (cr); */
      /* cairo_pattern_destroy (pattern); */
      /* cairo_restore (cr); */

      cairo_save (cr);
      state->primary_tool->draw_cursor_handler (state, cr);
      cairo_restore (cr);
    }

  cairo_save (cr);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_set_line_width (cr, 1.5); // TODO: 1.5 because 1.0 is invisible...
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_stroke (cr);
  cairo_restore (cr);
}

static GdkTexture *
create_texture_from_raw_data (gint height, gint width, gint rowstride, const guchar *raw_data)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data ((guchar *) raw_data, GDK_COLORSPACE_RGB, /* has alpha */ TRUE, 8, width, height, rowstride, NULL, NULL);
  if (pixbuf == NULL)
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

  if (state->primary_tool->draw_cursor_handler != NULL)
    {
      cursor = gdk_cursor_new_from_name ("none", NULL);
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

      const struct raw_bitmap *icon = state->primary_tool->cursor_icon
        ? state->primary_tool->cursor_icon : state->primary_tool->icon;
      GdkTexture *texture = create_texture_from_raw_data (icon->height, icon->width,
                                                          icon->rowstride, icon->data);

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
on_click_pressed (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data, const GdkRGBA *p_color, const GdkRGBA *s_color)
{
  AppState *state = (AppState *) user_data;
  gint px = (gint) (x / state->zoom_level);
  gint py = (gint) (y / state->zoom_level);

  // TODO
  /* if (state->select_surface == NULL) */
  /*   { */
  /*     if (state->primary_tool->type == TOOL_SELECT_RECTANGLE) */
  /*       state->select_surface = create_surface (cairo_image_surface_get_width (state->main_surface), cairo_image_surface_get_height (state->main_surface), true); */
  /*   } */
  /* else if (state->primary_tool->type != TOOL_SELECT_RECTANGLE) */
  /*   { */
  /*     cairo_surface_destroy (state->select_surface); */
  /*     state->select_surface = NULL; */
  /*   } */

  if (state->is_drawing && state->preview_surface)
    {
      cairo_surface_destroy (state->preview_surface);
      state->preview_surface = NULL;
      state->is_drawing = FALSE;
      gtk_widget_queue_draw (state->drawing_area);
      return;
    }

  state->is_drawing = TRUE;
  state->start_point.x = px;
  state->start_point.y = py;
  state->last_point = state->start_point;
  state->p_color = p_color;
  state->s_color = s_color;

  if (state->preview_surface)
    cairo_surface_destroy (state->preview_surface);

  state->preview_surface = create_surface (cairo_image_surface_get_width (state->main_surface),
                                           cairo_image_surface_get_height (state->main_surface), true);
  if (state->primary_tool->type == TOOL_FREEHAND
      || state->primary_tool->type == TOOL_ERASER
      || state->primary_tool->type == TOOL_BRUSH
      || state->primary_tool->type == TOOL_BUCKET) // TODO
    {
      state->primary_tool->draw_handler (state, px, py, px, py);
      gtk_widget_queue_draw (state->drawing_area);
    }
}

static void
on_click_released (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (!state->is_drawing)
    return;

  if (state->preview_surface)
    {
      save_backup (&state->backup_manager, state->main_surface);

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

static void
on_click_primary_pressed (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  on_click_pressed (gesture, x, y, user_data, &state->primary_color, &state->secondary_color);
}

static void
on_click_secondary_pressed (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  on_click_pressed (gesture, x, y, user_data, &state->secondary_color, &state->primary_color);
}

static gboolean
on_scroll (GtkEventControllerScroll *controller, gdouble dx, gdouble dy, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  /* Retrieve the current event (may be NULL) to check modifiers */
  GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
  GdkModifierType modifiers = event != NULL ? gdk_event_get_modifier_state (event) : GDK_NO_MODIFIER_MASK;

  if (modifiers & GDK_CONTROL_MASK)
    {
      gdouble factor = (dy < 0) ? 1.1 : 0.9;
      zoom_set_value (state, state->zoom_level * factor);

      /* Return TRUE to indicate that the event has been handled
         and should not propagate further. */
      return TRUE;
    }

  /* Return FALSE to allow default processing for non-Control scrolling */
  return FALSE;
}

static void
on_color_changed (GtkColorDialogButton *btn, GParamSpec *pspec, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  state->primary_color = *gtk_color_dialog_button_get_rgba (btn);
  gpaint_color_swap_button_update_colors (GPAINT_COLOR_SWAP_BUTTON (state->color_swap_button));
}

static void
export_image (AppState *state, const gchar *filename)
{
  if (!filename)
    return;

  const gchar *ext = strrchr (filename, '.');

  /* if (ext && g_ascii_strcasecmp (ext, ".png") == 0) */
  /*   { */
  /*     cairo_status_t status = cairo_surface_write_to_png (state->main_surface, filename); */
  /*     if (status != CAIRO_STATUS_SUCCESS) */
  /*       g_warning ("Failed to save PNG image: %s", cairo_status_to_string (status)); */
  /*     return; */
  /*   } */

  /* For non-PNG formats, grab a GdkPixbuf from the surface and save it.
     This allows formats such as JPG, BMP, or GIF. */
  g_autoptr (GdkPixbuf) pixbuf;

  pixbuf = gdk_pixbuf_new_from_data (cairo_image_surface_get_data (state->main_surface),
                                     GDK_COLORSPACE_RGB, // TODO colospace
                                     TRUE,               // TODO has alpha
                                     8,                  // TODO rowstride
                                     cairo_image_surface_get_width (state->main_surface), cairo_image_surface_get_height (state->main_surface), cairo_image_surface_get_stride (state->main_surface), NULL, NULL);

  if (pixbuf != NULL)
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
  AppState *state = (AppState *) user_data;
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
  AppState *state = (AppState *) user_data;
  GtkFileDialog *dialog = GTK_FILE_DIALOG (gtk_file_dialog_new ());
  gtk_file_dialog_set_title (dialog, _ ("Save image"));
  gtk_file_dialog_set_modal (GTK_FILE_DIALOG (dialog), TRUE);

  // TODO
  /* GtkFileFilter *filter = gtk_file_filter_new (); */
  /* gtk_file_filter_add_pixbuf_formats (filter); */
  /* gtk_file_filter_add_pattern (filter, "*.png"); */
  /* gtk_file_filter_add_pattern (filter, "*.jpeg"); */
  /* gtk_file_filter_add_pattern (filter, "*.tiff"); */

  /* GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER); */
  /* g_list_store_append(filters, filter); */
  /* gtk_file_dialog_set_filters (dialog, G_LIST_MODEL(filters)); */

  gtk_file_dialog_save (dialog, GTK_WINDOW (state->window), NULL, on_save_response, state);
}

static void
on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
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
  AppState *state = (AppState *) user_data;
  GtkFileDialog *dialog = GTK_FILE_DIALOG (gtk_file_dialog_new ());
  gtk_file_dialog_set_title (dialog, "Open image");
  gtk_file_dialog_set_modal (GTK_FILE_DIALOG (dialog), TRUE);
  gtk_file_dialog_open (dialog, GTK_WINDOW (state->window), NULL, on_open_response, state);
}

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
      "./icons/line1.png",
      1,
  },
  {
      "./icons/line2.png",
      2,
  },
  {
      "./icons/line3.png",
      3,
  },
  {
      "./icons/line4.png",
      4,
  },
  {
      "./icons/line5.png",
      5,
  },
};

static ValueItem fills[] = {
  {
      "./icons/fill1.png",
      FILL_TRANSPARENT,
  },
  {
      "./icons/fill2.png",
      FILL_SECONDARY,
  },
  {
      "./icons/fill3.png",
      FILL_PRIMARY,
  },
};

static ValueItem eraser_sizes[] = {
  {
      "./icons/fill1.png",
      2,
  },
  {
      "./icons/fill2.png",
      4,
  },
  {
      "./icons/fill3.png",
      6,
  },
};

static void
on_width_selected (gpointer user_data, gint width)
{
  AppState *state = (AppState *) user_data;
  state->width = (gdouble) width;
}

static void
on_fill_selected (gpointer user_data, gint fill_type)
{
  AppState *state = (AppState *) user_data;
  state->fill_type = (FillType) fill_type;
}

static void
on_eraser_size_selected (gpointer user_data, gint eraser_size)
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
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), &state->primary_color);
}

static void
tool_toggled (GtkToggleButton *btn, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  state->primary_tool = (const Tool *) g_object_get_data (G_OBJECT (btn), "tool");
  update_cursor (state);
}

static GtkWidget *
create_toolbar_grid (AppState *state)
{
  // clang-format off
  const struct
  {
    const gchar *label;
    const Tool *tool;
  } tools[] =
    {
      // TODO
      /* { "Select rectangle", &global_select_rectangle_tool }, */
      { "Freehand", &global_freehand_tool },
      { "Brush",    &global_brush_tool },
      { "Line",     &global_line_tool },
      { "Rect",     &global_rectangle_tool },
      { "Ellipse",  &global_ellipse_tool },
      { "Eraser",   &global_eraser_tool },
      { "Picker",   &global_picker_tool },
      { "Bucket",   &global_bucket_tool },
    };
  // clang-format on

  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_spacing (GTK_BOX (vbox), 8);
  GtkWidget *grid = gtk_grid_new ();

  /* gtk_widget_set_margin_top (grid, 2); */
  /* gtk_widget_set_margin_bottom (grid, 2); */
  /* gtk_widget_set_margin_start (grid, 8); */
  /* gtk_widget_set_margin_end (grid, 8); */
  /* gtk_grid_set_row_spacing (GTK_GRID (grid), 4); */
  /* gtk_grid_set_column_spacing (GTK_GRID (grid), 4); */

  GtkWidget *prev_button = NULL;

  for (gint i = 0; i < (gint) G_N_ELEMENTS (tools); i++)
    {
      GtkWidget *btn = gtk_toggle_button_new ();

      g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new_from_data (tools[i].tool->icon->data,
                                                               tools[i].tool->icon->colorspace,
                                                               /* has alpha */ TRUE,
                                                               tools[i].tool->icon->bits_per_sample,
                                                               tools[i].tool->icon->height,
                                                               tools[i].tool->icon->width,
                                                               tools[i].tool->icon->rowstride,
                                                               NULL, NULL);
      GdkTexture *texture = gdk_texture_new_for_pixbuf (pixbuf);
      GtkWidget *icon_image = gtk_image_new_from_paintable (GDK_PAINTABLE (texture));

      gtk_button_set_child (GTK_BUTTON (btn), icon_image);
      gtk_widget_set_tooltip_text (btn, tools[i].label);
      g_object_set_data (G_OBJECT (btn), "tool", tools[i].tool);
      g_signal_connect (btn, "toggled", G_CALLBACK (tool_toggled), state);

      gint col = i % 2;
      gint row = i / 2;
      gtk_grid_attach (GTK_GRID (grid), btn, col, row, 1, 1);

      if (i == 0)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (btn), TRUE);

      if (prev_button != NULL)
        gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (btn), GTK_TOGGLE_BUTTON (prev_button));

      prev_button = btn;
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

  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);

  gtk_box_append (GTK_BOX (vbox), grid);
  gtk_box_append (GTK_BOX (vbox), state->width_selector);
  gtk_box_append (GTK_BOX (vbox), state->fill_selector);
  gtk_box_append (GTK_BOX (vbox), state->eraser_size_selector);
  return vbox;
}

static void
on_new_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  clear_canvas (state, state->main_surface);
}

/* static GtkWidget * */
/* create_actions_toolbar (GtkApplication *app, AppState *state, const gchar *name, AppAction *actions, size_t n_actions) */
/* { */
/*   for (size_t i = 0; i < n_actions; i++) */
/*     { */
/*       GSimpleAction *action = g_simple_action_new (actions[i].short_key, NULL); */
/*       g_signal_connect (action, "activate", actions[i].callback, state); */
/*       g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (action)); */

/*       if (actions[i].action != NULL) */
/*         *actions[i].action = action; */
/*     } */

/*   GMenu *menu = g_menu_new (); */

/*   for (size_t i = 0; i < n_actions; i++) */
/*     g_menu_append (menu, actions[i].label, actions[i].key); */

/*   GtkWidget *menu_btn = gtk_menu_button_new (); */
/*   gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_btn), G_MENU_MODEL (menu)); */
/*   gtk_menu_button_set_label (GTK_MENU_BUTTON (menu_btn), name); */

/*   for (size_t i = 0; i < n_actions; i++) */
/*     gtk_application_set_accels_for_action (app, actions[i].key, actions[i].accel); */

/*   return menu_btn; */
/* } */

static GtkWidget *
create_file_toolbar (GtkApplication *app, AppState *state)
{
  GMenu *file = g_menu_new();

  g_menu_append(file, "New", "app.new");
  g_menu_append(file, "Open", "app.open");
  g_menu_append(file, "Save", "app.save");
  g_menu_append(file, "Quit", "app.quit");

  GtkWidget *file_btn = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (file_btn), G_MENU_MODEL (file));
  gtk_menu_button_set_label (GTK_MENU_BUTTON (file_btn), "File");
  return file_btn;
}

static GtkWidget *
create_edit_toolbar (GtkApplication *app, AppState *state)
{
  GMenu *edit = g_menu_new();

  g_menu_append(edit, "Undo", "app.undo");
  g_menu_append(edit, "Redo", "app.redo");
  g_menu_append(edit, "Resize", "app.resize");

  GtkWidget *edit_btn = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (edit_btn), G_MENU_MODEL (edit));
  gtk_menu_button_set_label (GTK_MENU_BUTTON (edit_btn), "Edit");
  return edit_btn;
}

static GtkWidget *
create_view_toolbar (GtkApplication *app, AppState *state)
{
  GMenu *view = g_menu_new();

  g_menu_append(view, "Show Grid", "app.showgrid");

  GtkWidget *view_btn = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (view_btn), G_MENU_MODEL (view));
  gtk_menu_button_set_label (GTK_MENU_BUTTON (view_btn), "View");
  return view_btn;
}

static void
create_menus (GtkApplication *app, GtkWindow *window, GtkWidget *header_bar, AppState *state)
{
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), create_file_toolbar (app, state));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), create_edit_toolbar (app, state));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), create_view_toolbar (app, state));
}

// TODO rename
static void
resize_drawable_area_x (gpointer user_data, gint dx, gint dy, gint dirx, gint diry)
{
  AppState *state = (AppState *) user_data;
  save_backup (&state->backup_manager, state->main_surface);
  gint width = cairo_image_surface_get_width (state->main_surface);
  gint height = cairo_image_surface_get_height (state->main_surface);
  gint new_width = width + (dx / state->zoom_level) * dirx;
  gint new_height = height + (dy / state->zoom_level) * diry;

  if (new_width == 0)
    new_width = 1;

  if (new_height == 0)
    new_height = 1;

  cairo_surface_t *old_surface = state->main_surface;

  state->main_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, new_width, new_height);
  clear_canvas (state, state->main_surface);

  cairo_t *cr = cairo_create (state->main_surface);
  cairo_set_source_surface (cr, old_surface,
                            (dirx < 0) ? new_width - width : 0, (diry < 0) ? new_height - height : 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_destroy (old_surface);

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_widget_queue_draw (state->drawing_area);
  update_cursor_position (state, -1, -1); // TODO
}

/* Function to resize drawable area and preserve data */
static void
resize_drawable_area (AppState *state, gint new_width, gint new_height)
{
  save_backup (&state->backup_manager, state->main_surface);

  cairo_surface_t *old_surface = state->main_surface;

  state->main_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, new_width, new_height);
  clear_canvas (state, state->main_surface);

  cairo_t *cr = cairo_create (state->main_surface);
  cairo_set_source_surface (cr, old_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_destroy (old_surface);

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_widget_queue_draw (state->drawing_area);
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
  ResizeData *rd = (ResizeData *) user_data;
  const gchar *width_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->width_entry)));
  const gchar *height_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->height_entry)));

  gint new_width = atoi (width_text);
  gint new_height = atoi (height_text);
  resize_drawable_area (rd->state, new_width, new_height);
  gtk_window_destroy (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn))));
  g_free (rd);
}

static void
on_cancel_clicked (GtkButton *btn, gpointer user_data)
{
  ResizeData *rd = (ResizeData *) user_data;
  gtk_window_destroy (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn))));
  g_free (rd);
}

static void
on_keep_ratio_check_toggled (GtkCheckButton *self, gpointer user_data)
{
  if (!gtk_check_button_get_active (self))
    return;

  ResizeData *rd = (ResizeData *) user_data;
  const gchar *width_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->width_entry)));
  const gchar *height_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->height_entry)));
  gint new_width = atoi (width_text);
  gint new_height = atoi (height_text);
  g_object_set_data (G_OBJECT (rd->width_entry), "value", GINT_TO_POINTER (new_width));
  g_object_set_data (G_OBJECT (rd->height_entry), "value", GINT_TO_POINTER (new_height));
}

static void
on_resize (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

#if HAS_LIBADWAITA && ADW_CHECK_VERSION (1, 5, 0)
  AdwDialog *dialog = adw_dialog_new();

  adw_dialog_set_title(dialog, "Resize drawable area");
  adw_dialog_set_can_close(dialog, TRUE);
  adw_dialog_set_presentation_mode (dialog, ADW_DIALOG_FLOATING);
  adw_dialog_set_follows_content_size (dialog, TRUE);
#else
  GtkWidget *window = gtk_window_new ();
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (state->window));
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_title (GTK_WINDOW (window), "Resize drawable area");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (state->window));
#endif

  GtkWidget *grid = gtk_grid_new ();
  gtk_widget_set_margin_top (grid, 12);
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_widget_set_margin_start (grid, 12);
  gtk_widget_set_margin_end (grid, 12);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 8);

  GtkWidget *keep_ratio_check = gtk_check_button_new_with_label ("Keep ratio");

  struct Dimension
  {
    gint current;
    const char *label;
    GtkWidget *entry;
  } h =
    {
      .current = cairo_image_surface_get_height (state->main_surface),
      .label = "Height",
      .entry = NULL,
    }, w =
    {
      .current = cairo_image_surface_get_width (state->main_surface),
      .label = "Width",
      .entry = NULL,
    };

  struct Dimension *d[] = { &w, &h };

  for (gint i = 0; i < (gint) G_N_ELEMENTS (d); i++)
    {
      gchar buffer[64];
      gint n;
      struct Dimension *t = d[i];
      t->entry = my_entry_new_with_initial ("1"); // TODO. It shall handle set buffer

      n = snprintf (buffer, sizeof (buffer), "%d", t->current);
      gtk_entry_set_buffer (GTK_ENTRY (t->entry), gtk_entry_buffer_new (buffer, n));

      g_object_set_data (G_OBJECT (t->entry), "keep_ratio_check", keep_ratio_check);
      g_object_set_data (G_OBJECT (t->entry), "value", GINT_TO_POINTER (0));

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new (t->label), 0, i, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), t->entry, 1, i, 1, 1);
    }

  g_signal_connect (w.entry, "changed", G_CALLBACK (on_entry_changed), h.entry);
  g_signal_connect (h.entry, "changed", G_CALLBACK (on_entry_changed), w.entry);

  ResizeData *rd = g_new (ResizeData, 1);
  rd->state = state;
  rd->width_entry = w.entry;
  rd->height_entry = h.entry;

  GtkWidget *ok_button = gtk_button_new_with_label ("Ok");
  g_signal_connect (ok_button, "clicked", G_CALLBACK (on_ok_clicked), rd);

  GtkWidget *cancel_button = gtk_button_new_with_label ("Cancel");
  g_signal_connect (cancel_button, "clicked", G_CALLBACK (on_cancel_clicked), rd);

  g_signal_connect (keep_ratio_check, "toggled", G_CALLBACK (on_keep_ratio_check_toggled), rd);

  gtk_check_button_set_active (GTK_CHECK_BUTTON (keep_ratio_check), TRUE); // TODO: if true -- set up values in entries
  on_keep_ratio_check_toggled (GTK_CHECK_BUTTON (keep_ratio_check), rd);

  gtk_grid_attach (GTK_GRID (grid), keep_ratio_check, 0, 2, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), ok_button, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), cancel_button, 1, 3, 1, 1);

#if HAS_LIBADWAITA && ADW_CHECK_VERSION (1, 5, 0)
  adw_dialog_set_child (dialog, grid);
  adw_dialog_present (dialog, state->window);
#else
  gtk_window_set_child (GTK_WINDOW (window), grid);
  gtk_window_present (GTK_WINDOW (window));
#endif
}

/* Label change handlers to maintain aspect ratio */
static void
on_entry_changed (GtkEditable *editable, gpointer user_data)
{
  GtkWidget *other_entry = GTK_WIDGET (user_data);
  GtkWidget *keep_ratio_check = (GtkWidget *) g_object_get_data (G_OBJECT (other_entry), "keep_ratio_check");

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (keep_ratio_check)))
    {
      gint old_this = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (editable), "value"));
      gint old_other = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (other_entry), "value"));

      gint new_this = atoi (gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (editable))));
      gint new_other = new_this * old_other / old_this;

      gchar buffer[64];
      gint n = snprintf (buffer, sizeof (buffer), "%d", new_other);
      gtk_entry_set_buffer (GTK_ENTRY (other_entry), gtk_entry_buffer_new (buffer, n));
    }
}

static GtkWidget *
create_drawing_area (AppState *state)
{
  GtkWidget *drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (drawing_area), (gint) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (drawing_area), (gint) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area), draw_callback, state, NULL);

  struct
  {
    gint button;
    void (*callback) (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data);
  } buttons[2] =
    {
      { .button = GDK_BUTTON_PRIMARY,   .callback = on_click_primary_pressed },
      { .button = GDK_BUTTON_SECONDARY, .callback = on_click_secondary_pressed },
    };

  for (size_t i = 0; i < G_N_ELEMENTS (buttons); i++)
    {
      GtkGesture *drag = gtk_gesture_drag_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), buttons[i].button);
      g_signal_connect (drag, "drag-begin", G_CALLBACK (buttons[i].callback), state);
      g_signal_connect (drag, "drag-end", G_CALLBACK (on_click_released), state);
      gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (drag));
    }

  GtkEventController *motion = gtk_event_controller_motion_new ();
  g_signal_connect (motion, "motion", G_CALLBACK (motion_handler), state);
  g_signal_connect (motion, "leave", G_CALLBACK (on_leave), state);
  gtk_widget_add_controller (drawing_area, motion);

  return drawing_area;
}

cairo_surface_t *
get_surface (gpointer user_data)
{
  AppState *st = (AppState *) user_data;
  return st->main_surface;
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

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), create_zoom_box (state));

  // TODO
  /* state->grid_toggle = gtk_toggle_button_new (); */
  /* gtk_button_set_icon_name (GTK_BUTTON (state->grid_toggle), "grid-symbolic"); */
  /* gtk_widget_set_tooltip_text (state->grid_toggle, "Toggle grid"); */
  /* gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->grid_toggle), state->show_grid); */
  /* g_signal_connect (state->grid_toggle, "toggled", G_CALLBACK (on_grid_toggle), state); */
  /* gtk_header_bar_pack_end (GTK_HEADER_BAR (header), state->grid_toggle); */

  state->drawing_area = create_drawing_area (state);

  GtkWidget *grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (grid), state->drawing_area, 1, 1, 1, 1);

  {
    GtkWidget *top_left = gpaint_drag_tracker_new (resize_drawable_area_x, -1, -1, state);
    GtkWidget *cen_left = gpaint_drag_tracker_new (resize_drawable_area_x, -1,  0, state);
    GtkWidget *bot_left = gpaint_drag_tracker_new (resize_drawable_area_x, -1, +1, state);

    GtkWidget *top_center = gpaint_drag_tracker_new (resize_drawable_area_x, 0, -1, state);
    GtkWidget *bot_center = gpaint_drag_tracker_new (resize_drawable_area_x, 0, +1, state);

    GtkWidget *top_right = gpaint_drag_tracker_new (resize_drawable_area_x, +1, -1, state);
    GtkWidget *cen_right = gpaint_drag_tracker_new (resize_drawable_area_x, +1,  0, state);
    GtkWidget *bot_right = gpaint_drag_tracker_new (resize_drawable_area_x, +1, +1, state);

    gtk_widget_set_valign (cen_left, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (top_center, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (bot_center, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (cen_right, GTK_ALIGN_CENTER);

    gtk_grid_attach (GTK_GRID (grid), top_left, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), cen_left, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), bot_left, 0, 2, 1, 1);

    gtk_grid_attach (GTK_GRID (grid), top_center, 1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), bot_center, 1, 2, 1, 1);

    gtk_grid_attach (GTK_GRID (grid), top_right, 2, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), cen_right, 2, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), bot_right, 2, 2, 1, 1);
  }

  // TODO
  /* gtk_widget_set_margin_top (grid, 16); */
  gtk_widget_set_margin_bottom (grid, 16);
  /* gtk_widget_set_margin_start (grid, 16); */
  gtk_widget_set_margin_end (grid, 16);

  GtkWidget *scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scrolled, TRUE); // Allow vertical expansion
  gtk_widget_set_hexpand (scrolled, TRUE); // Allow horizontal expansion
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), grid);
  state->scrolled = scrolled;

  GtkEventController *scroll_controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  g_signal_connect (scroll_controller, "scroll", G_CALLBACK (on_scroll), state);
  gtk_widget_add_controller (scrolled, scroll_controller);

  state->color_swap_button = gpaint_color_swap_button_new (get_primary_color, get_secondary_color, swap_colors, state);
  state->image_info = gtk_label_new ("");
  state->current_position = gtk_label_new ("");

  GtkColorDialog *dialog = gtk_color_dialog_new ();
  gtk_color_dialog_set_with_alpha (dialog, TRUE);
  state->color_btn = gtk_color_dialog_button_new (dialog);
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), &state->primary_color);
  g_signal_connect (state->color_btn, "notify::rgba", G_CALLBACK (on_color_changed), state);
  /* gtk_header_bar_pack_start (GTK_HEADER_BAR (header), state->color_btn); */

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append (GTK_BOX (hbox), state->color_swap_button);
  gtk_box_append (GTK_BOX (hbox), state->color_btn);
  gtk_box_append (GTK_BOX (hbox), state->image_info);
  gtk_box_append (GTK_BOX (hbox), state->current_position);

  state->info_widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign (state->info_widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (hbox), state->info_widget);

  GtkWidget *hframe = gtk_frame_new (NULL);
  gtk_frame_set_child (GTK_FRAME (hframe), hbox);

  GtkWidget *toolbar_grid = create_toolbar_grid (state);
  GtkWidget *vframe = gtk_frame_new (NULL);
  gtk_frame_set_child (GTK_FRAME (vframe), toolbar_grid);

  GtkWidget *content_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (content_hbox), vframe);
  gtk_box_append (GTK_BOX (content_hbox), scrolled);

  {
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    // TODO
    /* gtk_box_append (GTK_BOX (vbox), my_layer_widget_new (get_surface, state)); */

    gtk_box_append (GTK_BOX (content_hbox), vbox);
  }

  GtkWidget *main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (main_vbox), content_hbox);
  gtk_box_append (GTK_BOX (main_vbox), hframe);

  gtk_window_set_child (GTK_WINDOW (window), main_vbox);

  create_menus (app, GTK_WINDOW (window), header, state);
  update_cursor (state);
  gtk_window_present (GTK_WINDOW (window));
  update_cursor_position (state, -1, -1);

  struct
  {
    GActionEntry *actions;
    size_t count;
  } entries[] =
    {
      { .actions = file_actions, G_N_ELEMENTS (file_actions) },
      { .actions = edit_actions, G_N_ELEMENTS (edit_actions) },
      { .actions = view_actions, G_N_ELEMENTS (view_actions) },
    };

  for (size_t i = 0; i < G_N_ELEMENTS (entries); i++)
    g_action_map_add_action_entries(G_ACTION_MAP(app),
                                    entries[i].actions,
                                    entries[i].count,
                                    state);

  for (guint i = 0; i < G_N_ELEMENTS(app_accels); i++)
    gtk_application_set_accels_for_action(app,
                                          app_accels[i].action,
                                          app_accels[i].accels);

  state->show_grid_action = g_action_map_lookup_action(G_ACTION_MAP(app), "showgrid");

  state->backup_manager.undo_action = G_SIMPLE_ACTION (g_action_map_lookup_action(G_ACTION_MAP (app), "undo"));
  state->backup_manager.redo_action = G_SIMPLE_ACTION (g_action_map_lookup_action(G_ACTION_MAP (app), "redo"));

  g_simple_action_set_enabled (state->backup_manager.undo_action, !g_queue_is_empty (state->backup_manager.undo));
  g_simple_action_set_enabled (state->backup_manager.redo_action, !g_queue_is_empty (state->backup_manager.redo));
}

gint
main (gint argc, gchar **argv)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  AppState state;
  state.main_surface = create_surface (200, 200, true);
  state.p_color = &state.primary_color;
  state.s_color = &state.secondary_color;
  state.primary_color = (GdkRGBA) { 0.0, 0.0, 0.0, 1.0 };
  state.secondary_color = (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
  state.zoom_level = 1.0;
  state.width = 1.0;
  state.brush_size = 3.0;
  state.fill_type = FILL_TRANSPARENT;
  state.primary_tool = &global_freehand_tool;
  state.is_drawing = FALSE;
  state.preview_surface = NULL;
  // TODO state.select_surface = NULL;
  init_backup_manager (&state.backup_manager);

  g_autoptr (AdwApplication) app = adw_application_new ("org.gnu.paint", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), &state);
  gint status = g_application_run (G_APPLICATION (app), argc, argv);
  free_backup_manager (&state.backup_manager);
  return status;
}
