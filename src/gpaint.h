
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
  const gchar *label;
  const Tool *tool;
  GtkWidget *btn;
} ToolEntry;

// TODO
/* typedef struct */
/* { */
/*   uint8_t red, green, blue, alpha; */
/* } GpaintRGBA; */

typedef struct
{
  GtkApplication *application;
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

  GtkWidget *layers;
  GAction *antialiasing_action;
  GAction *show_grid_action;
  GAction *cut_action;
  GAction *copy_action;
  GtkWidget *scrolled;
  GtkAdjustment *vadj, *hadj;
  cairo_antialias_t antialiasing;

  gboolean smooth_preview;

  double velocity_x;
  double velocity_y;
  gint64 last_drag_time;
  guint inertia_timeout_id;

  cairo_format_t format;
  cairo_surface_t *main_surface;
  cairo_surface_t *preview_surface;

  // int width, height; // TODO

  cairo_surface_t *selected_surface;
  gboolean has_selection;
  GdkRectangle selected_rect;
  gboolean is_dragging_selection;
  gdouble drag_start_x, drag_start_y;
  gdouble selection_start_x;
  gdouble selection_start_y;

  struct _BackupManager backup_manager;
  GdkRGBA *p_color;
  GdkRGBA *s_color;
  GdkRGBA primary_color;
  GdkRGBA secondary_color;
  gboolean is_drawing;
  gdouble zoom_level;
  const Tool *tool;
  GdkCursor *cursors[32]; // TODO
  gdouble width;
  FillType fill_type;
  gdouble eraser_size;
  gdouble brush_size;
  Point start_point;
  Point last_point;
  ToolEntry *tools;

  gdouble cursor_x, cursor_y;
} AppState;

#include "backup.h"
#include "gpaint-cairo.h"

#define GPAINT_GDK_RGBA_GREY(a) ((GdkRGBA) { a, a, a, 1.0 })
#define GPAINT_GDK_TRANSPARENT ((GdkRGBA) { 0.0, 0.0, 0.0, 0.0 })
#define GPAINT_GDK_BLACK ((GdkRGBA) { 0.0, 0.0, 0.0, 1.0 })
#define GPAINT_TRANSPARENT_FIRST_COLOR 0.8
#define GPAINT_TRANSPARENT_SECOND_COLOR 0.7

static inline cairo_surface_t *
gpaint_get_current_surface (AppState *state)
{
  return state->main_surface;
}

// TODO

#ifndef g_autoptr
#define g_autoptr(type) type *
#endif

#ifndef g_autofree
#define g_autofree
#endif

// TODO
/* static void */
/* set_action_enabled_by_name (GActionMap *action_map, const char *action_name,
 * gboolean enabled) */
/* { */
/*   GAction *action = g_action_map_lookup_action (action_map, action_name); */
/*   if (action) */
/*     { */
/*       g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled); */
/*     } */
/*   else */
/*     { */
/*       g_warning ("Action '%s' not found.", action_name); */
/*     } */
/* } */
