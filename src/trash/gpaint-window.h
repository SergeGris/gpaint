
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GPAINT_TYPE_WINDOW (gpaint_window_get_type ())

G_DECLARE_FINAL_TYPE (GpaintWindow, gpaint_window, GPAINT, WINDOW, AdwApplicationWindow)

G_END_DECLS
