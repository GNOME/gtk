/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

#define N_ROWS 10000

const gchar list_ui[] =
  "<interface>"
  "  <object class='GtkListStore' id='liststore1'>"
  "    <columns>"
  "      <column type='gchararray'/>"
  "      <column type='gchararray'/>"
  "      <column type='gchararray'/>"
  "      <column type='gboolean'/>"
  "      <column type='gint'/>"
  "      <column type='gint'/>"
  "    </columns>"
  "    <data>"
  "      <row><col id='0'>One</col><col id='1'>Two</col><col id='2'>Three</col><col id='3'>True</col><col id='4'>50</col><col id='5'>50</col></row>"
  "    </data>"
  "  </object>"
  "  <object class='GtkWindow' id='window1'>"
  "    <child>"
  "      <object class='GtkTreeView' id='treeview1'>"
  "        <property name='visible'>True</property>"
  "        <property name='model'>liststore1</property>"
  "        <child>"
  "          <object class='GtkTreeViewColumn' id='column1'>"
  "            <property name='title' translatable='yes'>First column</property>"
  "            <child>"
  "              <object class='GtkCellRendererText' id='renderer1'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='text'>0</attribute>"
  "              </attributes>"
  "            </child>"
  "            <child>"
  "              <object class='GtkCellRendererToggle' id='renderer2'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='active'>3</attribute>"
  "              </attributes>"
  "            </child>"
  "          </object>"
  "        </child>"
  "        <child>"
  "          <object class='GtkTreeViewColumn' id='column2'>"
  "            <property name='title' translatable='yes'>Second column</property>"
  "            <child>"
  "              <object class='GtkCellRendererText' id='renderer3'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='text'>1</attribute>"
  "              </attributes>"
  "            </child>"
  "            <child>"
  "              <object class='GtkCellRendererProgress' id='renderer4'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='value'>4</attribute>"
  "              </attributes>"
  "            </child>"
  "          </object>"
  "        </child>"
  "      </object>"
  "    </child>"
  "  </object>"
  "</interface>";

static void
walk_accessible_tree (AtkObject *accessible,
                      gpointer   data)
{
  gint *count = data;
  gint i;

  (*count)++;

  for (i = 0; i < atk_object_get_n_accessible_children (accessible); i++)
    {
      AtkObject *child = atk_object_ref_accessible_child (accessible, i);
      walk_accessible_tree (child, data);
      g_object_unref (child);
    }
}

static GtkWidget *
builder_get_toplevel (GtkBuilder *builder)
{
  GSList *list, *walk;
  GtkWidget *window = NULL;

  list = gtk_builder_get_objects (builder);
  for (walk = list; walk; walk = walk->next)
    {
      if (GTK_IS_WINDOW (walk->data) &&
          gtk_widget_get_parent (walk->data) == NULL)
        {
          window = walk->data;
          break;
        }
    }

  g_slist_free (list);

  return window;
}

static void
populate_list (GtkBuilder *builder)
{
  GtkTreeView *tv;
  GtkListStore *store;
  GtkTreeIter iter;
  gint i;

  tv = (GtkTreeView *)gtk_builder_get_object (builder, "treeview1");
  store = (GtkListStore *)gtk_tree_view_get_model (tv);

  /* append a many rows */
  for (i = 0; i < N_ROWS; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, "Bla", 1, "Bla bla", 2, "Bla bla bla", 3, i % 2 == 0 ? TRUE : FALSE, 4, i % 100, 5, i, -1);
    }
}

static void
test_performance_list (void)
{
  GtkBuilder *builder;
  gdouble elapsed;
  GtkWidget *window;
  GError *error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, list_ui, -1, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_assert (window);

  gtk_widget_show (window);

  g_test_timer_start ();

  populate_list (builder);

  elapsed = g_test_timer_elapsed ();
  g_test_minimized_result (elapsed, "large list test: %gsec", elapsed);
  g_object_unref (builder);
}

static void
test_a11y_performance_list (void)
{
  GtkBuilder *builder;
  gdouble elapsed;
  GtkWidget *window;
  GError *error = NULL;
  gint count_before;
  gint count_after;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, list_ui, -1, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_assert (window);

  gtk_widget_show (window);

  g_test_timer_start ();

  /* make sure all accessibles exist */
  count_before = 0;
  walk_accessible_tree (gtk_widget_get_accessible (window), &count_before);

  populate_list (builder);

  /* for good measure, do this again */
  count_after = 0;
  walk_accessible_tree (gtk_widget_get_accessible (window), &count_after);

  elapsed = g_test_timer_elapsed ();
  g_test_minimized_result (elapsed, "large list with a11y: %gsec", elapsed);
  g_object_unref (builder);

  g_test_message ("%d accessibles before, %d after\n", count_before, count_after);
}

