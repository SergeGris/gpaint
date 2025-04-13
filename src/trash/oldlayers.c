#include <cairo.h>
#include <gtk/gtk.h>

#define GPAINT_TYPE_LAYERS_WIDGET (gpaint_layers_widget_get_type ())
G_DECLARE_FINAL_TYPE (GPaintLayersWidget, gpaint_layers_widget, GPAINT, LAYERS_WIDGET, GtkBox)

/* Custom preview widget */
typedef struct _GpaintPreviewWidget GpaintPreviewWidget;

#define GPAINT_TYPE_PREVIEW_WIDGET (gpaint_preview_widget_get_type ())
G_DECLARE_FINAL_TYPE (GpaintPreviewWidget, gpaint_preview_widget, GPAINT, PREVIEW_WIDGET, GtkWidget)

struct _GPaintLayersWidget
{
  GtkBox parent_instance;

  /* Layer management */
  GPtrArray *layers;
  gint preview_size;
  gint spacing;
  GtkWidget *add_button;
};

/* Preview widget implementation */
struct _GpaintPreviewWidget
{
  GtkWidget parent_instance;
  cairo_surface_t *surface;
  gint target_size;
};

G_DEFINE_TYPE (GpaintPreviewWidget, gpaint_preview_widget, GTK_TYPE_WIDGET)

static void
gpaint_preview_widget_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  GpaintPreviewWidget *self = GPAINT_PREVIEW_WIDGET (widget);
  if (!self->surface)
    return;

  graphene_rect_t bounds;
  graphene_rect_init (&bounds, 0, 0,
                      gtk_widget_get_width (widget),
                      gtk_widget_get_height (widget));

  cairo_t *cr = gtk_snapshot_append_cairo (snapshot, &bounds);

  /* Calculate scaling */
  gdouble surf_w = cairo_image_surface_get_width (self->surface);
  gdouble surf_h = cairo_image_surface_get_height (self->surface);
  gdouble scale = MIN ((gdouble) self->target_size / surf_w,
                       (gdouble) self->target_size / surf_h);

  /* Draw centered preview */
  cairo_save (cr);
  cairo_translate (cr,
                   (self->target_size - surf_w * scale) / 2,
                   (self->target_size - surf_h * scale) / 2);
  cairo_scale (cr, scale, scale);
  cairo_set_source_surface (cr, self->surface, 0, 0);
  cairo_paint (cr);
  cairo_restore (cr);
}

static void
gpaint_preview_widget_dispose (GObject *object)
{
  GpaintPreviewWidget *self = GPAINT_PREVIEW_WIDGET (object);
  if (self->surface)
    {
      cairo_surface_destroy (self->surface);
      self->surface = NULL;
    }
  G_OBJECT_CLASS (gpaint_preview_widget_parent_class)->dispose (object);
}

static void
gpaint_preview_widget_class_init (GpaintPreviewWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->snapshot = gpaint_preview_widget_snapshot;

  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gpaint_preview_widget_dispose;
}

static void
gpaint_preview_widget_init (GpaintPreviewWidget *self)
{
  self->surface = NULL;
  self->target_size = 100;
}

GpaintPreviewWidget *
gpaint_preview_widget_new (cairo_surface_t *surface, gint target_size)
{
  GpaintPreviewWidget *preview = g_object_new (GPAINT_TYPE_PREVIEW_WIDGET, NULL);
  preview->surface = cairo_surface_reference (surface);
  preview->target_size = target_size;
  gtk_widget_set_size_request (GTK_WIDGET (preview), target_size, target_size);
  return preview;
}

/* Modified layer preview creation */
static GtkWidget *
create_layer_preview (GPaintLayersWidget *widget, cairo_surface_t *surface)
{
  GtkWidget *frame = gtk_frame_new (NULL);
  GtkWidget *button = gtk_toggle_button_new ();
  GtkWidget *preview = GTK_WIDGET (gpaint_preview_widget_new (surface, widget->preview_size));

  // TODO gtk_widget_add_css_class(frame, "layer-preview");
  gtk_frame_set_child (GTK_FRAME (frame), preview);
  gtk_button_set_child (GTK_BUTTON (button), frame);

  return button;
}

