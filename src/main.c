
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#if HAVE_ADWAITA
#include <adwaita.h>
#endif

#include <gtk/gtk.h>

#include <locale.h>

#include "formats.h"
#include "gpaint.h"
#include "tools/tools.h"

#include "widgets/border-widget.h"
#include "widgets/color-swap-button.h"
#include "widgets/drag-square.h"
#include "widgets/number-entry.h"
#include "widgets/value-selector.h"

#include "cursor.c"
#include "zoom.c"

#ifndef ADW_CHECK_VERSION
#define ADW_CHECK_VERSION(major, minor, patch) 0
#endif

static void update_cursor (AppState *state);
static void update_cursor_position (AppState *state, double x, double y);

static void
set_can_copy_surface (AppState *state)
{
  g_simple_action_set_enabled (G_SIMPLE_ACTION (state->cut_action), state->has_selection);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (state->copy_action), state->has_selection);
}

static void
tool_select (AppState *state, ToolType type)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->tools[type].btn), TRUE);
  update_cursor (state);
}

// TODO
static void
commit_selection (AppState *state)
{
  if (!state->has_selection)
    return;

  cairo_t *cr = cairo_create (state->main_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_surface (cr, state->selected_surface, state->selected_rect.x, state->selected_rect.y);
  cairo_paint (cr);
  cairo_destroy (cr);

  // Clear temporary selection state
  g_clear_pointer (&state->selected_surface, cairo_surface_destroy);
  memset (&state->selected_rect, 0, sizeof (state->selected_rect));
  state->has_selection = FALSE;
  set_can_copy_surface (state);
  gtk_widget_queue_draw (state->drawing_area); // TODO?
}

static inline void
clear_selection (AppState *state)
{
  if (!state->has_selection)
    return;

  /* cairo_t *cr = cairo_create (state->main_surface); */
  /* cairo_set_operator (cr, CAIRO_OPERATOR_OVER); */
  /* cairo_set_source_surface (cr, state->selected_surface, */
  /*                           state->selected_rect.x, */
  /*                           state->selected_rect.y); */
  /* cairo_paint (cr); */
  /* cairo_destroy (cr); */

  // Clear temporary selection state
  g_clear_pointer (&state->selected_surface, cairo_surface_destroy);
  memset (&state->selected_rect, 0, sizeof (state->selected_rect));
  state->has_selection = FALSE;
  set_can_copy_surface (state);
}

// TODO
static inline gboolean
my_dots_in_rect (int x, int y, const GdkRectangle *rect)
{
  return x >= rect->x && y >= rect->y && x <= rect->x + rect->width && y <= rect->y + rect->height;
}

static cairo_surface_t *
cut_rectangle (cairo_surface_t *src, const GdkRectangle *rect, const GdkRGBA *color)
{
  // Create a new surface for the extracted region.
  cairo_surface_t *dest = cairo_image_surface_create (cairo_image_surface_get_format (src), rect->width, rect->height);
  if (cairo_surface_status (dest) != CAIRO_STATUS_SUCCESS)
    {
      g_warning ("Failed to create destination surface.\n");
      return NULL;
    }

  // Copy the rectangular region from src to dest.
  cairo_t *cr_dest = cairo_create (dest);
  // Offset the source so that the desired rectangle maps to (0,0) in dest.
  cairo_set_source_surface (cr_dest, src, -rect->x, -rect->y);
  cairo_paint (cr_dest);
  cairo_destroy (cr_dest);

  // Now fill the rectangle area in the source surface with the specified
  // color.
  cairo_t *cr_src = cairo_create (src);
  cairo_set_operator (cr_src, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cr_src, color);
  cairo_rectangle (cr_src, rect->x, rect->y, rect->width, rect->height);
  cairo_fill (cr_src);
  cairo_destroy (cr_src);

  return dest;
}

static void
motion_handler (GtkEventControllerMotion *ctrl, double x, double y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  update_cursor_position (state, x, y);

  if (state->tool->type == TOOL_SELECT_RECTANGLE)
    update_cursor (state);

  if (state->is_dragging_selection && state->has_selection)
    {
      // Convert widget coordinates to surface coordinates
      double current_x = x / state->zoom_level;
      double current_y = y / state->zoom_level;

      // Calculate delta from drag start
      double dx = current_x - state->drag_start_x;
      double dy = current_y - state->drag_start_y;

      // Update selection position
      state->selected_rect.x = state->selection_start_x + dx;
      state->selected_rect.y = state->selection_start_y + dy;

      // Constrain to canvas boundaries
      state->selected_rect.x = CLAMP (state->selected_rect.x, -state->selected_rect.width, cairo_image_surface_get_width (state->main_surface));
      state->selected_rect.y = CLAMP (state->selected_rect.y, -state->selected_rect.height, cairo_image_surface_get_height (state->main_surface));

      gtk_widget_queue_draw (state->drawing_area);
    }

  if (!state->is_drawing)
    return;

  int px = x / state->zoom_level;
  int py = y / state->zoom_level;

  if (state->preview_surface)
    {
      cairo_t *cr = cairo_create (state->preview_surface);

      if (!state->tool->motion_handler)
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      // TODO
      /* if (!g_variant_get_boolean (g_action_get_state
       * (state->antialiasing_action))) */
      /*   cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE); */
      /* else */
      /*   cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT); */

      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);
      // Let the tool draw its preview into preview_surface:
      if (!state->tool->motion_handler)
        state->tool->draw_handler (state, state->start_point.x, state->start_point.y, px, py);
      else
        {
          cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
          state->tool->motion_handler (state, px, py);
        }

      cairo_destroy (cr);
    }

  gtk_widget_queue_draw (state->drawing_area);
}

static void
on_leave (GtkEventControllerMotion *ctrl, double x, double y, gpointer user_data)
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

static void
on_toggle_show_grid (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  g_autoptr (GVariant) current = g_action_get_state (G_ACTION (action));
  gboolean value = g_variant_get_boolean (current);
  g_simple_action_set_state (action, g_variant_new_boolean (!value));
  gtk_widget_queue_draw (state->drawing_area);
}

/* static void */
/* on_toggle_antialiasing (GSimpleAction *action, GVariant *parameter, gpointer user_data) */
/* { */
/*   AppState *state = (AppState *) user_data; */
/*   g_autoptr (GVariant) current = g_action_get_state (G_ACTION (action)); */
/*   gboolean value = g_variant_get_boolean (current); */
/*   g_simple_action_set_state (action, g_variant_new_boolean (!value)); */
/*   state->antialiasing = !value ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE; // TODO */
/*   gtk_widget_queue_draw (state->drawing_area); */
/* } */

#if 0 // TODO

/* Flip the surface horizontally (left-right) */
cairo_surface_t* flip_horizontal(cairo_surface_t *surface) {
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);

    cairo_surface_t *flipped = cairo_image_surface_create(cairo_image_surface_get_format(surface), width, height);
    cairo_t *cr = cairo_create(flipped);

    // Translate to the right edge then scale X by -1
    cairo_translate(cr, width, 0);
    cairo_scale(cr, -1, 1);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
    return flipped;
}

/* Flip the surface vertically (top-bottom) */
cairo_surface_t* flip_vertical(cairo_surface_t *surface) {
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);

    cairo_surface_t *flipped = cairo_image_surface_create(cairo_image_surface_get_format(surface), width, height);
    cairo_t *cr = cairo_create(flipped);

    // Translate to the bottom edge then scale Y by -1
    cairo_translate(cr, 0, height);
    cairo_scale(cr, 1, -1);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
    return flipped;
}

/* Rotate the surface by a specified angle (in degrees)
   The function calculates the bounding box for the rotated image and returns a new surface.
*/
cairo_surface_t* rotate_surface(cairo_surface_t *surface, double angle_degrees) {
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);

    // Convert angle from degrees to radians.
    double angle = angle_degrees * (M_PI / 180.0);

    // Calculate the new dimensions to accommodate the rotated image.
    double cos_angle = fabs(cos(angle));
    double sin_angle = fabs(sin(angle));
    int new_width = (int)(width * cos_angle + height * sin_angle);
    int new_height = (int)(width * sin_angle + height * cos_angle);

    cairo_surface_t *rotated = cairo_image_surface_create(cairo_image_surface_get_format(surface), new_width, new_height);
    cairo_t *cr = cairo_create(rotated);

    // Move the origin to the center of the new surface.
    cairo_translate(cr, new_width / 2.0, new_height / 2.0);
    // Rotate by the desired angle.
    cairo_rotate(cr, angle);
    // Move the origin back to the top-left of the original surface.
    cairo_translate(cr, -width / 2.0, -height / 2.0);

    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
    return rotated;
}
#endif

/* static void */
/* on_flip_rotate (GSimpleAction *action, GVariant *parameter, gpointer
 * user_data) */
/* { */
/*   AppState *state = (AppState *) user_data; */

/* #if HAS_LIBADWAITA && ADW_CHECK_VERSION(1, 5, 0) */
/*   AdwDialog *dialog = adw_dialog_new (); */

/*   adw_dialog_set_title (dialog, "Flip/Rotate"); */
/*   adw_dialog_set_can_close (dialog, TRUE); */
/*   adw_dialog_set_presentation_mode (dialog, ADW_DIALOG_FLOATING); */
/*   adw_dialog_set_follows_content_size (dialog, TRUE); */
/* #else */
/*   GtkWidget *window = gtk_window_new (); */
/*   gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW
 * (state->window)); */
/*   gtk_window_set_modal (GTK_WINDOW (window), TRUE); */
/*   gtk_window_set_title (GTK_WINDOW (window), "Flip/Rotate"); */
/*   gtk_window_set_resizable (GTK_WINDOW (window), FALSE); */
/*   gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW
 * (state->window)); */
