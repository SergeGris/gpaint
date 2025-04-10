

#include "tools-internal.h"

static void draw_drag_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void drag_begin (AppState *state);
static void drag_update (AppState *state, gdouble dx, gdouble dy);
static void drag_end (AppState *state);

const Tool global_drag_tool = {
  .type = TOOL_DRAG,
  .icon = &grab_data,
  .cursor = "grab",
  .draw_handler = draw_drag_handler,
  .motion_handler = NULL,
  .drag_begin = drag_begin,
  .drag_update = drag_update,
  .drag_end = drag_end,
  .is_drawing = FALSE,
};

static void
draw_drag_handler (AppState *state, gint x0, gint y0, gint x1, gint y1)
{
}

/*
 * update_adjustments:
 *   Given a delta (dx, dy) in pixels, compute new adjustment values
 *   by subtracting the delta from the current value, and clamp the result
 *   within the allowed scrolling range: [lower, (upper - page_size)].
 */
static void
update_adjustments (AppState *state, double dx, double dy)
{
  double curr_x = gtk_adjustment_get_value (state->hadj);
  double curr_y = gtk_adjustment_get_value (state->vadj);

  double lower_x = gtk_adjustment_get_lower (state->hadj);
  double upper_x = gtk_adjustment_get_upper (state->hadj) - gtk_adjustment_get_page_size (state->hadj);
  double lower_y = gtk_adjustment_get_lower (state->vadj);
  double upper_y = gtk_adjustment_get_upper (state->vadj) - gtk_adjustment_get_page_size (state->vadj);

  double new_x = CLAMP (curr_x - dx, lower_x, upper_x);
  double new_y = CLAMP (curr_y - dy, lower_y, upper_y);

  gtk_adjustment_set_value (state->hadj, new_x);
  gtk_adjustment_set_value (state->vadj, new_y);
}

/* Inertial scrolling callback:
   Applies friction and updates the adjustments with the decaying velocity.
   Clamping is applied on every update.
*/
/* static gboolean */
/* inertial_scroll_callback(gpointer user_data) */
/* { */
/*     AppState *state = (AppState *) user_data; */
/*     double dt = 0.016; // assume 16ms per frame */
/*     // Apply friction factor (e.g., 0.95) */
/*     state->velocity_x *= 0.95; */
/*     state->velocity_y *= 0.95; */

/*     // Update adjustments using velocity */
/*     update_adjustments(state, state->velocity_x * dt, state->velocity_y * dt); */

/*     // Stop inertial scrolling if velocities are very low */
/*     if (fabs(state->velocity_x) < 1.0 && fabs(state->velocity_y) < 1.0) */
/*         return G_SOURCE_REMOVE; */
/*     return G_SOURCE_CONTINUE; */
/* } */

static void
drag_begin (AppState *state)
{
  g_autoptr (GdkCursor) cursor = gdk_cursor_new_from_name ("grabbing", NULL);
  gtk_widget_set_cursor (state->drawing_area, cursor);
}

static void
drag_update (AppState *state, gdouble dx, gdouble dy)
{
  // TODO
  /*
   *   Called repeatedly during a drag gesture. It receives incremental x
   *   and y values (in pixels) representing the change since the last event.
   *   It applies these changes to the adjustments (with proper clamping) and computes
   *   an instantaneous velocity based on the time difference.
   */

  /* // Update scrolling based on drag delta */
  /* update_adjustments(state, x, y); */

  /* // Get current time in microseconds */
  /* gint64 now = g_get_monotonic_time(); */
  /* double dt = (state->last_drag_time > 0) ? (now - state->last_drag_time) / 1000000.0 : 0.016; */

  /* // Compute instantaneous velocity (pixels per second) */
  /* if (dt > 0) { */
  /*     state->velocity_x = x / dt; */
  /*     state->velocity_y = y / dt; */
  /* } */

  /* // Store current time for next update */
  /* state->last_drag_time = now; */

  // TODO
  double x_offset = gtk_adjustment_get_value (state->hadj);
  double y_offset = gtk_adjustment_get_value (state->vadj);

  gtk_adjustment_set_value (state->hadj, x_offset - dx);
  gtk_adjustment_set_value (state->vadj, y_offset - dy);
}

static void
drag_end (AppState *state)
{
  g_autoptr (GdkCursor) cursor = gdk_cursor_new_from_name ("grab", NULL);
  gtk_widget_set_cursor (state->drawing_area, cursor);

  // TODO
  /* state->last_drag_time = 0; */
  /* if (fabs(state->velocity_x) > 0.0 || fabs(state->velocity_y) > 0.0) */
  /*   state->inertia_timeout_id = g_timeout_add(16, inertial_scroll_callback, state); */
}