const gchar tree_ui[] =
  "<interface>"
  "  <object class='GtkTreeStore' id='treestore1'>"
  "    <columns>"
  "      <column type='gchararray'/>"
  "      <column type='gchararray'/>"
  "      <column type='gchararray'/>"
  "      <column type='gboolean'/>"
  "      <column type='gint'/>"
  "      <column type='gint'/>"
  "    </columns>"
  "  </object>"
  "  <object class='GtkWindow' id='window1'>"
  "    <child>"
  "      <object class='GtkTreeView' id='treeview1'>"
  "        <property name='visible'>True</property>"
  "        <property name='model'>treestore1</property>"
  "        <child>"
  "          <object class='GtkTreeViewColumn' id='column1'>"
  "            <property name='title' translatable='yes'>First column</property>"
  "            <child>"
  "              <object class='GtkCellRendererText' id='renderer1'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='text'>0</attribute>"
  "              </attributes>"
  "            </child>"
  "            <child>"
  "              <object class='GtkCellRendererToggle' id='renderer2'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='active'>3</attribute>"
  "              </attributes>"
  "            </child>"
  "          </object>"
  "        </child>"
  "        <child>"
  "          <object class='GtkTreeViewColumn' id='column2'>"
  "            <property name='title' translatable='yes'>Second column</property>"
  "            <child>"
  "              <object class='GtkCellRendererText' id='renderer3'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='text'>1</attribute>"
  "              </attributes>"
  "            </child>"
  "            <child>"
  "              <object class='GtkCellRendererProgress' id='renderer4'>"
  "              </object>"
  "              <attributes>"
  "                <attribute name='value'>4</attribute>"
  "              </attributes>"
  "            </child>"
  "          </object>"
  "        </child>"
  "      </object>"
  "    </child>"
  "  </object>"
  "</interface>";

static void
populate_tree (GtkBuilder *builder)
{
  GtkTreeView *tv;
  GtkTreeStore *store;
  GtkTreeIter iter;
  gint i;

  tv = (GtkTreeView *)gtk_builder_get_object (builder, "treeview1");
  store = (GtkTreeStore *)gtk_tree_view_get_model (tv);

  /* append a thousand rows */
  for (i = 0; i < N_ROWS / 3; i++)
    {
      gtk_tree_store_append (store, &iter, NULL);
      gtk_tree_store_set (store, &iter, 0, "Bla", 1, "Bla bla", 2, "Bla bla bla", 3, i % 2 == 0 ? TRUE : FALSE, 4, i % 100, 5, i, -1);
      gtk_tree_store_append (store, &iter, &iter);
      gtk_tree_store_set (store, &iter, 0, "Bla", 1, "Bla bla", 2, "Bla bla bla", 3, i % 2 == 0 ? TRUE : FALSE, 4, i % 100, 5, i, -1);
      gtk_tree_store_append (store, &iter, &iter);
      gtk_tree_store_set (store, &iter, 0, "Bla", 1, "Bla bla", 2, "Bla bla bla", 3, i % 2 == 0 ? TRUE : FALSE, 4, i % 100, 5, i, -1);
    }

  gtk_tree_view_expand_all (tv);
}

static void
test_performance_tree (void)
{
  GtkBuilder *builder;
  gdouble elapsed;
  GtkWidget *window;
  GError *error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, tree_ui, -1, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_assert (window);

  gtk_widget_show (window);

  g_test_timer_start ();

  populate_tree (builder);

  elapsed = g_test_timer_elapsed ();
  g_test_minimized_result (elapsed, "large tree test: %gsec", elapsed);
  g_object_unref (builder);
}

static void
test_a11y_performance_tree (void)
{
  GtkBuilder *builder;
  gdouble elapsed;
  GtkWidget *window;
  GError *error = NULL;
  gint count_before;
  gint count_after;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, tree_ui, -1, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_assert (window);

  gtk_widget_show (window);

  g_test_timer_start ();

  /* make sure all accessibles exist */
  count_before = 0;
  walk_accessible_tree (gtk_widget_get_accessible (window), &count_before);

  populate_tree (builder);

  /* for good measure, do this again */
  count_after = 0;
  walk_accessible_tree (gtk_widget_get_accessible (window), &count_after);

  elapsed = g_test_timer_elapsed ();
  g_test_minimized_result (elapsed, "large tree with a11y: %gsec", elapsed);
  g_object_unref (builder);

  g_test_message ("%d accessibles before, %d after\n", count_before, count_after);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  if (!g_test_perf ())
    return 0;

  g_test_add_func ("/performance/list", test_performance_list);
  g_test_add_func ("/a11y/performance/list", test_a11y_performance_list);
  g_test_add_func ("/performance/tree", test_performance_tree);
  g_test_add_func ("/a11y/performance/tree", test_a11y_performance_tree);

  return g_test_run ();
}
