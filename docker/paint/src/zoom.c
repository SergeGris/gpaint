
static void
zoom_update_label (AppState *state)
{
  gchar label[1024];
  snprintf (label, sizeof (label), "Zoom: %.0f%%", state->zoom_level * 100.0);
  gtk_label_set_text (GTK_LABEL (state->zoom_label), label);
}

static void
zoom_set_value (AppState *state, gdouble zoom)
{
  state->zoom_level = CLAMP (zoom, 0.125, 64.0);
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (gint) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
  zoom_update_label (state);
  gtk_widget_queue_draw (state->drawing_area);
}

static void
zoom_in (GtkButton *btn, AppState *state)
{
  zoom_set_value (state, fmin (64.0, state->zoom_level * 1.2));
}

static void
zoom_out (GtkButton *btn, AppState *state)
{
  zoom_set_value (state, fmax (0.125, state->zoom_level / 1.2));
}

static void
zoom_reset (GtkButton *btn, AppState *state)
{
  zoom_set_value (state, 1.0);
}

static GtkWidget *
create_zoom_box (AppState *state)
{
  GtkWidget *zoom_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *zin = gtk_button_new_from_icon_name ("zoom-in-symbolic");
  GtkWidget *zout = gtk_button_new_from_icon_name ("zoom-out-symbolic");
  GtkWidget *zreset = gtk_button_new_from_icon_name ("zoom-fit-best-symbolic");
  state->zoom_level = 1.0;
  state->zoom_label = gtk_label_new ("Zoom: 100%");
  g_signal_connect (zin, "clicked", G_CALLBACK (zoom_in), state);
  g_signal_connect (zout, "clicked", G_CALLBACK (zoom_out), state);
  g_signal_connect (zreset, "clicked", G_CALLBACK (zoom_reset), state);
  gtk_box_append (GTK_BOX (zoom_box), zin);
  gtk_box_append (GTK_BOX (zoom_box), zout);
  gtk_box_append (GTK_BOX (zoom_box), zreset);
  gtk_box_append (GTK_BOX (zoom_box), state->zoom_label);
  return zoom_box;
}
