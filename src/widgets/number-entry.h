
#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_ENTRY (my_entry_get_type ())
G_DECLARE_FINAL_TYPE (MyEntry, my_entry, MY, ENTRY, GtkEntry)

GtkWidget *my_entry_new (void);
GtkWidget *my_entry_new_with_initial (const gchar *initial);

G_END_DECLS
