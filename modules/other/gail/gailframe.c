/* GAIL - The GNOME Accessibility Implementation Library
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

#include <string.h>
#include <gtk/gtk.h>
#include "gailframe.h"

static void                  gail_frame_class_init       (GailFrameClass  *klass);
static G_CONST_RETURN gchar* gail_frame_get_name         (AtkObject       *obj);

static gpointer parent_class = NULL;

GType
gail_frame_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailFrameClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_frame_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailFrame), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };

    type = g_type_register_static (GAIL_TYPE_CONTAINER,
                                   "GailFrame", &tinfo, 0);
  }

  return type;
}

static void
gail_frame_class_init (GailFrameClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_name = gail_frame_get_name;

  parent_class = g_type_class_peek_parent (klass);
}

AtkObject* 
gail_frame_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_FRAME (widget), NULL);

  object = g_object_new (GAIL_TYPE_FRAME, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  accessible->role = ATK_ROLE_PANEL;

  return accessible;
}

static G_CONST_RETURN gchar*
gail_frame_get_name (AtkObject *obj)
{
  G_CONST_RETURN gchar *name;
  g_return_val_if_fail (GAIL_IS_FRAME (obj), NULL);

  name = ATK_OBJECT_CLASS (parent_class)->get_name (obj);
  if (name != NULL)
  {
    return name;
  }
  else
  {
    /*
     * Get the text on the label
     */
    GtkWidget *widget;

    widget = GTK_ACCESSIBLE (obj)->widget;
    if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return NULL;
    }
    return gtk_frame_get_label (GTK_FRAME (widget));
  }
}
