/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gailbox.h"

static void         gail_box_class_init            (GailBoxClass  *klass); 
static AtkStateSet* gail_box_ref_state_set         (AtkObject     *accessible);

static GailContainerClass *parent_class = NULL;

GType
gail_box_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailBoxClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_box_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailBox), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };

    type = g_type_register_static (GAIL_TYPE_CONTAINER,
                                   "GailBox", &tinfo, 0);
  }
  return type;
}

static void
gail_box_class_init (GailBoxClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  class->ref_state_set = gail_box_ref_state_set;
}

AtkObject* 
gail_box_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_BOX (widget), NULL);

  object = g_object_new (GAIL_TYPE_BOX, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  accessible->role = ATK_ROLE_FILLER;

  return accessible;
}

static AtkStateSet*
gail_box_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (accessible);
  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (widget == NULL)
    return state_set;

  if (GTK_IS_VBOX (widget) || GTK_IS_VBUTTON_BOX (widget))
    atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);
  else if (GTK_IS_HBOX (widget) || GTK_IS_HBUTTON_BOX (widget))
    atk_state_set_add_state (state_set, ATK_STATE_HORIZONTAL);

  return state_set;
}
