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
#include "gailtogglebutton.h"

static void      gail_toggle_button_class_init        (GailToggleButtonClass *klass);

static void      gail_toggle_button_init              (GailToggleButton      *button);

static void      gail_toggle_button_toggled_gtk       (GtkWidget             *widget);

static void      gail_toggle_button_real_notify_gtk   (GObject               *obj,
                                                       GParamSpec            *pspec);

static void      gail_toggle_button_real_initialize   (AtkObject             *obj,
                                                       gpointer              data);

static AtkStateSet* gail_toggle_button_ref_state_set  (AtkObject             *accessible);

G_DEFINE_TYPE (GailToggleButton, gail_toggle_button, GAIL_TYPE_BUTTON)

static void
gail_toggle_button_class_init (GailToggleButtonClass *klass)
{
  GailWidgetClass *widget_class;
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  widget_class = (GailWidgetClass*)klass;
  widget_class->notify_gtk = gail_toggle_button_real_notify_gtk;

  class->ref_state_set = gail_toggle_button_ref_state_set;
  class->initialize = gail_toggle_button_real_initialize;
}

static void
gail_toggle_button_init (GailToggleButton *button)
{
}

static void
gail_toggle_button_real_initialize (AtkObject *obj,
                                    gpointer  data)
{
  ATK_OBJECT_CLASS (gail_toggle_button_parent_class)->initialize (obj, data);

  g_signal_connect (data,
                    "toggled",
                    G_CALLBACK (gail_toggle_button_toggled_gtk),
                    NULL);

  if (GTK_IS_CHECK_BUTTON (data))
    obj->role = ATK_ROLE_CHECK_BOX;
  else
    obj->role = ATK_ROLE_TOGGLE_BUTTON;
}

static void
gail_toggle_button_toggled_gtk (GtkWidget       *widget)
{
  AtkObject *accessible;
  GtkToggleButton *toggle_button;

  toggle_button = GTK_TOGGLE_BUTTON (widget);

  accessible = gtk_widget_get_accessible (widget);
  atk_object_notify_state_change (accessible, ATK_STATE_CHECKED, 
                                  toggle_button->active);
} 

static AtkStateSet*
gail_toggle_button_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkToggleButton *toggle_button;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gail_toggle_button_parent_class)->ref_state_set (accessible);
  widget = GTK_ACCESSIBLE (accessible)->widget;
 
  if (widget == NULL)
    return state_set;

  toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (gtk_toggle_button_get_active (toggle_button))
    atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

  if (gtk_toggle_button_get_inconsistent (toggle_button))
    {
      atk_state_set_remove_state (state_set, ATK_STATE_ENABLED);
      atk_state_set_add_state (state_set, ATK_STATE_INDETERMINATE);
    }
 
  return state_set;
}

static void
gail_toggle_button_real_notify_gtk (GObject           *obj,
                                    GParamSpec        *pspec)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (obj);
  AtkObject *atk_obj;
  gboolean sensitive;
  gboolean inconsistent;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (toggle_button));
  sensitive = gtk_widget_get_sensitive (GTK_WIDGET (toggle_button));
  inconsistent = gtk_toggle_button_get_inconsistent (toggle_button);

  if (strcmp (pspec->name, "inconsistent") == 0)
    {
      atk_object_notify_state_change (atk_obj, ATK_STATE_INDETERMINATE, inconsistent);
      atk_object_notify_state_change (atk_obj, ATK_STATE_ENABLED, (sensitive && !inconsistent));
    }
  else if (strcmp (pspec->name, "sensitive") == 0)
    {
      /* Need to override gailwidget behavior of notifying for ENABLED */
      atk_object_notify_state_change (atk_obj, ATK_STATE_SENSITIVE, sensitive);
      atk_object_notify_state_change (atk_obj, ATK_STATE_ENABLED, (sensitive && !inconsistent));
    }
  else
    GAIL_WIDGET_CLASS (gail_toggle_button_parent_class)->notify_gtk (obj, pspec);
}
