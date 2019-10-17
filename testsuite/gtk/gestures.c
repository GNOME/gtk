#include <gtk/gtk.h>

#define GDK_COMPILATION
#include "gdk/gdkeventsprivate.h"

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
  GdkDevice *device;
  GdkSeat *seat;
  GdkSurface *surface;
  GdkEvent *ev;

  display = gtk_widget_get_display (widget);
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);
  surface = gtk_native_get_surface (gtk_widget_get_native (widget));

  if (point == &mouse_state)
    {
      ev = gdk_event_new (GDK_BUTTON_PRESS);
      ev->any.surface = g_object_ref (surface);
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
      ev->any.surface = g_object_ref (surface);
      ev->touch.time = GDK_CURRENT_TIME;
      ev->touch.x = point->x;
      ev->touch.y = point->y;
      ev->touch.sequence = EVENT_SEQUENCE (point);

      if (point == &touch_state[0])
        ev->touch.emulating_pointer = TRUE;
    }

  gdk_event_set_device (ev, device);

  gtk_main_do_event (ev);

  g_object_unref (ev);

  point->widget = widget;
}

static void
point_update (PointState *point,
              GtkWidget  *widget,
              gdouble     x,
              gdouble     y)
{
  GdkDisplay *display;
  GdkDevice *device;
  GdkSeat *seat;
  GdkSurface *surface;
  GdkEvent *ev;

  display = gtk_widget_get_display (widget);
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);
  surface = gtk_native_get_surface (gtk_widget_get_native (widget));

  point->x = x;
  point->y = y;

  if (point == &mouse_state)
    {
      ev = gdk_event_new (GDK_MOTION_NOTIFY);
      ev->any.surface = g_object_ref (surface);
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
      ev->any.surface = g_object_ref (surface);
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

  g_object_unref (ev);
}

static void
point_release (PointState *point,
               guint       button)
{
  GdkDisplay *display;
  GdkDevice *device;
  GdkSeat *seat;
  GdkSurface *surface;
  GdkEvent *ev;

  if (point->widget == NULL)
    return;

  display = gtk_widget_get_display (point->widget);
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);
  surface = gtk_native_get_surface (gtk_widget_get_native (point->widget));

  if (!point->widget)
    return;

  if (point == &mouse_state)
    {
      if ((point->state & (GDK_BUTTON1_MASK << (button - 1))) == 0)
        return;

      ev = gdk_event_new (GDK_BUTTON_RELEASE);
      ev->any.surface = g_object_ref (surface);
      ev->button.time = GDK_CURRENT_TIME;
      ev->button.x = point->x;
      ev->button.y = point->y;
      ev->button.state = point->state;

      point->state &= ~(GDK_BUTTON1_MASK << (button - 1));
    }
  else
    {
      ev = gdk_event_new (GDK_TOUCH_END);
      ev->any.surface = g_object_ref (surface);
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

  g_object_unref (ev);
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
  GtkEventController *controller;
  GString *str;
  gboolean exit;
} LegacyData;

static gboolean
legacy_cb (GtkEventControllerLegacy *c, GdkEvent *button, gpointer data)
{
  if (gdk_event_get_event_type (button) == GDK_BUTTON_PRESS)
    {
      LegacyData *ld = data;
      GtkWidget *w;

      w = gtk_event_controller_get_widget (ld->controller);

      if (ld->str->len > 0)
        g_string_append (ld->str, ", ");
      g_string_append_printf (ld->str, "legacy %s", gtk_widget_get_name (w));

      return ld->exit;
    }

  return GDK_EVENT_PROPAGATE;
}

typedef struct {
  GString *str;
  GtkEventSequenceState state;
} GestureData;

