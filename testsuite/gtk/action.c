/* Copyright (C) 2019 Red Hat, Inc.
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

static int win_activated;
static int box_activated;

static void
win_activate (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  win_activated++;
}

static void
box_activate (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  box_activated++;
}

static void
test_action (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  GSimpleActionGroup *win_actions;
  GSimpleActionGroup *box_actions;
  GActionEntry win_entries[] = {
    { "action", win_activate, NULL, NULL, NULL },
  };
  GActionEntry box_entries[] = {
    { "action", box_activate, NULL, NULL, NULL },
  };

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new ();

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), button);

  win_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (win_actions),
                                   win_entries,
                                   G_N_ELEMENTS (win_entries),
                                   NULL);

  box_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (box_actions),
                                   box_entries,
                                   G_N_ELEMENTS (box_entries),
                                   NULL);

  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (win_actions));
  gtk_widget_insert_action_group (box, "box", G_ACTION_GROUP (box_actions));

  g_assert_cmpint (win_activated, ==, 0);
  g_assert_cmpint (box_activated, ==, 0);

  gtk_widget_activate_action (button, "win.action", NULL);

  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box_activated, ==, 0);

  gtk_widget_activate_action (box, "win.action", NULL);

  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box_activated, ==, 0);

  gtk_widget_activate_action (button, "box.action", NULL);

  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box_activated, ==, 1);

  gtk_widget_activate_action (window, "box.action", NULL);

  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box_activated, ==, 1);

  gtk_widget_destroy (window);
  g_object_unref (win_actions);
  g_object_unref (box_actions);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/action/inheritance", test_action);

  return g_test_run();
}
