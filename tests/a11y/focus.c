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
#include <string.h>
const gchar data[] =
  "<interface>"
  "  <object class='GtkWindow' id='window1'>"
  "    <property name='visible'>True</property>"
  "    <child>"
  "      <object class='GtkBox' id='box1'>"
  "        <property name='visible'>True</property>"
  "        <child>"
  "          <object class='GtkEntry' id='entry1'>"
  "            <property name='visible'>True</property>"
  "            <property name='text'>entry1</property>"
  "          </object>"
  "        </child>"
  "        <child>"
  "          <object class='GtkEntry' id='entry2'>"
  "            <property name='visible'>True</property>"
  "            <property name='text'>entry2</property>"
  "          </object>"
  "        </child>"
  "      </object>"
  "    </child>"
  "  </object>"
  "</interface>";

static void
got_active (GObject *win, GParamSpec *pspec, gpointer data)
{
  gtk_main_quit ();
}

static void
test_focus_change (void)
{
  GtkBuilder *builder;
  GError *error;
  GtkWidget *window;
  GtkWidget *entry1;
  GtkWidget *entry2;
  AtkObject *wa;
  AtkObject *ea1;
  AtkObject *ea2;
  GtkWidget *focus;
  AtkStateSet *set;
  gboolean ret;

  builder = gtk_builder_new ();
  error = NULL;
  gtk_builder_add_from_string (builder, data, -1, &error);
  g_assert_no_error (error);
  window = (GtkWidget*)gtk_builder_get_object (builder, "window1");
  entry1 = (GtkWidget*)gtk_builder_get_object (builder, "entry1");
  entry2 = (GtkWidget*)gtk_builder_get_object (builder, "entry2");

  wa = gtk_widget_get_accessible (window);
  ea1 = gtk_widget_get_accessible (entry1);
  ea2 = gtk_widget_get_accessible (entry2);

#if 0
  g_signal_connect (window, "notify::is-active", G_CALLBACK (got_active), NULL);
  gtk_widget_show (window);
  gtk_main ();
  g_assert (gtk_window_is_active (GTK_WINDOW (window)));
#endif

  focus = gtk_window_get_focus (GTK_WINDOW (window));
  g_assert (focus == entry1);

  set = atk_object_ref_state_set (ea1);
  ret = atk_state_set_contains_state (set, ATK_STATE_FOCUSED);
  g_assert (ret);
  g_object_unref (set);
  set = atk_object_ref_state_set (ea2);
  ret = atk_state_set_contains_state (set, ATK_STATE_FOCUSED);
  g_assert (!ret);
  g_object_unref (set);

  gtk_widget_grab_focus (entry2);

  focus = gtk_window_get_focus (GTK_WINDOW (window));
  g_assert (focus == entry2);

  set = atk_object_ref_state_set (ea1);
  ret = atk_state_set_contains_state (set, ATK_STATE_FOCUSED);
  g_assert (!ret);
  g_object_unref (set);
  set = atk_object_ref_state_set (ea2);
  ret = atk_state_set_contains_state (set, ATK_STATE_FOCUSED);
  g_assert (ret);
  g_object_unref (set);

  g_object_unref (builder);
}


int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/focus/change", test_focus_change);

  return g_test_run ();
}

