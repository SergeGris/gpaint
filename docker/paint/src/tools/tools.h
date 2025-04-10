
#pragma once

#include "gpaint.h"
#include <glib.h>

typedef enum
{
  TOOL_SELECT_RECTANGLE,
  TOOL_FREEHAND,
  TOOL_BRUSH,
  TOOL_LINE,
  TOOL_RECTANGLE,
  TOOL_ELLIPSE,
  TOOL_BUCKET,
  TOOL_ERASER
} ToolType;

struct raw_bitmap
{
  gint height, width, stride, rowstride;
  gboolean has_alpha;
  GdkColorspace colorspace;
  gint bits_per_sample;
  const guchar data[1024];
};

struct Tool
{
  ToolType type;
  const struct raw_bitmap *icon;
  const struct raw_bitmap *cursor_icon;
  const gchar *cursor;
  void (*draw_handler) (AppState *state, gint x0, gint y0, gint x1, gint y1);
  void (*motion_handler) (AppState *state, gint x, gint y);
  void (*draw_cursor_handler) (AppState *state, cairo_t *cr);
};

extern const Tool global_select_rectangle_tool;
extern const Tool global_freehand_tool;
extern const Tool global_brush_tool;
extern const Tool global_line_tool;
extern const Tool global_rectangle_tool;
extern const Tool global_ellipse_tool;
extern const Tool global_eraser_tool;
extern const Tool global_picker_tool;
extern const Tool global_bucket_tool;
