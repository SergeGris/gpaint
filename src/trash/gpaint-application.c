#include "gpaint-application.h"
#include "config.h"
#include "gpaint-window.h"
#include <glib/gi18n.h>

G_DEFINE_FINAL_TYPE (GpaintApplication, gpaint_application, ADW_TYPE_APPLICATION)

GpaintApplication *
gpaint_application_new (const char *application_id, GApplicationFlags flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);
  return g_object_new (GPAINT_TYPE_APPLICATION, "application-id", application_id, "flags", flags, "resource-base-path", "/org/gnu/paint", NULL);
}

static void
gpaint_application_activate (GApplication *app)
{
  GtkWindow *window;
  g_assert (GPAINT_IS_APPLICATION (app));
  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL)
    {
      /* Create a new window and store it in our application instance */
      window = g_object_new (GPAINT_TYPE_WINDOW, "application", app, NULL);
      GPAINT_APPLICATION (app)->window = GTK_WIDGET (window);
      /* Optionally, initialize other widget pointers here (or later in your window code) */
    }
  gtk_window_present (window);
}

static void
gpaint_application_class_init (GpaintApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
  app_class->activate = gpaint_application_activate;
}

static void
gpaint_application_about_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  static const char *developers[] = { "", NULL };
  GpaintApplication *self = GPAINT_APPLICATION (user_data);
  GtkWindow *window = NULL;
  g_assert (GPAINT_IS_APPLICATION (self));
  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  adw_show_about_dialog (GTK_WIDGET (window), "application-name", "gpaint", "application-icon", "org.gnu.paint", "developer-name", "", "translator-credits", _ ("translator-credits"), "version", "0.1.0", "developers", developers,
                         "copyright", "© 2025 ", NULL);
}

static void
gpaint_application_quit_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GpaintApplication *self = GPAINT_APPLICATION (user_data);
  g_assert (GPAINT_IS_APPLICATION (self));
  g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
  { "quit", gpaint_application_quit_action },
  { "about", gpaint_application_about_action },
};

static void
gpaint_application_init (GpaintApplication *self)
{
  /* Initialize additional widget pointers to NULL */
  self->window = NULL;
  self->drawing_area = NULL;
  self->color_btn = NULL;
  self->color_swap_button = NULL;
  self->zoom_label = NULL;
  self->grid_toggle = NULL;
  self->file_toolbar = NULL;
  self->status_bar = NULL;
  self->width_selector = NULL;
  self->fill_selector = NULL;
  self->eraser_size_selector = NULL;
  self->action_group = NULL;

  /* Add application actions */
  g_action_map_add_action_entries (G_ACTION_MAP (self), app_actions, G_N_ELEMENTS (app_actions), self);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.quit", (const char *[]) { "<primary>q", NULL });
}
