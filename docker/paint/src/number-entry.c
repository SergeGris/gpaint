#include <gtk/gtk.h>
#include <errno.h>
#include <glib.h>

G_BEGIN_DECLS

#define MY_TYPE_ENTRY (my_entry_get_type())
G_DECLARE_FINAL_TYPE(MyEntry, my_entry, MY, ENTRY, GtkEntry)

struct _MyEntry {
    GtkEntry parent_instance;

    GtkPopover *popover;
    GtkLabel *error_label;
    gchar *last_valid_value;
    guint timeout_id;
};

GtkWidget *my_entry_new(void);
GtkWidget *my_entry_new_with_initial(const gchar *initial);

G_END_DECLS

G_DEFINE_TYPE(MyEntry, my_entry, GTK_TYPE_ENTRY)

static void show_error(MyEntry *self, const gchar *message);
static void revert_to_last_valid(MyEntry *self);
static gboolean hide_error(MyEntry *self);

static void on_insert_text(GtkEditable *editable,
                          const gchar *text,
                          gint length,
                          gint *position,
                          gpointer user_data) {
    for (gint i = 0; i < length; i++) {
        if (!g_ascii_isdigit(text[i])) {
            MyEntry *self = MY_ENTRY(editable);
            show_error(self, "Non-digit characters not allowed");
            g_signal_stop_emission_by_name(editable, "insert-text");
            return;
        }
    }
}

static void on_changed(GtkEditable *editable, gpointer user_data) {
    MyEntry *self = MY_ENTRY(editable);
    const gchar *current_text = gtk_editable_get_text(editable);

    if (current_text[0] == '\0') {
        show_error(self, "Value cannot be empty");
        revert_to_last_valid(self);
        return;
    }

    if (strcmp(current_text, "0") == 0) {
        show_error(self, "Zero is not allowed");
        revert_to_last_valid(self);
        return;
    }

    gchar *endptr;
    errno = 0;
    long value = strtol(current_text, &endptr, 10);

    if (errno == ERANGE) {
        show_error(self, "Value exceeds maximum limit");
        revert_to_last_valid(self);
        return;
    }

    g_free(self->last_valid_value);
    self->last_valid_value = g_strdup(current_text);
}

static void show_error(MyEntry *self, const gchar *message) {
    if (self->timeout_id != 0) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }

    gtk_label_set_text(self->error_label, message);
    gtk_popover_popup(self->popover);
    self->timeout_id = g_timeout_add_seconds(3, (GSourceFunc)hide_error, self);
}

static gboolean hide_error(MyEntry *self) {
    gtk_popover_popdown(self->popover);
    self->timeout_id = 0;
    return G_SOURCE_REMOVE;
}

static void revert_to_last_valid(MyEntry *self) {
  return;
    g_signal_handlers_block_by_func(self, on_changed, NULL);
    gtk_editable_set_text(GTK_EDITABLE(self), self->last_valid_value ? self->last_valid_value : "1");
    g_signal_handlers_unblock_by_func(self, on_changed, NULL);
}

static void my_entry_init(MyEntry *self) {
    self->popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_set_parent(GTK_WIDGET(self->popover), GTK_WIDGET(self));

    self->error_label = GTK_LABEL(gtk_label_new(""));
    gtk_popover_set_child(self->popover, GTK_WIDGET(self->error_label));

    self->timeout_id = 0;
    self->last_valid_value = NULL;

    GtkEditable *editable = GTK_EDITABLE(self);
    g_signal_connect(editable, "insert-text", G_CALLBACK(on_insert_text), NULL);
    g_signal_connect(editable, "changed", G_CALLBACK(on_changed), NULL);
}

static void my_entry_class_init(MyEntryClass *klass) {
    // Class initialization not needed for this example
}

GtkWidget *my_entry_new(void) {
    return my_entry_new_with_initial("1");
}

GtkWidget *my_entry_new_with_initial(const gchar *initial) {
    MyEntry *entry = MY_ENTRY(g_object_new(MY_TYPE_ENTRY, NULL));

    // Set default valid value
    entry->last_valid_value = g_strdup("1");

    // Set initial value if provided
    if (initial != NULL) {
        gtk_editable_set_text(GTK_EDITABLE(entry), initial);
    }

    return GTK_WIDGET(entry);
}
