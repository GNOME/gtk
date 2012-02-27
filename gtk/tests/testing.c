/* Gtk+ testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <math.h>

/* Use a keyval that requires Shift to be active (in typical layouts)
 * like GDK_KEY_ampersand, which is '<shift>6'
 */
#define KEYVAL_THAT_REQUIRES_SHIFT GDK_KEY_ampersand


/* --- test functions --- */
static void
test_button_clicks (void)
{
  int a = 0, b = 0, c = 0;
  GtkWidget *window = gtk_test_display_button_window ("Test Window",
                                                      "Test: gtk_test_widget_click",
                                                      "IgnoreMe1", &a,
                                                      "ClickMe", &b,
                                                      "IgnoreMe2", &c,
                                                      NULL);
  GtkWidget *button = gtk_test_find_widget (window, "*Click*", GTK_TYPE_BUTTON);
  gboolean simsuccess;
  g_assert (button != NULL);
  simsuccess = gtk_test_widget_click (button, 1, 0);
  g_assert (simsuccess == TRUE);
  while (gtk_events_pending ())
    gtk_main_iteration ();
  g_assert (a == 0);
  g_assert (b > 0);
  g_assert (c == 0);
}

static void
test_button_keys (void)
{
  int a = 0, b = 0, c = 0;
  GtkWidget *window = gtk_test_display_button_window ("Test Window",
                                                      "Test: gtk_test_widget_send_key",
                                                      "IgnoreMe1", &a,
                                                      "ClickMe", &b,
                                                      "IgnoreMe2", &c,
                                                      NULL);
  GtkWidget *button = gtk_test_find_widget (window, "*Click*", GTK_TYPE_BUTTON);
  gboolean simsuccess;
  g_assert (button != NULL);
  gtk_widget_grab_focus (button);
  g_assert (gtk_widget_has_focus (button));
  simsuccess = gtk_test_widget_send_key (button, GDK_KEY_Return, 0);
  g_assert (simsuccess == TRUE);
  while (gtk_events_pending ())
    gtk_main_iteration ();
  g_assert (a == 0);
  g_assert (b > 0);
  g_assert (c == 0);
}

static gboolean
store_last_key_release (GtkWidget   *widget,
                        GdkEventKey *event,
                        gpointer     user_data)
{
  *((gint *)user_data) = event->keyval;
  return FALSE;
}

static void
test_send_shift_key (void)
{
  GtkWidget *window = gtk_test_display_button_window ("Test Window",
                                                      "Test: test_send_shift_key()",
                                                      "IgnoreMe1", NULL,
                                                      "SendMeKeys", NULL,
                                                      "IgnoreMe2", NULL,
                                                      NULL);
  GtkWidget *button = gtk_test_find_widget (window, "SendMeKeys", GTK_TYPE_BUTTON);
  gint last_key_release = 0;
  gboolean simsuccess;
  g_assert (button != NULL);
  g_signal_connect (button, "key-release-event",
                    G_CALLBACK (store_last_key_release),
                    &last_key_release);
  gtk_widget_grab_focus (button);
  g_assert (gtk_widget_has_focus (button));
  simsuccess = gtk_test_widget_send_key (button, KEYVAL_THAT_REQUIRES_SHIFT, 0 /*modifiers*/);
  g_assert (simsuccess == TRUE);
  while (gtk_events_pending ())
    gtk_main_iteration ();
  g_assert_cmpint (KEYVAL_THAT_REQUIRES_SHIFT, ==, last_key_release);
}

static void
test_slider_ranges (void)
{
  GtkWidget *child;
  GtkWidget *window = gtk_test_create_simple_window ("Test Window", "Test: gtk_test_warp_slider");
  GtkWidget *hscale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                -50, +50, 5);

  child = gtk_bin_get_child (GTK_BIN (window));
  gtk_container_add (GTK_CONTAINER (child), hscale);
  gtk_widget_show (hscale);
  gtk_widget_show_now (window);
  while (gtk_events_pending ())
    gtk_main_iteration ();
  gtk_test_slider_set_perc (hscale, 0.0);
  while (gtk_events_pending ())
    gtk_main_iteration ();
  g_assert (gtk_test_slider_get_value (hscale) == -50);
  gtk_test_slider_set_perc (hscale, 50.0);
  while (gtk_events_pending ())
    gtk_main_iteration ();
  g_assert (fabs (gtk_test_slider_get_value (hscale)) < 0.0001);
  gtk_test_slider_set_perc (hscale, 100.0);
  while (gtk_events_pending ())
    gtk_main_iteration ();
  g_assert (gtk_test_slider_get_value (hscale) == +50.0);
}

static void
test_text_access (void)
{
  const int N_WIDGETS = 3;
  GtkWidget *widgets[N_WIDGETS];
  int i = 0;
  widgets[i++] = gtk_test_create_widget (GTK_TYPE_LABEL, NULL);
  widgets[i++] = gtk_test_create_widget (GTK_TYPE_ENTRY, NULL);
  widgets[i++] = gtk_test_create_widget (GTK_TYPE_TEXT_VIEW, NULL);
  g_assert (i == N_WIDGETS);
  for (i = 0; i < N_WIDGETS; i++)
    gtk_test_text_set (widgets[i], "foobar");
  for (i = 0; i < N_WIDGETS; i++)
    {
      gchar *text  = gtk_test_text_get (widgets[i]);
      g_assert (strcmp (text, "foobar") == 0);
      g_free (text);
    }
  for (i = 0; i < N_WIDGETS; i++)
    gtk_test_text_set (widgets[i], "");
  for (i = 0; i < N_WIDGETS; i++)
    {
      gchar *text  = gtk_test_text_get (widgets[i]);
      g_assert (strcmp (text, "") == 0);
      g_free (text);
    }
}

