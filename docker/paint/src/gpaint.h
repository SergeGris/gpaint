
#pragma once

#include <gtk/gtk.h>

typedef struct Tool Tool;

struct _BackupManager
{
  GQueue *undo; // Stack of backups for undo (most recent at head)
  GQueue *redo; // Stack of backups for redo (most recent at head)
  GSimpleAction *undo_action;
  GSimpleAction *redo_action;
};

typedef struct
{
  gint x;
  gint y;
} Point;

typedef enum
{
  FILL_TRANSPARENT,
  FILL_PRIMARY,
  FILL_SECONDARY,
} FillType;

typedef struct
{
  GtkWidget *window;
  GtkWidget *drawing_area;
  GtkWidget *color_btn;
  GtkWidget *color_swap_button;
  GtkWidget *zoom_label;
  GtkWidget *grid_toggle;
  GtkWidget *file_toolbar;
  GtkWidget *image_info;
  GtkWidget *current_position;
  GtkWidget *width_selector;
  GtkWidget *fill_selector;
  GtkWidget *eraser_size_selector;
  GtkWidget *info_widget;

  GAction *show_grid_action;
  GtkWidget *scrolled;
  GSimpleActionGroup *action_group;

  cairo_surface_t *main_surface;
  cairo_surface_t *preview_surface;
  cairo_surface_t *select_surface;
  struct _BackupManager backup_manager;
  const GdkRGBA *p_color;
  const GdkRGBA *s_color;
  GdkRGBA primary_color;
  GdkRGBA secondary_color;
  gboolean is_drawing;
  gdouble zoom_level;
  const Tool *primary_tool;
  gdouble width;
  FillType fill_type;
  gdouble eraser_size;
  gdouble brush_size;
  Point start_point;
  Point last_point;
  gint button;

  gdouble cursor_x, cursor_y;
} AppState;

#include "gpaint-cairo.h"
#include "backup.h"

// TODO
static void set_action_enabled_by_name(GActionMap *action_map, const char *action_name, gboolean enabled) {
  GAction *action = g_action_map_lookup_action(action_map, action_name);
  if (action) {
    g_simple_action_set_enabled(G_SIMPLE_ACTION( action), enabled);
  } else {
    g_warning("Action '%s' not found.", action_name);
  }
}
