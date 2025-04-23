/* --- New widget class: ValueSelector --- */
#define VALUE_SELECTOR_TYPE (value_selector_get_type ())
G_DECLARE_FINAL_TYPE (ValueSelector, value_selector, VALUE, SELECTOR, GtkBox)

struct _ValueSelector
{
  GtkBox parent_instance;
  GtkWidget *list_box;
  void (*callback) (gpointer user_data, int value);
  AppState *state;
};

G_DEFINE_TYPE (ValueSelector, value_selector, GTK_TYPE_BOX)

static void
on_value_selector_row_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  ValueSelector *selector = VALUE_SELECTOR (user_data);
  if (!row || selector->callback == NULL)
    return;
  /* Instead of using the row index, retrieve the value value stored in the row's data */
  int *val = g_object_get_data (G_OBJECT (row), "value");
  if (val)
    selector->callback (selector->state, GPOINTER_TO_INT (val));
}

static void
value_selector_init (ValueSelector *selector)
{
  gtk_box_set_spacing (GTK_BOX (selector), 8);
  gtk_widget_set_margin_top (GTK_WIDGET (selector), 2);
  gtk_widget_set_margin_bottom (GTK_WIDGET (selector), 2);
  gtk_widget_set_margin_start (GTK_WIDGET (selector), 8);
  gtk_widget_set_margin_end (GTK_WIDGET (selector), 8);

  selector->list_box = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (selector->list_box), GTK_SELECTION_SINGLE);
  g_signal_connect (selector->list_box, "row-selected", G_CALLBACK (on_value_selector_row_selected), selector);

  /* Create rows for value values 1 to 5 */
  for (int i = 0; i < 5; i++)
    {
      GtkWidget *row = gtk_list_box_row_new ();
      char filename[32];
      snprintf (filename, sizeof (filename), "line%d.png", i + 1);
      GtkWidget *image = gtk_image_new_from_file (filename);
      gtk_widget_set_size_request (image, 64, 8);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), image);
      gtk_widget_set_margin_bottom (row, 4);
      g_object_set_data_full (G_OBJECT (row), "value", GINT_TO_POINTER (i + 1), NULL);
      gtk_list_box_append (GTK_LIST_BOX (selector->list_box), row);
    }
  gtk_box_append (GTK_BOX (selector), selector->list_box);
  /* Programmatically select the first row and set current_value */
  GtkListBoxRow *first = gtk_list_box_get_row_at_index (GTK_LIST_BOX (selector->list_box), 0);
  if (first)
    {
      gtk_list_box_select_row (GTK_LIST_BOX (selector->list_box), first);
    }
}

static void
value_selector_class_init (ValueSelectorClass *klass)
{
  /* Nothing needed here for now */
}

GtkWidget *
value_selector_new (void (*callback) (gpointer user_data, int value), gpointer user_data)
{
  ValueSelector *t = g_object_new (VALUE_SELECTOR_TYPE, NULL);
  t->callback = callback;
  t->state = user_data;
  return GTK_WIDGET (t);
}

/* /\* --- Usage Example (replace value_widget function with the following) --- *\/ */
/* static GtkWidget *value_widget(AppState *state) { */
/*     ValueSelector *selector = value_selector_new(); */
/*     /\* Optionally, connect to a "value-changed" signal if you add one *\/ */
/*     return GTK_WIDGET(selector); */
/* } */