/* #endif */

/*   GtkWidget *flip_vertical = gtk_flip_new_with_label(NULL, "Flip vertical");
 */
/*   GtkWidget *flip_horizontal =
 * gtk_flip_new_with_label_from_widget(GTK_RADIO_BUTTON(flip_vertical), "Flip
 * horizontal"); */
/*   GtkWidget *rotate_by_angle =
 * gtk_flip_new_with_label_from_widget(GTK_RADIO_BUTTON(flip_horizontal),
 * "Rotate by angle"); */

/*   // Connect signals */
/*   g_signal_connect(flip_vertical, "toggled", G_CALLBACK(on_flip_toggled),
 * "Option A"); */
/*   g_signal_connect(flip_vertical, "toggled", G_CALLBACK(on_flip_toggled),
 * "Option B"); */
/*   g_signal_connect(rotate_by_angle, "toggled", G_CALLBACK(on_flip_toggled),
 * "Option C"); */

/*   // Add radio buttons to the box */
/*   gtk_box_append(GTK_BOX(box), flip_a); */
/*   gtk_box_append(GTK_BOX(box), flip_b); */
/*   gtk_box_append(GTK_BOX(box), flip_c); */

/* #if HAS_LIBADWAITA && ADW_CHECK_VERSION(1, 5, 0) */
/*   adw_dialog_set_child (dialog, grid); */
/*   adw_dialog_present (dialog, state->window); */
/* #else */
/*   gtk_window_set_child (GTK_WINDOW (window), grid); */
/*   gtk_window_present (GTK_WINDOW (window)); */
/* #endif */
/* } */

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
/*   { "New",  "app.new",  "new",  { "<Primary>n", NULL }, G_CALLBACK
 * (on_new_file) }, */
/*   { "Open", "app.open", "open", { "<Primary>o", NULL }, G_CALLBACK
 * (on_open_file) }, */
/*   { "Save", "app.save", "save", { "<Primary>s", NULL }, G_CALLBACK
 * (on_save_file) }, */
/*   { "Quit", "app.quit", "quit", { "<Primary>q", NULL }, G_CALLBACK (on_quit)
 * }, */
/* }; */

/* static AppAction edit_actions[] = { */
/*   { "Undo", "app.undo", "undo", { "<Primary>z", NULL }, G_CALLBACK (on_undo)
 * }, */
/*   { "Redo", "app.redo", "redo", { "<Primary>y", NULL }, G_CALLBACK (on_redo)
 * }, */
/*   { "Resize", "app.resize", "resize", { NULL, NULL }, G_CALLBACK (on_resize)
 * }, */
/* }; */

// TODO handle some shit
/* Callback to collect PNG bytes from Cairo */
static cairo_status_t
png_write_callback (void *closure, const unsigned char *data, unsigned int length)
{
  GByteArray *array = (GByteArray *) closure;
  g_byte_array_append (array, data, length);
  return CAIRO_STATUS_SUCCESS;
}

/* Sets the given Cairo surface as an image on the clipboard as PNG data */
static void
set_image_to_clipboard (cairo_surface_t *surface, GtkWidget *parent)
{
  /* Encode the surface as PNG into a memory buffer */
  GByteArray *byte_array = g_byte_array_new ();
  cairo_status_t status = cairo_surface_write_to_png_stream (surface, png_write_callback, byte_array);
  if (status != CAIRO_STATUS_SUCCESS)
    {
      g_warning ("Failed to encode Cairo surface to PNG");
      g_byte_array_unref (byte_array);
      return;
    }

  /* Create a GBytes from the byte array */
  GBytes *png_bytes = g_byte_array_free_to_bytes (byte_array);

  /* Create a GdkContentProvider for the PNG data */
  GdkContentProvider *provider = gdk_content_provider_new_for_bytes ("image/png", png_bytes);

  /* Get the display from the parent widget */
  GdkDisplay *display = gtk_widget_get_display (parent);

  /* Get the clipboard from the display */
  GdkClipboard *clipboard = gdk_display_get_clipboard (display);

  /* Set the clipboard content */
  gdk_clipboard_set_content (clipboard, provider);

  /* Cleanup */
  g_object_unref (provider);
}

/*
 * g_input_stream_png_read:
 *   A custom read function that wraps a GInputStream so it can be used by
 *   cairo_image_surface_create_from_png_stream. It reads exactly the requested
 *   number of bytes from the stream, returning CAIRO_STATUS_SUCCESS on
 * success.
 */
static cairo_status_t
g_input_stream_png_read (void *closure, unsigned char *data, unsigned int length)
{
  GInputStream *stream = (GInputStream *) closure;
  unsigned int total_read = 0;
  while (total_read < length)
    {
      gssize ret = g_input_stream_read (stream, data + total_read, length - total_read, NULL, NULL);
      if (ret < 0)
        return CAIRO_STATUS_READ_ERROR;
      if (ret == 0)
        break; /* End-of-stream */
      total_read += ret;
    }
  if (total_read < length)
    return CAIRO_STATUS_READ_ERROR; /* Unexpected EOF */
  return CAIRO_STATUS_SUCCESS;
}

/*
 * on_image_surface_ready:
 *   This callback receives the Cairo image surface (or NULL on error).
 *   In this example it saves the surface to "clipboard_image.png".
 */
static void
on_image_surface_ready (cairo_surface_t *surface, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (surface && cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      /* g_print("Image surface obtained: %d×%d\n", */
      /*         cairo_image_surface_get_width(surface), */
      /*         cairo_image_surface_get_height(surface)); */

      // Now you can use the surface in your app
      if (state->selected_surface)
        cairo_surface_destroy (state->selected_surface);

      state->selected_surface = surface;
      state->selected_rect = (GdkRectangle) {
        gtk_adjustment_get_value (state->hadj) / state->zoom_level,
        gtk_adjustment_get_value (state->vadj) / state->zoom_level,
        cairo_image_surface_get_width (surface),
        cairo_image_surface_get_height (surface),
      };
      state->has_selection = TRUE;
      set_can_copy_surface (state);
      tool_select (state, TOOL_SELECT_RECTANGLE);
      gtk_widget_queue_draw (state->drawing_area);

      /* cairo_surface_write_to_png(surface, "clipboard_image.png"); */
      /* cairo_surface_destroy(surface); */
    }
  else
    {
      g_warning ("Failed to create Cairo surface from clipboard data.");
    }
}

/*
 * on_clipboard_image_received:
 *   This asynchronous callback is invoked when the clipboard read is complete.
 *   It obtains a GInputStream (expected to contain PNG data), creates a Cairo
 *   surface from it, then calls on_image_surface_ready with the result.
 */
static void
on_clipboard_image_received (GObject *clipboard, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  const char *mime_type = NULL;
  g_autoptr (GInputStream) stream = gdk_clipboard_read_finish (GDK_CLIPBOARD (clipboard), res, &mime_type, &error);
  if (error)
    {
      g_warning ("Error reading clipboard: %s", error->message);
      g_clear_error (&error);
      on_image_surface_ready (NULL, user_data);
      return;
    }
  if (!stream)
    {
      g_warning ("Clipboard returned no data.");
      on_image_surface_ready (NULL, user_data);
      return;
    }

  /* Create a Cairo surface from the PNG data stream. */
  cairo_surface_t *surface = cairo_image_surface_create_from_png_stream (g_input_stream_png_read, stream);
  on_image_surface_ready (surface, user_data);
}

/*
 * retrieve_clipboard_image:
 *   Retrieves an image from the clipboard (PNG only in this example) and
 * decodes it into a Cairo image surface. The resulting surface (or NULL on
 * failure) is passed to on_image_surface_ready.
 */
static void
retrieve_clipboard_image (GtkWidget *widget, gpointer user_data)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkClipboard *clipboard = gdk_display_get_clipboard (display);
  /* In this example we only support PNG. (Extend here for other MIME types.)
   */
  const char *mime_types[] = { "image/png", NULL };
  gdk_clipboard_read_async (clipboard, mime_types, 0, NULL /* No GCancellable */, on_clipboard_image_received, user_data);
}

static void
on_cut (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (state->selected_surface)
    {
      set_image_to_clipboard (state->selected_surface, state->window);
      clear_selection (state);
      gtk_widget_queue_draw (state->drawing_area);
      // Fill original area with background color
      // fill_rectangle(state->main_surface, &state->selected_rect,
      // state->secondary_color); clear_selection(state);
    }
}

static void
on_copy (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (state->selected_surface)
    set_image_to_clipboard (state->selected_surface, state->window);
}

static void
on_paste (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (state->has_selection)
    commit_selection (state);

  retrieve_clipboard_image (state->window, user_data);
}

static void
on_selectall (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (state->is_dragging_selection)
    return;

  if (state->has_selection)
    commit_selection (state);

  tool_select (state, TOOL_SELECT_RECTANGLE);
  gtk_widget_queue_draw (state->drawing_area);

  if (state->selected_surface)
    g_clear_pointer (&state->selected_surface, cairo_surface_destroy);

  state->selected_rect.x = state->selected_rect.y = 0;
  state->selected_rect.width = cairo_image_surface_get_width (state->main_surface);
  state->selected_rect.height = cairo_image_surface_get_height (state->main_surface);
  state->selected_surface = cut_rectangle (state->main_surface, &state->selected_rect, state->s_color);
  state->has_selection = TRUE;
  set_can_copy_surface (state);
}

static void
on_zoom_in (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  zoom_in (NULL, state);
}

static void
on_zoom_out (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  zoom_out (NULL, state);
}

static void
on_zoom_reset (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  zoom_reset (NULL, state);
}

