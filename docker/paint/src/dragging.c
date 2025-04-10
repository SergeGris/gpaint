
#define DRAGGABLE_TYPE_SQUARE (draggable_square_get_type ())
G_DECLARE_FINAL_TYPE (DraggableSquare, draggable_square, DRAGGABLE, SQUARE, GtkDrawingArea)

struct _DraggableSquare
{
  GtkDrawingArea parent_instance;
  double start_x, start_y;
  double current_x, current_y;
  gboolean is_dragging;
};

G_DEFINE_TYPE (DraggableSquare, draggable_square, GTK_TYPE_DRAWING_AREA)

static gboolean
on_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  DraggableSquare *square = DRAGGABLE_SQUARE (widget);

  // Set the color of the square
  cairo_set_source_rgb (cr, 0.2, 0.6, 1); // Example color
  cairo_rectangle (cr, square->current_x, square->current_y, 100, 100);
  cairo_fill (cr);

  return FALSE;
}

static gboolean
on_button_press (GtkGestureDrag *gesture, double start_x, double start_y, GtkWidget *widget)
{
  DraggableSquare *square = DRAGGABLE_SQUARE (widget);
  square->start_x = start_x;
  square->start_y = start_y;
  square->is_dragging = TRUE;
  return TRUE;
}

static gboolean
on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, GtkWidget *widget)
{
  DraggableSquare *square = DRAGGABLE_SQUARE (widget);

  if (square->is_dragging)
    {
      /* square->current_x += offset_x; */
      /* square->current_y += offset_y; */
      g_print ("Delta X: %f, Delta Y: %f\n", offset_x, offset_y);
      gtk_widget_queue_draw (widget);
    }

  return TRUE;
}

static gboolean
on_button_release (GtkGestureDrag *gesture, double offset_x, double offset_y, GtkWidget *widget)
{
  DraggableSquare *square = DRAGGABLE_SQUARE (widget);
  square->is_dragging = FALSE;
  return TRUE;
}

static void
draggable_square_init (DraggableSquare *square)
{
  square->current_x = 0; // Initial position
  square->current_y = 0;

  gtk_widget_set_size_request (GTK_WIDGET (square), 20, 20);

  GtkGesture *drag = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), GDK_BUTTON_PRIMARY);
  gtk_widget_add_controller (GTK_WIDGET (square), GTK_EVENT_CONTROLLER (drag));

  g_signal_connect (drag, "drag-begin", G_CALLBACK (on_button_press), square);
  g_signal_connect (drag, "drag-update", G_CALLBACK (on_drag_update), square);
  g_signal_connect (drag, "drag-end", G_CALLBACK (on_button_release), square);

  g_signal_connect (GTK_WIDGET (square), "draw", G_CALLBACK (on_draw), NULL);
}

static void
draggable_square_class_init (DraggableSquareClass *klass)
{
}

GtkWidget *
draggable_square_new (void)
{
  return g_object_new (DRAGGABLE_TYPE_SQUARE, NULL);
}