/* Rest of GPaintLayersWidget implementation */
G_DEFINE_TYPE (GPaintLayersWidget, gpaint_layers_widget, GTK_TYPE_BOX)

static void
on_layer_toggled (GtkToggleButton *button, gpointer user_data)
{
  GPaintLayersWidget *self = GPAINT_LAYERS_WIDGET (user_data);

  GtkWidget *child;
  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_TOGGLE_BUTTON (child) && child != GTK_WIDGET (button) && child != self->add_button)
        {
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child), FALSE);
        }
    }
}

void
gpaint_layers_widget_add_layer (GPaintLayersWidget *widget, cairo_surface_t *layer)
{
  GtkWidget *preview = create_layer_preview (widget, layer);
  g_ptr_array_add (widget->layers, cairo_surface_reference (layer));

  g_signal_connect (preview, "toggled", G_CALLBACK (on_layer_toggled), widget);
  gtk_box_insert_child_after (GTK_BOX (widget), preview, widget->add_button);
}

void
gpaint_layers_widget_remove_layer (GPaintLayersWidget *widget, guint index)
{
  if (index >= widget->layers->len)
    return;

  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (widget));
  for (guint i = 0; child && i <= index; i++)
    {
      if (i == index && child != widget->add_button)
        {
          gtk_box_remove (GTK_BOX (widget), child);
          break;
        }
      child = gtk_widget_get_next_sibling (child);
    }

  if (index < widget->layers->len)
    {
      cairo_surface_t *surface = g_ptr_array_remove_index (widget->layers, index);
      cairo_surface_destroy (surface);
    }
}

static void
gpaint_layers_widget_dispose (GObject *object)
{
  // TODO TODO TODO
  /* GPaintLayersWidget *self = GPAINT_LAYERS_WIDGET(object); */

  /* GtkWidget *child; */
  /* while ((child = gtk_widget_get_first_child(GTK_WIDGET(self)))) { */
  /*   if (child != self->add_button) { */
  /*     gtk_box_remove(GTK_BOX(self), child); */
  /*   } */
  /* } */

  /* g_ptr_array_free(self->layers, TRUE); */
  /* self->layers = NULL; */

  G_OBJECT_CLASS (gpaint_layers_widget_parent_class)->dispose (object);
}

static void
gpaint_layers_widget_class_init (GPaintLayersWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gpaint_layers_widget_dispose;
}

static void
on_add_layer_clicked (GtkButton *button, gpointer user_data)
{
  GPaintLayersWidget *widget = GPAINT_LAYERS_WIDGET (user_data);

  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 200, 200);
  cairo_t *cr = cairo_create (surface);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr);
  cairo_destroy (cr);

  gpaint_layers_widget_add_layer (widget, surface);
  cairo_surface_destroy (surface);
}

static void
gpaint_layers_widget_init (GPaintLayersWidget *self)
{
  self->layers = g_ptr_array_new_with_free_func ((GDestroyNotify) cairo_surface_destroy);
  self->preview_size = 100;
  self->spacing = 8;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), self->spacing);

  self->add_button = gtk_button_new_with_label ("Add Layer");
  g_signal_connect (self->add_button, "clicked", G_CALLBACK (on_add_layer_clicked), self);
  gtk_box_append (GTK_BOX (self), self->add_button);
}

GtkWidget *
gpaint_layers_widget_new (void)
{
  return g_object_new (GPAINT_TYPE_LAYERS_WIDGET, NULL);
}

/* #include <cairo.h> */
/* #include <gtk/gtk.h> */
/* #include <stdlib.h> */

/* #ifndef CLAMP */
/* #define CLAMP(val, min, max) (((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))) */
/* #endif */

/* /\* ===================== MYLayerItem ===================== *\/ */
/* /\* A layer item widget that shows a preview of a cairo surface and control buttons *\/ */
/* /\* typedef struct _MYLayerItem MYLayerItem; *\/ */
/* /\* typedef struct _MYLayerItemClass MYLayerItemClass; *\/ */