////
static const GActionEntry file_actions[] = {
  { "new",  on_new_file,  NULL, NULL, NULL },
  { "open", on_open_file, NULL, NULL, NULL },
  { "save", on_save_file, NULL, NULL, NULL },
  { "quit", on_quit,      NULL, NULL, NULL },
};

static const GActionEntry edit_actions[] = {
  { "undo",      on_undo,      NULL, NULL, NULL },
  { "redo",      on_redo,      NULL, NULL, NULL },

  { "cut",       on_cut,       NULL, NULL, NULL },
  { "copy",      on_copy,      NULL, NULL, NULL },
  { "paste",     on_paste,     NULL, NULL, NULL },
  { "selectall", on_selectall, NULL, NULL, NULL },

  { "resize",    on_resize,    NULL, NULL, NULL },
};

static const GActionEntry view_actions[] = {
  { "showgrid",  on_toggle_show_grid, NULL, "false", NULL },
  // TODO
  /* { "antialiasing", on_toggle_antialiasing, NULL, "false", NULL }, */

  // TODO
  { "zoomin",    on_zoom_in,          NULL, NULL,    NULL },
  { "zoomout",   on_zoom_out,         NULL, NULL,    NULL },
  { "zoomreset", on_zoom_reset,       NULL, NULL,    NULL },
};

/* static const GActionEntry image_actions[] = */
/*   { */
/*     { "rotate", on_rotate, NULL, NULL, NULL }, */
/*     { "invertcolors", on_invert_colors, NULL, NULL, NULL }, */
/*   }; */

// clang-format off
static const struct
{
  const gchar *action;
  const gchar *accels[3]; // TODO Must be NULL sentinel
} app_accels[] =
  {
    { "app.new",  { "<Primary>n" } },
    { "app.open", { "<Primary>o" } },
    { "app.save", { "<Primary>s" } },
    { "app.quit", { "<Primary>q" } },

    { "app.cut",       { "<Primary>x" } },
    { "app.copy",      { "<Primary>c" } },
    { "app.paste",     { "<Primary>v" } },
    { "app.selectall", { "<Primary>a" } },

    { "app.undo", { "<Primary>z" } },
    { "app.redo", { "<Primary>y" } },

    { "app.zoomin",    { "<Primary>plus", "<Primary>equal" } },
    { "app.zoomout",   { "<Primary>minus" } },
    { "app.zoomreset", { "<Primary>0" } },
  };
// clang-format on

static void
update_cursor_position (AppState *state, double x, double y)
{
  cairo_surface_t *surface = state->main_surface;
  const int width = cairo_image_surface_get_width (surface);
  const int height = cairo_image_surface_get_height (surface);

  int color_depth = gpaint_cairo_get_color_depth (state->main_surface);
  char image_info[256];

  if (color_depth)
    g_snprintf (image_info, sizeof (image_info), "%d×%d×%d", width, height, color_depth);
  else
    g_snprintf (image_info, sizeof (image_info), "%d×%d", width, height);

  gtk_label_set_text (GTK_LABEL (state->image_info), image_info);

  int px = x / state->zoom_level;
  int py = y / state->zoom_level;

  state->cursor_x = x;
  state->cursor_y = y;

  if (x < 0 || px >= width || y < 0 || py >= height)
    {
      gtk_widget_set_visible (state->current_position, FALSE);
      // TODO
      gtk_label_set_text (GTK_LABEL (state->current_position), "");
    }
  else
    {
      gtk_widget_set_visible (state->current_position, TRUE);
      gchar position[256];
      g_snprintf (position, sizeof (position), "[%d, %d]", MIN (px, width - 1), MIN (py, height - 1));
      gtk_label_set_text (GTK_LABEL (state->current_position), position);
    }

  gtk_widget_queue_draw (state->drawing_area);
}

///
// TODO
/*
 * Returns the intersection (in child widget coordinates) of the child's
 * allocation and the viewport's allocation (which represents the visible
 * area). Assumes that `child` is a descendant of a GtkViewport.
 */
static void
my_get_visible_rect (AppState *state, double *x, double *y, double *width, double *height)
{
  *x = gtk_adjustment_get_value (state->hadj);
  *y = gtk_adjustment_get_value (state->vadj);
  *width = gtk_adjustment_get_page_size (state->hadj);
  *height = gtk_adjustment_get_page_size (state->vadj);
}
///

static void
draw_callback (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  const double pixel_size = state->zoom_level;
  const int surface_width = cairo_image_surface_get_width (state->main_surface);
  const int surface_height = cairo_image_surface_get_height (state->main_surface);
  cairo_pattern_t *pattern;
  const double bg[2] = { 0x54 / 255.0, 0xA8 / 255.0 };
  const double line_color = (bg[0] + bg[1]) / 2.0; // TODO

  double s_x, s_y, s_width, s_height;
  my_get_visible_rect (state, &s_x, &s_y, &s_width, &s_height);

  // TODO
  const int d = 2 * MAX (8.0, pixel_size); // TODO: Preserve some space for if drawing
                                           // area scaled and not properly aligned.
  const double r = pixel_size;
  const GdkRectangle v = {
    .x = (int) (s_x / r) - d,
    .y = (int) (s_y / r) - d,
    .width = (int) (s_width / r) + 2 * d,
    .height = (int) (s_height / r) + 2 * d,
  };

  // draw_transparent_square (cr, v.x, v.y, v.width, v.height, 1.0);

  cairo_save (cr);
  cairo_scale (cr, pixel_size, pixel_size);
  // TODO use draw_transparent_square
  double color = (pixel_size > 4.0) ? bg[1] : (bg[0] + bg[1]) / 2.0;
  cairo_set_source_rgb (cr, color, color, color);
  cairo_rectangle (cr, v.x, v.y, v.width, v.height);
  cairo_fill (cr);
  cairo_restore (cr);

  // TODO. Kinda slow
  if (pixel_size > 4.0)
    {
      /* Draw checkerboard background */
      cairo_save (cr);
      int k = (int) log2 (pixel_size); // TODO make it adaptive for zoom.
      cairo_scale (cr, pixel_size / k, pixel_size / k);

      int grid_width = (v.x + v.width) * k;
      int grid_height = (v.y + v.height) * k;

      cairo_new_path (cr);
      const double dash[] = { 1.0, 1.0 };
      cairo_set_dash (cr, dash, 1, 0);
      cairo_set_line_width (cr, 1.0);

      for (int y = v.y * k; y < grid_height; y++)
        {
          double x0, x1, y0, y1;

          x0 = v.x * k + (y & 1);
          x1 = grid_width;
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
  if ((!state->is_drawing || !state->tool->override_main_surface) && !state->selected_surface)
    {
      cairo_save (cr);
      cairo_scale (cr, pixel_size, pixel_size);

      pattern = cairo_pattern_create_for_surface (state->main_surface);
      cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);
      cairo_set_source (cr, pattern);

      cairo_rectangle (cr, v.x, v.y, v.width, v.height);
      cairo_clip (cr);

      cairo_paint (cr);
      cairo_pattern_destroy (pattern);
      cairo_restore (cr);
    }

  // TODO
  // Overlay preview layer if available:
  if (state->preview_surface)
    {
      cairo_save (cr);
      // TODO cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
      cairo_scale (cr, pixel_size, pixel_size);
      pattern = cairo_pattern_create_for_surface (state->preview_surface);
      cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST); // TODO
      cairo_set_source (cr, pattern);
      cairo_rectangle (cr, v.x, v.y, v.width, v.height);
      cairo_clip (cr);
      // cairo_set_source_surface(cr, state->preview_surface, 0, 0);
      cairo_paint (cr);
      cairo_pattern_destroy (pattern);
      cairo_restore (cr);
    }

  // TODO
  if (state->selected_surface)
    {
      cairo_save (cr);
      cairo_scale (cr, pixel_size, pixel_size);
      pattern = cairo_pattern_create_for_surface (state->main_surface);
      cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);
      cairo_set_source (cr, pattern);
      cairo_paint (cr);
      cairo_pattern_destroy (pattern);

      cairo_pattern_t *selection_pattern = cairo_pattern_create_for_surface (state->selected_surface);
      cairo_pattern_set_filter (selection_pattern, CAIRO_FILTER_NEAREST);

      // Position the selection pattern
      cairo_matrix_t matrix;
      cairo_matrix_init_translate (&matrix, -state->selected_rect.x, -state->selected_rect.y);
      cairo_pattern_set_matrix (selection_pattern, &matrix);

      // Set source and paint
      cairo_set_source (cr, selection_pattern);
      cairo_rectangle (cr, state->selected_rect.x, state->selected_rect.y, state->selected_rect.width, state->selected_rect.height);
      cairo_clip (cr);
      cairo_pattern_destroy (selection_pattern);

      cairo_paint (cr);
      cairo_restore (cr);
    }

  if (g_variant_get_boolean (g_action_get_state (state->show_grid_action)) && pixel_size >= 4.0)
    {
      cairo_save (cr);

      double t = line_color;
      cairo_set_source_rgb (cr, t, t, t);
      cairo_set_line_width (cr, 1.0);

      // TODO cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
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

  if (state->tool->type == TOOL_SELECT_RECTANGLE)
    {
      cairo_save (cr);
      cairo_scale (cr, pixel_size, pixel_size);
      cairo_set_line_width (cr, 2.0 / pixel_size);

      // Define the rectangle path
      cairo_rectangle (cr, state->selected_rect.x, state->selected_rect.y, state->selected_rect.width, state->selected_rect.height);

      // First stroke: black dashes
      double dashes1[] = { 4.0 / pixel_size, 4.0 / pixel_size };
      cairo_set_dash (cr, dashes1, 2, 0); // No offset
      gdk_cairo_set_source_rgba (cr, &GPAINT_GDK_BLACK);
      cairo_stroke_preserve (cr);

      // Second stroke: cyan dashes with offset
      double dashes2[] = { 4.0 / pixel_size, 4.0 / pixel_size };
      cairo_set_dash (cr, dashes2, 2, 4.0 / pixel_size); // Offset by dash length to alternate
      cairo_set_source_rgb (cr, 0.0, 1.0, 1.0);
      cairo_stroke (cr); // Stroke the remaining path
      cairo_restore (cr);
    }

  /* if (state->tool->type == TOOL_SELECT_RECTANGLE) */
  /*   { */
  /*     cairo_save (cr); */
  /*     cairo_scale (cr, pixel_size, pixel_size); */
  /*     cairo_set_line_width (cr, 2.0 / pixel_size); */

  /*     // Define the dash pattern: 4 units on, 4 units off */
  /*     const double dashes[] = { 4.0 / pixel_size, 4.0 / pixel_size }; */
  /*     const int num_dashes = (int) G_N_ELEMENTS (dashes); */

  /*     // TODO */
  /*     // First pass: draw black dashes */
  /*     cairo_set_dash (cr, dashes, num_dashes, 0); */
  /*     cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); */
  /*     cairo_rectangle (cr, */
  /*                      state->selected_rect.x, state->selected_rect.y, */
  /*                      state->selected_rect.width,
   * state->selected_rect.height); */
  /*     cairo_stroke (cr); */

  /*     // Second pass: draw cyan dashes, offset to interleave with black */
  /*     cairo_set_dash (cr, dashes, num_dashes, dashes[0]); */
  /*     cairo_set_source_rgb (cr, 0.0, 1.0, 1.0); */
  /*     cairo_rectangle (cr, state->selected_rect.x, state->selected_rect.y,
   */
  /*                      state->selected_rect.width,
   * state->selected_rect.height); */
  /*     cairo_stroke (cr); */

  /*     cairo_restore (cr); */
  /*   } */

  /* if (state->tool->type == TOOL_SELECT_RECTANGLE) */
  /*   { */
  /*     cairo_save (cr); */
  /*     cairo_scale (cr, pixel_size, pixel_size); */
  /*     cairo_set_line_width (cr, 2.0 / pixel_size); */

  /*     // Define a dashed line pattern */
  /*     double dashes[] = { 4.0 / pixel_size, 4.0 / pixel_size }; // 2 units
   * on, 2 units off */
  /*     cairo_set_dash (cr, dashes, 2, 0);                        // Set the
   * dash pattern */
  /*     cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);                 // Black
   * color */

  /*     // Draw a rectangle */
  /*     cairo_rectangle (cr, state->selected_rect.x, state->selected_rect.y,
   */
  /*                      state->selected_rect.width,
   * state->selected_rect.height); */
  /*     cairo_stroke (cr); */
  /*     cairo_restore (cr); */
  /*   } */

  // TODO
  // Overlay cursor layer if available:
  if (state->tool->draw_cursor_handler && state->cursor_x >= 0.0 && state->cursor_y >= 0.0)
    {
      cairo_save (cr);
      state->tool->draw_cursor_handler (state, cr);
      cairo_restore (cr);
    }

  // TODO Drawing border...
  /* cairo_save (cr); */
  /* cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); */
  /* cairo_set_line_width (cr, 1.5); // TODO: 1.5 because 1.0 is invisible...
   */
  /* cairo_rectangle (cr, 0, 0, width, height); */
  /* cairo_stroke (cr); */
  /* cairo_restore (cr); */
}

