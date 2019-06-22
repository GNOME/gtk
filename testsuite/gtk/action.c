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

/* Test that inheriting actions along the widget
 * hierarchy works as expected. Since GtkWidget does
 * not expose the actions, we test this by observing
 * the effect of activating them.
 */
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

static int cut_activated;
static int copy_activated;
static int paste_activated;
static int visibility_changed;

static void
cut_activate (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  cut_activated++;
}

static void
copy_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  copy_activated++;
}

static void
paste_activate (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  paste_activated++;
}

static void
visibility_changed_cb (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  visibility_changed++;
}

/* Spot-check that GtkText has the class actions
 * for the context menu. Here we test that the clipboard
 * actions are present, and that toggling visibility
 * via the action works.
 */
static void
test_text (void)
{
  GtkWidget *box;
  GtkWidget *text;
  GSimpleActionGroup *clipboard_actions;
  GActionEntry clipboard_entries[] = {
    { "cut", cut_activate, NULL, NULL, NULL },
    { "copy", copy_activate, NULL, NULL, NULL },
    { "paste", paste_activate, NULL, NULL, NULL },
  };
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  text = gtk_text_new ();

  gtk_container_add (GTK_CONTAINER (box), text);

  clipboard_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (clipboard_actions),
                                   clipboard_entries,
                                   G_N_ELEMENTS (clipboard_entries),
                                   NULL);

  gtk_widget_insert_action_group (box, "clipboard", G_ACTION_GROUP (clipboard_actions));

  gtk_widget_activate_action (text, "cut.clipboard", NULL);
  gtk_widget_activate_action (text, "copy.clipboard", NULL);
  gtk_widget_activate_action (text, "paste.clipboard", NULL);

  g_assert_cmpint (cut_activated, ==, 0);
  g_assert_cmpint (copy_activated, ==, 0);
  g_assert_cmpint (paste_activated, ==, 0);

  g_signal_connect (text, "notify::visibility",
                    G_CALLBACK (visibility_changed_cb), NULL);

  gtk_widget_activate_action (text, "misc.toggle-visibility", NULL);

  g_assert_cmpint (visibility_changed, ==, 1);

  gtk_widget_destroy (box);
  g_object_unref (clipboard_actions);
}

/* Test that inheritance works for individual actions
 * even if they are in groups with the same prefix.
 * This is a change from the way things work in GTK3.
 */
static void
test_overlap (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GActionEntry win_entries[] = {
    { "win", win_activate, NULL, NULL, NULL },
  };
  GActionEntry box_entries[] = {
    { "box", box_activate, NULL, NULL, NULL },
  };
  GSimpleActionGroup *win_actions;
  GSimpleActionGroup *box_actions;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_container_add (GTK_CONTAINER (window), box);

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

  gtk_widget_insert_action_group (window, "actions", G_ACTION_GROUP (win_actions));
  gtk_widget_insert_action_group (box, "actions", G_ACTION_GROUP (box_actions));

  win_activated = 0;
  box_activated = 0;

  gtk_widget_activate_action (box, "actions.win", NULL);

  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box_activated, ==, 0);

  gtk_widget_activate_action (box, "actions.box", NULL);

  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box_activated, ==, 1);

  gtk_widget_destroy (window);
  g_object_unref (win_actions);
  g_object_unref (box_actions);
}

static int toggled;
static int act1;
static int act2;

static void
visibility_toggled (GObject *object,
                    GParamSpec *pspec,
                    gpointer data)
{
  toggled++;
}

static void
activate1 (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  act1++;
}

static void
activate2 (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  act2++;
}

/* Test that overlap also works as expected between
 * class action and inserted groups. Class actions
 * take precedence over inserted groups in the same
 * muxer, but inheritance works as normal between
 * muxers.
 */
static void
test_overlap2 (void)
{
  GtkWidget *text;
  GtkWidget *child;
  GSimpleActionGroup *group1;
  GSimpleActionGroup *group2;
  GActionEntry entries1[] = {
    { "toggle-visibility", activate1, NULL, NULL, NULL },
  };
  GActionEntry entries2[] = {
    { "toggle-visibility", activate2, NULL, NULL, NULL },
  };

  text = gtk_text_new ();
  g_signal_connect (text, "notify::visibility",
                    G_CALLBACK (visibility_toggled), NULL);

  child = gtk_label_new ("");
  gtk_widget_set_parent (child, text);

  g_assert_cmpint (toggled, ==, 0);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 0);

  gtk_widget_activate_action (child, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 1);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 0);

  group1 = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group1),
                                   entries1,
                                   G_N_ELEMENTS (entries1),
                                   NULL);
  gtk_widget_insert_action_group (text, "misc", G_ACTION_GROUP (group1));
  gtk_widget_activate_action (child, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 2);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 0);

  group2 = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group2),
                                   entries2,
                                   G_N_ELEMENTS (entries2),
                                   NULL);
  gtk_widget_insert_action_group (child, "misc", G_ACTION_GROUP (group2));

  gtk_widget_activate_action (child, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 2);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 1);

  gtk_widget_destroy (text);
  g_object_unref (group1);
  g_object_unref (group2);
}

/* Test that gtk_widget_class_query_action
 * yields the expected results
 */
static void
test_introspection (void)
{
  GtkWidgetClass *class = g_type_class_ref (GTK_TYPE_TEXT);
  guint i;
  GType owner;
  const char *name;
  const GVariantType *params;
  const char *property;
  struct {
    GType owner;
    const char *name;
    const char *params;
    const char *property;
  } expected[] = {
    { GTK_TYPE_TEXT, "clipboard.cut", NULL, NULL },
    { GTK_TYPE_TEXT, "clipboard.copy", NULL, NULL },
    { GTK_TYPE_TEXT, "clipboard.paste", NULL, NULL },
    { GTK_TYPE_TEXT, "selection.delete", NULL, NULL },
    { GTK_TYPE_TEXT, "selection.select-all", NULL, NULL },
    { GTK_TYPE_TEXT, "misc.insert-emoji", NULL, NULL },
    { GTK_TYPE_TEXT, "misc.toggle-visibility", NULL, "visibility" },
  };

  i = 0;
  while (gtk_widget_class_query_action (class,
                                        i,
                                        &owner,
                                        &name,
                                        &params,
                                        &property))
    {
      g_assert (expected[i].owner == owner);
      g_assert (strcmp (expected[i].name, name) == 0);
      g_assert ((expected[i].params == NULL && params == NULL) ||
                strcmp (expected[i].params, g_variant_type_peek_string (params)) == 0);
      g_assert ((expected[i].property == NULL && property == NULL) ||
                strcmp (expected[i].property, property) == 0);
      i++;
    }
  g_assert (i == G_N_ELEMENTS (expected));

  g_type_class_unref (class);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/action/inheritance", test_action);
  g_test_add_func ("/action/text", test_text);
  g_test_add_func ("/action/overlap", test_overlap);
  g_test_add_func ("/action/overlap2", test_overlap2);
  g_test_add_func ("/action/introspection", test_introspection);

  return g_test_run();
}