/* #define MY_TYPE_LAYER_ITEM (my_layer_item_get_type ()) */
/* G_DECLARE_FINAL_TYPE (MYLayerItem, my_layer_item, MY, LAYER_ITEM, GtkBox) */

/* struct _MYLayerItem */
/* { */
/*   GtkBox parent_instance; */
/*   cairo_surface_t *surface; */
/*   gboolean is_locked; */
/*   gboolean is_visible; */
/*   gboolean selected; */
/*   GtkWidget *drawing_area; */
/*   GtkWidget *select_button;     // Toggle button for selection */
/*   GtkWidget *lock_button;       // Toggle button for locking */
/*   GtkWidget *visibility_button; // Toggle button for visibility */
/*   GtkWidget *move_up_button; */
/*   GtkWidget *move_down_button; */
/*   GtkWidget *delete_button; */
/* }; */

/* G_DEFINE_TYPE (MYLayerItem, my_layer_item, GTK_TYPE_BOX) */

/* static gboolean */
/* my_layer_item_draw (GtkWidget *widget, cairo_t *cr, gpointer data) */
/* { */
/*   MYLayerItem *self = MY_LAYER_ITEM (data); */
/*   if (self->surface && self->is_visible) */
/*     { */
/*       GtkAllocation alloc; */
/*       gtk_widget_get_allocation (widget, &alloc); */
/*       int sw = cairo_image_surface_get_width (self->surface); */
/*       int sh = cairo_image_surface_get_height (self->surface); */
/*       double scale_x = (double) alloc.width / sw; */
/*       double scale_y = (double) alloc.height / sh; */
/*       cairo_scale (cr, scale_x, scale_y); */
/*       cairo_set_source_surface (cr, self->surface, 0, 0); */
/*       cairo_paint (cr); */
/*     } */
/*   return FALSE; */
/* } */

/* static void */
/* my_layer_item_toggle_selected (GtkToggleButton *button, gpointer data) */
/* { */
/*   MYLayerItem *self = MY_LAYER_ITEM (data); */
/*   self->selected = gtk_toggle_button_get_active (button); */
/* } */

/* static void */
/* my_layer_item_toggle_lock (GtkToggleButton *button, gpointer data) */
/* { */
/*   MYLayerItem *self = MY_LAYER_ITEM (data); */
/*   self->is_locked = gtk_toggle_button_get_active (button); */
/* } */

/* static void */
/* my_layer_item_toggle_visibility (GtkToggleButton *button, gpointer data) */
/* { */
/*   MYLayerItem *self = MY_LAYER_ITEM (data); */
/*   self->is_visible = gtk_toggle_button_get_active (button); */
/*   gtk_widget_queue_draw (self->drawing_area); */
/* } */

/* static void */
/* my_layer_item_init (MYLayerItem *self) */
/* { */
/*   gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL); */
/*   self->is_locked = FALSE; */
/*   self->is_visible = TRUE; */
/*   self->selected = FALSE; */
/*   self->surface = NULL; */

/*   /\* Create a drawing area for preview *\/ */
/*   self->drawing_area = gtk_drawing_area_new (); */
/*   gtk_widget_set_size_request (self->drawing_area, 100, 100); */
/*   g_signal_connect (self->drawing_area, "draw", G_CALLBACK (my_layer_item_draw), self); */

/*   /\* Create toggle button for selection *\/ */
/*   self->select_button = gtk_toggle_button_new (); */
/*   gtk_button_set_label (GTK_BUTTON (self->select_button), "Select"); */
/*   g_signal_connect (self->select_button, "toggled", G_CALLBACK (my_layer_item_toggle_selected), self); */

/*   /\* Create lock and visibility buttons *\/ */
/*   self->lock_button = gtk_toggle_button_new (); */
/*   gtk_button_set_label (GTK_BUTTON (self->lock_button), "Lock"); */
/*   g_signal_connect (self->lock_button, "toggled", G_CALLBACK (my_layer_item_toggle_lock), self); */