/* static GdkTexture * */
/* create_texture_from_raw_data (int height, int width, int rowstride, const
 * guchar *raw_data) */
/* { */
/*   g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new_from_data ((guchar *)
 * raw_data, GDK_COLORSPACE_RGB, */
/*                                                            /\* has alpha *\/
 * TRUE, 8, width, height, */
/*                                                            rowstride, NULL,
 * NULL); */
/*   if (!pixbuf) */
/*     return NULL; */
/*   GdkTexture *texture = gdk_texture_new_for_pixbuf (pixbuf); */
/*   return texture; */
/* } */

/* Updates cursor based on current tool – update per GTK4 docs if needed */
static void
update_cursor (AppState *state)
{
  g_autoptr (GdkCursor) cursor = NULL;

  // TODO
  if (state->has_selection && my_dots_in_rect ((int) (state->cursor_x / state->zoom_level),
                                               (int) (state->cursor_y / state->zoom_level),
                                               &state->selected_rect))
    cursor = gdk_cursor_new_from_name (state->is_dragging_selection ? "grabbing" : "grab", NULL);
  else if (state->tool->draw_cursor_handler)
    cursor = gdk_cursor_new_from_name ("none", NULL);
  else if (state->cursors[state->tool->type])
    {
      gtk_widget_set_cursor (state->drawing_area, state->cursors[state->tool->type]);
      return;
    }

  gtk_widget_set_cursor (state->drawing_area, cursor);
}

static void
on_click_pressed (GtkGestureDrag *gesture, double x, double y, gpointer user_data, GdkRGBA *p_color, GdkRGBA *s_color)
{
  AppState *state = (AppState *) user_data;
  int px = (int) (x / state->zoom_level);
  int py = (int) (y / state->zoom_level);

  if (state->has_selection)
    update_cursor (state);

  if (state->tool->type == TOOL_SELECT_RECTANGLE && my_dots_in_rect (px, py, &state->selected_rect))
    {
      state->is_dragging_selection = TRUE;
      state->drag_start_x = x / state->zoom_level;
      state->drag_start_y = y / state->zoom_level;
      state->selection_start_x = state->selected_rect.x;
      state->selection_start_y = state->selected_rect.y;
      update_cursor (state); // TODO...
      return;
    }

  if (state->has_selection)
    commit_selection (state);

  if (state->is_drawing && state->preview_surface)
    {
      g_clear_pointer (&state->preview_surface, cairo_surface_destroy);
      state->is_drawing = FALSE;
      gtk_widget_queue_draw (state->drawing_area);
      return;
    }

  if (state->tool->drag_begin)
    state->tool->drag_begin (state);

  state->is_drawing = TRUE;
  state->start_point.x = px;
  state->start_point.y = py;
  state->last_point = state->start_point;
  state->p_color = p_color;
  state->s_color = s_color;

  if (state->preview_surface)
    g_clear_pointer (&state->preview_surface, cairo_surface_destroy);

  state->preview_surface = create_surface (cairo_image_surface_get_width (state->main_surface), cairo_image_surface_get_height (state->main_surface));

  if (state->tool->override_main_surface)
    copy_surface (state->preview_surface, state->main_surface);

  // TODO
  /* [TOOL_LINE]             	= { "Line", &global_line_tool }, */
  /* [TOOL_RECTANGLE]        	= { "Rect", &global_rectangle_tool }, */
  /* [TOOL_ELLIPSE]          	= { "Ellipse", &global_ellipse_tool }, */
  /* [TOOL_SELECT_RECTANGLE] 	= { "Select rectangle",
   * &global_select_rectangle_tool }, */
  /* [TOOL_DRAG]             	= { "Drag", &global_drag_tool }, */

  if (state->tool->type == TOOL_FREEHAND || state->tool->type == TOOL_SYMMETRIC_FREEHAND || state->tool->type == TOOL_ERASER || state->tool->type == TOOL_BRUSH || state->tool->type == TOOL_BUCKET || state->tool->type == TOOL_PICKER) // TODO
    {
      state->tool->draw_handler (state, px, py, px, py);
      gtk_widget_queue_draw (state->drawing_area);
    }
}