static void
press_cb (GtkGesture *g, gint n_press, gdouble x, gdouble y, gpointer data)
{
  GtkEventController *c = GTK_EVENT_CONTROLLER (g);
  GdkEventSequence *sequence;
  GtkPropagationPhase phase;
  GestureData *gd = data;
  const gchar *name;
  
  name = g_object_get_data (G_OBJECT (g), "name");
  phase = gtk_event_controller_get_propagation_phase (c);

  if (gd->str->len > 0)
    g_string_append (gd->str, ", ");
  g_string_append_printf (gd->str, "%s %s", phase_nick (phase), name);

  sequence = gtk_gesture_get_last_updated_sequence (g);

  if (sequence)
    g_string_append_printf (gd->str, " (%x)", GPOINTER_TO_UINT (sequence));

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
begin_cb (GtkGesture *g, GdkEventSequence *sequence, gpointer data)
{
  GestureData *gd = data;
  const gchar *name;

  name = g_object_get_data (G_OBJECT (g), "name");

  if (gd->str->len > 0)
    g_string_append (gd->str, ", ");
  g_string_append_printf (gd->str, "%s began", name);

  if (gd->state != GTK_EVENT_SEQUENCE_NONE)
    gtk_gesture_set_state (g, gd->state);
}

static void
end_cb (GtkGesture *g, GdkEventSequence *sequence, gpointer data)
{
  GestureData *gd = data;
  const gchar *name;

  name = g_object_get_data (G_OBJECT (g), "name");

  if (gd->str->len > 0)
    g_string_append (gd->str, ", ");
  g_string_append_printf (gd->str, "%s ended", name);
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

  if (sequence != NULL)
    g_string_append_printf (gd->str, " (%x)", GPOINTER_TO_UINT (sequence));
}


static GtkGesture *
add_gesture (GtkWidget *w, const gchar *name, GtkPropagationPhase phase, GString *str, GtkEventSequenceState state)
{
  GtkGesture *g;
  GestureData *data;

  data = g_new (GestureData, 1);
  data->str = str;
  data->state = state;

  g = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (g), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (g), 1);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (g), phase);
  gtk_widget_add_controller (w, GTK_EVENT_CONTROLLER (g));

  g_object_set_data (G_OBJECT (g), "name", (gpointer)name);

  g_signal_connect (g, "pressed", G_CALLBACK (press_cb), data);
  g_signal_connect (g, "cancel", G_CALLBACK (cancel_cb), data);
  g_signal_connect (g, "update", G_CALLBACK (update_cb), data);
  g_signal_connect (g, "sequence-state-changed", G_CALLBACK (state_changed_cb), data);

  return g;
}

static GtkGesture *
add_mt_gesture (GtkWidget *w, const gchar *name, GtkPropagationPhase phase, GString *str, GtkEventSequenceState state)
{
  GtkGesture *g;
  GestureData *data;

  data = g_new (GestureData, 1);
  data->str = str;
  data->state = state;

  g = gtk_gesture_rotate_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (g), phase);
  gtk_widget_add_controller (w, GTK_EVENT_CONTROLLER (g));

  g_object_set_data (G_OBJECT (g), "name", (gpointer)name);

  g_signal_connect (g, "begin", G_CALLBACK (begin_cb), data);
  g_signal_connect (g, "update", G_CALLBACK (update_cb), data);
  g_signal_connect (g, "end", G_CALLBACK (end_cb), data);
  g_signal_connect (g, "sequence-state-changed", G_CALLBACK (state_changed_cb), data);

  return g;
}

static void
add_legacy (GtkWidget *w, GString *str, gboolean exit)
{
  LegacyData *data;

  data = g_new (LegacyData, 1);
  data->controller = gtk_event_controller_legacy_new ();
  data->str = str;
  data->exit = exit;

  gtk_event_controller_set_propagation_phase (data->controller, GTK_PHASE_BUBBLE);
  gtk_widget_add_controller (w, data->controller);
  g_signal_connect (data->controller, "event", G_CALLBACK (legacy_cb), data);
}

static void
test_phases (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

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

  gtk_widget_get_allocation (A, &allocation);

  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

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
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_legacy (A, str, GDK_EVENT_PROPAGATE);
  add_legacy (B, str, GDK_EVENT_PROPAGATE);
  add_legacy (C, str, GDK_EVENT_PROPAGATE);

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);

  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "bubble c3, "
                   "legacy C, "
                   "bubble b3, "
                   "legacy B, "
                   "bubble a3, "
                   "legacy A");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_early_exit (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_legacy (A, str, GDK_EVENT_PROPAGATE);
  add_legacy (B, str, GDK_EVENT_STOP);
  add_legacy (C, str, GDK_EVENT_PROPAGATE);

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);

  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "bubble c3, "
                   "legacy C, "
                   "bubble b3, "
                   "legacy B");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_claim_capture (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);

  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

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
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);
  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

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
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);
  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

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
test_early_claim_capture (void)
{
  GtkWidget *A, *B, *C;
  GtkGesture *g;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  g = add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);
  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "b1 state claimed");

  /* Reset the string */
  g_string_erase (str, 0, str->len);

  gtk_gesture_set_state (g, GTK_EVENT_SEQUENCE_DENIED);

  g_assert_cmpstr (str->str, ==,
                   "capture c1, "
                   "c1 state claimed, "
                   "b1 state denied");

  point_release (&mouse_state, 1);

  g_string_free (str, TRUE);
  gtk_widget_destroy (A);
}

