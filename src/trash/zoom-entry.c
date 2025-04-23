/*
 * eog-zoom-entry.c
 * This file is part of eog
 *
 * Author: Felix Riemann <friemann@gnome.org>
 *
 * Copyright (C) 2017 GNOME Foundation
 *
 * Based on code (ev-zoom-action.c) by:
 *      - Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* #include "gpaint-zoom-entry.h" */
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>

#define GPAINT_TYPE_ZOOM_ENTRY (gpaint_zoom_entry_get_type ())
G_DECLARE_FINAL_TYPE (GpaintZoomEntry, gpaint_zoom_entry, GPAINT, ZOOM_ENTRY, GtkBox);
GtkWidget *gpaint_zoom_entry_new (GtkWidget *view, GMenu *menu);

#define GPAINT_SCROLL_VIEW_MAX_ZOOM_FACTOR 10.0
#define GPAINT_SCROLL_VIEW_MIN_ZOOM_FACTOR 0.1

enum
{
  PROP_0,
  PROP_SCROLL_VIEW,
  PROP_MENU
};

typedef struct _GpaintZoomEntryPrivate
{
  GtkWidget *btn_zoom_in;
  GtkWidget *btn_zoom_out;
  GtkWidget *value_entry;

  GtkWidget *view;

  GMenu *menu;
  GMenuModel *zoom_free_section;
  GtkWidget *popup;
  gboolean popup_shown;
} GpaintZoomEntryPrivate;

struct _GpaintZoomEntry
{
  GtkBox box;

  GpaintZoomEntryPrivate *priv;
};

static const gdouble zoom_levels[] = { (1.0 / 3.0), (1.0 / 2.0), 1.0, /* 100% */
                                       (1.0 / 0.75), 2.0, 5.0, 10.0, 15.0, 20.0 };

G_DEFINE_TYPE_WITH_PRIVATE (GpaintZoomEntry, gpaint_zoom_entry, GTK_TYPE_BOX);

static void gpaint_zoom_entry_reset_zoom_level (GpaintZoomEntry *entry);
static void gpaint_zoom_entry_set_zoom_level (GpaintZoomEntry *entry, gdouble zoom);
static void gpaint_zoom_entry_update_sensitivity (GpaintZoomEntry *entry);

static gchar *
gpaint_zoom_entry_format_zoom_value (gdouble value)
{
  gchar *name;
  /* Mimic the zoom calculation from GpaintWindow to get matching displays */
  const gint zoom_percent = (gint) floor (value * 100. + 0.5);

  /* L10N: This is a percentage value used for the image zoom.
   * This should be translated similar to the statusbar zoom value. */
  name = g_strdup_printf (_ ("%d%%"), zoom_percent);

  return name;
}

static void
gpaint_zoom_entry_populate_free_zoom_section (GpaintZoomEntry *zoom_entry)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (zoom_levels); i++)
    {
      GMenuItem *item;
      gchar *name;

      if (zoom_levels[i] > GPAINT_SCROLL_VIEW_MAX_ZOOM_FACTOR)
        break;

      name = gpaint_zoom_entry_format_zoom_value (zoom_levels[i]);

      item = g_menu_item_new (name, NULL);
      g_menu_item_set_action_and_target (item, "win.zoom-set", "d", zoom_levels[i]);
      g_menu_append_item (G_MENU (zoom_entry->priv->zoom_free_section), item);
      g_object_unref (item);
      g_free (name);
    }
}

static void
gpaint_zoom_entry_activate_cb (GtkEntry *gtk_entry, GpaintZoomEntry *entry)
{
  const gchar *text = gtk_entry_get_text (gtk_entry);
  gchar *end_ptr = NULL;
  double zoom_perc;

  if (!text || text[0] == '\0')
    {
      gpaint_zoom_entry_reset_zoom_level (entry);
      return;
    }
  zoom_perc = g_strtod (text, &end_ptr);

  if (end_ptr)
    {
      /* Skip whitespace after the digits */
      while (end_ptr[0] != '\0' && g_ascii_isspace (end_ptr[0]))
        end_ptr++;
      if (end_ptr[0] != '\0' && end_ptr[0] != '%')
        {
          gpaint_zoom_entry_reset_zoom_level (entry);
          return;
        }
    }

  gpaint_scroll_view_set_zoom (entry->priv->view, zoom_perc / 100.0);
}

