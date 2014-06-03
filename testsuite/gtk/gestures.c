#include <gtk/gtk.h>

typedef struct {
  GtkWidget *widget;
  gint x;
  gint y;
  guint state;
  guint pressed : 1;
} PointState;

static PointState mouse_state;
static PointState touch_state[10]; /* touchpoint 0 gets pointer emulation,
                                    * use it first in tests for consistency.
                                    */

#define EVENT_SEQUENCE(point) (GdkEventSequence*) ((point) - touch_state + 1)

static void
point_press (PointState *point,
             GtkWidget  *widget,
             guint       button)
{
  GdkDisplay *display;
  GdkDeviceManager *dm;
  GdkDevice *device;
  GdkEvent *ev;

  display = gtk_widget_get_display (widget);
  dm = gdk_display_get_device_manager (display);
  device = gdk_device_manager_get_client_pointer (dm);

  if (point == &mouse_state)
    {
      ev = gdk_event_new (GDK_BUTTON_PRESS);
      ev->any.window = g_object_ref (gtk_widget_get_window (widget));
      ev->button.time = GDK_CURRENT_TIME;
      ev->button.x = point->x;
      ev->button.y = point->y;
      ev->button.button = button;
      ev->button.state = point->state;

      point->state |= GDK_BUTTON1_MASK << (button - 1);
    }
  else
    {
      ev = gdk_event_new (GDK_TOUCH_BEGIN);
      ev->any.window = g_object_ref (gtk_widget_get_window (widget));
      ev->touch.time = GDK_CURRENT_TIME;
      ev->touch.x = point->x;
      ev->touch.y = point->y;
      ev->touch.sequence = EVENT_SEQUENCE (point);

      if (point == &touch_state[0])
        ev->touch.emulating_pointer = TRUE;
    }

  gdk_event_set_device (ev, device);

  gtk_main_do_event (ev);

  gdk_event_free (ev);

  point->widget = widget;
}

static void
point_update (PointState *point,
              GtkWidget  *widget,
              gdouble     x,
              gdouble     y)
{
  GdkDisplay *display;
  GdkDeviceManager *dm;
  GdkDevice *device;
  GdkEvent *ev;

  display = gtk_widget_get_display (widget);
  dm = gdk_display_get_device_manager (display);
  device = gdk_device_manager_get_client_pointer (dm);

  if (point == &mouse_state)
    {
      ev = gdk_event_new (GDK_MOTION_NOTIFY);
      ev->any.window = g_object_ref (gtk_widget_get_window (widget));
      ev->button.time = GDK_CURRENT_TIME;
      ev->motion.x = x;
      ev->motion.y = y;
      ev->motion.state = point->state;
    }
  else
    {
      if (!point->widget || widget != point->widget)
        return;

      ev = gdk_event_new (GDK_TOUCH_UPDATE);
      ev->any.window = g_object_ref (gtk_widget_get_window (widget));
      ev->touch.time = GDK_CURRENT_TIME;
      ev->touch.x = x;
      ev->touch.y = y;
      ev->touch.sequence = EVENT_SEQUENCE (point);
      ev->touch.state = 0;

      if (point == &touch_state[0])
        ev->touch.emulating_pointer = TRUE;
    }

  gdk_event_set_device (ev, device);

  gtk_main_do_event (ev);

  gdk_event_free (ev);

  point->x = x;
  point->y = y;
}

