#ifndef BORDERED_WIDGET_H
#define BORDERED_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TYPE_BORDERED_WIDGET (bordered_widget_get_type ())
G_DECLARE_FINAL_TYPE (BorderedWidget, bordered_widget, BORDERED, WIDGET, GtkWidget)

GtkWidget *bordered_widget_new (void);
void bordered_widget_set_child (BorderedWidget *self, GtkWidget *child);
GtkWidget *bordered_widget_get_child (BorderedWidget *self);

G_END_DECLS

#endif // BORDERED_WIDGET_H