static gboolean
focus_out_cb (GpaintZoomEntry *zoom_entry)
{
  gpaint_zoom_entry_reset_zoom_level (zoom_entry);

  return FALSE;
}

static void
popup_menu_closed (GtkWidget *popup, GpaintZoomEntry *zoom_entry)
{
  if (zoom_entry->priv->popup != popup)
    return;

  zoom_entry->priv->popup_shown = FALSE;
  zoom_entry->priv->popup = NULL;
}

static GtkWidget *
get_popup (GpaintZoomEntry *zoom_entry)
{
  GdkRectangle rect;

  if (zoom_entry->priv->popup)
    return zoom_entry->priv->popup;

  zoom_entry->priv->popup = gtk_popover_new_from_model (GTK_WIDGET (zoom_entry), G_MENU_MODEL (zoom_entry->priv->menu));
  g_signal_connect (zoom_entry->priv->popup, "closed", G_CALLBACK (popup_menu_closed), zoom_entry);
  gtk_entry_get_icon_area (GTK_ENTRY (zoom_entry->priv->value_entry), GTK_ENTRY_ICON_SECONDARY, &rect);
  gtk_popover_set_relative_to (GTK_POPOVER (zoom_entry->priv->popup), zoom_entry->priv->value_entry);
  gtk_popover_set_pointing_to (GTK_POPOVER (zoom_entry->priv->popup), &rect);
  gtk_popover_set_position (GTK_POPOVER (zoom_entry->priv->popup), GTK_POS_BOTTOM);
  gtk_widget_set_size_request (zoom_entry->priv->popup, 150, -1);

  return zoom_entry->priv->popup;
}

static void
gpaint_zoom_entry_icon_press_cb (GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, gpointer data)
{
  GpaintZoomEntry *zoom_entry;
  guint button;

  g_return_if_fail (GPAINT_IS_ZOOM_ENTRY (data));
  g_return_if_fail (icon_pos == GTK_ENTRY_ICON_SECONDARY);

  if (!gdk_event_get_button (event, &button) || button != GDK_BUTTON_PRIMARY)
    return;

  zoom_entry = GPAINT_ZOOM_ENTRY (data);

  gtk_widget_show (get_popup (zoom_entry));
  zoom_entry->priv->popup_shown = TRUE;
}

static void
gpaint_zoom_entry_view_zoom_changed_cb (GpaintScrollView *view, gdouble zoom, gpointer data)
{
  GpaintZoomEntry *zoom_entry = GPAINT_ZOOM_ENTRY (data);

  gpaint_zoom_entry_set_zoom_level (zoom_entry, zoom);
}

static void
button_sensitivity_changed_cb (GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
  g_return_if_fail (GPAINT_IS_ZOOM_ENTRY (user_data));

  gpaint_zoom_entry_update_sensitivity (GPAINT_ZOOM_ENTRY (user_data));
}

static void
gpaint_zoom_entry_constructed (GObject *object)
{
  GpaintZoomEntry *zoom_entry = GPAINT_ZOOM_ENTRY (object);

  G_OBJECT_CLASS (gpaint_zoom_entry_parent_class)->constructed (object);

  g_signal_connect (zoom_entry->priv->view, "zoom-changed", G_CALLBACK (gpaint_zoom_entry_view_zoom_changed_cb), zoom_entry);
  gpaint_zoom_entry_reset_zoom_level (zoom_entry);

  zoom_entry->priv->zoom_free_section = g_menu_model_get_item_link (G_MENU_MODEL (zoom_entry->priv->menu), 1, G_MENU_LINK_SECTION);
  gpaint_zoom_entry_populate_free_zoom_section (zoom_entry);

  g_signal_connect (zoom_entry->priv->btn_zoom_in, "notify::sensitive", G_CALLBACK (button_sensitivity_changed_cb), zoom_entry);
  g_signal_connect (zoom_entry->priv->btn_zoom_out, "notify::sensitive", G_CALLBACK (button_sensitivity_changed_cb), zoom_entry);
  gpaint_zoom_entry_update_sensitivity (zoom_entry);
}