static void
test_late_claim_capture (void)
{
  GtkWidget *A, *B, *C;
  GtkGesture *g;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  g = add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (A, "a3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c3", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);
  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "c2 state claimed");

  /* Reset the string */
  g_string_erase (str, 0, str->len);

  gtk_gesture_set_state (g, GTK_EVENT_SEQUENCE_CLAIMED);

  g_assert_cmpstr (str->str, ==,
                   "c2 cancelled, "
                   "c1 cancelled, "
                   "b1 state claimed");

  point_release (&mouse_state, 1);

  g_string_free (str, TRUE);
  gtk_widget_destroy (A);
}

static void
test_group (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkGesture *g1, *g2;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

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

  gtk_widget_get_allocation (A, &allocation);

  gtk_widget_get_allocation (A, &allocation);
  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

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

static void
test_gestures_outside_grab (void)
{
  GtkWidget *A, *B, *C, *D;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  D = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_show (D);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (B, "b2", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a2", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);
  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "c2 state claimed");

  /* Set a grab on another window */
  g_string_erase (str, 0, str->len);
  gtk_grab_add (D);

  g_assert_cmpstr (str->str, ==,
                   "c1 cancelled, "
                   "c2 cancelled, "
                   "b1 cancelled, "
                   "a1 cancelled");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
  gtk_widget_destroy (D);
}

static void
test_gestures_inside_grab (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (C, "c2", GTK_PHASE_TARGET, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_gesture (B, "b2", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (A, "a2", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_NONE);

  gtk_widget_get_allocation (A, &allocation);
  point_update (&mouse_state, A, allocation.width / 2, allocation.height / 2);
  point_press (&mouse_state, A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1, "
                   "capture b1, "
                   "capture c1, "
                   "target c2, "
                   "c2 state claimed");

  /* Set a grab on B */
  g_string_erase (str, 0, str->len);
  gtk_grab_add (B);
  g_assert_cmpstr (str->str, ==,
                   "a1 cancelled");

  /* Update with the grab under effect */
  g_string_erase (str, 0, str->len);
  point_update (&mouse_state, A, (allocation.width / 2),
                (allocation.height / 2));
  g_assert_cmpstr (str->str, ==,
                   "b1 updated, "
                   "c1 updated, "
                   "c2 updated");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_multitouch_on_single (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_NONE);
  add_gesture (B, "b1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_CLAIMED);

  gtk_widget_get_allocation (A, &allocation);

  /* First touch down */
  point_update (&touch_state[0], A, allocation.width / 2, allocation.height / 2);
  point_press (&touch_state[0], A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1 (1), "
                   "capture b1 (1), "
                   "b1 state claimed (1)");

  /* Second touch down */
  g_string_erase (str, 0, str->len);
  point_update (&touch_state[1], A, (allocation.width / 2),
                (allocation.height / 2));
  point_press (&touch_state[1], A, 1);

  g_assert_cmpstr (str->str, ==,
                   "a1 state denied (2), "
                   "b1 state denied (2)");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_multitouch_activation (void)
{
  GtkWidget *A, *B, *C;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  add_mt_gesture (C, "c1", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_widget_get_allocation (A, &allocation);

  /* First touch down */
  point_update (&touch_state[0], A, allocation.width / 2, allocation.height / 2);
  point_press (&touch_state[0], A, 1);

  g_assert_cmpstr (str->str, ==, "");

  /* Second touch down */
  point_update (&touch_state[1], A, (allocation.width / 2),
                (allocation.height / 2));
  point_press (&touch_state[1], A, 1);

  g_assert_cmpstr (str->str, ==,
                   "c1 began, "
                   "c1 state claimed (2), "
                   "c1 state claimed (1)");

  /* First touch up */
  g_string_erase (str, 0, str->len);
  point_release (&touch_state[0], 1);

  g_assert_cmpstr (str->str, ==,
                   "c1 ended");

  /* A third touch down triggering again action */
  g_string_erase (str, 0, str->len);
  point_update (&touch_state[2], A, (allocation.width / 2),
                (allocation.height / 2));
  point_press (&touch_state[2], A, 1);

  g_assert_cmpstr (str->str, ==,
                   "c1 began, "
                   "c1 state claimed (3)");

  /* One touch up, gesture is finished again */
  g_string_erase (str, 0, str->len);
  point_release (&touch_state[2], 1);

  g_assert_cmpstr (str->str, ==,
                   "c1 ended");

  /* Another touch up, gesture remains inactive */
  g_string_erase (str, 0, str->len);
  point_release (&touch_state[1], 1);

  g_assert_cmpstr (str->str, ==, "");

  g_string_free (str, TRUE);

  gtk_widget_destroy (A);
}

static void
test_multitouch_interaction (void)
{
  GtkWidget *A, *B, *C;
  GtkGesture *g;
  GString *str;
  GtkAllocation allocation;

  A = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (A, "A");
  B = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name (B, "B");
  C = gtk_image_new ();
  gtk_widget_set_hexpand (C, TRUE);
  gtk_widget_set_vexpand (C, TRUE);
  gtk_widget_set_name (C, "C");

  gtk_container_add (GTK_CONTAINER (A), B);
  gtk_container_add (GTK_CONTAINER (B), C);

  gtk_widget_show (A);

  str = g_string_new ("");

  g = add_gesture (A, "a1", GTK_PHASE_CAPTURE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  add_mt_gesture (C, "c1", GTK_PHASE_BUBBLE, str, GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_widget_get_allocation (A, &allocation);

  /* First touch down, a1 claims the sequence */
  point_update (&touch_state[0], A, allocation.width / 2, allocation.height / 2);
  point_press (&touch_state[0], A, 1);

  g_assert_cmpstr (str->str, ==,
                   "capture a1 (1), "
                   "a1 state claimed (1)");

  /* Second touch down, a1 denies and c1 takes over */
  g_string_erase (str, 0, str->len);
  point_update (&touch_state[1], A, (allocation.width / 2),
                (allocation.height / 2));
  point_press (&touch_state[1], A, 1);

  /* Denying sequences in touch-excess situation is a responsibility of the caller */
  gtk_gesture_set_state (g, GTK_EVENT_SEQUENCE_DENIED);

  g_assert_cmpstr (str->str, ==,
                   "a1 state denied (2), "
                   "c1 began, "
                   "c1 state claimed (1), "
                   "c1 state claimed (2), "
                   "a1 state denied (1)");

  /* Move first point, only c1 should update */
  g_string_erase (str, 0, str->len);
  point_update (&touch_state[0], A, (allocation.width / 2),
                (allocation.height / 2));

  g_assert_cmpstr (str->str, ==,
                   "c1 updated");

  /* First touch up */
  g_string_erase (str, 0, str->len);
  point_release (&touch_state[0], 1);

  g_assert_cmpstr (str->str, ==,
                   "c1 ended");

  /* A third touch down triggering again action on c1 */
  g_string_erase (str, 0, str->len);
  point_update (&touch_state[2], A, (allocation.width / 2),
                (allocation.height / 2));
  point_press (&touch_state[2], A, 1);

  g_assert_cmpstr (str->str, ==,
                   "a1 state denied (3), "
                   "c1 began, "
                   "c1 state claimed (3)");

  /* One touch up, gesture is finished again */
  g_string_erase (str, 0, str->len);
  point_release (&touch_state[2], 1);

  g_assert_cmpstr (str->str, ==,
                   "c1 ended");

  /* Another touch up, gesture remains inactive */
  g_string_erase (str, 0, str->len);
  point_release (&touch_state[1], 1);

  g_assert_cmpstr (str->str, ==, "");

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
  g_test_add_func ("/gestures/claim/capture", test_claim_capture);
  g_test_add_func ("/gestures/claim/target", test_claim_target);
  g_test_add_func ("/gestures/claim/bubble", test_claim_bubble);
  g_test_add_func ("/gestures/claim/early-capture", test_early_claim_capture);
  g_test_add_func ("/gestures/claim/late-capture", test_late_claim_capture);
  g_test_add_func ("/gestures/group", test_group);
  g_test_add_func ("/gestures/grabs/gestures-outside-grab", test_gestures_outside_grab);
  g_test_add_func ("/gestures/grabs/gestures-inside-grab", test_gestures_inside_grab);
  g_test_add_func ("/gestures/multitouch/gesture-single", test_multitouch_on_single);
  g_test_add_func ("/gestures/multitouch/multitouch-activation", test_multitouch_activation);
  g_test_add_func ("/gestures/multitouch/interaction", test_multitouch_interaction);

  return g_test_run ();
}
