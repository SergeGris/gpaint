#include <gtk/gtk.h>
#include <cairo.h>

#define GPAINT_TYPE_LAYERS_WIDGET (gpaint_layers_widget_get_type())
G_DECLARE_FINAL_TYPE(GPaintLayersWidget, gpaint_layers_widget, GPAINT, LAYERS_WIDGET, GtkBox);

#define GPAINT_TYPE_PREVIEW_WIDGET (gpaint_preview_widget_get_type())
G_DECLARE_FINAL_TYPE(GPaintPreviewWidget, gpaint_preview_widget, GPAINT, PREVIEW_WIDGET, GtkWidget);

typedef struct _LayerRow
{
  GtkWidget *row;
  GtkToggleButton *toggle;
  GtkWidget *delete_button;
  GtkToggleButton *special_toggle;
  GPaintPreviewWidget *preview;
} LayerRow;

struct _GPaintLayersWidget
{
  GtkBox parent_instance;
  gint width, height;
  GList *layer_rows;
  GtkWidget *selected_toggle;
  gint preview_size;
  gint spacing;
  GtkWidget *add_button;
};

struct _GPaintPreviewWidget
{
  GtkWidget parent_instance;
  cairo_surface_t *surface;
  gint target_size;
};

G_DEFINE_TYPE(GPaintPreviewWidget, gpaint_preview_widget, GTK_TYPE_WIDGET);

static void gpaint_preview_widget_snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
  GPaintPreviewWidget *self = GPAINT_PREVIEW_WIDGET(widget);
  if (!self->surface) return;
  graphene_rect_t bounds;
  graphene_rect_init(&bounds, 0, 0, gtk_widget_get_width(widget), gtk_widget_get_height(widget));
  cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &bounds);
  gdouble surf_w = cairo_image_surface_get_width(self->surface);
  gdouble surf_h = cairo_image_surface_get_height(self->surface);
  gdouble scale = MIN((gdouble)self->target_size / surf_w, (gdouble)self->target_size / surf_h);
  cairo_save(cr);
  cairo_translate(cr, (self->target_size - surf_w * scale) / 2, (self->target_size - surf_h * scale) / 2);
  cairo_scale(cr, scale, scale);
  cairo_set_source_surface(cr, self->surface, 0, 0);
  cairo_paint(cr);
  cairo_restore(cr);
  cairo_destroy(cr);
}

static void gpaint_preview_widget_dispose(GObject *object) {
  GPaintPreviewWidget *self = GPAINT_PREVIEW_WIDGET(object);
  if (self->surface) {
    cairo_surface_destroy(self->surface);
    self->surface = NULL;
  }
  G_OBJECT_CLASS(gpaint_preview_widget_parent_class)->dispose(object);
}

static void gpaint_preview_widget_class_init(GPaintPreviewWidgetClass *klass) {
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->snapshot = gpaint_preview_widget_snapshot;
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->dispose = gpaint_preview_widget_dispose;
}

static void gpaint_preview_widget_init(GPaintPreviewWidget *self) {
  self->surface = NULL;
  self->target_size = 100;
}

GPaintPreviewWidget *gpaint_preview_widget_new(cairo_surface_t *surface, gint target_size) {
  GPaintPreviewWidget *preview = g_object_new(GPAINT_TYPE_PREVIEW_WIDGET, NULL);
  preview->surface = cairo_surface_reference(surface);
  preview->target_size = target_size;
  gtk_widget_set_size_request(GTK_WIDGET(preview), target_size, target_size);
  return preview;
}

void gpaint_preview_widget_set_surface(GPaintPreviewWidget *widget, cairo_surface_t *surface) {
  if (widget->surface)
    cairo_surface_destroy(widget->surface);
  widget->surface = cairo_surface_reference(surface);
  gtk_widget_queue_draw(GTK_WIDGET(widget));
}

void
gpaint_layers_widget_queue_redraw (GtkWidget *widget)
{
  GPaintLayersWidget *self = GPAINT_LAYERS_WIDGET(widget);

  for (GList *l = self->layer_rows; l != NULL; l = l->next) {
    // TODO redraw only selected
    LayerRow *row = l->data;
    gtk_widget_queue_draw (GTK_WIDGET (row->preview));
  }
}