static void
gpaint_zoom_entry_finalize (GObject *object)
{
  GpaintZoomEntry *zoom_entry = GPAINT_ZOOM_ENTRY (object);

  g_clear_object (&zoom_entry->priv->menu);
  g_clear_object (&zoom_entry->priv->zoom_free_section);
  g_clear_object (&zoom_entry->priv->view);

  G_OBJECT_CLASS (gpaint_zoom_entry_parent_class)->finalize (object);
}

static void
gpaint_zoom_entry_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GpaintZoomEntry *zoom_entry = GPAINT_ZOOM_ENTRY (object);

  switch (prop_id)
    {
    case PROP_SCROLL_VIEW:
      zoom_entry->priv->view = g_value_dup_object (value);
      break;
    case PROP_MENU:
      zoom_entry->priv->menu = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gpaint_zoom_entry_set_zoom_level (GpaintZoomEntry *entry, gdouble zoom)
{
  gchar *zoom_str;

  zoom = CLAMP (zoom, GPAINT_SCROLL_VIEW_MIN_ZOOM_FACTOR, GPAINT_SCROLL_VIEW_MAX_ZOOM_FACTOR);
  zoom_str = gpaint_zoom_entry_format_zoom_value (zoom);
  gtk_entry_set_text (GTK_ENTRY (entry->priv->value_entry), zoom_str);
  g_free (zoom_str);
}

static void
gpaint_zoom_entry_reset_zoom_level (GpaintZoomEntry *entry)
{
  const gdouble zoom = gpaint_scroll_view_get_zoom (entry->priv->view);
  gpaint_zoom_entry_set_zoom_level (entry, zoom);
}

static void
gpaint_zoom_entry_update_sensitivity (GpaintZoomEntry *entry)
{
  const gboolean current_state = gtk_widget_is_sensitive (entry->priv->value_entry);
  const gboolean new_state = gtk_widget_is_sensitive (entry->priv->btn_zoom_in) | gtk_widget_is_sensitive (entry->priv->btn_zoom_out);

  if (current_state != new_state)
    {
      gtk_widget_set_sensitive (entry->priv->value_entry, new_state);
    }
}

static void
gpaint_zoom_entry_class_init (GpaintZoomEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wklass = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gpaint_zoom_entry_constructed;
  object_class->set_property = gpaint_zoom_entry_set_property;
  object_class->finalize = gpaint_zoom_entry_finalize;

  gtk_widget_class_set_template_from_resource (wklass, "/org/gnome/gpaint/ui/gpaint-zoom-entry.ui");
  gtk_widget_class_bind_template_child_private (wklass, GpaintZoomEntry, btn_zoom_in);
  gtk_widget_class_bind_template_child_private (wklass, GpaintZoomEntry, btn_zoom_out);
  gtk_widget_class_bind_template_child_private (wklass, GpaintZoomEntry, value_entry);

  gtk_widget_class_bind_template_callback (wklass, gpaint_zoom_entry_activate_cb);
  gtk_widget_class_bind_template_callback (wklass, gpaint_zoom_entry_icon_press_cb);

  g_object_class_install_property (object_class, PROP_SCROLL_VIEW,
                                   g_param_spec_object ("scroll-view", "GpaintScrollView", "The GpaintScrollView to work with", GPAINT_TYPE_SCROLL_VIEW, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MENU, g_param_spec_object ("menu", "Menu", "The zoom popup menu", G_TYPE_MENU, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gpaint_zoom_entry_init (GpaintZoomEntry *entry)
{
  entry->priv = gpaint_zoom_entry_get_instance_private (entry);
  gtk_widget_init_template (GTK_WIDGET (entry));

  g_signal_connect_swapped (entry->priv->value_entry, "focus-out-event", G_CALLBACK (focus_out_cb), entry);
}

GtkWidget *
gpaint_zoom_entry_new (GpaintScrollView *view, GMenu *menu)
{
  g_return_val_if_fail (GPAINT_IS_SCROLL_VIEW (view), NULL);
  g_return_val_if_fail (G_IS_MENU (menu), NULL);

  return g_object_new (GPAINT_TYPE_ZOOM_ENTRY, "scroll-view", view, "menu", menu, NULL);
}
