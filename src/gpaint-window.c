
#include "config.h"

#include "gpaint-window.h"

struct _GpaintWindow
{
  AdwApplicationWindow parent_instance;
};

G_DEFINE_FINAL_TYPE (GpaintWindow, gpaint_window, ADW_TYPE_APPLICATION_WINDOW)

static void
gpaint_window_class_init (GpaintWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
}

static void
gpaint_window_init (GpaintWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
