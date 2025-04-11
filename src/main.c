
#include "config.h"

#include <glib/gi18n.h>

#if HAS_ADWAITA
# include <adwaita.h>
#endif

#include <gtk/gtk.h>

#include "color-swap-button.h"
#include "drag-square.h"
#include "formats/formats.h"
#include "gpaint.h"
#include "number-entry.c"
#include "tools/tools.h"
#include "value-selector.c"
#include "border-widget.h"

#include "layers.c"
#include "zoom.c"
#include "select.c"

#ifndef ADW_CHECK_VERSION
# define ADW_CHECK_VERSION(major, minor, patch) 0
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
  cairo_set_source_surface (cr, state->selected_surface,
                            state->selected_rect.x,
                            state->selected_rect.y);
  cairo_paint (cr);
  cairo_destroy (cr);

  // Clear temporary selection state
  g_clear_pointer (&state->selected_surface, cairo_surface_destroy);
  memset (&state->selected_rect, 0, sizeof (state->selected_rect));
  state->has_selection = FALSE;
  set_can_copy_surface (state);
}

void
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
static gboolean
my_dots_in_rect (int x, int y, const GdkRectangle *rect)
{
  return x >= rect->x
      && y >= rect->y
      && x <= rect->x + rect->width
      && y <= rect->y + rect->height;
}

cairo_surface_t *
cut_rectangle (cairo_surface_t *src,
               const GdkRectangle *rect,
               const GdkRGBA *color)
{
  // Create a new surface for the extracted region.
  cairo_surface_t *dest = cairo_image_surface_create (cairo_image_surface_get_format (src),
                                                      rect->width, rect->height);
  if (cairo_surface_status (dest) != CAIRO_STATUS_SUCCESS)
    {
      fprintf (stderr, "Failed to create destination surface.\n");
      return NULL;
    }

  // Copy the rectangular region from src to dest.
  cairo_t *cr_dest = cairo_create (dest);
  // Offset the source so that the desired rectangle maps to (0,0) in dest.
  cairo_set_source_surface (cr_dest, src, -rect->x, -rect->y);
  cairo_paint (cr_dest);
  cairo_destroy (cr_dest);

  // Now fill the rectangle area in the source surface with the specified color.
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
      state->selected_rect.x = CLAMP (state->selected_rect.x, -state->selected_rect.width,
                                      cairo_image_surface_get_width (state->main_surface));
      state->selected_rect.y = CLAMP (state->selected_rect.y, -state->selected_rect.height,
                                      cairo_image_surface_get_height (state->main_surface));

      gtk_widget_queue_draw (state->drawing_area);
    }

  if (!state->is_drawing)
    return;

  int px = x / state->zoom_level;
  int py = y / state->zoom_level;

  if (state->preview_surface)
    {
      cairo_t *cr = cairo_create (state->preview_surface);

      if (state->tool->motion_handler == NULL)
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);
      // Let the tool draw its preview into preview_surface:
      if (state->tool->motion_handler == NULL)
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

#if 0 // TODO

/* Flip the surface horizontally (left-right) */
cairo_surface_t* flip_horizontal(cairo_surface_t *surface) {
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);

    cairo_surface_t *flipped = cairo_image_surface_create(
        cairo_image_surface_get_format(surface), width, height);
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

    cairo_surface_t *rotated = cairo_image_surface_create(
        cairo_image_surface_get_format(surface), new_width, new_height);
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
/* on_flip_rotate (GSimpleAction *action, GVariant *parameter, gpointer user_data) */
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
/*   gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (state->window)); */
/*   gtk_window_set_modal (GTK_WINDOW (window), TRUE); */
/*   gtk_window_set_title (GTK_WINDOW (window), "Flip/Rotate"); */
/*   gtk_window_set_resizable (GTK_WINDOW (window), FALSE); */
/*   gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (state->window)); */
/* #endif */

