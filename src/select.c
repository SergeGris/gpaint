

/* Callback that is called when an option is clicked.
 * It updates the label displayed on the menu button.
 */
static void
on_option_clicked(GtkButton *button, gpointer user_data)
{
    // user_data is a pointer to the label widget inside the menu button.
    GtkWidget *label = user_data;
    const gchar *selected = gtk_button_get_label(button);
    gtk_label_set_text(GTK_LABEL(label), selected);

    // Hide the popover by popping it down.
    GtkWidget *menu_button = gtk_widget_get_parent(label);
    if (GTK_IS_MENU_BUTTON(menu_button))
        gtk_menu_button_popdown(GTK_MENU_BUTTON(menu_button));
}

/* Creates a GtkMenuButton with an attached popover containing a list
 * of selection options. When an option is clicked, the button label updates.
 */
static GtkWidget *
create_dropdown_button(void)
{
    // Create a menu button.
    GtkWidget *menu_button = gtk_menu_button_new();

    // Create a label to show the current selection.
    GtkWidget *button_label = gtk_label_new("Select Option");
    // Set the label as the child of the menu button.
    gtk_menu_button_set_child(GTK_MENU_BUTTON(menu_button), button_label);

    // Create a popover for the menu button.
    GtkWidget *popover = gtk_popover_new();

    // Create a vertical box to hold the option buttons.
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(vbox, 5);
    gtk_widget_set_margin_bottom(vbox, 5);
    gtk_widget_set_margin_start(vbox, 5);
    gtk_widget_set_margin_end(vbox, 5);

    // List of options
    const gchar *options[] = { "Option 1", "Option 2", "Option 3", NULL };
    for (int i = 0; options[i] != NULL; i++) {
        GtkWidget *option = gtk_button_new_with_label(options[i]);
        gtk_widget_set_halign(option, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(vbox), option);
        g_signal_connect(option, "clicked", G_CALLBACK(on_option_clicked), button_label);
    }

    // Add the vertical box as the child of the popover.
    gtk_popover_set_child(GTK_POPOVER(popover), vbox);
    // Associate the popover with the menu button.
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), popover);

    return menu_button;
}
