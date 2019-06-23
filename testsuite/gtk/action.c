/* GtkAction tests.
 *
 * Authors: Jan Arne Petersen <jpetersen@openismus.com>
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

#define GDK_DISABLE_DEPRECATION_WARNINGS
#include <gtk/gtk.h>

/* Fixture */

typedef struct
{
  GtkAction *action;
} ActionTest;

static void
action_test_setup (ActionTest    *fixture,
                   gconstpointer  test_data)
{
  fixture->action = gtk_action_new ("name", "label", NULL, NULL);
}

static void
action_test_teardown (ActionTest    *fixture,
                      gconstpointer  test_data)
{
  g_object_unref (fixture->action);
}

static void
notify_count_emmisions (GObject    *object,
			GParamSpec *pspec,
			gpointer    data)
{
  unsigned int *i = data;
  (*i)++;
}

static void
menu_item_label_notify_count (ActionTest    *fixture,
                              gconstpointer  test_data)
{
  GtkWidget *item = gtk_menu_item_new ();
  unsigned int emmisions = 0;

  g_object_ref_sink (item);
  g_signal_connect (item, "notify::label",
		    G_CALLBACK (notify_count_emmisions), &emmisions);

  gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (item),
					 fixture->action);

  g_assert_cmpuint (emmisions, ==, 1);

  gtk_action_set_label (fixture->action, "new label");

  g_assert_cmpuint (emmisions, ==, 2);

  g_object_unref (item);
}

#if !GLIB_CHECK_VERSION(2,60,0)
gboolean
g_strv_equal (const gchar * const *strv1,
              const gchar * const *strv2)
{
  g_return_val_if_fail (strv1 != NULL, FALSE);
  g_return_val_if_fail (strv2 != NULL, FALSE);

  if (strv1 == strv2)
    return TRUE;

  for (; *strv1 != NULL && *strv2 != NULL; strv1++, strv2++)
    {
      if (!g_str_equal (*strv1, *strv2))
        return FALSE;
    }

  return (*strv1 == NULL && *strv2 == NULL);
}
#endif

static void
g_test_action_muxer (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  const char **prefixes;
  const char * const expected[] = { "win", NULL };
  const char * const expected1[] = { "group1", "win", NULL };
  const char * const expected2[] = { "group2", "win", NULL };
  const char * const expected3[] = { "group1", "group2", "win", NULL };
  GActionGroup *win;
  GActionGroup *group1;
  GActionGroup *group2;
  GActionGroup *grp;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new_with_label ("test");

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), button);

  win = G_ACTION_GROUP (g_simple_action_group_new ());
  gtk_widget_insert_action_group (window, "win", win);

  prefixes = gtk_widget_list_action_prefixes (window);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected));
  g_free (prefixes);

  prefixes = gtk_widget_list_action_prefixes (box);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected));
  g_free (prefixes);

  prefixes = gtk_widget_list_action_prefixes (button);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected));
  g_free (prefixes);

  grp = gtk_widget_get_action_group (window, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (window, "bla");
  g_assert (grp == NULL);

  grp = gtk_widget_get_action_group (box, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (box, "bla");
  g_assert (grp == NULL);

  grp = gtk_widget_get_action_group (button, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (button, "bla");
  g_assert (grp == NULL);

  group1 = G_ACTION_GROUP (g_simple_action_group_new ());
  gtk_widget_insert_action_group (button, "group1", group1);

  prefixes = gtk_widget_list_action_prefixes (window);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected));
  g_free (prefixes);

  prefixes = gtk_widget_list_action_prefixes (box);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected));
  g_free (prefixes);

  prefixes = gtk_widget_list_action_prefixes (button);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected1));
  g_free (prefixes);

  grp = gtk_widget_get_action_group (window, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (window, "group1");
  g_assert (grp == NULL);

  grp = gtk_widget_get_action_group (box, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (box, "group1");
  g_assert (grp == NULL);

  grp = gtk_widget_get_action_group (button, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (button, "group1");
  g_assert (grp == group1);

  group2 = G_ACTION_GROUP (g_simple_action_group_new ());
  gtk_widget_insert_action_group (box, "group2", group2);

  prefixes = gtk_widget_list_action_prefixes (window);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected));
  g_free (prefixes);

  prefixes = gtk_widget_list_action_prefixes (box);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected2));
  g_free (prefixes);

  prefixes = gtk_widget_list_action_prefixes (button);
  g_assert (g_strv_equal ((const char * const *)prefixes, expected3));
  g_free (prefixes);

  grp = gtk_widget_get_action_group (window, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (window, "group2");
  g_assert (grp == NULL);

  grp = gtk_widget_get_action_group (box, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (box, "group2");
  g_assert (grp == group2);

  grp = gtk_widget_get_action_group (button, "win");
  g_assert (grp == win);
  grp = gtk_widget_get_action_group (button, "group2");
  g_assert (grp == group2);

  gtk_widget_destroy (window);
  g_object_unref (win);
  g_object_unref (group1);
  g_object_unref (group2);
}

/* main */

static void
test_reinsert (void)
{
  GtkWidget *widget;
  GActionGroup *group;

  widget = gtk_label_new ("");
  group = G_ACTION_GROUP (g_simple_action_group_new ());

  gtk_widget_insert_action_group (widget, "test", group);
  g_assert (gtk_widget_get_action_group (widget, "test") == group);

  g_clear_object (&group);

  group = gtk_widget_get_action_group (widget, "test");
  gtk_widget_insert_action_group (widget, "test", group);

  gtk_widget_destroy (widget);
}

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add ("/Action/MenuItem/label-notify-count",
              ActionTest, NULL,
              action_test_setup,
              menu_item_label_notify_count,
              action_test_teardown);

  g_test_add_func ("/action/muxer/update-parent", g_test_action_muxer);
  g_test_add_func ("/action/reinsert", test_reinsert);

  return g_test_run ();
}
