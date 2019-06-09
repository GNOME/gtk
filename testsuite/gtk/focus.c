#include <gtk/gtk.h>

const char *
widget_name (GtkWidget *widget)
{
  if (gtk_widget_get_name (widget))
    return gtk_widget_get_name (widget);
  else if (GTK_IS_LABEL (widget))
    return gtk_label_get_label (GTK_LABEL (widget));
  else if (GTK_IS_EDITABLE (widget))
    return gtk_editable_get_text (GTK_EDITABLE (widget));
  else
    return G_OBJECT_TYPE_NAME (widget);
}

static char *
mode_to_string (GdkCrossingMode mode)
{
  return g_enum_to_string (GDK_TYPE_CROSSING_MODE, mode);
}

static char *
detail_to_string (GdkNotifyType detail)
{
  return g_enum_to_string (GDK_TYPE_NOTIFY_TYPE, detail);
}

static void
focus_in (GtkEventController *controller,
          GdkCrossingMode mode,
          GdkNotifyType detail,
          GString *s)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  gboolean is_focus;
  gboolean contains_focus;

  g_object_get (controller,
                "is-focus", &is_focus,
                "contains-focus", &contains_focus,
                NULL);
  g_string_append_printf (s, "%s: focus-in %s %s is-focus: %d contains-focus: %d\n",
                          widget_name (widget),
                          mode_to_string (mode),
                          detail_to_string (detail),
                          is_focus,
                          contains_focus);
}

static void
focus_out (GtkEventController *controller,
           GdkCrossingMode mode,
           GdkNotifyType detail,
           GString *s)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  gboolean is_focus;
  gboolean contains_focus;

  g_object_get (controller,
                "is-focus", &is_focus,
                "contains-focus", &contains_focus,
                NULL);
  g_string_append_printf (s, "%s: focus-out %s %s is-focus: %d contains-focus: %d\n",
                          widget_name (widget),
                          mode_to_string (mode),
                          detail_to_string (detail),
                          is_focus,
                          contains_focus);
}

static void
add_controller (GtkWidget *widget, GString *s)
{
  GtkEventController *controller;

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "focus-in", G_CALLBACK (focus_in), s);
  g_signal_connect (controller, "focus-out", G_CALLBACK (focus_out), s);
  gtk_widget_add_controller (widget, controller);
}