static void
test_xserver_sync (void)
{
  GtkWidget *window = gtk_test_create_simple_window ("Test Window", "Test: test_xserver_sync");
  GtkWidget *darea = gtk_drawing_area_new ();
  GtkWidget *child;
  GdkWindow *gdk_window;
  GTimer *gtimer = g_timer_new();
  gint sync_is_slower = 0, repeat = 5;

  child = gtk_bin_get_child (GTK_BIN (window));
  //gdk_window = gtk_widget_get_window (darea);

  gtk_widget_set_size_request (darea, 320, 200);
  gtk_container_add (GTK_CONTAINER (child), darea);
  gtk_widget_show (darea);
  gtk_widget_show_now (window);

  while (repeat--)
    {
      gint i, many = 200;
      double nosync_time, sync_time;
      cairo_t *cr;

      while (gtk_events_pending ())
        gtk_main_iteration ();
      gdk_window = gtk_widget_get_window (darea);
      cr = gdk_cairo_create (gdk_window);
      cairo_set_source_rgba (cr, 0, 1, 0, 0.1);
      /* run a number of consecutive drawing requests, just using drawing queue */
      g_timer_start (gtimer);
      for (i = 0; i < many; i++)
        {
          cairo_paint (cr);
        }
      g_timer_stop (gtimer);
      nosync_time = g_timer_elapsed (gtimer, NULL);
      gdk_flush();
      while (gtk_events_pending ())
        gtk_main_iteration ();
      g_timer_start (gtimer);
      /* run a number of consecutive drawing requests with intermediate drawing syncs */
      for (i = 0; i < many; i++)
        {
          cairo_paint (cr);
          gdk_test_render_sync (gdk_window);
        }
      g_timer_stop (gtimer);
      sync_time = g_timer_elapsed (gtimer, NULL);
      sync_is_slower += sync_time > nosync_time * 1.5;
    }
  g_timer_destroy (gtimer);
  g_assert (sync_is_slower > 0);
}

static void
test_spin_button_arrows (void)
{
  GtkWidget *child;
  GtkWidget *window = gtk_test_create_simple_window ("Test Window", "Test: test_spin_button_arrows");
  GtkWidget *spinner = gtk_spin_button_new_with_range (0, 100, 5);
  gboolean simsuccess;
  double oldval, newval;

  gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);

  child = gtk_bin_get_child (GTK_BIN (window));
  gtk_container_add (GTK_CONTAINER (child), spinner);
  gtk_widget_show (spinner);
  gtk_widget_show_now (window);
  gtk_test_slider_set_perc (spinner, 0);
  /* check initial spinner value */
  oldval = gtk_test_slider_get_value (spinner);
  g_assert (oldval == 0);
  /* check simple increment */
  simsuccess = gtk_test_spin_button_click (GTK_SPIN_BUTTON (spinner), 1, TRUE);
  g_assert (simsuccess == TRUE);
  while (gtk_events_pending ()) /* let spin button timeout/idle handlers update */
    gtk_main_iteration ();
  newval = gtk_test_slider_get_value (spinner);
  g_assert (newval > oldval);
  /* check maximum warp */
  simsuccess = gtk_test_spin_button_click (GTK_SPIN_BUTTON (spinner), 3, TRUE);
  g_assert (simsuccess == TRUE);
  while (gtk_events_pending ()) /* let spin button timeout/idle handlers update */
    gtk_main_iteration ();
  oldval = gtk_test_slider_get_value (spinner);
  g_assert (oldval == 100);
  /* check simple decrement */
  oldval = gtk_test_slider_get_value (spinner);
  simsuccess = gtk_test_spin_button_click (GTK_SPIN_BUTTON (spinner), 1, FALSE);
  g_assert (simsuccess == TRUE);
  while (gtk_events_pending ()) /* let spin button timeout/idle handlers update */
    gtk_main_iteration ();
  newval = gtk_test_slider_get_value (spinner);
  g_assert (newval < oldval);
  /* check minimum warp */
  simsuccess = gtk_test_spin_button_click (GTK_SPIN_BUTTON (spinner), 3, FALSE);
  g_assert (simsuccess == TRUE);
  while (gtk_events_pending ()) /* let spin button timeout/idle handlers update */
    gtk_main_iteration ();
  oldval = gtk_test_slider_get_value (spinner);
  g_assert (oldval == 0);
}

static void
test_statusbar_remove_all (void)
{
  GtkWidget *statusbar;

  g_test_bug ("640487");

  statusbar = gtk_statusbar_new ();
  g_object_ref_sink (statusbar);

  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 1, "bla");
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 1, "bla");
  gtk_statusbar_remove_all (GTK_STATUSBAR (statusbar), 1);

  g_object_unref (statusbar);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);
  g_test_bug_base ("http://bugzilla.gnome.org/");
  gtk_test_register_all_types();

  g_test_add_func ("/tests/statusbar-remove-all", test_statusbar_remove_all);
  g_test_add_func ("/ui-tests/text-access", test_text_access);
  g_test_add_func ("/ui-tests/button-clicks", test_button_clicks);
  g_test_add_func ("/ui-tests/keys-events", test_button_keys);
  g_test_add_func ("/ui-tests/send-shift-key", test_send_shift_key);
  g_test_add_func ("/ui-tests/slider-ranges", test_slider_ranges);
  g_test_add_func ("/ui-tests/xserver-sync", test_xserver_sync);
  g_test_add_func ("/ui-tests/spin-button-arrows", test_spin_button_arrows);

  return g_test_run();
}
