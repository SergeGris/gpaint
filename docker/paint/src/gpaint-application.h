
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GPAINT_TYPE_APPLICATION (gpaint_application_get_type ())

/* Custom application instance structure with additional widget pointers */
struct _GpaintApplication
{
  AdwApplication parent_instance;

  /* Additional widget fields */
  GtkWidget *window;
  GtkWidget *drawing_area;
  GtkWidget *color_btn;
  GtkWidget *color_swap_button;
  GtkWidget *zoom_label;
  GtkWidget *grid_toggle;
  GtkWidget *file_toolbar;
  GtkWidget *status_bar;
  GtkWidget *width_selector;
  GtkWidget *fill_selector;
  GtkWidget *eraser_size_selector;
  GSimpleActionGroup *action_group;
};

G_DECLARE_FINAL_TYPE (GpaintApplication, gpaint_application, GPAINT, APPLICATION, AdwApplication)

GpaintApplication *gpaint_application_new (const char *application_id, GApplicationFlags flags);

G_END_DECLS
