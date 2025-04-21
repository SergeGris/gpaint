
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GPAINT_TYPE_DRAG_TRACKER (gpaint_drag_tracker_get_type ())
G_DECLARE_FINAL_TYPE (GpaintDragTracker, gpaint_drag_tracker, GPAINT, DRAG_TRACKER, GtkDrawingArea);

typedef void (*ResizeCallback) (gpointer state, gint dx, gint dy, gint dirx, gint diry);

extern GtkWidget *gpaint_drag_tracker_new (ResizeCallback on_resize, gint dirx, gint diry, gpointer user_data);

G_END_DECLS
