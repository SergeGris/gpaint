
#include <gtk/gtk.h>

#include "gpaint.h"

static void zoom_set_value (AppState *state, gdouble zoom);

static const int zoom_levels[] = { 5, 8, 12, 16, 25, 33, 50, 66, 100, 125, 150, 175, 200, 500, 1000, 1200, 1600, 2400, 3600 };

static void
set_zoom (GtkWidget *w, int zoom)
{
  char label[64];
  g_snprintf (label, sizeof (label), "%'d%%", zoom);
  gtk_menu_button_set_label (GTK_MENU_BUTTON (w), label);
}

/* Action handler: receives the integer zoom, updates the MenuButton’s label */
static void
on_select (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  AppState *state = (AppState *) user_data;
  int zoom = g_variant_get_int32 (parameter); /* retrieve the int param */

  set_zoom (state->zoom_label, zoom);
  zoom_set_value (state, (double) zoom / 100.0);
}

/* Builds a GMenuModel whose items invoke "app.select" with the zoom int */
static GMenuModel *
build_menu_model (void)
{
  GMenu *menu = g_menu_new ();
  char label[16];

  for (size_t i = 0; i < G_N_ELEMENTS (zoom_levels); i++)
    {
      int z = zoom_levels[i];
      g_snprintf (label, sizeof (label), "%'d%%", z);

      /* Create a new menu item with label "100%", action "app.select" */
      GMenuItem *item = g_menu_item_new (label, NULL);

      /* Use the combined helper to set both action and its int target */
      g_menu_item_set_action_and_target (item, "app.select", "i", z);

      g_menu_append_item (menu, item);
      g_object_unref (item);
    }

  return G_MENU_MODEL (menu);
}

static GtkWidget *
zoom_menu_new (AppState *state)
{
  GtkWidget *menu_btn = gtk_menu_button_new ();
  gtk_menu_button_set_label (GTK_MENU_BUTTON (menu_btn), "100%");

  g_autoptr (GSimpleAction) action = g_simple_action_new ("select", G_VARIANT_TYPE_INT32); // TODO: LEAKS?
  g_signal_connect (action, "activate", G_CALLBACK (on_select), state);
  g_action_map_add_action (G_ACTION_MAP (state->application), G_ACTION (action));
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_btn), build_menu_model ());
  return menu_btn;
}

static void
zoom_update_label (AppState *state)
{
  gchar label[64];
  snprintf (label, sizeof (label), "%.0f%%", state->zoom_level * 100.0);
  // gtk_label_set_text (GTK_LABEL (state->zoom_label), label);
  gtk_menu_button_set_label (GTK_MENU_BUTTON (state->zoom_label), label);
}

static void
zoom_set_value (AppState *state, gdouble zoom)
{
  state->zoom_level = CLAMP (zoom, 0.05, 64.0);
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  zoom_update_label (state);
  gtk_widget_queue_draw (state->drawing_area);
}

static void
zoom_in (GtkButton *btn, AppState *state)
{
  zoom_set_value (state, fmin (64.0, state->zoom_level * 1.5));
}

static void
zoom_out (GtkButton *btn, AppState *state)
{
  zoom_set_value (state, fmax (0.5, state->zoom_level / 1.5));
}

static void
zoom_reset (GtkButton *btn, AppState *state)
{
  zoom_set_value (state, 1.0);
}

static GtkWidget *
create_zoom_box (AppState *state)
{
  GtkWidget *menu_btn = zoom_menu_new (state);

  GtkWidget *zoom_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *in = gtk_button_new_from_icon_name ("zoom-in-symbolic");
  GtkWidget *out = gtk_button_new_from_icon_name ("zoom-out-symbolic");
  // GtkWidget *zreset = gtk_button_new_from_icon_name
  // ("zoom-fit-best-symbolic");
  state->zoom_level = 1.0;
  state->zoom_label = menu_btn;
  g_signal_connect (in, "clicked", G_CALLBACK (zoom_in), state);
  g_signal_connect (out, "clicked", G_CALLBACK (zoom_out), state);
  gtk_menu_button_set_has_frame (GTK_MENU_BUTTON (menu_btn), TRUE);
  gtk_box_append (GTK_BOX (zoom_box), out);
  gtk_box_append (GTK_BOX (zoom_box), menu_btn);
  gtk_box_append (GTK_BOX (zoom_box), in);

  return zoom_box;
}