void gpaint_preview_widget_queue_redraw(GPaintPreviewWidget *widget) {
  gtk_widget_queue_draw(GTK_WIDGET(widget));
}

enum {
  SURFACE_SELECTED,
  N_SIGNALS
};

static guint gpaint_layers_widget_signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE(GPaintLayersWidget, gpaint_layers_widget, GTK_TYPE_BOX);

static void on_layer_toggled(GtkToggleButton *button, gpointer user_data) {
  GPaintLayersWidget *self = GPAINT_LAYERS_WIDGET(user_data);

  if (button == self->selected_toggle)
    return;

  if (gtk_toggle_button_get_active(button)) {
    self->selected_toggle = GTK_WIDGET(button);
    cairo_surface_t *surface = g_object_get_data(G_OBJECT(button), "surface");
    g_signal_emit(self, gpaint_layers_widget_signals[SURFACE_SELECTED], 0, surface);
  }

  for (GList *l = self->layer_rows; l != NULL; l = l->next) {
    LayerRow *row = l->data;
    // TODO. On press on selected it unselects it...
    if (gtk_toggle_button_get_active (row->toggle) && self->selected_toggle != GTK_WIDGET (button))
      gtk_toggle_button_set_active (row->toggle, FALSE);
  }
}

GtkWidget *gpaint_layers_widget_get_selected_toggle(GPaintLayersWidget *widget) {
  return widget->selected_toggle;
}

cairo_surface_t *gpaint_layers_widget_get_selected_surface(GPaintLayersWidget *widget) {
  if (!widget->selected_toggle) return NULL;
  return g_object_get_data(G_OBJECT(widget->selected_toggle), "surface");
}

static void update_delete_buttons(GPaintLayersWidget *widget) {
  gboolean only_one = g_list_length(widget->layer_rows) <= 1;
  for (GList *l = widget->layer_rows; l != NULL; l = l->next) {
    LayerRow *row = l->data;
    gtk_widget_set_sensitive(row->delete_button, !only_one);
  }
}

static void on_delete_button_clicked(GtkButton *button, gpointer user_data) {
  GPaintLayersWidget *widget = GPAINT_LAYERS_WIDGET(user_data);
  for (GList *l = widget->layer_rows; l != NULL; l = l->next) {
    LayerRow *row = l->data;
    if (gtk_widget_is_ancestor(GTK_WIDGET(button), row->row)) {
      if (g_list_length(widget->layer_rows) <= 1) return;
      gboolean was_selected = (widget->selected_toggle == (GtkWidget *)row->toggle);
      gtk_box_remove(GTK_BOX(widget), row->row);
      widget->layer_rows = g_list_delete_link(widget->layer_rows, l);
      if (was_selected && widget->layer_rows) {
        LayerRow *next = widget->layer_rows->data;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(next->toggle), TRUE);
      }
      update_delete_buttons(widget);
      return;
    }
  }
}

static LayerRow *create_layer_row(GPaintLayersWidget *widget, cairo_surface_t *surface) {
  LayerRow *lrow = g_malloc(sizeof(LayerRow));
  lrow->row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

  lrow->toggle = GTK_TOGGLE_BUTTON(gtk_toggle_button_new());
  GPaintPreviewWidget *preview = gpaint_preview_widget_new(surface, widget->preview_size);
  lrow->preview = preview;
  gtk_button_set_child(GTK_BUTTON(lrow->toggle), GTK_WIDGET(preview));
  g_object_set_data_full(G_OBJECT(lrow->toggle), "surface",
                           cairo_surface_reference(surface),
                           (GDestroyNotify)cairo_surface_destroy);
  g_signal_connect(lrow->toggle, "toggled", G_CALLBACK(on_layer_toggled), widget);

  lrow->delete_button = gtk_button_new_with_label("✕");
  g_signal_connect(lrow->delete_button, "clicked", G_CALLBACK(on_delete_button_clicked), widget);

  lrow->special_toggle = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label("★"));
  gtk_box_append(GTK_BOX(lrow->row), GTK_WIDGET (lrow->toggle));
  gtk_box_append(GTK_BOX(lrow->row), lrow->delete_button);
  gtk_box_append(GTK_BOX(lrow->row), GTK_WIDGET(lrow->special_toggle));
  return lrow;
}

