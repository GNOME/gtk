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

#include <libgtkhtml/gtkhtml.h>
#include "gailhtmlboxembedded.h"

static void       gail_html_box_embedded_class_init      (GailHtmlBoxEmbeddedClass *klass);
static gint       gail_html_box_embedded_get_n_children  (AtkObject             *obj);
static AtkObject* gail_html_box_embedded_ref_child       (AtkObject             *obj,
                                                          gint                  i);

G_DEFINE_TYPE (GailHtmlBoxEmbedded, gail_html_box_embedded, GAIL_TYPE_HTML_BOX)

AtkObject*
gail_html_box_embedded_new (GObject *obj)
{
  gpointer object;
  AtkObject *atk_object;

  g_return_val_if_fail (HTML_IS_BOX_EMBEDDED (obj), NULL);
  object = g_object_new (GAIL_TYPE_HTML_BOX_EMBEDDED, NULL);
  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, obj);
  atk_object->role = ATK_ROLE_PANEL;
  return atk_object;
}

static void
gail_html_box_embedded_class_init (GailHtmlBoxEmbeddedClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_n_children = gail_html_box_embedded_get_n_children;
  class->ref_child = gail_html_box_embedded_ref_child;
}

static gint
gail_html_box_embedded_get_n_children (AtkObject *obj)
{
  AtkGObject *atk_gobject;
  HtmlBoxEmbedded *box_embedded;
  GObject *g_obj;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_EMBEDDED (obj), 0);
  atk_gobject = ATK_GOBJECT (obj); 
  g_obj = atk_gobject_get_object (atk_gobject);
  if (g_obj == NULL)
    /* State is defunct */
    return 0;

  g_return_val_if_fail (HTML_IS_BOX_EMBEDDED (g_obj), 0);

  box_embedded = HTML_BOX_EMBEDDED (g_obj);
  g_return_val_if_fail (box_embedded->widget, 0);
  return 1;
}

static AtkObject*
gail_html_box_embedded_ref_child (AtkObject *obj,
                                  gint      i)
{
  AtkGObject *atk_gobject;
  HtmlBoxEmbedded *box_embedded;
  GObject *g_obj;
  AtkObject *atk_child;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_EMBEDDED (obj), NULL);

  if (i != 0)
	  return NULL;

  atk_gobject = ATK_GOBJECT (obj);
  g_obj = atk_gobject_get_object (atk_gobject);
  if (g_obj == NULL)
    /* State is defunct */
    return NULL;

  g_return_val_if_fail (HTML_IS_BOX_EMBEDDED (g_obj), NULL);

  box_embedded = HTML_BOX_EMBEDDED (g_obj);
  g_return_val_if_fail (box_embedded->widget, NULL);

  atk_child = gtk_widget_get_accessible (box_embedded->widget);
  g_object_ref (atk_child);
  atk_object_set_parent (atk_child, obj);
  return atk_child;
}
