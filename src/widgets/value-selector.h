
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct
{
  const char *image_path; /* Image file path */
  gint value;
} ValueItem;

#define VALUE_TYPE_SELECTOR (value_selector_get_type ())
G_DECLARE_FINAL_TYPE (ValueSelector, value_selector, VALUE, SELECTOR, GtkBox)

struct _ValueSelector
{
  GtkBox parent_instance;
  GtkDropDown *dropdown;
  GtkStringList *model;
  GArray *values;
  void (*value_changed_callback) (gpointer user_data, gint value);
  gpointer user_data;
};

extern GtkWidget *value_selector_new (const ValueItem *items, guint num_items, void (*callback) (gpointer user_data, int value), gpointer user_data);

G_END_DECLS
