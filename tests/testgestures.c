#include <gtk/gtk.h>

#define GTK_COMPILATION
#include <gtk/gtkgesturesinterpreter.h>
#undef GTK_COMPILATION

static GHashTable *strokes;
static guint shown_gesture = 0;
static guint timeout_id = 0;
static guint m_shaped_gesture_id = 0;

static void
free_stroke (GArray *array)
{
  g_array_free (array, TRUE);
}

static gboolean
hide_gesture_cb (gpointer user_data)
{
  timeout_id = 0;
  shown_gesture = 0;

  g_hash_table_remove_all (strokes);
  gtk_widget_queue_draw (GTK_WIDGET (user_data));

  return FALSE;
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   user_data)
{
  GdkPoint *point;
  GHashTableIter iter;
  gpointer value;
  gint i;

  if (shown_gesture != 0)
    {
      gint width, height, size;

      cairo_save (cr);
      width = gdk_window_get_width (gtk_widget_get_window (widget));
      height = gdk_window_get_height (gtk_widget_get_window (widget));

      size = MIN (width, height);
      size -= size / 4;

      cairo_translate (cr, width / 2, height / 2);

      cairo_set_line_width (cr, 10);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
      cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);

      switch (shown_gesture)
        {
        case GTK_GESTURE_SWIPE_LEFT:
        case GTK_GESTURE_SWIPE_RIGHT:
        case GTK_GESTURE_SWIPE_UP:
        case GTK_GESTURE_SWIPE_DOWN:
          if (shown_gesture == GTK_GESTURE_SWIPE_RIGHT)
            cairo_rotate (cr, (G_PI * 3) / 2);
          else if (shown_gesture == GTK_GESTURE_SWIPE_LEFT)
            cairo_rotate (cr, G_PI / 2);
          else if (shown_gesture == GTK_GESTURE_SWIPE_UP)
            cairo_rotate (cr, G_PI);

          cairo_move_to (cr, 0, - size / 2);
          cairo_line_to (cr, 0, size / 2);

          cairo_move_to (cr, 0, size / 2);
          cairo_rel_line_to (cr, -size / 4, -size / 4);

          cairo_move_to (cr, 0, size / 2);
          cairo_rel_line_to (cr, size / 4, -size / 4);

          cairo_stroke (cr);
          break;
        case GTK_GESTURE_CIRCULAR_COUNTERCLOCKWISE:
          cairo_move_to (cr, 0, - size / 2);
          cairo_rel_line_to (cr, size / 8, - size / 8);

          cairo_move_to (cr, 0, - size / 2);
          cairo_rel_line_to (cr, size / 8, size / 8);

          cairo_new_sub_path (cr);
          cairo_arc (cr, 0, 0, size / 2, (3 * G_PI) / 2, (5 * G_PI) / 4);

          cairo_stroke (cr);
          break;
        case GTK_GESTURE_CIRCULAR_CLOCKWISE:
          cairo_move_to (cr, 0, - size / 2);
          cairo_rel_line_to (cr, - size / 8, - size / 8);

          cairo_move_to (cr, 0, - size / 2);
          cairo_rel_line_to (cr, - size / 8, size / 8);

          cairo_new_sub_path (cr);
          cairo_arc_negative (cr, 0, 0, size / 2, (3 * G_PI) / 2, (7* G_PI) / 4);

          cairo_stroke (cr);
          break;
        default:
          if (shown_gesture == m_shaped_gesture_id)
            {
              PangoAttrList *attr_list;
              PangoLayout *layout;

              layout = gtk_widget_create_pango_layout (widget, "M");

              attr_list = pango_layout_get_attributes (layout);

              if (!attr_list)
                attr_list = pango_attr_list_new ();

              pango_attr_list_insert (attr_list,
                                      pango_attr_size_new_absolute (200 * PANGO_SCALE));
              pango_layout_set_attributes (layout, attr_list);

              cairo_move_to (cr, -100, -100);
              pango_cairo_show_layout (cr, layout);
            }
          break;
        }

      cairo_restore (cr);
    }

  g_hash_table_iter_init (&iter, strokes);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GArray *coords = value;

      if (coords->len > 0)
	{
	  cairo_save (cr);
	  cairo_set_line_width (cr, 2);

	  point = &g_array_index (coords, GdkPoint, 0);
	  cairo_move_to (cr, point->x, point->y);

	  for (i = 0; i < coords->len; i++)
	    {
	      point = &g_array_index (coords, GdkPoint, i);
	      cairo_line_to (cr, point->x, point->y);
	    }

	  cairo_stroke (cr);
	  cairo_restore (cr);
	}
    }

  return FALSE;
}

static gboolean
append_event_coordinates (GdkEvent *event)
{
  GdkPoint point;
  gdouble x, y;
  guint touch_id;
  GArray *coords;

  if (!gdk_event_get_coords (event, &x, &y))
    return FALSE;

  if (!gdk_event_get_touch_id (event, &touch_id))
    touch_id = 0;

  coords = g_hash_table_lookup (strokes,
				GUINT_TO_POINTER (touch_id));
  point.x = x;
  point.y = y;
  g_array_append_val (coords, point);

  return TRUE;
}

