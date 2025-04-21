
#pragma once

#include <cairo.h>
#include <gtk/gtk.h>

#define GPAINT_TYPE_LAYERS_WIDGET (gpaint_layers_widget_get_type ())
G_DECLARE_FINAL_TYPE (GPaintLayersWidget, gpaint_layers_widget, GPAINT, LAYERS_WIDGET, GtkBox);

#define GPAINT_TYPE_PREVIEW_WIDGET (gpaint_preview_widget_get_type ())
G_DECLARE_FINAL_TYPE (GPaintPreviewWidget, gpaint_preview_widget, GPAINT, PREVIEW_WIDGET, GtkWidget);

typedef struct _LayerRow
{
  GtkWidget *row;
  GtkToggleButton *toggle;
  GtkWidget *delete_button;
  GtkToggleButton *special_toggle;
  GPaintPreviewWidget *preview;
} LayerRow;

struct _GPaintLayersWidget
{
  GtkBox parent_instance;
  gint width, height;
  GList *layer_rows;
  GtkWidget *selected_toggle;
  gint preview_size;
  gint spacing;
  GtkWidget *add_button;
};

struct _GPaintPreviewWidget
{
  GtkWidget parent_instance;
  cairo_surface_t *surface;
  gint target_size;
};
