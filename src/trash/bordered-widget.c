#include "bordered-widget.h"

struct _BorderedWidget
{
  GtkWidget parent_instance;
  GtkWidget *child;
  int border_width;
  GdkRGBA border_color;
};

G_DEFINE_TYPE (BorderedWidget, bordered_widget, GTK_TYPE_WIDGET)

static void
bordered_widget_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  BorderedWidget *self = BORDERED_WIDGET (widget);

  // Get the widget's allocation
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);

  // Draw the border
  graphene_rect_t border_rect = GRAPHENE_RECT_INIT (0, 0, width, height);
  GskRoundedRect border = {
    .bounds = border_rect,
  };

  float border_widths[4] = { self->border_width, self->border_width, self->border_width, self->border_width };
  GdkRGBA colors[4] = { self->border_color, self->border_color, self->border_color, self->border_color };
  gtk_snapshot_append_border (snapshot, &border, border_widths, &self->border_color);

  // Render the child widget
  if (self->child)
    {
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (self->border_width, self->border_width));
      gtk_widget_snapshot_child (widget, self->child, snapshot);
    }
}

static void
bordered_widget_measure (GtkWidget *widget, GtkOrientation orientation, int for_size, int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
  BorderedWidget *self = BORDERED_WIDGET (widget);
  int child_min = 0, child_nat = 0;

  if (self->child)
    {
      gtk_widget_measure (self->child, orientation, for_size, &child_min, &child_nat, NULL, NULL);
    }

  int border_space = 2 * self->border_width;
  *minimum = child_min + border_space;
  *natural = child_nat + border_space;
}

static void
bordered_widget_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
  BorderedWidget *self = BORDERED_WIDGET (widget);

  if (self->child)
    {
      GtkAllocation child_allocation = { .x = self->border_width, .y = self->border_width, .width = width - 2 * self->border_width, .height = height - 2 * self->border_width };
      gtk_widget_size_allocate (self->child, &child_allocation, baseline);
    }
}

static void
bordered_widget_init (BorderedWidget *self)
{
  self->border_width = 1;                        // Default border width
  gdk_rgba_parse (&self->border_color, "black"); // Default border color
}

static void
bordered_widget_class_init (BorderedWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->snapshot = bordered_widget_snapshot;
  widget_class->measure = bordered_widget_measure;
  widget_class->size_allocate = bordered_widget_size_allocate;
}

GtkWidget *
bordered_widget_new (void)
{
  return g_object_new (TYPE_BORDERED_WIDGET, NULL);
}

void
bordered_widget_set_child (BorderedWidget *self, GtkWidget *child)
{
  g_return_if_fail (BORDERED_IS_WIDGET (self));

  if (self->child)
    {
      gtk_widget_unparent (self->child);
    }

  self->child = child;

  if (child)
    {
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

GtkWidget *
bordered_widget_get_child (BorderedWidget *self)
{
  g_return_val_if_fail (BORDERED_IS_WIDGET (self), NULL);
  return self->child;
}