/*   self->visibility_button = gtk_toggle_button_new (); */
/*   gtk_button_set_label (GTK_BUTTON (self->visibility_button), "Hide"); */
/*   g_signal_connect (self->visibility_button, "toggled", G_CALLBACK (my_layer_item_toggle_visibility), self); */

/*   /\* Create move up, move down, and delete buttons *\/ */
/*   self->move_up_button = gtk_button_new_with_label ("Up"); */
/*   self->move_down_button = gtk_button_new_with_label ("Down"); */
/*   self->delete_button = gtk_button_new_with_label ("Delete"); */

/*   /\* Pack the widgets into this box *\/ */
/*   gtk_box_append (GTK_BOX (self), self->drawing_area); */
/*   gtk_box_append (GTK_BOX (self), self->select_button); */
/*   gtk_box_append (GTK_BOX (self), self->lock_button); */
/*   gtk_box_append (GTK_BOX (self), self->visibility_button); */
/*   gtk_box_append (GTK_BOX (self), self->move_up_button); */
/*   gtk_box_append (GTK_BOX (self), self->move_down_button); */
/*   gtk_box_append (GTK_BOX (self), self->delete_button); */
/* } */

/* static void */
/* my_layer_item_class_init (MYLayerItemClass *klass) */
/* { */
/*   // No special class initialization. */
/* } */

/* MYLayerItem * */
/* my_layer_item_new (cairo_surface_t *surface) */
/* { */
/*   MYLayerItem *item = g_object_new (MY_TYPE_LAYER_ITEM, NULL); */
/*   item->surface = surface; */
/*   return item; */
/* } */

/* cairo_surface_t * */
/* my_layer_item_get_surface (MYLayerItem *item) */
/* { */
/*   return item->surface; */
/* } */

/* /\* ===================== MYLayerManager ===================== *\/ */
/* /\* This widget keeps a list of MYLayerItem widgets and a plus button to add new layers. *\/ */
/* /\* typedef struct _MYLayerManager MYLayerManager; *\/ */
/* /\* typedef struct _MYLayerManagerClass MYLayerManagerClass; *\/ */

/* #define MY_TYPE_LAYER_MANAGER (my_layer_manager_get_type ()) */
/* G_DECLARE_FINAL_TYPE (MYLayerManager, my_layer_manager, MY, LAYER_MANAGER, GtkBox) */

/* struct _MYLayerManager */
/* { */
/*   GtkBox parent_instance; */
/*   GList *layer_items;    // List of MYLayerItem* */
/*   GtkWidget *list_box;   // Container (vertical GtkBox) for layer items */
/*   GtkWidget *add_button; // Button to add a new layer */
/* }; */

/* G_DEFINE_TYPE (MYLayerManager, my_layer_manager, GTK_TYPE_BOX) */

/* static void */
/* my_layer_manager_update_selection (MYLayerManager *manager) */
/* { */
/*   /\* Ensure only one layer is selected *\/ */
/*   for (GList *l = manager->layer_items; l; l = l->next) */
/*     { */
/*       MYLayerItem *item = l->data; */
/*       if (item->selected) */
/*         { */
/*           for (GList *m = manager->layer_items; m; m = m->next) */
/*             { */
/*               MYLayerItem *other = m->data; */
/*               if (other != item) */
/*                 { */
/*                   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (other->select_button), FALSE); */
/*                 } */
/*             } */
/*           break; */
/*         } */
/*     } */
/* } */

/* static void */
/* on_layer_item_toggled (GtkToggleButton *button, gpointer user_data) */
/* { */
/*   MYLayerManager *manager = MY_LAYER_MANAGER (user_data); */
/*   my_layer_manager_update_selection (manager); */
/* } */

