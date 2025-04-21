#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GPAINT_COLOR_SWAP_BUTTON_TYPE (gpaint_color_swap_button_get_type ())
G_DECLARE_FINAL_TYPE (GpaintColorSwapButton, gpaint_color_swap_button, GPAINT, COLOR_SWAP_BUTTON, GtkButton)

typedef void (*ColorSwapCallback) (gpointer user_data);
typedef const GdkRGBA *(*ColorGetCallback) (gpointer user_data);

extern GtkWidget *gpaint_color_swap_button_new (ColorGetCallback get_primary, ColorGetCallback get_secondary, ColorSwapCallback swap_colors, gpointer user_data);
extern void
gpaint_color_swap_button_update_colors (GpaintColorSwapButton *self);

G_END_DECLS