static gboolean
stop_loop (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
wait (void)
{
  GMainLoop *loop;

  loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add (1000, stop_loop, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}

static void
test_window_focus (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *label1;
  GtkWidget *label2;
  GtkWidget *entry1;
  GtkWidget *entry2;
  GtkWidget *menubutton;
  GtkWidget *popover;
  GtkWidget *button;
  GString *s = g_string_new ("");

  /*
   * The tree look like this, with [] indicating
   * focus locations:
   *
   *       window
   *         |
   *  +----[box]-----+-------------------+
   *  |      |       |                   |
   * label1 box1    box2------+       menubutton
   *         |       |        |          |
   *      [entry1]  label2  [entry2]  popover
   *                                     |
   *                                  [button]
   */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "window");
  add_controller (window, s);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_can_focus (box, TRUE);
  gtk_widget_set_name (box, "box");
  add_controller (box, s);
  gtk_container_add (GTK_CONTAINER (window), box);
  label1 = gtk_label_new ("label1");
  gtk_widget_set_name (label1, "label1");
  add_controller (label1, s);
  gtk_container_add (GTK_CONTAINER (box), label1);
  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_name (box1, "box1");
  add_controller (box1, s);
  gtk_container_add (GTK_CONTAINER (box), box1);
  entry1 = gtk_text_new ();
  gtk_widget_set_name (entry1, "entry1");
  add_controller (entry1, s);
  gtk_container_add (GTK_CONTAINER (box1), entry1);
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_name (box2, "box2");
  add_controller (box2, s);
  gtk_container_add (GTK_CONTAINER (box), box2);
  label2 = gtk_label_new ("label2");
  gtk_widget_set_name (label2, "label2");
  add_controller (label2, s);
  gtk_container_add (GTK_CONTAINER (box2), label2);
  entry2 = gtk_text_new ();
  gtk_widget_set_name (entry2, "entry2");
  add_controller (entry2, s);
  gtk_container_add (GTK_CONTAINER (box2), entry2);
  popover = gtk_popover_new (NULL);
  add_controller (popover, s);
  gtk_widget_set_name (popover, "popover");
  button = gtk_button_new ();
  add_controller (button, s);
  gtk_widget_set_name (button, "button");
  gtk_container_add (GTK_CONTAINER (popover), button);
  menubutton = gtk_menu_button_new ();
  add_controller (menubutton, s);
  gtk_widget_set_name (menubutton, "menubutton");
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (menubutton), popover);
  gtk_container_add (GTK_CONTAINER (box), menubutton);

  g_assert_null (gtk_window_get_focus (GTK_WINDOW (window)));

  gtk_widget_show (window);

  wait ();

  /* show puts the initial focus on box */
  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == box);

  if (g_test_verbose ())
    g_print ("-> box\n%s\n", s->str);

  g_assert_cmpstr (s->str, ==,
"window: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_VIRTUAL is-focus: 0 contains-focus: 1\n"
"box: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_ANCESTOR is-focus: 1 contains-focus: 0\n"
"window: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_ANCESTOR is-focus: 1 contains-focus: 0\n");
  g_string_truncate (s, 0);

  wait ();

  gtk_widget_grab_focus (entry1);

  if (g_test_verbose ())
    g_print ("box -> entry1\n%s\n", s->str);

  g_assert_cmpstr (s->str, ==,
"box: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_INFERIOR is-focus: 0 contains-focus: 1\n"
"box1: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_VIRTUAL is-focus: 0 contains-focus: 1\n"
"entry1: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_ANCESTOR is-focus: 1 contains-focus: 0\n");

  g_string_truncate (s, 0);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry1);

  wait ();

  gtk_widget_grab_focus (entry2);

  if (g_test_verbose ())
    g_print ("entry1 -> entry2\n%s\n", s->str);

  g_assert_cmpstr (s->str, ==,
"entry1: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_NONLINEAR is-focus: 0 contains-focus: 0\n"
"box1: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_NONLINEAR_VIRTUAL is-focus: 0 contains-focus: 0\n"
"box2: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_NONLINEAR_VIRTUAL is-focus: 0 contains-focus: 1\n"
"entry2: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_NONLINEAR is-focus: 1 contains-focus: 0\n");

  g_string_truncate (s, 0);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry2);

  wait ();

  gtk_widget_grab_focus (box);

  if (g_test_verbose ())
    g_print ("entry2 -> box\n%s", s->str);

  g_assert_cmpstr (s->str, ==,
"entry2: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_ANCESTOR is-focus: 0 contains-focus: 0\n"
"box2: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_VIRTUAL is-focus: 0 contains-focus: 0\n"
"box: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_INFERIOR is-focus: 1 contains-focus: 0\n");

  g_string_truncate (s, 0);

  wait ();

  gtk_menu_button_popup (GTK_MENU_BUTTON (menubutton));

  if (g_test_verbose ())
    g_print ("box -> button\n%s", s->str);

  g_assert_cmpstr (s->str, ==,
"box: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_INFERIOR is-focus: 0 contains-focus: 1\n"
"menubutton: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_VIRTUAL is-focus: 0 contains-focus: 1\n"
"popover: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_VIRTUAL is-focus: 0 contains-focus: 1\n"
"button: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_ANCESTOR is-focus: 1 contains-focus: 0\n");

  g_string_truncate (s, 0);

  wait ();

  gtk_widget_grab_focus (box);

  if (g_test_verbose ())
    g_print ("button -> box\n%s", s->str);

  g_assert_cmpstr (s->str, ==,
"button: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_ANCESTOR is-focus: 0 contains-focus: 0\n"
"popover: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_VIRTUAL is-focus: 0 contains-focus: 0\n"
"menubutton: focus-out GDK_CROSSING_NORMAL GDK_NOTIFY_VIRTUAL is-focus: 0 contains-focus: 0\n"
"box: focus-in GDK_CROSSING_NORMAL GDK_NOTIFY_INFERIOR is-focus: 1 contains-focus: 0\n");

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == box);

  wait ();

  gtk_widget_hide (window);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == box);

  gtk_window_set_focus (GTK_WINDOW (window), entry1);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry1);

  gtk_widget_destroy (window);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/focus/window", test_window_focus);

  return g_test_run ();
}