/* static void */
/* on_delete_button_clicked (GtkButton *button, gpointer user_data) */
/* { */
/*   MYLayerManager *manager = MY_LAYER_MANAGER (user_data); */
/*   for (GList *l = manager->layer_items; l; l = l->next) */
/*     { */
/*       MYLayerItem *item = l->data; */
/*       if (GTK_WIDGET (button) == item->delete_button) */
/*         { */
/*           // TODO gtk_widget_destroy(GTK_WIDGET(item)); */
/*           manager->layer_items = g_list_remove (manager->layer_items, item); */
/*           break; */
/*         } */
/*     } */
/* } */

/* static void */
/* on_move_up_button_clicked (GtkButton *button, gpointer user_data) */
/* { */
/*   MYLayerManager *manager = MY_LAYER_MANAGER (user_data); */
/*   for (GList *l = manager->layer_items; l; l = l->next) */
/*     { */
/*       MYLayerItem *item = l->data; */
/*       if (GTK_WIDGET (button) == item->move_up_button && l->prev) */
/*         { */
/*           manager->layer_items = g_list_remove (manager->layer_items, item); */
/*           // TODO manager->layer_items = g_list_insert(manager->layer_items, item, g_list_position(l->prev)); */
/*           /\* Rebuild the container *\/ */
/*           gtk_container_foreach (GTK_CONTAINER (manager->list_box), (GtkCallback) gtk_widget_destroy, NULL); */
/*           for (GList *m = manager->layer_items; m; m = m->next) */
/*             { */
/*               gtk_box_append (GTK_BOX (manager->list_box), GTK_WIDGET (m->data)); */
/*             } */
/*           break; */
/*         } */
/*     } */
/* } */

/* static void */
/* on_move_down_button_clicked (GtkButton *button, gpointer user_data) */
/* { */
/*   MYLayerManager *manager = MY_LAYER_MANAGER (user_data); */
/*   for (GList *l = manager->layer_items; l; l = l->next) */
/*     { */
/*       MYLayerItem *item = l->data; */
/*       if (GTK_WIDGET (button) == item->move_down_button && l->next) */
/*         { */
/*           manager->layer_items = g_list_remove (manager->layer_items, item); */
/*           manager->layer_items = g_list_insert (manager->layer_items, item, l->next->next); */
/*           /\* Rebuild the container *\/ */
/*           gtk_container_foreach (GTK_CONTAINER (manager->list_box), (GtkCallback) gtk_widget_destroy, NULL); */
/*           for (GList *m = manager->layer_items; m; m = m->next) */
/*             { */
/*               gtk_box_append (GTK_BOX (manager->list_box), GTK_WIDGET (m->data)); */
/*             } */
/*           break; */
/*         } */
/*     } */
/* } */

/* static void */
/* on_add_button_clicked (GtkButton *button, gpointer user_data) */
/* { */
/*   MYLayerManager *manager = MY_LAYER_MANAGER (user_data); */
/*   /\* For demonstration, create a new blank white surface of size 100x100 *\/ */
/*   cairo_surface_t *new_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100); */
/*   cairo_t *cr = cairo_create (new_surface); */
/*   cairo_set_source_rgb (cr, 1, 1, 1); */
/*   cairo_paint (cr); */
/*   cairo_destroy (cr); */

/*   MYLayerItem *item = my_layer_item_new (new_surface); */
/*   /\* Connect toggle signals for selection management *\/ */
/*   g_signal_connect (item->select_button, "toggled", G_CALLBACK (on_layer_item_toggled), manager); */
/*   g_signal_connect (item->delete_button, "clicked", G_CALLBACK (on_delete_button_clicked), manager); */
/*   g_signal_connect (item->move_up_button, "clicked", G_CALLBACK (on_move_up_button_clicked), manager); */
/*   g_signal_connect (item->move_down_button, "clicked", G_CALLBACK (on_move_down_button_clicked), manager); */

/*   manager->layer_items = g_list_append (manager->layer_items, item); */
/*   gtk_box_append (GTK_BOX (manager->list_box), GTK_WIDGET (item)); */
/* } */

