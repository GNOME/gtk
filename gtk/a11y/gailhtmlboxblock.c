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
#include "gailhtmlboxblock.h"

static void       gail_html_box_block_class_init      (GailHtmlBoxBlockClass *klass);
static gint       gail_html_box_block_get_n_children  (AtkObject             *obj);
static AtkObject* gail_html_box_block_ref_child       (AtkObject             *obj,
                                                       gint                  i);

G_DEFINE_TYPE (GailHtmlBoxBlock, gail_html_box_block, GAIL_TYPE_HTML_BOX)

AtkObject*
gail_html_box_block_new (GObject *obj)
{
  GObject *object;
  AtkObject *atk_object;

  g_return_val_if_fail (HTML_IS_BOX_BLOCK (obj), NULL);
  object = g_object_new (GAIL_TYPE_HTML_BOX_BLOCK, NULL);
  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, obj);
  atk_object->role = ATK_ROLE_PANEL;
  return atk_object;
}

static void
gail_html_box_block_class_init (GailHtmlBoxBlockClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_n_children = gail_html_box_block_get_n_children;
  class->ref_child = gail_html_box_block_ref_child;
}

static gint
gail_html_box_block_get_n_children (AtkObject *obj)
{
  AtkGObject *atk_gobject;
  HtmlBox *box;
  gint n_children = 0;
  GObject *g_obj;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_BLOCK (obj), 0);
  atk_gobject = ATK_GOBJECT (obj); 
  g_obj = atk_gobject_get_object (atk_gobject);
  if (g_obj == NULL)
    return 0;

  g_return_val_if_fail (HTML_IS_BOX (g_obj), 0);
  box = HTML_BOX (g_obj);

  if (box)
    {
      HtmlBox *child;

      child = box->children;

      while (child)
        {
          n_children++;
          child = child->next;
        }
    }
  return n_children;
}

static AtkObject *
gail_html_box_block_ref_child (AtkObject *obj,
                               gint      i)
{
  AtkGObject *atk_gobject;
  GObject *g_obj;
  HtmlBox *box;
  AtkObject *atk_child = NULL;
  gint n_children = 0;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_BLOCK (obj), NULL);
  atk_gobject = ATK_GOBJECT (obj); 
  g_obj = atk_gobject_get_object (atk_gobject);
  if (g_obj == NULL)
    return NULL;

  g_return_val_if_fail (HTML_IS_BOX (g_obj), NULL);
  box = HTML_BOX (g_obj);

  if (box)
    {
      HtmlBox *child;

      child = box->children;

      while (child)
        {
          if (n_children == i)
            {
              atk_child = atk_gobject_get_accessible (G_OBJECT (child));
              g_object_ref (atk_child);
              break;
            }
          n_children++;
          child = child->next;
        }
    }
  return atk_child;
}
