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

/* main */

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

  return g_test_run ();
}

