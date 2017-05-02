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

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include "gailframe.h"

static void                  gail_frame_class_init       (GailFrameClass  *klass);
static void                  gail_frame_init             (GailFrame       *frame);
static void                  gail_frame_initialize       (AtkObject       *accessible,
                                                          gpointer         data);
static const gchar*          gail_frame_get_name         (AtkObject       *obj);

G_DEFINE_TYPE (GailFrame, gail_frame, GAIL_TYPE_CONTAINER)

static void
gail_frame_class_init (GailFrameClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_frame_initialize;
  class->get_name = gail_frame_get_name;
}

static void
gail_frame_init (GailFrame       *frame)
{
}

static void
gail_frame_initialize (AtkObject *accessible,
                       gpointer  data)
{
  ATK_OBJECT_CLASS (gail_frame_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_PANEL;
}

static const gchar*
gail_frame_get_name (AtkObject *obj)
{
  const gchar *name;
  g_return_val_if_fail (GAIL_IS_FRAME (obj), NULL);

  name = ATK_OBJECT_CLASS (gail_frame_parent_class)->get_name (obj);
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
