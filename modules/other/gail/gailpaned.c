/* GAIL - The GNOME Accessibility Enabling Library
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
#include "gailpaned.h"

static void         gail_paned_class_init          (GailPanedClass *klass); 

static void         gail_paned_init                (GailPaned      *paned);

static void         gail_paned_real_initialize     (AtkObject      *obj,
                                                    gpointer       data);
static void         gail_paned_size_allocate_gtk   (GtkWidget      *widget,
                                                    GtkAllocation  *allocation);
static void         atk_value_interface_init       (AtkValueIface  *iface);
static void         gail_paned_get_current_value   (AtkValue       *obj,
                                                    GValue         *value);
static void         gail_paned_get_maximum_value   (AtkValue       *obj,
                                                    GValue         *value);
static void         gail_paned_get_minimum_value   (AtkValue       *obj,
                                                    GValue         *value);
static gboolean     gail_paned_set_current_value   (AtkValue       *obj,
                                                    const GValue   *value);

G_DEFINE_TYPE_WITH_CODE (GailPaned, gail_paned, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gail_paned_class_init (GailPanedClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_paned_real_initialize;
}

static void
gail_paned_init (GailPaned *paned)
{
}

static void
gail_paned_real_initialize (AtkObject *obj,
                            gpointer  data)
{
  ATK_OBJECT_CLASS (gail_paned_parent_class)->initialize (obj, data);

  g_signal_connect (data,
                    "size_allocate",
                    G_CALLBACK (gail_paned_size_allocate_gtk),
                    NULL);

  obj->role = ATK_ROLE_SPLIT_PANE;
}
 
static void
gail_paned_size_allocate_gtk (GtkWidget      *widget,
                              GtkAllocation  *allocation)
{
  AtkObject *obj = gtk_widget_get_accessible (widget);

  g_object_notify (G_OBJECT (obj), "accessible-value");
}


static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gail_paned_get_current_value;
  iface->get_maximum_value = gail_paned_get_maximum_value;
  iface->get_minimum_value = gail_paned_get_minimum_value;
  iface->set_current_value = gail_paned_set_current_value;
}

static void
gail_paned_get_current_value (AtkValue             *obj,
                              GValue               *value)
{
  GtkWidget* widget;
  gint current_value;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    /* State is defunct */
    return;

  current_value = gtk_paned_get_position (GTK_PANED (widget));
  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value,current_value);
}

static void
gail_paned_get_maximum_value (AtkValue             *obj,
                              GValue               *value)
{
  GtkWidget* widget;
  gint maximum_value;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    /* State is defunct */
    return;

  g_object_get (GTK_PANED (widget),
                "max-position", &maximum_value,
                NULL);
  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, maximum_value);
}

static void
gail_paned_get_minimum_value (AtkValue             *obj,
                              GValue               *value)
{
  GtkWidget* widget;
  gint minimum_value;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    /* State is defunct */
    return;

  g_object_get (GTK_PANED (widget),
                "min-position", &minimum_value,
                NULL);
  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, minimum_value);
}

/*
 * Calling atk_value_set_current_value() is no guarantee that the value is
 * acceptable; it is necessary to listen for accessible-value signals
 * and check whether the current value has been changed or check what the 
 * maximum and minimum values are.
 */

static gboolean
gail_paned_set_current_value (AtkValue             *obj,
                              const GValue         *value)
{
  GtkWidget* widget;
  gint new_value;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  if (G_VALUE_HOLDS_INT (value))
    {
      new_value = g_value_get_int (value);
      gtk_paned_set_position (GTK_PANED (widget), new_value);

      return TRUE;
    }
  else
    return FALSE;
}
