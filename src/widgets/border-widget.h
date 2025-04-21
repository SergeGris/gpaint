
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GPAINT_TYPE_BORDER_WIDGET (gpaint_border_widget_get_type ())
G_DECLARE_FINAL_TYPE (GpaintBorderWidget, gpaint_border_widget, GPAINT, BORDER_WIDGET, GtkWidget)

extern GtkWidget *gpaint_border_widget_new (void);
extern void gpaint_border_widget_set_child (GpaintBorderWidget *self, GtkWidget *child);
extern void gpaint_border_widget_set_border_width (GpaintBorderWidget *self, guint width);
extern void gpaint_border_widget_set_border_color (GpaintBorderWidget *self, const GdkRGBA *color);

G_END_DECLS