/*   GtkWidget *flip_vertical = gtk_flip_new_with_label(NULL, "Flip vertical"); */
/*   GtkWidget *flip_horizontal = gtk_flip_new_with_label_from_widget(GTK_RADIO_BUTTON(flip_vertical), "Flip horizontal"); */
/*   GtkWidget *rotate_by_angle = gtk_flip_new_with_label_from_widget(GTK_RADIO_BUTTON(flip_horizontal), "Rotate by angle"); */

/*   // Connect signals */
/*   g_signal_connect(flip_vertical, "toggled", G_CALLBACK(on_flip_toggled), "Option A"); */
/*   g_signal_connect(flip_vertical, "toggled", G_CALLBACK(on_flip_toggled), "Option B"); */
/*   g_signal_connect(rotate_by_angle, "toggled", G_CALLBACK(on_flip_toggled), "Option C"); */

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
void
set_image_to_clipboard (cairo_surface_t *surface, GtkWidget *parent)
{
  /* Encode the surface as PNG into a memory buffer */
  GByteArray *byte_array = g_byte_array_new ();
  cairo_status_t status = cairo_surface_write_to_png_stream (surface,
                                                             png_write_callback,
                                                             byte_array);
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
 *   number of bytes from the stream, returning CAIRO_STATUS_SUCCESS on success.
 */
static cairo_status_t
g_input_stream_png_read(void *closure, unsigned char *data, unsigned int length)
{
    GInputStream *stream = (GInputStream *) closure;
    unsigned int total_read = 0;
    while (total_read < length) {
        gssize ret = g_input_stream_read(stream, data + total_read, length - total_read, NULL, NULL);
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
on_image_surface_ready(cairo_surface_t *surface, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (surface && cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
    /* g_print("Image surface obtained: %d×%d\n", */
    /*         cairo_image_surface_get_width(surface), */
    /*         cairo_image_surface_get_height(surface)); */

    // Now you can use the surface in your app
    if (state->selected_surface)
      cairo_surface_destroy(state->selected_surface);

    state->selected_surface = surface;
    state->selected_rect = (GdkRectangle) { 0, 0, cairo_image_surface_get_width(surface),
                                            cairo_image_surface_get_height(surface) };
    state->has_selection = TRUE;
    set_can_copy_surface (state);

    tool_select (state, TOOL_SELECT_RECTANGLE);

    gtk_widget_queue_draw(state->drawing_area);

    /* cairo_surface_write_to_png(surface, "clipboard_image.png"); */
    /* cairo_surface_destroy(surface); */
  } else {
    g_warning("Failed to create Cairo surface from clipboard data.");
  }
}

/*
 * on_clipboard_image_received:
 *   This asynchronous callback is invoked when the clipboard read is complete.
 *   It obtains a GInputStream (expected to contain PNG data), creates a Cairo
 *   surface from it, then calls on_image_surface_ready with the result.
 */
static void
on_clipboard_image_received(GObject      *clipboard,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    GError *error = NULL;
    const char *mime_type = NULL;
    GInputStream *stream = gdk_clipboard_read_finish(GDK_CLIPBOARD(clipboard), res, &mime_type, &error);
    if (error) {
        g_warning("Error reading clipboard: %s", error->message);
        g_clear_error(&error);
        on_image_surface_ready(NULL, user_data);
        return;
    }
    if (!stream) {
        g_warning("Clipboard returned no data.");
        on_image_surface_ready(NULL, user_data);
        return;
    }
    /* Create a Cairo surface from the PNG data stream. */
    cairo_surface_t *surface = cairo_image_surface_create_from_png_stream(g_input_stream_png_read, stream);
    g_object_unref(stream);
    on_image_surface_ready(surface, user_data);
}

/*
 * retrieve_clipboard_image:
 *   Retrieves an image from the clipboard (PNG only in this example) and decodes
 *   it into a Cairo image surface. The resulting surface (or NULL on failure) is
 *   passed to on_image_surface_ready.
 */
static void
retrieve_clipboard_image(GtkWidget *widget, gpointer user_data)
{
  GdkDisplay *display = gtk_widget_get_display(widget);
  GdkClipboard *clipboard = gdk_display_get_clipboard(display);
  /* In this example we only support PNG. (Extend here for other MIME types.) */
  const char *mime_types[] = { "image/png", NULL };
  gdk_clipboard_read_async(clipboard,
                           mime_types,
                           0,
                           NULL,  /* No GCancellable */
                           on_clipboard_image_received,
                           user_data);
}

static void
on_cut (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (state->selected_surface) {
    set_image_to_clipboard(state->selected_surface, state->window);
    clear_selection (state);
    gtk_widget_queue_draw (state->drawing_area);
    // Fill original area with background color
    //fill_rectangle(state->main_surface, &state->selected_rect, state->secondary_color);
    //clear_selection(state);
  }
}

static void
on_copy (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;

  if (state->selected_surface)
    set_image_to_clipboard(state->selected_surface, state->window);
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
  gtk_widget_queue_draw(state->drawing_area);

  if (state->selected_surface)
    g_clear_pointer (&state->selected_surface, cairo_surface_destroy);

  state->selected_rect.x = state->selected_rect.y = 0;
  state->selected_rect.width = cairo_image_surface_get_width (state->main_surface);
  state->selected_rect.height = cairo_image_surface_get_height (state->main_surface);
  state->selected_surface = cut_rectangle (state->main_surface, &state->selected_rect, state->s_color);
  state->has_selection = TRUE;
  set_can_copy_surface (state);
}

////
static const GActionEntry file_actions[] =
  {
    { "new", on_new_file, NULL, NULL, NULL },
    { "open", on_open_file, NULL, NULL, NULL },
    { "save", on_save_file, NULL, NULL, NULL },
    { "quit", on_quit, NULL, NULL, NULL },
  };

static const GActionEntry edit_actions[] =
  {
    { "undo", on_undo, NULL, NULL, NULL },
    { "redo", on_redo, NULL, NULL, NULL },

    { "cut", on_cut, NULL, NULL, NULL },
    { "copy", on_copy, NULL, NULL, NULL },
    { "paste", on_paste, NULL, NULL, NULL },
    { "selectall", on_selectall, NULL, NULL, NULL },

    { "resize", on_resize, NULL, NULL, NULL },
  };

static const GActionEntry view_actions[] =
  {
    { "showgrid", on_toggle_show_grid, NULL, "false", NULL },
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
  const gchar *accels[2];
} app_accels[] =
  {
    { "app.new",  { "<Primary>n", NULL } },
    { "app.open", { "<Primary>o", NULL } },
    { "app.save", { "<Primary>s", NULL } },
    { "app.quit", { "<Primary>q", NULL } },

    { "app.cut",       { "<Primary>x", NULL } },
    { "app.copy",      { "<Primary>c", NULL } },
    { "app.paste",     { "<Primary>v", NULL } },
    { "app.selectall", { "<Primary>a", NULL } },

    { "app.undo", { "<Primary>z", NULL } },
    { "app.redo", { "<Primary>y", NULL } }
  };
// clang-format on

static void
update_cursor_position (AppState *state, double x, double y)
{
  cairo_surface_t *surface = state->main_surface;
  const int width = cairo_image_surface_get_width (surface);
  const int height = cairo_image_surface_get_height (surface);

  // TODO undocumanted?
  const cairo_format_t format = cairo_image_surface_get_format (surface);
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
#ifdef CAIRO_FORMAT_RGB96F
    case CAIRO_FORMAT_RGB96F: // TODO
      color_depth = 96;
      break;
#endif
#ifdef CAIRO_FORMAT_RGBA128F
    case CAIRO_FORMAT_RGBA128F: // TODO
      color_depth = 128;
      break;
#endif
    case CAIRO_FORMAT_INVALID:
    default:
      color_depth = 0;
      break;
    }

  char image_info[256];

  if (color_depth)
    snprintf (image_info, sizeof (image_info), "%d×%d×%d", width, height, color_depth);
  else
    snprintf (image_info, sizeof (image_info), "%d×%d", width, height);

  gtk_label_set_text (GTK_LABEL (state->image_info), image_info);

  int px = x / state->zoom_level;
  int py = y / state->zoom_level;

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
my_get_visible_rect (AppState *state,
                     double *x,
                     double *y,
                     double *width,
                     double *height)
{
  GtkAdjustment *hadj = state->hadj;
  GtkAdjustment *vadj = state->vadj;
  double x_offset = gtk_adjustment_get_value (hadj);
  double y_offset = gtk_adjustment_get_value (vadj);
  double page_width = gtk_adjustment_get_page_size (hadj);
  double page_height = gtk_adjustment_get_page_size (vadj);

  *x = x_offset;
  *y = y_offset;
  *width = page_width;
  *height = page_height;
}
///

static void
draw_surface_on_rect (cairo_t *cr, cairo_surface_t *surface, const GdkRectangle *rect)
{

}

static void
draw_callback (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  const double pixel_size = state->zoom_level;
  const int surface_width = cairo_image_surface_get_width (state->main_surface);
  const int surface_height = cairo_image_surface_get_height (state->main_surface);
  cairo_pattern_t *pattern;
  const double bg[2] = { 0.8, 0.9 };
  const double line_color = 0.7; // TODO

  double s_x, s_y, s_width, s_height;
  my_get_visible_rect (state, &s_x, &s_y, &s_width, &s_height);

  const int d = 2; // TODO: Preserve some space for if drawing area scaled and not properly aligned.
  const double r = pixel_size;
  const GdkRectangle v = {
    .x = (int) (s_x / r) - d,
    .y = (int) (s_y / r) - d,
    .width = (int) (s_width / r) + 2 * d,
    .height = (int) (s_height / r) + 2 * d,
  };

  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_save (cr);
  cairo_scale (cr, pixel_size, pixel_size);

  // TODO use draw_transparent_square
  double color = bg[1];
  cairo_set_source_rgb (cr, color, color, color);
  cairo_rectangle (cr, v.x, v.y, v.width, v.height);
  cairo_fill (cr);
  cairo_restore (cr);

  if (pixel_size > 4.0)
    {
      /* Draw checkerboard background */
      cairo_save (cr);
      int k = 2; // TODO make it adaptive for zoom.
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
      cairo_matrix_init_translate (&matrix,
                                   -state->selected_rect.x,
                                   -state->selected_rect.y);
      cairo_pattern_set_matrix (selection_pattern, &matrix);

      // Set source and paint
      cairo_set_source (cr, selection_pattern);
      cairo_rectangle (cr,
                       state->selected_rect.x,
                       state->selected_rect.y,
                       state->selected_rect.width,
                       state->selected_rect.height);
      cairo_clip (cr);
      cairo_pattern_destroy (selection_pattern);

      cairo_paint (cr);
      cairo_restore (cr);
    }

  if (state->tool->type == TOOL_SELECT_RECTANGLE)
    {
      cairo_save (cr);
      cairo_scale (cr, pixel_size, pixel_size);
      cairo_set_line_width (cr, 2.0 / pixel_size);

      // Define a dashed line pattern
      double dashes[] = { 4.0 / pixel_size, 4.0 / pixel_size }; // 2 units on, 2 units off
      cairo_set_dash (cr, dashes, 2, 0);                        // Set the dash pattern
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);                 // Black color

      // Draw a rectangle
      cairo_rectangle (cr, state->selected_rect.x, state->selected_rect.y,
                       state->selected_rect.width, state->selected_rect.height);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

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
  /* cairo_set_line_width (cr, 1.5); // TODO: 1.5 because 1.0 is invisible... */
  /* cairo_rectangle (cr, 0, 0, width, height); */
  /* cairo_stroke (cr); */
  /* cairo_restore (cr); */
}

static GdkTexture *
create_texture_from_raw_data (int height, int width, int rowstride, const guchar *raw_data)
{
  g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new_from_data ((guchar *) raw_data, GDK_COLORSPACE_RGB, /* has alpha */ TRUE, 8, width, height, rowstride, NULL, NULL);
  if (pixbuf == NULL)
    return NULL;
  GdkTexture *texture = gdk_texture_new_for_pixbuf (pixbuf);
  return texture;
}

/* Updates cursor based on current tool – update per GTK4 docs if needed */
static void
update_cursor (AppState *state)
{
  g_autoptr (GdkCursor) cursor = NULL;

  // TODO
  if (state->has_selection && my_dots_in_rect (state->cursor_x / state->zoom_level,
                                               state->cursor_y / state->zoom_level,
                                               &state->selected_rect))
    {
      cursor = gdk_cursor_new_from_name (state->is_dragging_selection ? "grabbing" : "grab", NULL);
    }
  else if (state->tool->draw_cursor_handler)
    {
      cursor = gdk_cursor_new_from_name ("none", NULL);
    }
  else if (state->tool->cursor)
    {
      cursor = gdk_cursor_new_from_name (state->tool->cursor, NULL);

      if (!cursor)
        {
          g_warning ("Failed to create GdkCursor from name: %s", state->tool->cursor);
          return;
        }
    }
  else
    {
      const struct raw_bitmap *icon = state->tool->cursor_icon
                                          ? state->tool->cursor_icon
                                          : state->tool->icon;
      g_autoptr (GdkTexture) texture = create_texture_from_raw_data (icon->height, icon->width,
                                                                     icon->rowstride, icon->data);

      cursor = gdk_cursor_new_from_texture (texture, icon->hotspot_x, icon->hotspot_y, NULL);

      if (!cursor)
        {
          g_warning ("Failed to create GdkCursor from texture.");
          return;
        }
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

  state->preview_surface = create_surface (cairo_image_surface_get_width (state->main_surface),
                                           cairo_image_surface_get_height (state->main_surface));

  if (state->tool->override_main_surface)
    copy_surface (state->preview_surface, state->main_surface);

  if (state->tool->type == TOOL_FREEHAND
      || state->tool->type == TOOL_SYMMETRIC_FREEHAND
      || state->tool->type == TOOL_ERASER
      || state->tool->type == TOOL_BRUSH
      || state->tool->type == TOOL_BUCKET) // TODO
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

      cairo_t *cr = create_cairo (state->main_surface,
                                  state->tool->override_main_surface
                                      ? CAIRO_OPERATOR_SOURCE
                                      : CAIRO_OPERATOR_OVER);
      cairo_set_source_surface (cr, state->preview_surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      g_clear_pointer (&state->preview_surface, cairo_surface_destroy);
    }

  state->is_drawing = FALSE;

  /// TODO
  //gtk_widget_queue_draw (gpaint_layers_widget_get_selected_button (GPAINT_LAYERS_WIDGET (state->layers)));
  // TODO gpaint_preview_widget_queue_redraw (GPAINT_LAYERS_WIDGET (state->layers));
  // TODO gpaint_layers_widget_queue_redraw (state->layers);
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
  /*         cairo_image_surface_get_width(state->main_surface) - state->selected_rect.width); */
  /*     state->selected_rect.y = CLAMP(state->selected_rect.y, 0, */
  /*         cairo_image_surface_get_height(state->main_surface) - state->selected_rect.height); */

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

static void
export_image (AppState *state, const gchar *filename)
{
  save_image (filename, state->main_surface, NULL);

  /* /\* For non-PNG formats, grab a GdkPixbuf from the surface and save it. */
  /*    This allows formats such as JPG, BMP, or GIF. *\/ */
  /* g_autoptr (GdkPixbuf) pixbuf; */

  /* pixbuf = gdk_pixbuf_new_from_data (cairo_image_surface_get_data (state->main_surface), */
  /*                                    GDK_COLORSPACE_RGB, // TODO colospace */
  /*                                    TRUE,               // TODO has alpha */
  /*                                    8,                  // TODO rowstride */
  /*                                    cairo_image_surface_get_width (state->main_surface), */
  /*                                    cairo_image_surface_get_height (state->main_surface), */
  /*                                    cairo_image_surface_get_stride (state->main_surface), */
  /*                                    NULL, NULL); */

  /* if (pixbuf != NULL) */
  /*   { */
  /*     GError *error = NULL; */
  /*     if (!gdk_pixbuf_save (pixbuf, filename, ext ? ext + 1 : "png", &error, NULL)) */
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
  save_image (path, state->main_surface, NULL);
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

  cairo_surface_t *new_surface = load_image (path);
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
  GdkRGBA t = state->primary_color;
  state->primary_color = state->secondary_color;
  state->secondary_color = t;
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

static GtkWidget *
create_toolbar_grid (AppState *state)
{
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

  for (int i = 0; state->tools[i].label; i++)
    {
      const Tool *tool = state->tools[i].tool;
      GtkWidget *btn = gtk_toggle_button_new ();

      g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new_from_data (tool->icon->data,
                                                               tool->icon->colorspace,
                                                               /* has alpha */ TRUE,
                                                               tool->icon->bits_per_sample,
                                                               tool->icon->height,
                                                               tool->icon->width,
                                                               tool->icon->rowstride,
                                                               NULL, NULL);
      g_autoptr (GdkTexture) texture = gdk_texture_new_for_pixbuf (pixbuf);
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
  gtk_box_append (GTK_BOX (vbox), create_dropdown_button ()); // TODO
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
create_edit_toolbar (GtkApplication *app, AppState *state)
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

static GtkWidget *
create_view_toolbar (GtkApplication *app, AppState *state)
{
  g_autoptr (GMenu) view = g_menu_new ();

  g_menu_append (view, "Show grid", "app.showgrid");

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
  cairo_set_source_surface (cr, old_surface,
                            (dirx < 0) ? new_width - width : 0,
                            (diry < 0) ? new_height - height : 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  g_clear_pointer (&old_surface, cairo_surface_destroy);

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), cairo_image_surface_get_width (state->main_surface) * state->zoom_level);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), cairo_image_surface_get_height (state->main_surface) * state->zoom_level);
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

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), cairo_image_surface_get_width (state->main_surface) * state->zoom_level);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), cairo_image_surface_get_height (state->main_surface) * state->zoom_level);
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

  int new_width = atoi (width_text);
  int new_height = atoi (height_text);
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

#if HAS_ADWAITA && ADW_CHECK_VERSION(1, 5, 0)
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
  } h =
    {
      .current = cairo_image_surface_get_height (state->main_surface),
      .label = "Height",
      .entry = NULL,
    },
    w =
    {
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

#if HAS_ADWAITA && ADW_CHECK_VERSION (1, 5, 0)
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
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (drawing_area), cairo_image_surface_get_width (state->main_surface) * state->zoom_level);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (drawing_area), cairo_image_surface_get_height (state->main_surface) * state->zoom_level);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area), draw_callback, state, NULL);

  struct
  {
    int button;
    void (*callback) (GtkGestureDrag *gesture, double x, double y, gpointer user_data);
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

cairo_surface_t *
get_surface (gpointer user_data)
{
  AppState *st = (AppState *) user_data;
  return st->main_surface;
}

static void
on_surface_selected (gpointer TODO, cairo_surface_t *surface, gpointer user_data)
{
  // TODO
  AppState *state = (AppState *) user_data;
  state->main_surface = surface;
  gtk_widget_queue_draw (state->drawing_area);
}

static void
activate (GtkApplication *app, AppState *state)
{
  GtkWidget *window = gtk_application_window_new (app); // Must be freed
  state->window = window;
  gtk_window_set_title (GTK_WINDOW (window), "Paint");
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

  GtkWidget *header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

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

  // TODO
  // Done for scrollbars
  /* gtk_widget_set_margin_top (grid, 16); */
  gtk_widget_set_margin_bottom (grid, 16);
  /* gtk_widget_set_margin_start (grid, 16); */
  gtk_widget_set_margin_end (grid, 16);

  GtkWidget *scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scrolled, TRUE);
  gtk_widget_set_hexpand (scrolled, TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), grid);
  state->scrolled = scrolled;
  state->hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled));
  state->vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled));

  GtkEventController *scroll_controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  g_signal_connect (scroll_controller, "scroll", G_CALLBACK (on_scroll), state);
  gtk_widget_add_controller (scrolled, scroll_controller);

  state->color_swap_button = gpaint_color_swap_button_new (get_primary_color,
                                                           get_secondary_color,
                                                           swap_colors,
                                                           state);
  state->image_info = gtk_label_new ("");
  state->current_position = gtk_label_new ("");

  GtkColorDialog *dialog = gtk_color_dialog_new ();
  gtk_color_dialog_set_with_alpha (dialog, TRUE);
  state->color_btn = gtk_color_dialog_button_new (dialog);
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (state->color_btn), &state->primary_color);
  g_signal_connect (state->color_btn, "notify::rgba", G_CALLBACK (on_color_changed), state);

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
  /* state->layers = gpaint_layers_widget_new(); */
  // TODO gpaint_layers_widget_get_selected_surface
  // TODO g_signal_connect (state->layers, "surface-selected", G_CALLBACK (on_surface_selected), state);
  GtkWidget *lrs = gtk_frame_new (NULL); // TODO RENAME
  GtkWidget *scr = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  /* gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scr), state->layers); */
  gtk_frame_set_child (GTK_FRAME (lrs), scr);

  gtk_box_append (GTK_BOX (content_hbox), vframe);
  gtk_box_append (GTK_BOX (content_hbox), scrolled);
  /* gtk_box_append (GTK_BOX (content_hbox), lrs); // TODO */

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

  const struct
  {
    const GActionEntry *actions;
    size_t count;
  } entries[] =
    {
      { .actions = file_actions, G_N_ELEMENTS (file_actions) },
      { .actions = edit_actions, G_N_ELEMENTS (edit_actions) },
      { .actions = view_actions, G_N_ELEMENTS (view_actions) },
    };

  for (size_t i = 0; i < G_N_ELEMENTS (entries); i++)
    g_action_map_add_action_entries (G_ACTION_MAP (app), entries[i].actions, entries[i].count, state);

  for (guint i = 0; i < G_N_ELEMENTS (app_accels); i++)
    gtk_application_set_accels_for_action (app, app_accels[i].action, app_accels[i].accels);

  state->show_grid_action = g_action_map_lookup_action (G_ACTION_MAP (app), "showgrid");

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
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  AppState state;
  state.main_surface = create_surface (64, 64);
  state.p_color = &state.primary_color;
  state.s_color = &state.secondary_color;
  state.primary_color = (GdkRGBA) { 0.0, 0.0, 0.0, 1.0 };
  state.secondary_color = (GdkRGBA) { 0.0, 0.0, 0.0, 0.0 };
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
  /* TODO state.last_drag_time = 0; */

  ToolEntry tools[] =
    {
      [TOOL_FREEHAND]         	= { "Freehand",         	&global_freehand_tool },
      [TOOL_BRUSH]            	= { "Brush",            	&global_brush_tool },
      [TOOL_LINE]             	= { "Line",             	&global_line_tool },
      [TOOL_RECTANGLE]        	= { "Rect",             	&global_rectangle_tool },
      [TOOL_ELLIPSE]          	= { "Ellipse",          	&global_ellipse_tool },
      [TOOL_ERASER]           	= { "Eraser",           	&global_eraser_tool },
      [TOOL_PICKER]           	= { "Picker",           	&global_picker_tool },
      [TOOL_BUCKET]           	= { "Bucket",           	&global_bucket_tool },
      [TOOL_SELECT_RECTANGLE] 	= { "Select rectangle", 	&global_select_rectangle_tool },
      [TOOL_DRAG]             	= { "Drag",             	&global_drag_tool },
      [TOOL_SYMMETRIC_FREEHAND] = { "Symmetric freehand", 	&global_symmetric_freehand_tool },
      { NULL, NULL, NULL },
    };

  state.tools = tools;

#if HAS_ADWAITA
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
