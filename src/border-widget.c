#include "border-widget.h"

struct _GpaintBorderWidget
{
  GtkWidget parent_instance;
  GtkWidget *child;
  guint border_width;
  GdkRGBA border_color;
};

struct _GpaintBorderWidgetClass
{
  GtkButtonClass parent_class;
};

G_DEFINE_TYPE (GpaintBorderWidget, gpaint_border_widget, GTK_TYPE_WIDGET);

static void
gpaint_border_widget_init (GpaintBorderWidget *self)
{
  self->child = NULL;
  self->border_width = 1;
  self->border_color = (GdkRGBA) { 0.0, 0.0, 0.0, 1.0 };
}

static void
gpaint_border_widget_dispose (GObject *object)
{
  GpaintBorderWidget *self = GPAINT_BORDER_WIDGET (object);
  g_clear_pointer (&self->child, gtk_widget_unparent);
  G_OBJECT_CLASS (gpaint_border_widget_parent_class)->dispose (object);
}

static void
gpaint_border_widget_measure (GtkWidget *widget,
			      GtkOrientation orientation,
			      gint for_size,
			      gint *minimum,
			      gint *natural,
			      gint *minimum_baseline, gint *natural_baseline)
{
  GpaintBorderWidget *self = GPAINT_BORDER_WIDGET (widget);
  gint child_min = 0, child_nat = 0;

  if (self->child != NULL && gtk_widget_get_visible (self->child))
    gtk_widget_measure (self->child, orientation, for_size, &child_min, &child_nat, NULL, NULL);

  *minimum = child_min + 2 * self->border_width;
  *natural = child_nat + 2 * self->border_width;
}

static void
gpaint_border_widget_size_allocate (GtkWidget *widget,
				    int width, int height, int baseline)
{
  GpaintBorderWidget *self = GPAINT_BORDER_WIDGET (widget);

  if (self->child && gtk_widget_get_visible (self->child))
    {
      GtkAllocation child_alloc =
        {
          .x = 0,
          .y = 0,
          .width = MAX (width - 2 * self->border_width, 0),
          .height = MAX (height - 2 * self->border_width, 0)
        };

      gtk_widget_size_allocate (self->child, &child_alloc, -1);
    }
}

static void
gpaint_border_widget_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  GpaintBorderWidget *self = GPAINT_BORDER_WIDGET (widget);
  const float width = gtk_widget_get_width (widget);
  const float height = gtk_widget_get_height (widget);

  graphene_rect_t rect;
  graphene_rect_init (&rect, 0, 0, width, height);
  gtk_snapshot_append_color (snapshot, &self->border_color, &rect);

  const gfloat content_width = MAX (width - 2 * self->border_width, 0);
  const gfloat content_height = MAX (height - 2 * self->border_width, 0);
  graphene_rect_init (&rect, self->border_width, self->border_width, content_width, content_height);
  gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 1.0, 1.0, 1.0, 1.0 }, &rect);

  if (self->child && gtk_widget_get_visible (self->child))
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (self->border_width, self->border_width));
      gtk_widget_snapshot_child (GTK_WIDGET (self), self->child, snapshot);
      gtk_snapshot_restore (snapshot);
    }
}

static void
gpaint_border_widget_class_init (GpaintBorderWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gpaint_border_widget_dispose;
  widget_class->measure = gpaint_border_widget_measure;
  widget_class->size_allocate = gpaint_border_widget_size_allocate;
  widget_class->snapshot = gpaint_border_widget_snapshot;
}

GtkWidget *
gpaint_border_widget_new (void)
{
  return g_object_new (GPAINT_TYPE_BORDER_WIDGET, NULL);
}

void
gpaint_border_widget_set_child (GpaintBorderWidget *self, GtkWidget *child)
{
  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child != NULL)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }
}

void
gpaint_border_widget_set_border_width (GpaintBorderWidget *self, guint width)
{
  if (self->border_width != width)
    {
      self->border_width = width;
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

void
gpaint_border_widget_set_border_color (GpaintBorderWidget *self,
				       const GdkRGBA *color)
{
  self->border_color = *color;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}
