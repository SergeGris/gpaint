#include <gtk/gtk.h>

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

G_DEFINE_TYPE (ValueSelector, value_selector, GTK_TYPE_BOX)

/* Factory setup: Create image widget for each list item */
static void
factory_setup (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *image = gtk_image_new ();
  // TODO gtk_image_set_pixel_size (GTK_IMAGE (image), 32); /* Set image size */
  gtk_list_item_set_child (list_item, image);
}

/* Factory bind: Load image from path */
static void
factory_bind (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *image = gtk_list_item_get_child (list_item);
  const char *path = gtk_string_object_get_string (GTK_STRING_OBJECT (gtk_list_item_get_item (list_item)));
  gtk_image_set_from_file (GTK_IMAGE (image), path);
}

static void
on_dropdown_notify_selected (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  ValueSelector *selector = VALUE_SELECTOR (user_data);
  guint index = gtk_drop_down_get_selected (selector->dropdown);
  if (index < selector->values->len)
    {
      gint value = g_array_index (selector->values, gint, index);
      if (selector->value_changed_callback)
        selector->value_changed_callback (selector->user_data, value);
    }
}

static void
value_selector_init (ValueSelector *selector)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (selector), GTK_ORIENTATION_HORIZONTAL);
  selector->values = g_array_new (FALSE, FALSE, sizeof (gint));
  selector->model = GTK_STRING_LIST (gtk_string_list_new (NULL));

  /* Create factory with image support */
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (factory_setup), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (factory_bind), NULL);

  /* Create dropdown with image factory */
  selector->dropdown = GTK_DROP_DOWN (gtk_drop_down_new (G_LIST_MODEL (selector->model), NULL));
  gtk_drop_down_set_factory (selector->dropdown, factory);
  gtk_box_append (GTK_BOX (selector), GTK_WIDGET (selector->dropdown));
  g_signal_connect (selector->dropdown, "notify::selected", G_CALLBACK (on_dropdown_notify_selected), selector);
  gtk_widget_set_halign (GTK_WIDGET (selector), GTK_ALIGN_CENTER);
}

static void
value_selector_finalize (GObject *object)
{
  ValueSelector *selector = VALUE_SELECTOR (object);
  g_array_unref (selector->values);
  G_OBJECT_CLASS (value_selector_parent_class)->finalize (object);
}

static void
value_selector_class_init (ValueSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = value_selector_finalize;
}

GtkWidget *
value_selector_new (const ValueItem *items, guint num_items, void (*callback) (gpointer user_data, gint value), gpointer user_data)
{
  ValueSelector *selector = (ValueSelector *) g_object_new (VALUE_TYPE_SELECTOR, NULL);
  selector->value_changed_callback = callback;
  selector->user_data = user_data;

  GPtrArray *paths = g_ptr_array_new_with_free_func (g_free);
  for (guint i = 0; i < num_items; i++)
    {
      g_ptr_array_add (paths, g_strdup (items[i].image_path));
      g_array_append_val (selector->values, items[i].value);
    }
  g_ptr_array_add (paths, NULL);

  selector->model = GTK_STRING_LIST (gtk_string_list_new ((const gchar *const *) paths->pdata));
  g_ptr_array_unref (paths);

  gtk_drop_down_set_model (selector->dropdown, G_LIST_MODEL (selector->model));
  if (num_items > 0)
    gtk_drop_down_set_selected (selector->dropdown, 0);

  // TODO
  gtk_widget_set_size_request (GTK_WIDGET (selector->dropdown), 64, 32); // Set appropriate size for images

  return GTK_WIDGET (selector);
}

#if 0
#include <gtk/gtk.h>

/* Define the ValueItem structure */
typedef struct
{
  const char *image_path;
  int value;
} ValueItem;

/* Define the ValueSelector type */
#define VALUE_SELECTOR_TYPE (value_selector_get_type ())
G_DECLARE_FINAL_TYPE (ValueSelector, value_selector, VALUE, SELECTOR, GtkBox)

/* Instance structure for ValueSelector */
struct _ValueSelector
{
  GtkBox parent_instance;
  GtkWidget *list_box;
  void (*value_changed_callback) (gpointer user_data, int value);
  gpointer user_data;
};

G_DEFINE_TYPE (ValueSelector, value_selector, GTK_TYPE_BOX)

/* Callback function for row selection */
static void
on_value_selector_row_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  ValueSelector *selector = VALUE_SELECTOR (user_data);

  if (row != NULL && selector->value_changed_callback)
    {
      int value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "value"));
      selector->value_changed_callback (selector->user_data, value);
    }
}

/* Initialize the ValueSelector instance */
static void
value_selector_init (ValueSelector *selector)
{
  selector->list_box = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (selector->list_box), GTK_SELECTION_SINGLE);
  g_signal_connect (selector->list_box, "row-selected", G_CALLBACK (on_value_selector_row_selected), selector);
  gtk_box_append (GTK_BOX (selector), selector->list_box);
}

/* Finalize function to free allocated resources */
static void
value_selector_finalize (GObject *object)
{
  G_OBJECT_CLASS (value_selector_parent_class)->finalize (object);
}

/* Class initialization */
static void
value_selector_class_init (ValueSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = value_selector_finalize;
}

/* Function to create a new ValueSelector instance */
GtkWidget *
value_selector_new (const ValueItem *items, guint num_items, void (*callback) (gpointer user_data, int value), gpointer user_data)
{
  ValueSelector *selector = g_object_new (VALUE_SELECTOR_TYPE, NULL);
  selector->value_changed_callback = callback;
  selector->user_data = user_data;

  for (guint i = 0; i < num_items; i++)
    {
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *image = gtk_image_new_from_file (items[i].image_path);
      // TODO gtk_widget_set_size_request(image, 64, 64); // Set appropriate size for images
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), image);
      g_object_set_data (G_OBJECT (row), "value", GINT_TO_POINTER (items[i].value));
      gtk_list_box_append (GTK_LIST_BOX (selector->list_box), row);

      gtk_widget_set_margin_top (GTK_WIDGET (row), 2);
      gtk_widget_set_margin_bottom (GTK_WIDGET (row), 2);
    }

  // TODO
  gtk_widget_set_halign (GTK_WIDGET (selector), GTK_ALIGN_CENTER);

  /* Select the first row by default */
  GtkListBoxRow *first_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (selector->list_box), 0);

  if (first_row)
    gtk_list_box_select_row (GTK_LIST_BOX (selector->list_box), first_row);

  return GTK_WIDGET (selector);
}
#endif
