/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gtkrangeaccessible.h"
#include "gailadjustment.h"


static void atk_action_interface_init (AtkActionIface *iface);
static void atk_value_interface_init  (AtkValueIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkRangeAccessible, gtk_range_accessible, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gtk_range_accessible_value_changed (GtkAdjustment *adjustment,
                                    gpointer       data)
{
  g_object_notify (G_OBJECT (data), "accessible-value");
}

static void
gtk_range_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (obj);
  GtkAdjustment *adj;
  GtkRange *gtk_range;

  ATK_OBJECT_CLASS (gtk_range_accessible_parent_class)->initialize (obj, data);

  gtk_range = GTK_RANGE (data);
  /*
   * If a GtkAdjustment already exists for the GtkRange,
   * create the GailAdjustment
   */
  adj = gtk_range_get_adjustment (gtk_range);
  if (adj)
    {
      range->adjustment = gail_adjustment_new (adj);
      g_signal_connect (adj,
                        "value-changed",
                        G_CALLBACK (gtk_range_accessible_value_changed),
                        range);
    }
  else
    range->adjustment = NULL;

  obj->role = ATK_ROLE_SLIDER;
}

static void
gtk_range_accessible_finalize (GObject *object)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (object);

  if (range->adjustment)
    {
      /* The GtkAdjustment may live on so we need to disconnect
       * the signal handler
       */
      if (GAIL_ADJUSTMENT (range->adjustment)->adjustment)
        g_signal_handlers_disconnect_by_func (GAIL_ADJUSTMENT (range->adjustment)->adjustment,
                                              (void *)gtk_range_accessible_value_changed,
                                              range);

      g_object_unref (range->adjustment);
      range->adjustment = NULL;
    }

  if (range->action_idle_handler)
    {
      g_source_remove (range->action_idle_handler);
      range->action_idle_handler = 0;
    }

  G_OBJECT_CLASS (gtk_range_accessible_parent_class)->finalize (object);
}

static void
gtk_range_accessible_notify_gtk (GObject    *obj,
                                 GParamSpec *pspec)
{
  GtkAdjustment *adj;
  GtkWidget *widget = GTK_WIDGET (obj);
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      /* Get rid of the GailAdjustment for the GtkAdjustment
       * which was associated with the range.
       */
      if (range->adjustment)
        {
          g_object_unref (range->adjustment);
          range->adjustment = NULL;
        }

      /* Create the GailAdjustment when notify for "adjustment" property
       * is received
       */
      adj = gtk_range_get_adjustment (GTK_RANGE (widget));
      range->adjustment = gail_adjustment_new (adj);
      g_signal_connect (adj,
                        "value-changed",
                        G_CALLBACK (gtk_range_accessible_value_changed),
                        range);
    }
  else
    GAIL_WIDGET_CLASS (gtk_range_accessible_parent_class)->notify_gtk (obj, pspec);
}


static void
gtk_range_accessible_class_init (GtkRangeAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class = (GailWidgetClass*)klass;

  widget_class->notify_gtk = gtk_range_accessible_notify_gtk;

  class->initialize = gtk_range_accessible_initialize;

  gobject_class->finalize = gtk_range_accessible_finalize;
}

static void
gtk_range_accessible_init (GtkRangeAccessible *range)
{
}

static void
gtk_range_accessible_get_current_value (AtkValue *obj,
                                        GValue   *value)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (obj);

  if (range->adjustment == NULL)
    return;

  atk_value_get_current_value (ATK_VALUE (range->adjustment), value);
}

static void
gtk_range_accessible_get_maximum_value (AtkValue *obj,
                                        GValue   *value)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (obj);
  GtkRange *gtk_range;
  GtkAdjustment *gtk_adjustment;
  gdouble max = 0;

  if (range->adjustment == NULL)
    return;

  atk_value_get_maximum_value (ATK_VALUE (range->adjustment), value);

  gtk_range = GTK_RANGE (gtk_accessible_get_widget (GTK_ACCESSIBLE (range)));

  gtk_adjustment = gtk_range_get_adjustment (gtk_range);
  max = g_value_get_double (value);
  max -=  gtk_adjustment_get_page_size (gtk_adjustment);

  if (gtk_range_get_restrict_to_fill_level (gtk_range))
    max = MIN (max, gtk_range_get_fill_level (gtk_range));

  g_value_set_double (value, max);
}

static void
gtk_range_accessible_get_minimum_value (AtkValue *obj,
                                        GValue   *value)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (obj);

  if (range->adjustment == NULL)
    return;

  atk_value_get_minimum_value (ATK_VALUE (range->adjustment), value);
}

static void
gtk_range_accessible_get_minimum_increment (AtkValue *obj,
                                            GValue   *value)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (obj);

  if (range->adjustment == NULL)
    return;

  atk_value_get_minimum_increment (ATK_VALUE (range->adjustment), value);
}

static gboolean
gtk_range_accessible_set_current_value (AtkValue     *obj,
                                        const GValue *value)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return FALSE;

  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      GtkRange *range = GTK_RANGE (widget);
      gdouble new_value;

      new_value = g_value_get_double (value);
      gtk_range_set_value (range, new_value);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gtk_range_accessible_get_current_value;
  iface->get_maximum_value = gtk_range_accessible_get_maximum_value;
  iface->get_minimum_value = gtk_range_accessible_get_minimum_value;
  iface->get_minimum_increment = gtk_range_accessible_get_minimum_increment;
  iface->set_current_value = gtk_range_accessible_set_current_value;
}

static gboolean
idle_do_action (gpointer data)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (data);
  GtkWidget *widget;

  range->action_idle_handler = 0;
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (range));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  gtk_widget_activate (widget);

  return TRUE;
}

static gboolean
gtk_range_accessible_do_action (AtkAction *action,
                                gint       i)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (action);
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  if (i != 0)
    return FALSE;

  if (range->action_idle_handler)
    return FALSE;

  range->action_idle_handler = gdk_threads_add_idle (idle_do_action, range);

  return TRUE;
}

static gint
gtk_range_accessible_get_n_actions (AtkAction *action)
{
    return 1;
}

static const gchar *
gtk_range_accessible_get_keybinding (AtkAction *action,
                                     gint       i)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (action);
  GtkWidget *widget;
  GtkWidget *label;
  AtkRelationSet *set;
  AtkRelation *relation;
  GPtrArray *target;
  gpointer target_object;
  guint key_val;
  gchar *return_value = NULL;

  if (i != 0)
    return NULL;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (range));
  if (widget == NULL)
   return NULL;

  set = atk_object_ref_relation_set (ATK_OBJECT (action));

  if (!set)
    return NULL;

  label = NULL;
  relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
  if (relation)
    {
      target = atk_relation_get_target (relation);
      target_object = g_ptr_array_index (target, 0);
      label = gtk_accessible_get_widget (GTK_ACCESSIBLE (target_object));
    }
  g_object_unref (set);

  if (GTK_IS_LABEL (label))
    {
      key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
      if (key_val != GDK_KEY_VoidSymbol)
         return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
    }

  return return_value;
}

static const gchar *
gtk_range_accessible_action_get_name (AtkAction *action,
                                      gint       i)
{
  if (i != 0)
    return NULL;

  return "activate";
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_range_accessible_do_action;
  iface->get_n_actions = gtk_range_accessible_get_n_actions;
  iface->get_keybinding = gtk_range_accessible_get_keybinding;
  iface->get_name = gtk_range_accessible_action_get_name;
}