void gpaint_layers_widget_add_layer(GPaintLayersWidget *widget, cairo_surface_t *surface) {
  LayerRow *lrow = create_layer_row(widget, surface);
  widget->layer_rows = g_list_append(widget->layer_rows, lrow);
  gtk_box_insert_child_after(GTK_BOX(widget), lrow->row, widget->add_button);
  if (g_list_length(widget->layer_rows) == 1)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lrow->toggle), TRUE);
  update_delete_buttons(widget);
}

void gpaint_layers_widget_select_next(GPaintLayersWidget *widget) {
  if (!widget->selected_toggle || !widget->layer_rows) return;
  for (GList *l = widget->layer_rows; l != NULL; l = l->next) {
    LayerRow *row = l->data;
    if ((GtkWidget *)row->toggle == widget->selected_toggle) {
      GList *next = l->next ? l->next : widget->layer_rows;
      LayerRow *next_row = next->data;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(next_row->toggle), TRUE);
      return;
    }
  }
}

void gpaint_layers_widget_select_prev(GPaintLayersWidget *widget) {
  if (!widget->selected_toggle || !widget->layer_rows) return;
  for (GList *l = widget->layer_rows; l != NULL; l = l->next) {
    LayerRow *row = l->data;
    if ((GtkWidget *)row->toggle == widget->selected_toggle) {
      GList *prev = l->prev ? l->prev : g_list_last(widget->layer_rows);
      LayerRow *prev_row = prev->data;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prev_row->toggle), TRUE);
      return;
    }
  }
}

void gpaint_layers_widget_update_preview(GPaintLayersWidget *widget, cairo_surface_t *surface) {
  for (GList *l = widget->layer_rows; l; l = l->next) {
    LayerRow *row = l->data;
    cairo_surface_t *surf = g_object_get_data(G_OBJECT(row->toggle), "surface");
    if (surf == surface) {
      gpaint_preview_widget_set_surface(row->preview, surface);
      return;
    }
  }
}

static void gpaint_layers_widget_dispose(GObject *object) {
  GPaintLayersWidget *self = GPAINT_LAYERS_WIDGET(object);
  for (GList *l = self->layer_rows; l; l = l->next) {
    LayerRow *row = l->data;
    // Free allocated LayerRow
    g_free(row);
  }
  g_list_free(self->layer_rows);
  self->layer_rows = NULL;
  G_OBJECT_CLASS(gpaint_layers_widget_parent_class)->dispose(object);
}

static void on_add_layer_clicked(GtkButton *button, gpointer user_data) {
  GPaintLayersWidget *widget = GPAINT_LAYERS_WIDGET(user_data);
  cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, widget->width, widget->height);
  cairo_t *cr = cairo_create(surface);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint(cr);
  cairo_destroy(cr);
  gpaint_layers_widget_add_layer(widget, surface);
  cairo_surface_destroy(surface);
}

static void gpaint_layers_widget_init(GPaintLayersWidget *self) {
  self->layer_rows = NULL;
  self->width = self->height = 300;
  self->preview_size = 100;
  self->spacing = 8;
  self->selected_toggle = NULL;
  gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing(GTK_BOX(self), self->spacing);
  self->add_button = gtk_button_new_with_label("Add Layer");
  g_signal_connect(self->add_button, "clicked", G_CALLBACK(on_add_layer_clicked), self);
  gtk_box_append(GTK_BOX(self), self->add_button);
  cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, self->width, self->height);
  cairo_t *cr = cairo_create(surface);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint(cr);
  cairo_destroy(cr);
  gpaint_layers_widget_add_layer(self, surface);
  cairo_surface_destroy(surface);
}

static void gpaint_layers_widget_class_init(GPaintLayersWidgetClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->dispose = gpaint_layers_widget_dispose;
  gpaint_layers_widget_signals[SURFACE_SELECTED] = g_signal_new("surface-selected",
      G_TYPE_FROM_CLASS(klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
}

GtkWidget *gpaint_layers_widget_new(void) {
  return g_object_new(GPAINT_TYPE_LAYERS_WIDGET, NULL);
}