static gboolean
motion_notify_cb (GtkWidget *widget,
                  GdkEvent  *event,
                  gpointer   user_data)
{
  GtkGesturesInterpreter *interpreter;
  GdkModifierType state;

  interpreter = user_data;

  if (!gdk_event_get_state (event, &state))
    return FALSE;

  if ((state & GDK_BUTTON1_MASK) == 0)
    return FALSE;

  append_event_coordinates (event);
  gtk_gestures_interpreter_feed_event (interpreter, event);
  gtk_widget_queue_draw (widget);

  return FALSE;
}

static gboolean
button_release_cb (GtkWidget *widget,
                   GdkEvent  *event,
                   gpointer   user_data)
{
  GtkGesturesInterpreter *interpreter;
  guint gesture_id;

  interpreter = user_data;

  if (timeout_id != 0)
    g_source_remove (timeout_id);

  timeout_id = g_timeout_add (500, hide_gesture_cb, widget);

  append_event_coordinates (event);
  gtk_gestures_interpreter_feed_event (interpreter, event);

  if (gtk_gestures_interpreter_finish (interpreter, &gesture_id))
    shown_gesture = gesture_id;

  gtk_widget_queue_draw (widget);

  return FALSE;
}

static gboolean
button_press_cb (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  GtkGesturesInterpreter *interpreter;
  GArray *coords;
  guint touch_id;

  interpreter = user_data;

  if (timeout_id != 0)
    {
      g_hash_table_remove_all (strokes);
      g_source_remove (timeout_id);
      timeout_id = 0;
    }

  shown_gesture = 0;

  if (!gdk_event_get_touch_id (event, &touch_id))
    touch_id = 0;

  coords = g_array_new (FALSE, FALSE, sizeof (GdkPoint));
  g_hash_table_insert (strokes,
		       GUINT_TO_POINTER (touch_id),
		       coords);

  append_event_coordinates (event);
  gtk_gestures_interpreter_feed_event (interpreter, event);
  gtk_widget_queue_draw (widget);

  return FALSE;
}

static GtkWidget *
create_window (GtkGesturesInterpreter *interpreter)
{
  GtkWidget *window;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_app_paintable (window, TRUE);
  gtk_widget_set_size_request (window, 400, 400);
  gtk_window_set_title (GTK_WINDOW (window), "Gestures demo");

  gtk_widget_add_events (window,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_MASK |
			 GDK_TOUCH_MASK);

  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (window, "motion-notify-event",
                    G_CALLBACK (motion_notify_cb), interpreter);
  g_signal_connect (window, "button-release-event",
                    G_CALLBACK (button_release_cb), interpreter);
  g_signal_connect (window, "button-press-event",
                    G_CALLBACK (button_press_cb), interpreter);
  g_signal_connect (window, "draw",
                    G_CALLBACK (draw_cb), NULL);

  gtk_widget_show (window);

  return window;
}

static GtkGesture *
create_m_shaped_gesture (void)
{
  GtkGestureStroke *stroke;

  stroke = gtk_gesture_stroke_new ();

  gtk_gesture_stroke_append_vector (stroke, 0, 100);
  gtk_gesture_stroke_append_vector (stroke, 3 * G_PI_4, 50);
  gtk_gesture_stroke_append_vector (stroke, G_PI_4, 50);
  gtk_gesture_stroke_append_vector (stroke, G_PI, 100);

  return gtk_gesture_new (stroke, 0);
}

static GtkGesturesInterpreter *
create_interpreter (void)
{
  GtkGesturesInterpreter *interpreter;
  GtkGesture *gesture;

  interpreter = gtk_gestures_interpreter_new ();
  gtk_gestures_interpreter_add_gesture (interpreter, GTK_GESTURE_SWIPE_RIGHT);
  gtk_gestures_interpreter_add_gesture (interpreter, GTK_GESTURE_SWIPE_LEFT);
  gtk_gestures_interpreter_add_gesture (interpreter, GTK_GESTURE_SWIPE_UP);
  gtk_gestures_interpreter_add_gesture (interpreter, GTK_GESTURE_SWIPE_DOWN);
  gtk_gestures_interpreter_add_gesture (interpreter, GTK_GESTURE_CIRCULAR_CLOCKWISE);
  gtk_gestures_interpreter_add_gesture (interpreter, GTK_GESTURE_CIRCULAR_COUNTERCLOCKWISE);

  gesture = create_m_shaped_gesture ();
  m_shaped_gesture_id = gtk_gesture_register_static (gesture);
  gtk_gestures_interpreter_add_gesture (interpreter, m_shaped_gesture_id);

  return interpreter;
}

int
main (int argc, char *argv[])
{
  GtkGesturesInterpreter *interpreter;
  GtkWidget *window;

  gtk_init (&argc, &argv);

  strokes = g_hash_table_new_full (NULL, NULL, NULL,
                                   (GDestroyNotify) free_stroke);

  interpreter = create_interpreter ();
  window = create_window (interpreter);

  gtk_main ();

  gtk_widget_destroy (window);
  g_object_unref (interpreter);

  return 0;
}
