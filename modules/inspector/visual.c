/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "visual.h"

struct _GtkInspectorVisualPrivate
{
  GtkWidget *updates_switch;
  GtkWidget *direction_combo;
  GtkWidget *baselines_switch;
  GtkWidget *pixelcache_switch;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorVisual, gtk_inspector_visual, GTK_TYPE_LIST_BOX)

static void
updates_activate (GtkSwitch *sw)
{
  gdk_window_set_debug_updates (gtk_switch_get_active (sw));
}

static void
fix_direction_recurse (GtkWidget *widget, gpointer data)
{
  GtkTextDirection dir = GPOINTER_TO_INT (data);

  g_object_ref (widget);

  gtk_widget_set_direction (widget, dir);
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), fix_direction_recurse, data);

  g_object_unref (widget);
}

static GtkTextDirection initial_direction;

static void
fix_direction (GtkWidget *iw)
{
  fix_direction_recurse (iw, GINT_TO_POINTER (initial_direction));
}

static void
direction_changed (GtkComboBox *combo)
{
  GtkWidget *iw;
  const gchar *direction;

  iw = gtk_widget_get_toplevel (GTK_WIDGET (combo));
  fix_direction (iw);

  direction = gtk_combo_box_get_active_id (combo);
  if (g_strcmp0 (direction, "ltr") == 0)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
  else
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
}

static void
init_direction (GtkInspectorVisual *vis)
{
  const gchar *direction;

  initial_direction = gtk_widget_get_default_direction ();
  if (initial_direction == GTK_TEXT_DIR_LTR)
    direction = "ltr";
  else
    direction = "rtl";
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (vis->priv->direction_combo), direction);
}

static void
baselines_activate (GtkSwitch *sw)
{
  guint flags;

  flags = gtk_get_debug_flags ();

  if (gtk_switch_get_active (sw))
    flags |= GTK_DEBUG_BASELINES;
  else
    flags &= ~GTK_DEBUG_BASELINES;

  gtk_set_debug_flags (flags);
}

static void
pixelcache_activate (GtkSwitch *sw)
{
  guint flags;

  flags = gtk_get_debug_flags ();

  if (gtk_switch_get_active (sw))
    flags |= GTK_DEBUG_PIXEL_CACHE;
  else
    flags &= ~GTK_DEBUG_PIXEL_CACHE;

  gtk_set_debug_flags (flags);
}

static void
gtk_inspector_visual_init (GtkInspectorVisual *pt)
{
  pt->priv = gtk_inspector_visual_get_instance_private (pt);
  gtk_widget_init_template (GTK_WIDGET (pt));

  init_direction (pt);
}

static void
gtk_inspector_visual_class_init (GtkInspectorVisualClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/visual.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, updates_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, direction_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, baselines_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, pixelcache_switch);
  gtk_widget_class_bind_template_callback (widget_class, updates_activate);
  gtk_widget_class_bind_template_callback (widget_class, direction_changed);
  gtk_widget_class_bind_template_callback (widget_class, baselines_activate);
  gtk_widget_class_bind_template_callback (widget_class, pixelcache_activate);
}

GtkWidget *
gtk_inspector_visual_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_VISUAL, NULL));
}

// vim: set et sw=2 ts=2:
