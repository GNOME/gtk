/* crossing.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gtk/gtkprivate.h"

typedef struct
{
  GtkWidget                *widget;
  GtkEventControllerMotion *motion;
  guint                     enters;
  guint                     leaves;
} CrossingWidget;

static void
pointer_enter (GtkEventControllerMotion *controller,
               double                    x,
               double                    y,
               CrossingWidget           *data)
{
  data->enters++;
}

static void
pointer_leave (GtkEventControllerMotion *controller,
               CrossingWidget           *data)
{
  data->leaves++;
}

static void
test_pointer_crossing (void)
{
  CrossingWidget widgets[7] = { 0 };

  const int targets[] = { 3, 4, 2, 3, 6, -1, 1, 1, 4, -1 };
  GtkWidget *old_target = NULL;
  GtkWidget *window;

  window = gtk_window_new ();
  widgets[0].widget = window;
  widgets[1].widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  widgets[2].widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  widgets[3].widget = gtk_label_new ("First");
  widgets[4].widget = gtk_label_new ("Second");
  widgets[5].widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  widgets[6].widget = gtk_label_new ("Third");

  gtk_window_set_child (GTK_WINDOW (window), widgets[1].widget);
  gtk_box_append (GTK_BOX (widgets[1].widget), widgets[2].widget);
  gtk_box_append (GTK_BOX (widgets[1].widget), widgets[5].widget);
  gtk_box_append (GTK_BOX (widgets[2].widget), widgets[3].widget);
  gtk_box_append (GTK_BOX (widgets[2].widget), widgets[4].widget);
  gtk_box_append (GTK_BOX (widgets[5].widget), widgets[6].widget);

  for (guint i = 0; i < G_N_ELEMENTS (widgets); i++)
    {
      GtkEventController *controller = gtk_event_controller_motion_new ();

      widgets[i].motion = GTK_EVENT_CONTROLLER_MOTION (controller);
      gtk_widget_add_controller (widgets[i].widget, controller);
      g_signal_connect (controller, "enter", G_CALLBACK (pointer_enter), &widgets[i]);
      g_signal_connect (controller, "leave", G_CALLBACK (pointer_leave), &widgets[i]);
    }

  for (guint step = 0; step < G_N_ELEMENTS (targets); step++)
    {
      GtkWidget *target = targets[step] < 0 ? NULL : widgets[targets[step]].widget;

      for (guint i = 0; i < G_N_ELEMENTS (widgets); i++)
        {
          widgets[i].enters = 0;
          widgets[i].leaves = 0;
        }

      gtk_synthesize_crossing_events (GTK_ROOT (window), GTK_CROSSING_POINTER,
                                      old_target, target, 0, 0, GDK_CROSSING_NORMAL, NULL);

      for (guint i = 0; i < G_N_ELEMENTS (widgets); i++)
        {
          GtkWidget *widget = widgets[i].widget;
          gboolean contained = old_target &&
                               (old_target == widget || gtk_widget_is_ancestor (old_target, widget));
          gboolean contains = target &&
                              (target == widget || gtk_widget_is_ancestor (target, widget));

          g_assert_cmpint (!!(gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_PRELIGHT),
                           ==, contains);
          g_assert_cmpuint (widgets[i].enters, ==, !contained && contains);
          g_assert_cmpuint (widgets[i].leaves, ==, contained && !contains);
          g_assert_cmpint (gtk_event_controller_motion_contains_pointer (widgets[i].motion),
                           ==, contains);
          g_assert_cmpint (gtk_event_controller_motion_is_pointer (widgets[i].motion),
                           ==, target == widget);
        }

      old_target = target;
    }

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_check_button_crossing (void)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *indicator;
  GtkWidget *label;
  GtkWidget *old_target = NULL;
  GtkWidget *targets[5];

  window = gtk_window_new ();
  button = gtk_check_button_new_with_label ("Check button");
  gtk_window_set_child (GTK_WINDOW (window), button);
  indicator = gtk_widget_get_first_child (button);
  label = gtk_widget_get_next_sibling (indicator);
  g_assert_true (GTK_IS_LABEL (label));

  targets[0] = indicator;
  targets[1] = label;
  targets[2] = button;
  targets[3] = indicator;
  targets[4] = NULL;

  for (guint i = 0; i < G_N_ELEMENTS (targets); i++)
    {
      gtk_synthesize_crossing_events (GTK_ROOT (window), GTK_CROSSING_POINTER,
                                      old_target, targets[i], 0, 0, GDK_CROSSING_NORMAL, NULL);

      g_assert_cmpint (!!(gtk_widget_get_state_flags (button) & GTK_STATE_FLAG_PRELIGHT),
                       ==, targets[i] != NULL);
      g_assert_cmpint (!!(gtk_widget_get_state_flags (indicator) & GTK_STATE_FLAG_PRELIGHT),
                       ==, targets[i] != NULL);
      old_target = targets[i];
    }

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/crossing/pointer", test_pointer_crossing);
  g_test_add_func ("/crossing/check-button", test_check_button_crossing);

  return g_test_run ();
}
