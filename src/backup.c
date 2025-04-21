
#include "backup.h"

// Creates an empty surface of the same size and format as src.
static cairo_surface_t *
duplicate_surface (cairo_surface_t *src)
{
  const int width = cairo_image_surface_get_width (src);
  const int height = cairo_image_surface_get_height (src);
  const cairo_format_t format = cairo_image_surface_get_format (src);

  cairo_surface_t *copy = cairo_image_surface_create (format, width, height);
  if (!copy)
    {
      g_printerr ("Error creating surface copy\n");
      return NULL;
    }

  cairo_t *cr = cairo_create (copy);
  cairo_set_source_surface (cr, src, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  return copy;
}

void
init_backup_manager (BackupManager *manager)
{
  manager->undo = g_queue_new ();
  manager->redo = g_queue_new ();
}

static void
clear_queue (GQueue *queue)
{
  while (!g_queue_is_empty (queue))
    {
      cairo_surface_t *surface = (cairo_surface_t *) g_queue_pop_head (queue);
      cairo_surface_destroy (surface);
    }
}

// Save a new backup from the main surface. Clears redo history.
void
save_backup (BackupManager *manager, cairo_surface_t *main_surface)
{
  // Clear the redo queue if the user makes a new change.
  clear_queue (manager->redo);

  // Duplicate and push the current state onto the undo queue.
  cairo_surface_t *backup = duplicate_surface (main_surface);

  g_queue_push_head (manager->undo, backup);
  g_simple_action_set_enabled (manager->undo_action, !g_queue_is_empty (manager->undo));
  g_simple_action_set_enabled (manager->redo_action, !g_queue_is_empty (manager->redo));
}

// Applies the content of backup surface to the destination surface.
static void
apply_backup (AppState *state, cairo_surface_t *backup)
{
  if (cairo_image_surface_get_width (backup) != cairo_image_surface_get_width (state->main_surface) || cairo_image_surface_get_height (backup) != cairo_image_surface_get_height (state->main_surface))
    {
      state->main_surface = cairo_image_surface_create (state->format, cairo_image_surface_get_width (backup), cairo_image_surface_get_height (backup));
      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_width (state->main_surface) * state->zoom_level));
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (state->drawing_area), (int) (cairo_image_surface_get_height (state->main_surface) * state->zoom_level));
      gtk_widget_queue_draw (state->drawing_area);
    }

  // Clear the destination surface.
  clear_canvas (state->main_surface);
  gtk_widget_queue_draw (state->drawing_area); // TODO
  // Now draw the backup onto the destination.
  cairo_t *cr = cairo_create (state->main_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface (cr, backup, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static gboolean
move (BackupManager *manager, AppState *state, GQueue *from, GQueue *to)
{
  if (g_queue_is_empty (from))
    return FALSE;

  // Save the current state to the queue.
  cairo_surface_t *current = duplicate_surface (state->main_surface);

  if (!current)
    return FALSE;

  g_queue_push_head (to, current);

  // Pop the most recent backup from the queue.
  cairo_surface_t *backup = (cairo_surface_t *) g_queue_pop_head (from);

  if (!backup)
    return FALSE;

  // Apply the backup to the main surface.
  apply_backup (state, backup);
  cairo_surface_destroy (backup);
  g_simple_action_set_enabled (manager->undo_action, !g_queue_is_empty (manager->undo));
  g_simple_action_set_enabled (manager->redo_action, !g_queue_is_empty (manager->redo));
  return TRUE;
}

// Undo: move one backup back. Copies the most recent backup from the undo
// queue to the main surface, while saving the current state to the redo queue.
// Returns 1 on success, 0 if no backup available.
gboolean
move_backward (BackupManager *manager, AppState *state)
{
  return move (manager, state, manager->undo, manager->redo);
}

// Redo: move one backup forward. Copies the most recent backup from the redo
// queue to the main surface, while saving the current state to the undo queue.
// Returns TRUE on success, FALSE if no backup available.
gboolean
move_forward (BackupManager *manager, AppState *state)
{
  return move (manager, state, manager->redo, manager->undo);
}

// Free all backups from both undo and redo queues.
void
free_backup_manager (BackupManager *manager)
{
  clear_queue (manager->undo);
  clear_queue (manager->redo);
  g_queue_free (manager->undo);
  g_queue_free (manager->redo);
}
