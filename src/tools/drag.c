

#include "tools-internal.h"

static const struct raw_bitmap grab_data;
static void draw_drag_handler (AppState *state, gint x0, gint y0, gint x1, gint y1);
static void drag_begin (AppState *state);
static void drag_update (AppState *state, gdouble dx, gdouble dy);
static void drag_end (AppState *state);

const Tool global_drag_tool = {
  .type = TOOL_DRAG,
  .icon = &grab_data,
  .cursor_name = "grab",
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
  return;
}

/*
 * update_adjustments:
 *   Given a delta (dx, dy) in pixels, compute new adjustment values
 *   by subtracting the delta from the current value, and clamp the result
 *   within the allowed scrolling range: [lower, (upper - page_size)].
 */
// TODO
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
  gtk_widget_queue_draw (state->drawing_area); // TODO
}

/* Inertial scrolling callback:
   Applies friction and updates the adjustments with the decaying velocity.
   Clamping is applied on every update.
*/
static gboolean
inertial_scroll_callback(gpointer user_data)
{
    AppState *state = (AppState *) user_data;
    double dt = 0.016; // assume 16ms per frame
    // Apply friction factor (e.g., 0.95)
    state->velocity_x *= 0.95;
    state->velocity_y *= 0.95;

    // Update adjustments using velocity
    update_adjustments(state, state->velocity_x * dt, state->velocity_y *
dt);

    // Stop inertial scrolling if velocities are very low
    if (fabs(state->velocity_x) < 1.0 && fabs(state->velocity_y) < 1.0)
      {
        state->inertia_timeout_id = 0;
        return G_SOURCE_REMOVE;
      }
    return G_SOURCE_CONTINUE;
}

static void
drag_begin (AppState *state)
{
  if (state->inertial)
    {
      if (state->inertia_timeout_id) {
        g_source_remove(state->inertia_timeout_id);
        state->inertia_timeout_id = 0;
      }

      state->velocity_x = state->velocity_y = 0.0;
      state->last_drag_time = g_get_monotonic_time();
    }

  g_autoptr (GdkCursor) cursor = gdk_cursor_new_from_name ("grabbing", NULL);
  gtk_widget_set_cursor (state->drawing_area, cursor);
}

static void
drag_update (AppState *state, gdouble dx, gdouble dy)
{
  // TODO...
  printf("%lf %lf\n", dx, dy); // TODO
  if ((fabs (dx) + fabs(dy)) < 1.0)
    return;

  // TODO
  /*
   *   Called repeatedly during a drag gesture. It receives incremental x
   *   and y values (in pixels) representing the change since the last event.
   *   It applies these changes to the adjustments (with proper clamping) and
   * computes an instantaneous velocity based on the time difference.
   */

  /* /\* // Update scrolling based on drag delta *\/ */
  /* /\* update_adjustments(state, x, y); *\/ */

  /* // Get current time in microseconds */
  /* gint64 now = g_get_monotonic_time(); */
  /* double dt = (state->last_drag_time > 0) ? (now - state->last_drag_time) / 1000000.0 : 0.016; */

  /* // Compute instantaneous velocity (pixels per second) */
  /* if (dt > 0) { */
  /*     state->velocity_x = dx / dt; */
  /*     state->velocity_y = dy / dt; */
  /* } */

  /* // Store current time for next update */
  /* state->last_drag_time = now; */

  /* // TODO */
  /* double x_offset = gtk_adjustment_get_value (state->hadj); */
  /* double y_offset = gtk_adjustment_get_value (state->vadj); */

  /* gtk_adjustment_set_value (state->hadj, x_offset - dx); */
  /* gtk_adjustment_set_value (state->vadj, y_offset - dy); */

  if (state->inertial)
    {
      // Current time and delta-time in seconds
      const gint64 now = g_get_monotonic_time();
      double dt = (now - state->last_drag_time) * 1.0e-6;
      if (dt < 1e-3) dt = 1e-3;  // prevent division-by-zero spikes

      update_adjustments(state, dx, dy);

      // Compute new velocity (pixels per second)
      state->velocity_x = dx / dt;
      state->velocity_y = dy / dt;

      // Store time for next iteration
      state->last_drag_time = now;
    }
  else
    update_adjustments(state, dx, dy);
}

static void
drag_end (AppState *state)
{
  g_autoptr (GdkCursor) cursor = gdk_cursor_new_from_name ("grab", NULL);
  gtk_widget_set_cursor (state->drawing_area, cursor);

  // TODO
  if (state->inertial)
    {
  state->last_drag_time = 0;
  if (fabs(state->velocity_x) > 0.0 || fabs(state->velocity_y) > 0.0)
    state->inertia_timeout_id = g_timeout_add(16, inertial_scroll_callback, state);
    }
}

// clang-format off
static const guchar grab_bytes[] =
  {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18, 0x08, 0x06, 0x00, 0x00, 0x00, 0xE0, 0x77, 0x3D, 0xF8, 0x00, 0x00, 0x00, 0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7C, 0x08, 0x64, 0x88, 0x00, 0x00, 0x00, 0x81, 0x49, 0x44, 0x41, 0x54, 0x48, 0xC7, 0xED, 0x54, 0x41, 0x0A, 0xC0, 0x30, 0x08, 0x33, 0xA5, 0xFF, 0xFF, 0x72, 0x76, 0x98, 0x07, 0x29, 0xDB, 0xAA, 0xD6, 0xC1, 0x0A, 0x13, 0x7A, 0x2B, 0x89, 0x89, 0x51, 0x91, 0x58, 0x51, 0x9F, 0xBB, 0x10, 0x01, 0x27, 0x4F, 0x6C, 0x00, 0x6E, 0x8C, 0x96, 0xED, 0x9A, 0xA4, 0x28, 0x21, 0x57, 0x08, 0x68, 0x80, 0x52, 0xD6, 0x35, 0x29, 0xA8, 0x27, 0x35, 0x25, 0x04, 0xAB, 0x33, 0xF8, 0x09, 0x36, 0x27, 0xE8, 0x17, 0x79, 0xCE, 0x6C, 0xB9, 0x5F, 0xC1, 0x5D, 0xA6, 0xED, 0xB2, 0x39, 0x16, 0x6F, 0x6E, 0x91, 0x17, 0x64, 0xF6, 0x6F, 0x24, 0x80, 0x3D, 0x64, 0x91, 0x4E, 0x23, 0x0A, 0x30, 0x5C, 0xCB, 0x3D, 0x63, 0x5A, 0xA6, 0xA2, 0x4C, 0x81, 0x36, 0x84, 0x08, 0x41, 0x89, 0x8A, 0xD7, 0x67, 0x00, 0x5F, 0xD4, 0x99, 0xB2, 0xE7, 0x3B, 0x0A, 0x56, 0xB0, 0x0E, 0x79, 0x65, 0x3B, 0x11, 0xB5, 0x77, 0x8C, 0x06, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
static const struct raw_bitmap grab_data =
  {
    .hotspot_x = 0,
    .hotspot_y = 15,
    .size = sizeof (grab_bytes),
    .data = grab_bytes,
  };
// clang-format on