static void
point_release (PointState *point,
               guint       button)
{
  GdkDisplay *display;
  GdkDeviceManager *dm;
  GdkDevice *device;
  GdkEvent *ev;

  if (point->widget == NULL)
    return;

  display = gtk_widget_get_display (point->widget);
  dm = gdk_display_get_device_manager (display);
  device = gdk_device_manager_get_client_pointer (dm);

  if (!point->widget)
    return;

  if (point == &mouse_state)
    {
      if ((point->state & (GDK_BUTTON1_MASK << (button - 1))) == 0)
        return;

      ev = gdk_event_new (GDK_BUTTON_RELEASE);
      ev->any.window = g_object_ref (gtk_widget_get_window (point->widget));
      ev->button.time = GDK_CURRENT_TIME;
      ev->button.x = point->x;
      ev->button.y = point->y;
      ev->button.state = point->state;

      point->state &= ~(GDK_BUTTON1_MASK << (button - 1));
    }
  else
    {
      ev = gdk_event_new (GDK_TOUCH_END);
      ev->any.window = g_object_ref (gtk_widget_get_window (point->widget));
      ev->touch.time = GDK_CURRENT_TIME;
      ev->touch.x = point->x;
      ev->touch.y = point->y;
      ev->touch.sequence = EVENT_SEQUENCE (point);
      ev->touch.state = point->state;

      if (point == &touch_state[0])
        ev->touch.emulating_pointer = TRUE;
    }

  gdk_event_set_device (ev, device);

  gtk_main_do_event (ev);

  gdk_event_free (ev);
}

static const gchar *
phase_nick (GtkPropagationPhase phase)
{
 GTypeClass *class;
 GEnumValue *value;

 class = g_type_class_ref (GTK_TYPE_PROPAGATION_PHASE);
 value = g_enum_get_value ((GEnumClass*)class, phase);
 g_type_class_unref (class);  

 return value->value_nick;
}

static const gchar *
state_nick (GtkEventSequenceState state)
{
 GTypeClass *class;
 GEnumValue *value;

 class = g_type_class_ref (GTK_TYPE_EVENT_SEQUENCE_STATE);
 value = g_enum_get_value ((GEnumClass*)class, state);
 g_type_class_unref (class);  

 return value->value_nick;
}

typedef struct {
  GString *str;
  gboolean exit;
} LegacyData;

static gboolean
legacy_cb (GtkWidget *w, GdkEventButton *button, gpointer data)
{
  LegacyData *ld = data;

  if (ld->str->len > 0)
    g_string_append (ld->str, ", ");
  g_string_append_printf (ld->str, "legacy %s", gtk_widget_get_name (w));

  return ld->exit;
}

typedef struct {
  GString *str;
  GtkEventSequenceState state;
} GestureData;

static void
press_cb (GtkGesture *g, gint n_press, gdouble x, gdouble y, gpointer data)
{
  GtkEventController *c = GTK_EVENT_CONTROLLER (g);
  GtkPropagationPhase phase;
  GestureData *gd = data;
  const gchar *name;
  
  name = g_object_get_data (G_OBJECT (g), "name");
  phase = gtk_event_controller_get_propagation_phase (c);

  if (gd->str->len > 0)
    g_string_append (gd->str, ", ");
  g_string_append_printf (gd->str, "%s %s", phase_nick (phase), name);

  if (gd->state != GTK_EVENT_SEQUENCE_NONE)
    gtk_gesture_set_state (g, gd->state);
}

static void
cancel_cb (GtkGesture *g, GdkEventSequence *sequence, gpointer data)
{
  GestureData *gd = data;
  const gchar *name;
  
  name = g_object_get_data (G_OBJECT (g), "name");
  
  if (gd->str->len > 0)
    g_string_append (gd->str, ", ");
  g_string_append_printf (gd->str, "%s cancelled", name);
}

static void
update_cb (GtkGesture *g, GdkEventSequence *sequence, gpointer data)
{
  GestureData *gd = data;
  const gchar *name;
  
  name = g_object_get_data (G_OBJECT (g), "name");
  
  if (gd->str->len > 0)
    g_string_append (gd->str, ", ");
  g_string_append_printf (gd->str, "%s updated", name);
}

static void
state_changed_cb (GtkGesture *g, GdkEventSequence *sequence, GtkEventSequenceState state, gpointer data)
{
  GestureData *gd = data;
  const gchar *name;
  
  name = g_object_get_data (G_OBJECT (g), "name");
  
  if (gd->str->len > 0)
    g_string_append (gd->str, ", ");
  g_string_append_printf (gd->str, "%s state %s", name, state_nick (state));
}


