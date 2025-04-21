#include "color-swap-button.h"

#include "gpaint-cairo.h"

#define COLOR_SWAP_BUTTON_SIZE 48

struct _GpaintColorSwapButton
{
  GtkButton parent_instance;
  GtkWidget *primary_square;
  GtkWidget *secondary_square;
  const GdkRGBA *(*get_primary_color) (gpointer user_data);
  const GdkRGBA *(*get_secondary_color) (gpointer user_data);
  void (*swap_buttons) (gpointer user_data);
  gpointer user_data;
};

struct _GpaintColorSwapButtonClass
{
  GtkButtonClass parent_class;
};

G_DEFINE_TYPE (GpaintColorSwapButton, gpaint_color_swap_button, GTK_TYPE_BUTTON);

static void draw_primary_color (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer data);
static void draw_secondary_color (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer data);

static void
gpaint_color_swap_button_clicked (GtkWidget *widget, gpointer user_data)
{
  GpaintColorSwapButton *self = GPAINT_COLOR_SWAP_BUTTON (widget);

  if (self->swap_buttons != NULL)
    self->swap_buttons (self->user_data);

  gtk_widget_queue_draw (self->primary_square);
  gtk_widget_queue_draw (self->secondary_square);
}

static void
gpaint_color_swap_button_init (GpaintColorSwapButton *self)
{
  GtkWidget *fixed = gtk_fixed_new ();

  struct
  {
    GtkWidget **square;
    void (*draw_color) (GtkDrawingArea *, cairo_t *, gint, gint, gpointer);
  } primary = { .square = &self->primary_square, .draw_color = draw_primary_color },
    secondary = { .square = &self->secondary_square, .draw_color = draw_secondary_color },
    *squares[] = { &primary, &secondary };

  for (size_t i = 0; i < G_N_ELEMENTS (squares); i++)
    {
      GtkWidget *square = gtk_drawing_area_new ();
      gtk_widget_set_size_request (square, COLOR_SWAP_BUTTON_SIZE / 2, COLOR_SWAP_BUTTON_SIZE / 2);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (square), squares[i]->draw_color, self, NULL);
      *squares[i]->square = square;
    }

  gtk_fixed_put (GTK_FIXED (fixed), self->secondary_square, COLOR_SWAP_BUTTON_SIZE / 4, COLOR_SWAP_BUTTON_SIZE / 4);
  gtk_fixed_put (GTK_FIXED (fixed), self->primary_square, 0, 0);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (GTK_WIDGET (self), COLOR_SWAP_BUTTON_SIZE, COLOR_SWAP_BUTTON_SIZE);
  gtk_button_set_child (GTK_BUTTON (self), fixed);

  g_signal_connect (self, "clicked", G_CALLBACK (gpaint_color_swap_button_clicked), NULL);
}

static void
gpaint_color_swap_button_class_init (GpaintColorSwapButtonClass *klass)
{
  return;
}

GtkWidget *
gpaint_color_swap_button_new (
    const GdkRGBA *(*get_primary_color) (gpointer user_data),
    const GdkRGBA *(*get_secondary_color) (gpointer user_data),
    void (*swap_buttons) (gpointer user_data),
    gpointer user_data)
{
  GpaintColorSwapButton *button = (GpaintColorSwapButton *) g_object_new (
      GPAINT_COLOR_SWAP_BUTTON_TYPE, NULL);
  button->get_primary_color = get_primary_color;
  button->get_secondary_color = get_secondary_color;
  button->swap_buttons = swap_buttons;
  button->user_data = user_data;
  gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
  return GTK_WIDGET (button);
}

void
gpaint_color_swap_button_update_colors (GpaintColorSwapButton *self)
{
  gtk_widget_queue_draw (self->primary_square);
  gtk_widget_queue_draw (self->secondary_square);
}

static void
draw_color (cairo_t *cr, gint width, gint height, const GdkRGBA *color)
{
  draw_colored_square (cr, color, 0, 0, width, height, 12.0);
  const GdkRGBA border = { 0.2, 0.2, 0.2, 1.0 };
  const gdouble border_width = 1.0;
  cairo_save (cr);
  gdk_cairo_set_source_rgba (cr, &border);
  cairo_set_line_width (cr, border_width);
  cairo_rectangle (cr, border_width / 2.0, border_width / 2.0, width - border_width, height - border_width);
  cairo_stroke (cr);
  cairo_restore (cr);
}

static void
draw_primary_color (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer user_data)
{
  GpaintColorSwapButton *self = GPAINT_COLOR_SWAP_BUTTON (user_data);
  draw_color (cr, width, height, self->get_primary_color (self->user_data));
}

static void
draw_secondary_color (GtkDrawingArea *area, cairo_t *cr, gint width, gint height, gpointer user_data)
{
  GpaintColorSwapButton *self = GPAINT_COLOR_SWAP_BUTTON (user_data);
  draw_color (cr, width, height, self->get_secondary_color (self->user_data));
}
