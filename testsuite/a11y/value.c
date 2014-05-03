/*
 * Copyright (C) 2011 Red Hat Inc.
 * Copyright (C) 2012 SUSE LLC.
 *
 * Author:
 *      Mike Gorse <mgorse@suse.com>
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

static void
set_value (GtkWidget   *widget,
           gint value)
{
  if (GTK_IS_LEVEL_BAR (widget))
    gtk_level_bar_set_value (GTK_LEVEL_BAR (widget), value);
  else if (GTK_IS_RANGE (widget))
    {
      GtkAdjustment *adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
      gtk_adjustment_set_value (adjustment, value);
    }
  else if (GTK_IS_SPIN_BUTTON (widget))
    {
      GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
      gtk_adjustment_set_value (adjustment, value);
    }
}

typedef struct
{
  gint count;
  gchar *last_name;
} NotifyData;

static void
notify_cb (GObject    *obj,
           GParamSpec *pspec, NotifyData *data)
{
  data->count++;
  if (data->last_name)
    g_free (data->last_name);
  data->last_name = g_strdup (pspec->name);
}

static void
test_basic (GtkWidget *widget)
{
  NotifyData notify_data;
  AtkObject *atk_object;
  AtkValue *atk_value;
  gdouble value = 50;
  gdouble ret;
  gchar *text;

  atk_object = gtk_widget_get_accessible (widget);
  atk_value = ATK_VALUE (atk_object);

  memset (&notify_data, 0, sizeof (notify_data));
  g_signal_connect (atk_object, "notify", G_CALLBACK (notify_cb), &notify_data);
  set_value (widget, value);
  g_assert_cmpint (notify_data.count, ==, 1);
  g_assert_cmpstr (notify_data.last_name, ==, "accessible-value");

  text = NULL;
  atk_value_get_value_and_text (atk_value, &ret, &text);
  g_assert_cmpfloat (ret, ==, value);
  g_free (text);

  g_free (notify_data.last_name);
  g_signal_handlers_disconnect_by_func (atk_object, G_CALLBACK (notify_cb), &notify_data);
}

static void
setup_test (GtkWidget *widget)
{
  set_value (widget, 0);
}

static void
add_value_test (const gchar      *prefix,
               GTestFixtureFunc  test_func,
               GtkWidget        *widget)
{
  gchar *path;

  path = g_strdup_printf ("%s/%s", prefix, G_OBJECT_TYPE_NAME (widget));
  g_test_add_vtable (path,
                     0,
                     g_object_ref (widget),
                     (GTestFixtureFunc) setup_test,
                     (GTestFixtureFunc) test_func,
                     (GTestFixtureFunc) g_object_unref);
  g_free (path);
}

static void
add_value_tests (GtkWidget *widget)
{
  g_object_ref_sink (widget);
  add_value_test ("/value/basic", (GTestFixtureFunc) test_basic, widget);
  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  add_value_tests (gtk_spin_button_new_with_range (0, 100, 1));
  add_value_tests (gtk_level_bar_new_for_interval (0, 100));

  return g_test_run ();
}