/* static void */
/* my_layer_manager_init (MYLayerManager *self) */
/* { */
/*   self->layer_items = NULL; */
/*   self->list_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5); */
/*   self->add_button = gtk_button_new_with_label ("+"); */
/*   g_signal_connect (self->add_button, "clicked", G_CALLBACK (on_add_button_clicked), self); */

/*   gtk_box_append (GTK_BOX (self), self->list_box); */
/*   gtk_box_append (GTK_BOX (self), self->add_button); */
/* } */

/* static void */
/* my_layer_manager_class_init (MYLayerManagerClass *klass) */
/* { */
/*   // Nothing special needed. */
/* } */

/* GtkWidget * */
/* my_layer_manager_new (void) */
/* { */
/*   MYLayerManager *manager = g_object_new (MY_TYPE_LAYER_MANAGER, NULL); */
/*   return GTK_WIDGET (manager); */
/* } */

/* /\* Get the currently selected layer's surface, or NULL if none selected *\/ */
/* cairo_surface_t * */
/* my_layer_manager_get_current_surface (MYLayerManager *manager) */
/* { */
/*   for (GList *l = manager->layer_items; l; l = l->next) */
/*     { */
/*       MYLayerItem *item = l->data; */
/*       if (item->selected) */
/*         { */
/*           return my_layer_item_get_surface (item); */
/*         } */
/*     } */
/*   return NULL; */
/* } */

/* /\* #include <cairo.h> *\/ */
/* /\* #include <gtk/gtk.h> *\/ */

/* /\* #define MY_TYPE_LAYER_WIDGET (my_layer_widget_get_type ()) *\/ */
/* /\* G_DECLARE_FINAL_TYPE (MyLayerWidget, my_layer_widget, MY, LAYER_WIDGET, GtkBox) *\/ */

/* /\* struct _MyLayerWidget *\/ */
/* /\* { *\/ */
/* /\*   GtkBox parent_instance; *\/ */
/* /\*   cairo_surface_t *(*get_surface) (gpointer user_data); *\/ */
/* /\*   GtkWidget *drawing_area; *\/ */
/* /\*   GtkWidget *lock_button; *\/ */
/* /\*   GtkWidget *visibility_button; *\/ */
/* /\*   gboolean is_locked; *\/ */
/* /\*   gboolean is_visible; *\/ */
/* /\*   gpointer user_data; *\/ */
/* /\* }; *\/ */

/* /\* G_DEFINE_TYPE (MyLayerWidget, my_layer_widget, GTK_TYPE_BOX) *\/ */

/* /\* static void *\/ */
/* /\* on_draw (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer user_data) *\/ */
/* /\* { *\/ */
/* /\*   MyLayerWidget *self = (MyLayerWidget *) user_data; *\/ */

/* /\*   if (self->is_visible && self->get_surface) *\/ */
/* /\*     { *\/ */
/* /\*       gdouble scale_x = (gdouble) width / cairo_image_surface_get_width (self->get_surface (self->user_data)); *\/ */
/* /\*       gdouble scale_y = (gdouble) height / cairo_image_surface_get_height (self->get_surface (self->user_data)); *\/ */
/* /\*       cairo_scale (cr, scale_x, scale_y); *\/ */

/* /\*       cairo_set_source_surface (cr, self->get_surface (self->user_data), 0, 0); *\/ */
/* /\*       cairo_paint (cr); *\/ */
/* /\*       gtk_widget_set_size_request (self->drawing_area, 100, 100); *\/ */
/* /\*       gtk_widget_queue_draw (self->drawing_area); *\/ */
/* /\*     } *\/ */
/* /\* } *\/ */

/* /\* static void *\/ */
/* /\* on_lock_button_clicked (GtkButton *button, gpointer user_data) *\/ */
/* /\* { *\/ */
/* /\*   MyLayerWidget *self = MY_LAYER_WIDGET (user_data); *\/ */
/* /\*   self->is_locked = !self->is_locked; *\/ */

/* /\*   const char *icon_name = self->is_locked ? "object-locked-symbolic" : "object-unlocked-symbolic"; *\/ */
/* /\*   gtk_button_set_icon_name (button, icon_name); *\/ */