static void
on_click_released (GtkGestureDrag *gesture, double x, double y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  state->is_dragging_selection = FALSE;

  if (state->has_selection)
    update_cursor (state);

  if (!state->is_drawing)
    return;

  if (state->tool->drag_end)
    {
      state->tool->drag_end (state);
      state->is_drawing = FALSE;
    }

  if (state->tool->type == TOOL_SELECT_RECTANGLE)
    {
      if (state->selected_surface)
        g_clear_pointer (&state->selected_surface, cairo_surface_destroy);

      state->selected_surface = cut_rectangle (state->main_surface, &state->selected_rect, state->s_color);
      state->has_selection = TRUE;
      set_can_copy_surface (state);
    }
  else if (state->selected_surface)
    g_clear_pointer (&state->selected_surface, cairo_surface_destroy);

  if (state->preview_surface)
    {
      save_backup (&state->backup_manager, state->main_surface);

      cairo_t *cr = create_cairo (state->main_surface, state->tool->override_main_surface ? CAIRO_OPERATOR_SOURCE : CAIRO_OPERATOR_OVER, state->antialiasing);
      cairo_set_source_surface (cr, state->preview_surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      g_clear_pointer (&state->preview_surface, cairo_surface_destroy);
    }

  state->is_drawing = FALSE;

  /// TODO
  // gtk_widget_queue_draw (gpaint_layers_widget_get_selected_button
  // (GPAINT_LAYERS_WIDGET (state->layers)));
  //  TODO gpaint_preview_widget_queue_redraw (GPAINT_LAYERS_WIDGET
  //  (state->layers));
  //  TODO gpaint_layers_widget_queue_redraw (state->layers);
  ///

  gtk_widget_queue_draw (state->drawing_area);
}

static void
on_click_primary_pressed (GtkGestureDrag *gesture, double x, double y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  on_click_pressed (gesture, x, y, user_data, &state->primary_color, &state->secondary_color);
}

static void
on_click_secondary_pressed (GtkGestureDrag *gesture, double x, double y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  on_click_pressed (gesture, x, y, user_data, &state->secondary_color, &state->primary_color);
}

static void
drag_update (GtkGestureDrag *gesture, double x, double y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  /* if (state->dragging && state->has_selection) { */
  /*     // Get current mouse position in surface coordinates */
  /*     double current_x, current_y; */
  /*     gtk_gesture_drag_get_offset(gesture, &current_x, &current_y); */

  /*     // Calculate delta movement since last update */
  /*     double dx = (current_x - state->last_drag_x) / state->zoom_level; */
  /*     double dy = (current_y - state->last_drag_y) / state->zoom_level; */

  /*     // Update selection position */
  /*     state->selected_rect.x += dx; */
  /*     state->selected_rect.y += dy; */

  /*     // Constrain to canvas boundaries */
  /*     state->selected_rect.x = CLAMP(state->selected_rect.x, 0, */
  /*         cairo_image_surface_get_width(state->main_surface) -
   * state->selected_rect.width); */
  /*     state->selected_rect.y = CLAMP(state->selected_rect.y, 0, */
  /*         cairo_image_surface_get_height(state->main_surface) -
   * state->selected_rect.height); */

  /*     // Store current position for next update */
  /*     state->last_drag_x = current_x; */
  /*     state->last_drag_y = current_y; */

  /*     gtk_widget_queue_draw(state->drawing_area); */
  /* } */

  if (state->tool->drag_update)
    state->tool->drag_update (state, x, y);
}

static gboolean
on_scroll (GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  /* Retrieve the current event (may be NULL) to check modifiers */
  GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
  GdkModifierType modifiers = event ? gdk_event_get_modifier_state (event) : GDK_NO_MODIFIER_MASK;

  if (modifiers & GDK_CONTROL_MASK)
    {
      double d = (dy < 0) ? 1.2 : 0.8;
      zoom_set_value (state, state->zoom_level * d);

      /* Return TRUE to indicate that the event has been handled
         and should not propagate further. */
      return TRUE;
    }

  gtk_widget_queue_draw (state->drawing_area); // TODO
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

// TODO
static void
export_image (AppState *state, const gchar *filename)
{
  save_image (filename, state->main_surface, 1, NULL);

  /* /\* For non-PNG formats, grab a GdkPixbuf from the surface and save it. */
  /*    This allows formats such as JPG, BMP, or GIF. *\/ */
  /* g_autoptr (GdkPixbuf) pixbuf; */

  /* pixbuf = gdk_pixbuf_new_from_data (cairo_image_surface_get_data
   * (state->main_surface), */
  /*                                    GDK_COLORSPACE_RGB, // TODO colospace
   */
  /*                                    TRUE,               // TODO has alpha
   */
  /*                                    8,                  // TODO rowstride
   */
  /*                                    cairo_image_surface_get_width
   * (state->main_surface), */
  /*                                    cairo_image_surface_get_height
   * (state->main_surface), */
  /*                                    cairo_image_surface_get_stride
   * (state->main_surface), */
  /*                                    NULL, NULL); */

  /* if (pixbuf != NULL) */
  /*   { */
  /*     GError *error = NULL; */
  /*     if (!gdk_pixbuf_save (pixbuf, filename, ext ? ext + 1 : "png", &error,
   * NULL)) */
  /*       { */
  /*         g_warning ("Failed to save image: %s", error->message); */
  /*         g_error_free (error); */
  /*       } */
  /*   } */
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
  GError *error = NULL;
  // TODO
  /* if (!save_image (path, state->main_surface, 1, &error)) */
  /*   g_warning("Failed to save image %s", error->message); */

  GList surfaces = {
    .data = state->main_surface,
    .next = NULL,
    .prev = NULL,
  };

  for (size_t i = 0; i < G_N_ELEMENTS (gpaint_formats); i++)
    {
      error = NULL;
      puts (gpaint_formats[i].extensions[0]);
      if (!save_surfaces_with_ffmpeg (g_strdup_printf ("%s.%s", path, gpaint_formats[i].extensions[0]), &surfaces, gpaint_formats[i].codec_id, 1, gpaint_formats[i].default_options, &error))
        g_warning ("Failed to save image %s", error->message);
    }

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

  cairo_surface_t *new_surface = load_image_to_cairo_surface (path);
  cairo_status_t status = cairo_surface_status (new_surface);

  if (status != CAIRO_STATUS_SUCCESS)
    {
      /* Handle the error (e.g., show a message to the user) */
      g_printerr ("Failed to load image: %s\n", cairo_status_to_string (status));
      cairo_surface_destroy (new_surface);
      return;
    }

  /* Get image dimensions */
  int width = cairo_image_surface_get_width (new_surface);
  int height = cairo_image_surface_get_height (new_surface);

  /* Replace the existing main_surface with the new image surface */
  if (state->main_surface)
    g_clear_pointer (&state->main_surface, cairo_surface_destroy);

  state->main_surface = new_surface;

  /* Update the drawing area size */
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (int) (width * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (int) (height * state->zoom_level));

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

// clang-format off
static ValueItem line_widths[] =
  {
    { "./icons/line1.png", 1 },
    { "./icons/line2.png", 2 },
    { "./icons/line3.png", 3 },
    { "./icons/line4.png", 4 },
    { "./icons/line5.png", 5 },
  };

static ValueItem fills[] =
  {
    { "./icons/fill1.png", FILL_TRANSPARENT },
    { "./icons/fill2.png", FILL_SECONDARY },
    { "./icons/fill3.png", FILL_PRIMARY },
  };

static ValueItem eraser_sizes[] =
  {
    { "./icons/fill1.png", 2 },
    { "./icons/fill2.png", 4 },
    { "./icons/fill3.png", 6 },
  };
// clang-format on

static void
on_width_selected (gpointer user_data, int width)
{
  AppState *state = (AppState *) user_data;
  state->width = (double) width;
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
  const GdkRGBA t = state->primary_color;
  state->primary_color = state->secondary_color;
  state->secondary_color = t;
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), &state->primary_color);
}

static void
on_color_selected (GtkGestureClick *self, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  const GdkRGBA *color = (const GdkRGBA *) g_object_get_data (G_OBJECT (self), "color");
  GdkRGBA *target = (GdkRGBA *) g_object_get_data (G_OBJECT (self), "target"); // TODO RENAME

  *target = *color;
  gpaint_color_swap_button_update_colors (GPAINT_COLOR_SWAP_BUTTON (state->color_swap_button));
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), &state->primary_color);
}

static void
tool_toggled (GtkToggleButton *btn, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (state->has_selection && state->tool->type != TOOL_SELECT_RECTANGLE)
    commit_selection (state);

  state->tool = (const Tool *) g_object_get_data (G_OBJECT (btn), "tool");
  update_cursor (state);
}

static void
draw_colored_square0 (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer user_data)
{
  const GdkRGBA *color = (const GdkRGBA *) user_data;
  draw_colored_square (cr, color, 0, 0, width, height, 8.0);
  /* const GdkRGBA border = { 0.0, 0.0, 0.0, 1.0 }; */
  /* const gdouble border_width = 1.0; */
  /* cairo_save (cr); */
  /* gdk_cairo_set_source_rgba (cr, &border); */
  /* cairo_set_line_width (cr, border_width); */
  /* cairo_rectangle (cr, border_width / 2.0, border_width / 2.0, */
  /*                  width - border_width, height - border_width); */
  /* cairo_stroke (cr); */
  /* cairo_restore (cr); */
}

static GtkWidget *
create_color_grid (AppState *state)
{
  enum
  {
    SQUARE_SIZE = 24
  };

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *grid = gtk_grid_new ();

#define C(r, g, b) { r, g, b, 1.0 }
  static const GdkRGBA colors[][2] = {
    { C (1.0, 1.0, 1.0), C (0.0, 0.0, 0.0) },
    { C (0.5, 0.5, 0.5), C (0.125, 0.125, 0.125) },
    { { 0.0, 0.0, 0.0, 0.0 }, C (0.875, 0.875, 0.875) },
    { C (1.0, 0.0, 0.0), C (1.0, 1.0, 0.0) },
    { C (0.0, 1.0, 0.0), C (0.0, 1.0, 1.0) },
    { C (0.0, 0.0, 1.0), C (1.0, 0.0, 1.0) },
  };
#undef C

  g_autoptr (GtkCssProvider) css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (css_provider, "button { padding: 0; }");

  for (size_t i = 0; i < G_N_ELEMENTS (colors); i++)
    for (size_t j = 0; j < G_N_ELEMENTS (colors[0]); j++)
      {
        GtkWidget *btn = gtk_button_new ();
        GtkWidget *square = gtk_drawing_area_new ();

        gtk_style_context_add_provider (gtk_widget_get_style_context (btn), GTK_STYLE_PROVIDER (css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

        gtk_button_set_has_frame (GTK_BUTTON (btn), FALSE); // TODO
        // gtk_widget_set_size_request(btn, SQUARE_SIZE, SQUARE_SIZE);
        gtk_widget_set_size_request (square, SQUARE_SIZE, SQUARE_SIZE);

        gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (square), draw_colored_square0, &colors[i][j], NULL);
        gtk_button_set_child (GTK_BUTTON (btn), square);

        // clang-format off
        const struct
        {
          GdkRGBA *target;
          int button;
        } t[] =
          {
            { &state->primary_color, GDK_BUTTON_PRIMARY },
            { &state->secondary_color, GDK_BUTTON_SECONDARY },
          };
        // clang-format on

        for (size_t k = 0; k < G_N_ELEMENTS (t); k++)
          {
            GtkGesture *click = gtk_gesture_click_new ();
            g_object_set_data (G_OBJECT (click), "color", (gpointer) &colors[i][j]);
            g_object_set_data (G_OBJECT (click), "target", t[k].target);
            gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), t[k].button);
            g_signal_connect (click, "pressed", G_CALLBACK (on_color_selected), state);
            gtk_widget_add_controller (btn, GTK_EVENT_CONTROLLER (click));
          }

        /* g_signal_connect (btn, "clicked", G_CALLBACK (on_color_selected),
         * state); */

        gtk_grid_attach (GTK_GRID (grid), btn, i, j, 1, 1);
      }

  gtk_box_append (GTK_BOX (hbox), grid);
  return hbox;
}

