
#pragma once

#include <cairo.h>
#include <gio/gio.h>
#include <glib.h>

#include "gpaint.h"

// BackupManager holds two GQueue objects for undo and redo.
typedef struct _BackupManager BackupManager;

extern void init_backup_manager (BackupManager *manager);
extern void free_backup_manager (BackupManager *manager);

extern void save_backup (BackupManager *manager, cairo_surface_t *main_surface);

extern gboolean move_backward (BackupManager *manager, AppState *state);
extern gboolean move_forward (BackupManager *manager, AppState *state);