/* /\*   gtk_widget_queue_draw (self->drawing_area); *\/ */
/* /\* } *\/ */

/* /\* static void *\/ */
/* /\* on_visibility_button_clicked (GtkButton *button, gpointer user_data) *\/ */
/* /\* { *\/ */
/* /\*   MyLayerWidget *self = MY_LAYER_WIDGET (user_data); *\/ */
/* /\*   self->is_visible = !self->is_visible; *\/ */

/* /\*   const char *icon_name = self->is_visible ? "object-visible" : "object-hidden"; *\/ */
/* /\*   gtk_button_set_icon_name (button, icon_name); *\/ */

/* /\*   gtk_widget_queue_draw (self->drawing_area); *\/ */
/* /\* } *\/ */

/* /\* static void *\/ */
/* /\* my_layer_widget_init (MyLayerWidget *self) *\/ */
/* /\* { *\/ */
/* /\*   self->is_locked = FALSE; *\/ */
/* /\*   self->is_visible = TRUE; *\/ */

/* /\*   self->drawing_area = gtk_drawing_area_new (); *\/ */
/* /\*   gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->drawing_area), on_draw, self, NULL); *\/ */

/* /\*   self->lock_button = gtk_button_new_from_icon_name ("object-locked-symbolic"); *\/ */
/* /\*   self->visibility_button = gtk_button_new_from_icon_name ("object-visible"); *\/ */

/* /\*   g_signal_connect (self->lock_button, "clicked", G_CALLBACK (on_lock_button_clicked), self); *\/ */
/* /\*   g_signal_connect (self->visibility_button, "clicked", G_CALLBACK (on_visibility_button_clicked), self); *\/ */

/* /\*   GtkWidget *btn = gtk_button_new (); *\/ */
/* /\*   gtk_button_set_child (GTK_BUTTON (btn), self->drawing_area); *\/ */

/* /\*   GtkWidget *v = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0); *\/ */
/* /\*   GtkWidget *h = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); *\/ */
/* /\*   gtk_box_append (GTK_BOX (h), self->lock_button); *\/ */
/* /\*   gtk_box_append (GTK_BOX (h), self->visibility_button); *\/ */
/* /\*   gtk_box_append (GTK_BOX (v), btn); *\/ */
/* /\*   gtk_box_append (GTK_BOX (v), h); *\/ */
/* /\*   gtk_box_append (GTK_BOX (self), v); *\/ */
/* /\* } *\/ */

/* /\* static void *\/ */
/* /\* my_layer_widget_class_init (MyLayerWidgetClass *klass) *\/ */
/* /\* { *\/ */
/* /\*   return; *\/ */
/* /\* } *\/ */

/* /\* GtkWidget * *\/ */
/* /\* my_layer_widget_new (cairo_surface_t *(*get_surface) (gpointer user_data), gpointer user_data) *\/ */
/* /\* { *\/ */
/* /\*   MyLayerWidget *widget = g_object_new (MY_TYPE_LAYER_WIDGET, NULL); *\/ */
/* /\*   widget->get_surface = get_surface; *\/ */
/* /\*   widget->user_data = user_data; *\/ */
/* /\*   return GTK_WIDGET (widget); *\/ */
/* /\* } *\/ */

/* /\* typedef struct { *\/ */
/* /\*     cairo_surface_t *surface; *\/ */
/* /\*     gboolean visible; *\/ */
/* /\*     gboolean locked; *\/ */
/* /\* } Layer; *\/ */

/* /\* typedef struct { *\/ */
/* /\*     GList *layers; *\/ */
/* /\*     GtkWidget *preview; *\/ */
/* /\* } LayerManager; *\/ */

/* /\* static void draw_layer_preview(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) { *\/ */
/* /\*     Layer *layer = (Layer *)user_data; *\/ */
/* /\*     if (!layer->visible) return; *\/ */

/* /\*     cairo_set_source_surface(cr, layer->surface, 0, 0); *\/ */
/* /\*     cairo_paint(cr); *\/ */
/* /\* } *\/ */