static GtkWidget *
create_toolbar_grid (AppState *state)
{
  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_spacing (GTK_BOX (vbox), 8);
  GtkWidget *grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 4);

  // TODO
  gtk_widget_set_margin_start (vbox, 4);
  gtk_widget_set_margin_end (vbox, 4);

  /* gtk_widget_set_margin_top (grid, 2); */
  /* gtk_widget_set_margin_bottom (grid, 2); */
  /* gtk_widget_set_margin_start (grid, 8); */
  /* gtk_widget_set_margin_end (grid, 8); */
  /* gtk_grid_set_row_spacing (GTK_GRID (grid), 4); */
  /* gtk_grid_set_column_spacing (GTK_GRID (grid), 4); */

  GtkWidget *prev_button = NULL;

  for (int i = 0; i < TOOLS_COUNT; i++)
    {
      const Tool *tool = state->tools[i].tool;
      GtkWidget *btn = gtk_toggle_button_new ();

      GBytes *data = g_bytes_new_static (tool->icon->data, tool->icon->size);
      GError *error = NULL;

      g_autoptr (GdkTexture) texture = gdk_texture_new_from_bytes (data, &error);

      if (!texture)
        g_warning ("Failed to create texture for tool %s: %s", state->tools[i].label, error->message);

      GtkWidget *icon_image = gtk_image_new_from_paintable (GDK_PAINTABLE (texture));

      gtk_button_set_child (GTK_BUTTON (btn), icon_image);
      gtk_widget_set_tooltip_text (btn, state->tools[i].label);
      g_object_set_data (G_OBJECT (btn), "tool", (gpointer) tool);
      g_signal_connect (btn, "toggled", G_CALLBACK (tool_toggled), state);

      int col = i % 2;
      int row = i / 2;
      gtk_grid_attach (GTK_GRID (grid), btn, col, row, 1, 1);

      if (tool->type == TOOL_FREEHAND)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (btn), TRUE);

      if (prev_button)
        gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (btn), GTK_TOGGLE_BUTTON (prev_button));

      state->tools[i].btn = prev_button = btn;
    }

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
  clear_canvas (state->main_surface);
  gtk_widget_queue_draw (state->drawing_area);
}

/* static GtkWidget * */
/* create_actions_toolbar (GtkApplication *app, AppState *state, const gchar
 * *name, AppAction *actions, size_t n_actions) */
/* { */
/*   for (size_t i = 0; i < n_actions; i++) */
/*     { */
/*       GSimpleAction *action = g_simple_action_new (actions[i].short_key,
 * NULL); */
/*       g_signal_connect (action, "activate", actions[i].callback, state); */
/*       g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (action)); */

/*       if (actions[i].action != NULL) */
/*         *actions[i].action = action; */
/*     } */

/*   GMenu *menu = g_menu_new (); */

/*   for (size_t i = 0; i < n_actions; i++) */
/*     g_menu_append (menu, actions[i].label, actions[i].key); */

/*   GtkWidget *menu_btn = gtk_menu_button_new (); */
/*   gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_btn), G_MENU_MODEL
 * (menu)); */
/*   gtk_menu_button_set_label (GTK_MENU_BUTTON (menu_btn), name); */

/*   for (size_t i = 0; i < n_actions; i++) */
/*     gtk_application_set_accels_for_action (app, actions[i].key,
 * actions[i].accel); */

/*   return menu_btn; */
/* } */

static GtkWidget *
create_file_toolbar (AppState *state)
{
  g_autoptr (GMenu) file = g_menu_new ();

  g_menu_append (file, "New", "app.new");
  g_menu_append (file, "Open", "app.open");
  g_menu_append (file, "Save", "app.save");
  g_menu_append (file, "Quit", "app.quit");

  GtkWidget *file_btn = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (file_btn), G_MENU_MODEL (file));
  gtk_menu_button_set_label (GTK_MENU_BUTTON (file_btn), "File");
  return file_btn;
}

static GtkWidget *
create_edit_toolbar (AppState *state)
{
  g_autoptr (GMenu) edit = g_menu_new ();

  g_menu_append (edit, "Undo", "app.undo");
  g_menu_append (edit, "Redo", "app.redo");

  g_menu_append (edit, "Cut", "app.cut");
  g_menu_append (edit, "Copy", "app.copy");
  g_menu_append (edit, "Paste", "app.paste");
  g_menu_append (edit, "Select all", "app.selectall");

  g_menu_append (edit, "Resize", "app.resize");

  GtkWidget *edit_btn = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (edit_btn), G_MENU_MODEL (edit));
  gtk_menu_button_set_label (GTK_MENU_BUTTON (edit_btn), "Edit");
  return edit_btn;
}

static const struct
{
  const char *label;
  const char *key;
  cairo_antialias_t value;
} antialiasing_modes[] = {
  { "None",     "none",     CAIRO_ANTIALIAS_NONE     },
  { "Default",  "default",  CAIRO_ANTIALIAS_DEFAULT  },
  { "Gray",     "gray",     CAIRO_ANTIALIAS_GRAY     },
  { "Subpixel", "subpixel", CAIRO_ANTIALIAS_SUBPIXEL },
  { "Fast",     "fast",     CAIRO_ANTIALIAS_FAST     },
  { "Good",     "good",     CAIRO_ANTIALIAS_GOOD     },
  { "Best",     "best",     CAIRO_ANTIALIAS_BEST     },
};

static void
on_antialiasing_changed (GSimpleAction *action,
                         GVariant *value,
                         gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  const gchar *mode = g_variant_get_string (value, NULL);
  g_simple_action_set_state (action, value);
  // TODO
  for (size_t i = 0; i < G_N_ELEMENTS (antialiasing_modes); i++)
    if (g_strcmp0 (antialiasing_modes[i].key, mode) == 0)
      state->antialiasing = antialiasing_modes[i].value;
}

static void
setup_antialiasing_action (AppState *state)
{
  GSimpleAction *antialiasing_action = g_simple_action_new_stateful ("antialiasing",
                                                                     G_VARIANT_TYPE_STRING,
                                                                     g_variant_new_string (antialiasing_modes[0].key));
  g_signal_connect (antialiasing_action, "change-state", G_CALLBACK (on_antialiasing_changed), state);
  g_action_map_add_action (G_ACTION_MAP (state->application), G_ACTION (antialiasing_action));
}

static GtkWidget *
create_view_toolbar (AppState *state)
{
  // Create the main 'View' menu
  g_autoptr (GMenu) view_menu = g_menu_new ();
  g_menu_append (view_menu, "Show grid", "app.showgrid");
  g_menu_append (view_menu, "Enable antialiasing", "app.antialiasing");
  g_menu_append (view_menu, "Zoom in", "app.zoomin");
  g_menu_append (view_menu, "Zoom out", "app.zoomout");
  g_menu_append (view_menu, "Zoom reset", "app.zoomreset");

  g_autoptr (GMenu) antialiasing_menu = g_menu_new ();

  for (size_t i = 0; i < G_N_ELEMENTS (antialiasing_modes); i++)
    {
      g_autoptr (GMenuItem) item = g_menu_item_new (antialiasing_modes[i].label, "app.antialiasing");
      g_menu_item_set_attribute_value (item, "target",
                                       g_variant_new_string (antialiasing_modes[i].key));
      g_menu_append_item (antialiasing_menu, item);
    }

  g_autoptr (GMenuItem) antialiasing_submenu = g_menu_item_new_submenu ("Antialiassafing", G_MENU_MODEL (antialiasing_menu));
  g_menu_append_item (view_menu, antialiasing_submenu);

  // Create the menu button and set the menu model
  GtkWidget *view_button = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (view_button), G_MENU_MODEL (view_menu));
  gtk_menu_button_set_label (GTK_MENU_BUTTON (view_button), "View");

  return view_button;
}

/* static GtkWidget * */
/* create_view_toolbar (AppState *state) */
/* { */
/*   g_autoptr (GMenu) view = g_menu_new (); */

/*   g_menu_append (view, "Show grid", "app.showgrid"); */
/*   g_menu_append (view, "Enable antialiasing", "app.antialiasing"); */

/*   g_menu_append (view, "Zoom in", "app.zoomin"); */
/*   g_menu_append (view, "Zoom out", "app.zoomout"); */
/*   g_menu_append (view, "Zoom reset", "app.zoomreset"); */

/*   GtkWidget *view_btn = gtk_menu_button_new (); */
/*   gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (view_btn), G_MENU_MODEL (view)); */
/*   gtk_menu_button_set_label (GTK_MENU_BUTTON (view_btn), "View"); */
/*   return view_btn; */
/* } */

static void
create_menus (GtkWidget *header_bar, AppState *state)
{
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), create_file_toolbar (state));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), create_edit_toolbar (state));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), create_view_toolbar (state));

  setup_antialiasing_action (state);
}

