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
#include "gailrange.h"
#include "gailadjustment.h"
#include "gail-private-macros.h"

static void	    gail_range_class_init        (GailRangeClass *klass);

static void         gail_range_init              (GailRange      *range);

static void         gail_range_real_initialize   (AtkObject      *obj,
                                                  gpointer      data);

static void         gail_range_finalize          (GObject        *object);

static AtkStateSet* gail_range_ref_state_set     (AtkObject      *obj);


static void         gail_range_real_notify_gtk   (GObject        *obj,
                                                  GParamSpec     *pspec);

static void	    atk_value_interface_init	 (AtkValueIface  *iface);
static void	    gail_range_get_current_value (AtkValue       *obj,
                                                  GValue         *value);
static void	    gail_range_get_maximum_value (AtkValue       *obj,
                                                  GValue         *value);
static void	    gail_range_get_minimum_value (AtkValue       *obj,
                                                  GValue         *value);
static void         gail_range_get_minimum_increment (AtkValue       *obj,
                                                      GValue         *value);
static gboolean	    gail_range_set_current_value (AtkValue       *obj,
                                                  const GValue   *value);
static void         gail_range_value_changed     (GtkAdjustment  *adjustment,
                                                  gpointer       data);

static void         atk_action_interface_init    (AtkActionIface *iface);
static gboolean     gail_range_do_action        (AtkAction       *action,
                                                gint            i);
static gboolean     idle_do_action              (gpointer        data);
static gint         gail_range_get_n_actions    (AtkAction       *action);
static const gchar* gail_range_get_description  (AtkAction    *action,
                                                         gint          i);
static const gchar* gail_range_get_keybinding   (AtkAction     *action,
                                                         gint            i);
static const gchar* gail_range_action_get_name  (AtkAction    *action,
                                                        gint            i);
static gboolean   gail_range_set_description  (AtkAction       *action,
                                              gint            i,
                                              const gchar     *desc);

G_DEFINE_TYPE_WITH_CODE (GailRange, gail_range, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void	 
gail_range_class_init		(GailRangeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;

  widget_class->notify_gtk = gail_range_real_notify_gtk;

  class->ref_state_set = gail_range_ref_state_set;
  class->initialize = gail_range_real_initialize;

  gobject_class->finalize = gail_range_finalize;
}

static void
gail_range_init (GailRange      *range)
{
}

static void
gail_range_real_initialize (AtkObject *obj,
                            gpointer  data)
{
  GailRange *range = GAIL_RANGE (obj);
  GtkRange *gtk_range;

  ATK_OBJECT_CLASS (gail_range_parent_class)->initialize (obj, data);

  gtk_range = GTK_RANGE (data);
  /*
   * If a GtkAdjustment already exists for the GtkRange,
   * create the GailAdjustment
   */
  if (gtk_range->adjustment)
    {
      range->adjustment = gail_adjustment_new (gtk_range->adjustment);
      g_signal_connect (gtk_range->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_range_value_changed),
                        range);
    }
  else
    range->adjustment = NULL;
  range->activate_keybinding=NULL;
  range->activate_description=NULL;
  /*
   * Assumed to GtkScale (either GtkHScale or GtkVScale)
   */
  obj->role = ATK_ROLE_SLIDER;
}

static AtkStateSet*
gail_range_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;
  GtkRange *range;

  state_set = ATK_OBJECT_CLASS (gail_range_parent_class)->ref_state_set (obj);
  widget = GTK_ACCESSIBLE (obj)->widget;

  if (widget == NULL)
    return state_set;

  range = GTK_RANGE (widget);

  /*
   * We do not generate property change for orientation change as there
   * is no interface to change the orientation which emits a notification
   */
  if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
    atk_state_set_add_state (state_set, ATK_STATE_HORIZONTAL);
  else
    atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);

  return state_set;
}

static void	 
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gail_range_get_current_value;
  iface->get_maximum_value = gail_range_get_maximum_value;
  iface->get_minimum_value = gail_range_get_minimum_value;
  iface->get_minimum_increment = gail_range_get_minimum_increment;
  iface->set_current_value = gail_range_set_current_value;
}