static GtkGesture *
add_gesture (GtkWidget *w, const gchar *name, GtkPropagationPhase phase, GString *str, GtkEventSequenceState state)
{
  GtkGesture *g;
  GestureData *data;

  data = g_new (GestureData, 1);
  data->str = str;
  data->state = state;

  g = gtk_gesture_multi_press_new (w);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (g), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (g), 1);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (g), phase);

  g_object_set_data (G_OBJECT (g), "name", (gpointer)name);

  g_signal_connect (g, "pressed", G_CALLBACK (press_cb), data);
  g_signal_connect (g, "cancel", G_CALLBACK (cancel_cb), data);
  g_signal_connect (g, "update", G_CALLBACK (update_cb), data);
  g_signal_connect (g, "sequence-state-changed", G_CALLBACK (state_changed_cb), data);

  return g;
}

static void
add_legacy (GtkWidget *w, GString *str, gboolean exit)
{
  LegacyData *data;

  data = g_new (LegacyData, 1);
  data->str = str;
  data->exit = exit;
  g_signal_connect (w, "button-press-event", G_CALLBACK (legacy_cb), data);
}

static void
test_phases (void)
{
  GtkWidget *A, *B, *C;
  GString *str;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_event_box_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show_all (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  point_update (&mouse_state, C, 10, 10);
  point_press (&mouse_state, C, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "bubble c3, "
                   "bubble b3, "
                   "bubble a3");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_mixed (void)
{
  GtkWidget *A, *B, *C;
  GString *str;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_event_box_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show_all (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  add_legacy (A, str, GDK_EVENT_PROPAGATE);
  add_legacy (B, str, GDK_EVENT_PROPAGATE);
  add_legacy (C, str, GDK_EVENT_PROPAGATE);

  point_update (&mouse_state, C, 10, 10);
  point_press (&mouse_state, C, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "legacy C, "
                   "bubble c3, "
                   "legacy B, "
                   "bubble b3, "
                   "legacy A, "
                   "bubble a3");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_early_exit (void)
{
  GtkWidget *A, *B, *C;
  GString *str;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_event_box_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show_all (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  add_legacy (A, str, GDK_EVENT_PROPAGATE);
  add_legacy (B, str, GDK_EVENT_STOP);
  add_legacy (C, str, GDK_EVENT_PROPAGATE);

  point_update (&mouse_state, C, 10, 10);
  point_press (&mouse_state, C, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "legacy C, "
                   "bubble c3, "
                   "legacy B");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_claim_capture (void)
{
  GtkWidget *A, *B, *C;
  GString *str;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_event_box_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show_all (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  point_update (&mouse_state, C, 10, 10);
  point_press (&mouse_state, C, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "c1 state claimed");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_claim_target (void)
{
  GtkWidget *A, *B, *C;
  GString *str;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_event_box_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show_all (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  point_update (&mouse_state, C, 10, 10);
  point_press (&mouse_state, C, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "c2 state claimed");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_claim_bubble (void)
{
  GtkWidget *A, *B, *C;
  GString *str;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_event_box_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show_all (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  point_update (&mouse_state, C, 10, 10);
  point_press (&mouse_state, C, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "bubble c3, "
                   "bubble b3, "
                   "c3 cancelled, "
                   "c2 cancelled, "
                   "c1 cancelled, "
                   "b3 state claimed"
                   );

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_group (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkGesture *g1, *g2;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_event_box_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show_all (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  g1 = add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  g2 = add_gesture (C, "c3", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_gesture_group (g1, g2);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c4", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  point_update (&mouse_state, C, 10, 10);
  point_press (&mouse_state, C, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c3, "
                   "c3 state claimed, "
                   "c2 state claimed, "
                   "target c2");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/gestures/propagation/phases", test_phases);
  g_test_add_func ("/gestures/propagation/mixed", test_mixed);
  g_test_add_func ("/gestures/propagation/early-exit", test_early_exit);
  g_test_add_func ("/gestures/propagation/claim/capture", test_claim_capture);
  g_test_add_func ("/gestures/propagation/claim/target", test_claim_target);
  g_test_add_func ("/gestures/propagation/claim/bubble", test_claim_bubble);
  g_test_add_func ("/gestures/propagation/group", test_group);

  return g_test_run ();
}