// TODO rename
static void
resize_drawable_area_x (gpointer user_data, int dx, int dy, int dirx, int diry)
{
  AppState *state = (AppState *) user_data;
  save_backup (&state->backup_manager, state->main_surface);
  int width = cairo_image_surface_get_width (state->main_surface);
  int height = cairo_image_surface_get_height (state->main_surface);
  int new_width = width + (dx / state->zoom_level) * dirx;
  int new_height = height + (dy / state->zoom_level) * diry;

  if (new_width == 0)
    new_width = 1;

  if (new_height == 0)
    new_height = 1;

  cairo_surface_t *old_surface = state->main_surface;

  state->main_surface = cairo_image_surface_create (state->format, new_width, new_height);
  clear_canvas (state->main_surface);

  cairo_t *cr = cairo_create (state->main_surface);
  cairo_set_source_surface (cr, old_surface, (dirx < 0) ? new_width - width : 0, (diry < 0) ? new_height - height : 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  g_clear_pointer (&old_surface, cairo_surface_destroy);

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_widget_queue_draw (state->drawing_area);
  update_cursor_position (state, -1, -1); // TODO
}

/* Function to resize drawable area and preserve data */
static void
resize_drawable_area (AppState *state, int new_width, int new_height)
{
  save_backup (&state->backup_manager, state->main_surface);

  cairo_surface_t *old_surface = state->main_surface;

  state->main_surface = cairo_image_surface_create (state->format, new_width, new_height);
  clear_canvas (state->main_surface);

  cairo_t *cr = cairo_create (state->main_surface);
  cairo_set_source_surface (cr, old_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_destroy (old_surface);

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_widget_queue_draw (state->drawing_area);
}

static void on_entry_changed (GtkEditable *editable, gpointer user_data);

typedef struct
{
#if HAVE_ADWAITA && ADW_CHECK_VERSION(1, 5, 0)
  AdwDialog *dialog;
#else
  GtkWindow *dialog;
#endif
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

  int new_width = atoi (width_text);
  int new_height = atoi (height_text);
  resize_drawable_area (rd->state, new_width, new_height);

#if HAVE_ADWAITA && ADW_CHECK_VERSION(1, 5, 0)
  adw_dialog_close (rd->dialog);
#else
  gtk_window_destroy (GTK_WINDOW (rd->dialog));
#endif
  g_free (rd);
}

static void
on_cancel_clicked (GtkButton *btn, gpointer user_data)
{
  ResizeData *rd = (ResizeData *) user_data;
#if HAVE_ADWAITA && ADW_CHECK_VERSION(1, 5, 0)
  adw_dialog_close (rd->dialog);
#else
  gtk_window_destroy (GTK_WINDOW (rd->dialog));
#endif
  g_free (rd);
}

static void
on_keep_ratio_check_toggled (GtkCheckButton *self, gpointer user_data)
{
  if (!gtk_check_button_get_active (self))
    return;

  ResizeData *rd = (ResizeData *) user_data;
  const char *width_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->width_entry)));
  const char *height_text = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (rd->height_entry)));
  int new_width = atoi (width_text);
  int new_height = atoi (height_text);
  g_object_set_data (G_OBJECT (rd->width_entry), "value", GINT_TO_POINTER (new_width));
  g_object_set_data (G_OBJECT (rd->height_entry), "value", GINT_TO_POINTER (new_height));
}

static void
on_resize (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

#if HAVE_ADWAITA && ADW_CHECK_VERSION(1, 5, 0)
  AdwDialog *dialog = adw_dialog_new ();

  adw_dialog_set_title (dialog, "Resize drawable area");
  adw_dialog_set_can_close (dialog, TRUE);
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
    int current;
    const char *label;
    GtkWidget *entry;
  } h = {
    .current = cairo_image_surface_get_height (state->main_surface),
    .label = "Height",
    .entry = NULL,
  },
    w = {
      .current = cairo_image_surface_get_width (state->main_surface),
      .label = "Width",
      .entry = NULL,
    },
    *d[] = { &w, &h };

  for (int i = 0; i < (int) G_N_ELEMENTS (d); i++)
    {
      char buffer[64];
      int n;
      struct Dimension *t = d[i];
      t->entry = my_entry_new (); // TODO. It shall handle set buffer

      n = snprintf (buffer, sizeof (buffer), "%d", t->current);
      gtk_entry_set_buffer (GTK_ENTRY (t->entry), gtk_entry_buffer_new (buffer, n));

      g_object_set_data (G_OBJECT (t->entry), "keep-ratio", keep_ratio_check);
      g_object_set_data (G_OBJECT (t->entry), "value", GINT_TO_POINTER (0));

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new (t->label), 0, i, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), t->entry, 1, i, 1, 1);
    }

  g_signal_connect (w.entry, "changed", G_CALLBACK (on_entry_changed), h.entry);
  g_signal_connect (h.entry, "changed", G_CALLBACK (on_entry_changed), w.entry);

  ResizeData *rd = g_new (ResizeData, 1);
#if HAVE_ADWAITA && ADW_CHECK_VERSION(1, 5, 0)
  rd->dialog = dialog;
#else
  rd->dialog = GTK_WINDOW (window);
#endif
  rd->state = state;
  rd->width_entry = w.entry;
  rd->height_entry = h.entry;

  GtkWidget *ok_button = gtk_button_new_with_label ("Ok");
  g_signal_connect (ok_button, "clicked", G_CALLBACK (on_ok_clicked), rd);

  GtkWidget *cancel_button = gtk_button_new_with_label ("Cancel");
  g_signal_connect (cancel_button, "clicked", G_CALLBACK (on_cancel_clicked), rd);

  g_signal_connect (keep_ratio_check, "toggled", G_CALLBACK (on_keep_ratio_check_toggled), rd);

  gboolean keep_ratio_check_enabled = TRUE;
  gtk_check_button_set_active (GTK_CHECK_BUTTON (keep_ratio_check), keep_ratio_check_enabled);

  if (keep_ratio_check_enabled)
    on_keep_ratio_check_toggled (GTK_CHECK_BUTTON (keep_ratio_check), rd);

  gtk_grid_attach (GTK_GRID (grid), keep_ratio_check, 0, 2, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), ok_button, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), cancel_button, 1, 3, 1, 1);

#if HAVE_ADWAITA && ADW_CHECK_VERSION(1, 5, 0)
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
  GtkWidget *keep_ratio_check = (GtkWidget *) g_object_get_data (G_OBJECT (other_entry), "keep-ratio");

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (keep_ratio_check)))
    {
      int old_this = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (editable), "value"));
      int old_other = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (other_entry), "value"));

      int new_this = atoi (gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (editable))));
      int new_other = new_this * old_other / old_this;

      char buffer[64];
      int n = snprintf (buffer, sizeof (buffer), "%d", new_other);
      gtk_entry_set_buffer (GTK_ENTRY (other_entry), gtk_entry_buffer_new (buffer, n));
    }
}

static GtkWidget *
create_drawing_area (AppState *state)
{
  GtkWidget *drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (drawing_area), (int) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (drawing_area), (int) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area), draw_callback, state, NULL);

  struct
  {
    void (*callback) (GtkGestureDrag *gesture, double x, double y, gpointer user_data);
    guint button;
  } buttons[2] = {
    { .callback = on_click_primary_pressed,   .button = GDK_BUTTON_PRIMARY   },
    { .callback = on_click_secondary_pressed, .button = GDK_BUTTON_SECONDARY },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (buttons); i++)
    {
      GtkGesture *drag = gtk_gesture_drag_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), buttons[i].button);
      g_signal_connect (drag, "drag-begin", G_CALLBACK (buttons[i].callback), state);
      g_signal_connect (drag, "drag-update", G_CALLBACK (drag_update), state);
      g_signal_connect (drag, "drag-end", G_CALLBACK (on_click_released), state);
      gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (drag));
    }

  GtkEventController *motion = gtk_event_controller_motion_new ();
  g_signal_connect (motion, "motion", G_CALLBACK (motion_handler), state);
  g_signal_connect (motion, "leave", G_CALLBACK (on_leave), state);
  gtk_widget_add_controller (drawing_area, motion);

  return drawing_area;
}

/* cairo_surface_t * */
/* get_surface (gpointer user_data) */
/* { */
/*   AppState *st = (AppState *) user_data; */
/*   return st->main_surface; */
/* } */

// TODO
/* static void */
/* on_surface_selected (gpointer TODO, cairo_surface_t *surface, gpointer
 * user_data) */
/* { */
/*   // TODO */
/*   AppState *state = (AppState *) user_data; */
/*   state->main_surface = surface; */
/*   gtk_widget_queue_draw (state->drawing_area); */
/* } */