/* /\* static GtkWidget* create_layer_preview(Layer *layer, int width, int height) { *\/ */
/* /\*     GtkWidget *preview = gtk_drawing_area_new(); *\/ */
/* /\*     gtk_widget_set_size_request(preview, width, height); *\/ */
/* /\*     g_signal_connect(preview, "draw", G_CALLBACK(draw_layer_preview), layer); *\/ */
/* /\*     return preview; *\/ */
/* /\* } *\/ */

/* /\* static void toggle_layer_visibility(GtkToggleButton *toggle, gpointer user_data) { *\/ */
/* /\*     Layer *layer = (Layer *)user_data; *\/ */
/* /\*     layer->visible = gtk_toggle_button_get_active(toggle); *\/ */
/* /\* } *\/ */

/* /\* static void toggle_layer_lock(GtkToggleButton *toggle, gpointer user_data) { *\/ */
/* /\*     Layer *layer = (Layer *)user_data; *\/ */
/* /\*     layer->locked = gtk_toggle_button_get_active(toggle); *\/ */
/* /\* } *\/ */

/* /\* static void delete_layer(GtkButton *button, gpointer user_data) { *\/ */
/* /\*     LayerManager *manager = (LayerManager *)user_data; *\/ */
/* /\*     if (manager->layers) { *\/ */
/* /\*         Layer *layer = (Layer *)manager->layers->data; *\/ */
/* /\*         cairo_surface_destroy(layer->surface); *\/ */
/* /\*         g_free(layer); *\/ */
/* /\*         manager->layers = g_list_delete_link(manager->layers, manager->layers); *\/ */
/* /\*     } *\/ */
/* /\* } *\/ */

/* /\* static void move_layer_up(GtkButton *button, gpointer user_data) { *\/ */
/* /\*     // Logic to move a layer up in the list *\/ */
/* /\* } *\/ */

/* /\* static void move_layer_down(GtkButton *button, gpointer user_data) { *\/ */
/* /\*     // Logic to move a layer down in the list *\/ */
/* /\* } *\/ */

/* /\* static GtkWidget* create_layer_controls(Layer *layer) { *\/ */
/* /\*     GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); *\/ */
/* /\*     GtkWidget *visibility_toggle = gtk_toggle_button_new_with_label("Visible"); *\/ */
/* /\*     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(visibility_toggle), layer->visible); *\/ */
/* /\*     g_signal_connect(visibility_toggle, "toggled", G_CALLBACK(toggle_layer_visibility), layer); *\/ */

/* /\*     GtkWidget *lock_toggle = gtk_toggle_button_new_with_label("Locked"); *\/ */
/* /\*     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lock_toggle), layer->locked); *\/ */
/* /\*     g_signal_connect(lock_toggle, "toggled", G_CALLBACK(toggle_layer_lock), layer); *\/ */

/* /\*     GtkWidget *delete_button = gtk_button_new_with_label("Delete"); *\/ */
/* /\*     g_signal_connect(delete_button, "clicked", G_CALLBACK(delete_layer), NULL); *\/ */

/* /\*     gtk_box_append(GTK_BOX(box), visibility_toggle); *\/ */
/* /\*     gtk_box_append(GTK_BOX(box), lock_toggle); *\/ */
/* /\*     gtk_box_append(GTK_BOX(box), delete_button); *\/ */

/* /\*     return box; *\/ */
/* /\* } *\/ */

/* /\* static GtkWidget* create_layer_widget(Layer *layer, LayerManager *manager, int width, int height) { *\/ */
/* /\*     GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); *\/ */
/* /\*     GtkWidget *preview = create_layer_preview(layer, width, height); *\/ */
/* /\*     GtkWidget *controls = create_layer_controls(layer); *\/ */
/* /\*     gtk_box_append(GTK_BOX(box), preview); *\/ */
/* /\*     gtk_box_append(GTK_BOX(box), controls); *\/ */
/* /\*     return box; *\/ */
/* /\* } *\/ */
