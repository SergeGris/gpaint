
#include "drag-square.h"
#include "gpaint.h"

enum
{
  GPAINT_DRAG_TRACKER_SIZE = 8
};

struct _GpaintDragTracker
{
  GtkDrawingArea parent_instance;
  gboolean is_dragging;
  gint start_x;
  gint start_y;
  ResizeCallback on_resize;
  gint dirx, diry;
  gpointer user_data;
  GtkWidget *width_label;
  GtkWidget *height_label;
};

G_DEFINE_TYPE (GpaintDragTracker, gpaint_drag_tracker, GTK_TYPE_DRAWING_AREA);

static void
drag_begin (GtkGestureDrag *gesture, double x, double y, gpointer user_data)
{
  GpaintDragTracker *self = user_data;
  self->is_dragging = TRUE;
  self->start_x = (gint) x;
  self->start_y = (gint) y;

  if (self->dirx != 0)
    self->width_label = gtk_label_new (NULL);

  if (self->diry != 0)
    self->height_label = gtk_label_new (NULL);

  AppState *state = (AppState *) self->user_data;

  if (self->dirx != 0)
    {
      gtk_box_append (GTK_BOX (state->info_widget), self->width_label);
      gtk_widget_set_valign (self->width_label, GTK_ALIGN_CENTER);
    }

  if (self->diry != 0)
    {
      gtk_box_append (GTK_BOX (state->info_widget), self->height_label);
      gtk_widget_set_valign (self->height_label, GTK_ALIGN_CENTER);
    }
}

static void
drag_update (GtkGestureDrag *gesture, gdouble x, gdouble y, gpointer user_data)
{
  GpaintDragTracker *self = user_data;
  AppState *state = (AppState *) self->user_data;
  char buffer[128];
  gint tx = self->dirx * (gint) (x / state->zoom_level);
  gint ty = self->diry * (gint) (y / state->zoom_level);
  gint width = cairo_image_surface_get_width (state->main_surface);
  gint height = cairo_image_surface_get_height (state->main_surface);

  tx = CLAMP (tx, -width + 1, G_MAXINT);
  ty = CLAMP (ty, -height + 1, G_MAXINT);

  if (tx != 0)
    {
      snprintf (buffer, sizeof (buffer), "W: %d (%c%d)",
                width + tx, "+-"[tx < 0], abs (tx));
      gtk_label_set_text (GTK_LABEL (self->width_label), buffer);
    }

  if (ty != 0)
    {
      snprintf (buffer, sizeof (buffer), "H: %d (%c%d)",
                cairo_image_surface_get_height (state->main_surface) + ty, "+-"[ty < 0], abs (ty));
      gtk_label_set_text (GTK_LABEL (self->height_label), buffer);
    }
}

static void
drag_end (GtkGestureDrag *gesture, double x, double y, gpointer user_data)
{
  GpaintDragTracker *self = user_data;
  self->is_dragging = FALSE;
  self->on_resize (self->user_data, (gint) x, (gint) y, self->dirx, self->diry);

  AppState *state = (AppState *) self->user_data;

  if (self->dirx != 0)
    gtk_box_remove (GTK_BOX (state->info_widget), self->width_label);

  if (self->diry != 0)
    gtk_box_remove (GTK_BOX (state->info_widget), self->height_label);

  self->width_label = NULL;
  self->height_label = NULL;
}

static void
on_draw (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer user_data)
{
  cairo_set_line_width (cr, 2.0);
  gint square_size = GPAINT_DRAG_TRACKER_SIZE;
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_rectangle (cr, 0, 0, square_size, square_size);
  cairo_fill (cr);
  cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
  cairo_rectangle (cr, 0, 0, square_size, square_size);
  cairo_stroke (cr);
}

static void
gpaint_drag_tracker_init (GpaintDragTracker *self)
{
  self->is_dragging = FALSE;

  GtkGesture *drag = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), GDK_BUTTON_PRIMARY);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag));
  g_signal_connect (drag, "drag-begin", G_CALLBACK (drag_begin), self);
  g_signal_connect (drag, "drag-update", G_CALLBACK (drag_update), self);
  g_signal_connect (drag, "drag-end", G_CALLBACK (drag_end), self);
  gtk_widget_set_size_request (GTK_WIDGET (self), GPAINT_DRAG_TRACKER_SIZE, GPAINT_DRAG_TRACKER_SIZE);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self), on_draw, NULL, NULL);
}

static void
gpaint_drag_tracker_class_init (GpaintDragTrackerClass *klass)
{
  return;
}

GtkWidget *
gpaint_drag_tracker_new (ResizeCallback on_resize, gint dirx, gint diry, gpointer user_data)
{
  g_assert (abs (dirx) <= 1);
  g_assert (abs (diry) <= 1);

  GpaintDragTracker *drag_tracker = g_object_new (GPAINT_TYPE_DRAG_TRACKER, NULL);
  drag_tracker->on_resize = on_resize;
  drag_tracker->dirx = dirx;
  drag_tracker->diry = diry;
  drag_tracker->user_data = user_data;

  /* TODO
     - - nwse;
     - 0 ew;
     - + nesw;

     0 - nw;
     0 + nw;

     + - nesw;
     + 0 ew;
     + + nwse;
  */

#define SIGN(a) (((a) > 0) - ((a) < 0))

  const gchar *cursor_name;

  if (dirx == 0)
    cursor_name = "ns-resize";
  else if (diry == 0)
    cursor_name = "ew-resize";
  else if (SIGN (dirx) != SIGN (diry))
    cursor_name = "nesw-resize";
  else
    cursor_name = "nwse-resize";

  g_autoptr (GdkCursor) cursor = gdk_cursor_new_from_name (cursor_name, NULL);

  if (cursor != NULL)
    gtk_widget_set_cursor (GTK_WIDGET (drag_tracker), cursor);
  else
    g_warning ("Failed to create GdkCursor from name: %s.", cursor_name);

  return GTK_WIDGET (drag_tracker);
}