static void
activate (GtkApplication *app, AppState *state)
{
  state->application = app;
#if HAVE_ADWAITA
  GtkWidget *window = adw_window_new ();
  gtk_window_set_application (GTK_WINDOW (window), app); // TODO
#else
  GtkWidget *window = gtk_application_window_new (app); // Must be freed
#endif
  state->window = window;
  gtk_window_set_title (GTK_WINDOW (window), "Paint");
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

  GtkWidget *header = gtk_header_bar_new ();
  // TODO gtk_window_set_titlebar (GTK_WINDOW (window), header);

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), create_zoom_box (state));

  state->drawing_area = create_drawing_area (state);
  GtkWidget *bordered = gpaint_border_widget_new ();
  gpaint_border_widget_set_child (GPAINT_BORDER_WIDGET (bordered), state->drawing_area);

  GtkWidget *grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (grid), bordered, 1, 1, 1, 1);

  {
    GtkWidget *top_left = gpaint_drag_tracker_new (resize_drawable_area_x, -1, -1, state);
    GtkWidget *cen_left = gpaint_drag_tracker_new (resize_drawable_area_x, -1, 0, state);
    GtkWidget *bot_left = gpaint_drag_tracker_new (resize_drawable_area_x, -1, +1, state);

    GtkWidget *top_center = gpaint_drag_tracker_new (resize_drawable_area_x, 0, -1, state);
    GtkWidget *bot_center = gpaint_drag_tracker_new (resize_drawable_area_x, 0, +1, state);

    GtkWidget *top_right = gpaint_drag_tracker_new (resize_drawable_area_x, +1, -1, state);
    GtkWidget *cen_right = gpaint_drag_tracker_new (resize_drawable_area_x, +1, 0, state);
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

  // TODO NEED IF GRID IS NOT CENTERED
  /* // TODO */
  /* // Done for scrollbars */
  /* /\* gtk_widget_set_margin_top (grid, 16); *\/ */
  /* gtk_widget_set_margin_bottom (grid, 16); */
  /* /\* gtk_widget_set_margin_start (grid, 16); *\/ */
  /* gtk_widget_set_margin_end (grid, 16); */

  GtkWidget *scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scrolled, TRUE);
  gtk_widget_set_hexpand (scrolled, TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), grid);
  // TODO
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
  state->scrolled = scrolled;
  state->hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled));
  state->vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled));

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

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_box_append (GTK_BOX (hbox), state->color_swap_button);
  gtk_box_append (GTK_BOX (hbox), state->color_btn);
  gtk_box_append (GTK_BOX (hbox), create_color_grid (state));

  // g_autoptr (GdkTexture) cursor = cursor_loader_get_texture (gdk_display_get_default(), "default");
  // g_autoptr (GdkCursor) cursor = gdk_cursor_new_from_name ("default", NULL); // TODO
  // TODO gtk_box_append (GTK_BOX (hbox), gtk_image_new_from_paintable (GDK_PAINTABLE (cursor))); // TODO
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
  /* state->layers = gpaint_layers_widget_new(); */
  // TODO gpaint_layers_widget_get_selected_surface
  // TODO g_signal_connect (state->layers, "surface-selected", G_CALLBACK
  // (on_surface_selected), state);
  GtkWidget *lrs = gtk_frame_new (NULL); // TODO RENAME
  GtkWidget *scr = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  /* gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scr), state->layers);
   */
  gtk_frame_set_child (GTK_FRAME (lrs), scr);

  gtk_box_append (GTK_BOX (content_hbox), vframe);
  gtk_box_append (GTK_BOX (content_hbox), scrolled);
  /* gtk_box_append (GTK_BOX (content_hbox), lrs); // TODO */

  {
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    // TODO
    /* gtk_box_append (GTK_BOX (vbox), my_layer_widget_new (get_surface,
     * state)); */

    gtk_box_append (GTK_BOX (content_hbox), vbox);
  }

  GtkWidget *main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

#if HAVE_ADWAITA
  gtk_box_append (GTK_BOX (main_vbox), header);
#endif

  gtk_box_append (GTK_BOX (main_vbox), content_hbox);
  gtk_box_append (GTK_BOX (main_vbox), hframe);

#if HAVE_ADWAITA
  adw_window_set_content (ADW_WINDOW (window), main_vbox);
#else
  gtk_window_set_titlebar (GTK_WINDOW (window), header);
  gtk_window_set_child (GTK_WINDOW (window), main_vbox);
#endif

  create_menus (header, state);
  update_cursor (state);
  gtk_window_present (GTK_WINDOW (window));
  update_cursor_position (state, -1, -1);

  const struct
  {
    const GActionEntry *actions;
    gint count;
  } entries[] = {
    { .actions = file_actions, G_N_ELEMENTS (file_actions) },
    { .actions = edit_actions, G_N_ELEMENTS (edit_actions) },
    { .actions = view_actions, G_N_ELEMENTS (view_actions) },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (entries); i++)
    g_action_map_add_action_entries (G_ACTION_MAP (app), entries[i].actions, entries[i].count, state);

  for (guint i = 0; i < G_N_ELEMENTS (app_accels); i++)
    gtk_application_set_accels_for_action (app, app_accels[i].action, app_accels[i].accels);

  state->show_grid_action = g_action_map_lookup_action (G_ACTION_MAP (app), "showgrid");
  state->antialiasing_action = g_action_map_lookup_action (G_ACTION_MAP (app), "antialiasing");

  state->cut_action = g_action_map_lookup_action (G_ACTION_MAP (app), "cut");
  state->copy_action = g_action_map_lookup_action (G_ACTION_MAP (app), "copy");
  set_can_copy_surface (state);

  state->backup_manager.undo_action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (app), "undo"));
  state->backup_manager.redo_action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (app), "redo"));

  g_simple_action_set_enabled (state->backup_manager.undo_action, !g_queue_is_empty (state->backup_manager.undo));
  g_simple_action_set_enabled (state->backup_manager.redo_action, !g_queue_is_empty (state->backup_manager.redo));
}

int
main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  // TODO
  /* bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR); */
  /* bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8"); */
  /* textdomain (GETTEXT_PACKAGE); */

  // TODO
  /* for (size_t k = 0; k < G_N_ELEMENTS (gpaint_formats); k++) */
  /*   { */
  /*     const AVCodec *codec = avcodec_find_encoder
   * (gpaint_formats[k].codec_id); */
  /*     const enum AVPixelFormat *pix_fmts = codec->pix_fmts; */
  /*     if (pix_fmts) { */
  /*       printf("Supported pixel formats by %s:\n", codec->name); */
  /*       for (int i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) { */
  /*         printf("  %s\n", av_get_pix_fmt_name(pix_fmts[i])); */
  /*       } */
  /*     } else { */
  /*       printf("No specific pixel formats supported.\n"); */
  /*     } */
  /*   } */

  AppState state = { 0 };
  state.main_surface = create_surface (64, 64);
  state.p_color = &state.primary_color;
  state.s_color = &state.secondary_color;
  state.primary_color = GPAINT_GDK_BLACK;
  state.secondary_color = GPAINT_GDK_TRANSPARENT;
  state.zoom_level = 1.0;
  state.width = 1.0;
  state.brush_size = 3.0;
  state.fill_type = FILL_TRANSPARENT;
  state.tool = &global_freehand_tool;
  state.is_drawing = FALSE;
  state.preview_surface = NULL;
  init_backup_manager (&state.backup_manager);
  state.format = cairo_image_surface_get_format (state.main_surface);
  state.selected_surface = NULL;
  state.selected_rect = (GdkRectangle) { 0, 0, 0, 0 };
  state.has_selection = FALSE;
  state.cursor_x = state.cursor_y = 0.0;
  state.is_dragging_selection = state.has_selection = FALSE;
  state.antialiasing = CAIRO_ANTIALIAS_NONE;
  /* TODO state.last_drag_time = 0; */

  // clang-format off
  ToolEntry tools[] =
    {
      [TOOL_FREEHAND]		= { "Freehand",			&global_freehand_tool		},
      [TOOL_BRUSH]		= { "Brush",			&global_brush_tool		},
      [TOOL_LINE]		= { "Line",			&global_line_tool		},
      [TOOL_RECTANGLE]		= { "Rectangle",		&global_rectangle_tool		},
      [TOOL_ELLIPSE]		= { "Ellipse",			&global_ellipse_tool		},
      [TOOL_TRIANGLE]		= { "Triangle",			&global_triangle_tool		},
      [TOOL_ERASER]		= { "Eraser",			&global_eraser_tool		},
      [TOOL_PICKER]		= { "Picker",			&global_picker_tool		},
      [TOOL_BUCKET]		= { "Bucket",			&global_bucket_tool		},
      [TOOL_SELECT_RECTANGLE]	= { "Select rectangle",		&global_select_rectangle_tool	},
      [TOOL_DRAG]		= { "Drag",			&global_drag_tool		},
      [TOOL_SYMMETRIC_FREEHAND]	= { "Symmetric freehand",	&global_symmetric_freehand_tool },
    };
  // clang-format on

  G_STATIC_ASSERT (G_N_ELEMENTS (tools) == TOOLS_COUNT);

  for (size_t i = 0; i < TOOLS_COUNT; i++)
    {
      g_assert (tools[i].tool->cursor_name || tools[i].tool->cursor_icon || tools[i].tool->icon);

      if (tools[i].tool->cursor_name)
        {
          GdkCursor *cursor = gdk_cursor_new_from_name (tools[i].tool->cursor_name, NULL);

          if (!cursor)
            g_warning ("Failed to create GdkCursor from name: %s", tools[i].tool->cursor_name);
          else
            state.cursors[tools[i].tool->type] = g_steal_pointer (&cursor);
        }
      else
        {
          const struct raw_bitmap *icon = tools[i].tool->cursor_icon
                                            ? tools[i].tool->cursor_icon
                                            : tools[i].tool->icon;

          g_autoptr (GBytes) data = g_bytes_new_static (icon->data, icon->size);
          GError *error = NULL;

          g_autoptr (GdkTexture) texture = gdk_texture_new_from_bytes (data, &error);

          if (!texture)
            g_warning ("Failed to create texture: %s", error->message);
          else
            {
              GdkCursor *cursor = gdk_cursor_new_from_texture (texture, icon->hotspot_x, icon->hotspot_y, NULL);

              if (!cursor)
                g_warning ("Failed to create GdkCursor from texture.");
              else
                state.cursors[tools[i].tool->type] = g_steal_pointer (&cursor);
            }
        }
    }

  state.tools = tools;

#if HAVE_ADWAITA
  g_autoptr (AdwApplication) app = adw_application_new ("org.gnu.paint", G_APPLICATION_DEFAULT_FLAGS);
#else
  g_autoptr (GtkApplication) app = gtk_application_new ("org.gnu.paint", G_APPLICATION_DEFAULT_FLAGS);
#endif
  g_signal_connect (app, "activate", G_CALLBACK (activate), &state);
  int status = g_application_run (G_APPLICATION (app), argc, argv);
  free_backup_manager (&state.backup_manager);
  /* cairo_surface_destroy (state.main_surface); */
  return status;
}
