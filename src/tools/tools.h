
#pragma once

#include "gpaint.h"
#include <glib.h>

typedef enum
{
  TOOL_SELECT_RECTANGLE,
  TOOL_FREEHAND,
  TOOL_BRUSH,
  TOOL_PICKER,
  TOOL_BUCKET,

  // shapes
  TOOL_LINE,
  TOOL_RECTANGLE,
  TOOL_ELLIPSE,
  TOOL_TRIANGLE,

  TOOL_ERASER,
  TOOL_DRAG,
  TOOL_SYMMETRIC_FREEHAND,
  TOOLS_COUNT,
} ToolType;

struct raw_bitmap
{
  const guchar *data; // TODO
  guint size;
  gint hotspot_x, hotspot_y;
};

struct Tool
{
  ToolType type;
  const struct raw_bitmap *icon;
  const struct raw_bitmap *cursor_icon;
  const gchar *cursor_name;
  void (*draw_handler) (AppState *state, gint x0, gint y0, gint x1, gint y1);
  void (*motion_handler) (AppState *state, gint x, gint y);
  void (*drag_begin) (AppState *state);
  void (*drag_update) (AppState *state, gdouble dx, gdouble dy);
  void (*drag_end) (AppState *state);
  void (*draw_cursor_handler) (AppState *state, cairo_t *cr);
  GdkCursor *cursor;
  gboolean override_main_surface;
  gboolean is_drawing;
};

extern const Tool global_select_rectangle_tool;
extern const Tool global_freehand_tool;
extern const Tool global_brush_tool;
extern const Tool global_line_tool;
extern const Tool global_rectangle_tool;
extern const Tool global_triangle_tool;
extern const Tool global_ellipse_tool;
extern const Tool global_eraser_tool;
extern const Tool global_picker_tool;
extern const Tool global_bucket_tool;
extern const Tool global_drag_tool;
extern const Tool global_symmetric_freehand_tool;