static void	 
gail_range_get_current_value (AtkValue		*obj,
                              GValue		*value)
{
  GailRange *range;

  g_return_if_fail (GAIL_IS_RANGE (obj));

  range = GAIL_RANGE (obj);
  if (range->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_current_value (ATK_VALUE (range->adjustment), value);
}

static void	 
gail_range_get_maximum_value (AtkValue		*obj,
                              GValue		*value)
{
  GailRange *range;
  GtkRange *gtk_range;
  GtkAdjustment *gtk_adjustment;
  gdouble max = 0;

  g_return_if_fail (GAIL_IS_RANGE (obj));

  range = GAIL_RANGE (obj);
  if (range->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;
 
  atk_value_get_maximum_value (ATK_VALUE (range->adjustment), value);

  gtk_range = GTK_RANGE (gtk_accessible_get_widget (GTK_ACCESSIBLE (range)));
  g_return_if_fail (gtk_range);

  gtk_adjustment = gtk_range_get_adjustment (gtk_range);
  max = g_value_get_double (value);
  max -=  gtk_adjustment_get_page_size (gtk_adjustment);

  if (gtk_range_get_restrict_to_fill_level (gtk_range))
    max = MIN (max, gtk_range_get_fill_level (gtk_range));

  g_value_set_double (value, max);
}

static void	 
gail_range_get_minimum_value (AtkValue		*obj,
                              GValue		*value)
{
  GailRange *range;

  g_return_if_fail (GAIL_IS_RANGE (obj));

  range = GAIL_RANGE (obj);
  if (range->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_value (ATK_VALUE (range->adjustment), value);
}

static void
gail_range_get_minimum_increment (AtkValue *obj, GValue *value)
{
 GailRange *range;

  g_return_if_fail (GAIL_IS_RANGE (obj));

  range = GAIL_RANGE (obj);
  if (range->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_increment (ATK_VALUE (range->adjustment), value);
}

static gboolean	 gail_range_set_current_value (AtkValue		*obj,
                                               const GValue	*value)
{
  GtkWidget *widget;

  g_return_val_if_fail (GAIL_IS_RANGE (obj), FALSE);

  widget = GTK_ACCESSIBLE (obj)->widget;
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
gail_range_finalize (GObject            *object)
{
  GailRange *range = GAIL_RANGE (object);

  if (range->adjustment)
    {
      /*
       * The GtkAdjustment may live on so we need to dicsonnect the
       * signal handler
       */
      if (GAIL_ADJUSTMENT (range->adjustment)->adjustment)
        {
          g_signal_handlers_disconnect_by_func (GAIL_ADJUSTMENT (range->adjustment)->adjustment,
                                                (void *)gail_range_value_changed,
                                                range);
        }
      g_object_unref (range->adjustment);
      range->adjustment = NULL;
    }
  range->activate_keybinding=NULL;
  range->activate_description=NULL;
  if (range->action_idle_handler)
   {
    g_source_remove (range->action_idle_handler);
    range->action_idle_handler = 0;
   }

  G_OBJECT_CLASS (gail_range_parent_class)->finalize (object);
}


static void
gail_range_real_notify_gtk (GObject           *obj,
                            GParamSpec        *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GailRange *range = GAIL_RANGE (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      /*
       * Get rid of the GailAdjustment for the GtkAdjustment
       * which was associated with the range.
       */
      if (range->adjustment)
        {
          g_object_unref (range->adjustment);
          range->adjustment = NULL;
        }
      /*
       * Create the GailAdjustment when notify for "adjustment" property
       * is received
       */
      range->adjustment = gail_adjustment_new (GTK_RANGE (widget)->adjustment);
      g_signal_connect (GTK_RANGE (widget)->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_range_value_changed),
                        range);
    }
  else
    GAIL_WIDGET_CLASS (gail_range_parent_class)->notify_gtk (obj, pspec);
}

static void
gail_range_value_changed (GtkAdjustment    *adjustment,
                          gpointer         data)
{
  GailRange *range;

  g_return_if_fail (adjustment != NULL);
  gail_return_if_fail (data != NULL);

  range = GAIL_RANGE (data);

  g_object_notify (G_OBJECT (range), "accessible-value");
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_range_do_action;
  iface->get_n_actions = gail_range_get_n_actions;
  iface->get_description = gail_range_get_description;
  iface->get_keybinding = gail_range_get_keybinding;
  iface->get_name = gail_range_action_get_name;
  iface->set_description = gail_range_set_description;
}

static gboolean
gail_range_do_action (AtkAction *action,
                     gint      i)
{
  GailRange *range;
  GtkWidget *widget;
  gboolean return_value = TRUE;

  range = GAIL_RANGE (action);
  widget = GTK_ACCESSIBLE (action)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;
  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;
  if(i==0)
   {
    if (range->action_idle_handler)
      return_value = FALSE;
    else
      range->action_idle_handler = gdk_threads_add_idle (idle_do_action, range);
   }
  else
     return_value = FALSE;
  return return_value;
}

static gboolean
idle_do_action (gpointer data)
{
  GailRange *range;
  GtkWidget *widget;

  range = GAIL_RANGE (data);
  range->action_idle_handler = 0;
  widget = GTK_ACCESSIBLE (range)->widget;
  if (widget == NULL /* State is defunct */ ||
     !gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

   gtk_widget_activate (widget);

   return FALSE;
}

static gint
gail_range_get_n_actions (AtkAction *action)
{
    return 1;
}

static const gchar*
gail_range_get_description (AtkAction *action,
                              gint      i)
{
  GailRange *range;
  const gchar *return_value;

  range = GAIL_RANGE (action);
  if (i==0)
   return_value = range->activate_description;
  else
   return_value = NULL;
  return return_value;
}

static const gchar*
gail_range_get_keybinding (AtkAction *action,
                              gint      i)
{
  GailRange *range;
  gchar *return_value = NULL;
  range = GAIL_RANGE (action);
  if(i==0)
   {
    GtkWidget *widget;
    GtkWidget *label;
    AtkRelationSet *set;
    AtkRelation *relation;
    GPtrArray *target;
    gpointer target_object;
    guint key_val;

    range = GAIL_RANGE (action);
    widget = GTK_ACCESSIBLE (range)->widget;
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
      if (GTK_IS_ACCESSIBLE (target_object))
         label = GTK_ACCESSIBLE (target_object)->widget;
     }
    g_object_unref (set);
    if (GTK_IS_LABEL (label))
     {
      key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
      if (key_val != GDK_VoidSymbol)
         return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
      }
    g_free (range->activate_keybinding);
    range->activate_keybinding = return_value;
   }
  return return_value;
}

static const gchar*
gail_range_action_get_name (AtkAction *action,
                           gint      i)
{
  const gchar *return_value;
  
  if (i==0)
   return_value = "activate";
  else
   return_value = NULL;

  return return_value;
}

static gboolean
gail_range_set_description (AtkAction      *action,
                           gint           i,
                           const gchar    *desc)
{
  GailRange *range;
  gchar **value;

  range = GAIL_RANGE (action);
  
  if (i==0)
   value = &range->activate_description;
  else
   value = NULL;

  if (value)
   {
    g_free (*value);
    *value = g_strdup (desc);
    return TRUE;
   }
  else
   return FALSE;
}


