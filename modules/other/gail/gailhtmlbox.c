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

#include <gtk/gtk.h>
#include "gailhtmlbox.h"
#include "gailhtmlview.h"
#include <libgtkhtml/layout/htmlbox.h>

static void         gail_html_box_class_init               (GailHtmlBoxClass  *klass);
static void         gail_html_box_initialize               (AtkObject         *obj,
                                                            gpointer          data);
static gint         gail_html_box_get_index_in_parent      (AtkObject         *obj);
static AtkStateSet* gail_html_box_ref_state_set	           (AtkObject         *obj);

static void         gail_html_box_component_interface_init (AtkComponentIface *iface);
static guint        gail_html_box_add_focus_handler        (AtkComponent      *component,
                                                            AtkFocusHandler   handler);
static void         gail_html_box_get_extents              (AtkComponent      *component,
                                                            gint              *x,
                                                            gint              *y,
                                                            gint              *width,
                                                            gint              *height,
                                                            AtkCoordType      coord_type);
static gboolean     gail_html_box_grab_focus               (AtkComponent      *component);
static void         gail_html_box_remove_focus_handler     (AtkComponent      *component,
                                                            guint             handler_id);

static AtkGObjectClass *parent_class = NULL;

GType
gail_html_box_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo tinfo =
      {
        sizeof (GailHtmlBoxClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gail_html_box_class_init, /* class init */
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (GailHtmlBox), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };

      static const GInterfaceInfo atk_component_info =
      {
        (GInterfaceInitFunc) gail_html_box_component_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static (ATK_TYPE_GOBJECT,
                                     "GailHtmlBox", &tinfo, 0);
      g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
                                   &atk_component_info);
    }

  return type;
}

static void
gail_html_box_class_init (GailHtmlBoxClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  parent_class = g_type_class_ref (ATK_TYPE_GOBJECT);

  class->get_index_in_parent = gail_html_box_get_index_in_parent;
  class->ref_state_set = gail_html_box_ref_state_set;
  class->initialize = gail_html_box_initialize;
}

AtkObject*
gail_html_box_new (GObject *obj)
{
  GObject *object;
  AtkObject *atk_object;

  g_return_val_if_fail (HTML_IS_BOX (obj), NULL);
  object = g_object_new (GAIL_TYPE_HTML_BOX, NULL);
  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, obj);
  atk_object->role = ATK_ROLE_UNKNOWN;
  return atk_object;
}

static void
gail_html_box_initialize (AtkObject *obj,
                          gpointer  data)
{
  HtmlBox *box;
  ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

  box = HTML_BOX (data);

  /*
   * We do not set the parent here for the root node of a HtmlView
   */
  if (box->parent)
    { 
      atk_object_set_parent (obj,
                        atk_gobject_get_accessible (G_OBJECT (box->parent)));
    }
}

static gint
gail_html_box_get_index_in_parent (AtkObject *obj)
{
  AtkObject *parent;
  AtkGObject *atk_gobj;
  HtmlBox *box;
  HtmlBox *parent_box;
  gint n_children = 0;
  GObject *g_obj;

  g_return_val_if_fail (GAIL_IS_HTML_BOX (obj), -1);

  atk_gobj = ATK_GOBJECT (obj);
  g_obj = atk_gobject_get_object (atk_gobj);
  if (g_obj == NULL)
    return -1;

  g_return_val_if_fail (HTML_IS_BOX (g_obj), -1);
  box = HTML_BOX (g_obj);
  parent = atk_object_get_parent (obj);
  if (GAIL_IS_HTML_VIEW (parent))
    {
      return 0;
    }
  else if (ATK_IS_GOBJECT (parent))
    {
      parent_box = HTML_BOX (atk_gobject_get_object (ATK_GOBJECT (parent)));
    }
  else
    {
      g_assert_not_reached ();
      return -1;
    }

  if (parent_box)
    {
      HtmlBox *child;

      child = parent_box->children;

      while (child)
        {
          if (child == box)
            return n_children;

          n_children++;
          child = child->next;
        }
    }
  return -1;
}

static AtkStateSet*
gail_html_box_ref_state_set (AtkObject	  *obj)
{
  AtkGObject *atk_gobj;
  GObject *g_obj;
  AtkStateSet *state_set;

  g_return_val_if_fail (GAIL_IS_HTML_BOX (obj), NULL);
  atk_gobj = ATK_GOBJECT (obj);
  state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (obj);

  g_obj = atk_gobject_get_object (atk_gobj);
  if (g_obj == NULL)
    {
      /* Object is defunct */
      atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
    }
  else
    {
      HtmlBox *box;
  
      box = HTML_BOX (g_obj);

      if (HTML_BOX_GET_STYLE (box)->display != HTML_DISPLAY_NONE)
        {
          atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);
          atk_state_set_add_state (state_set, ATK_STATE_SHOWING);
        }
    }
  return state_set;
} 

static void
gail_html_box_component_interface_init (AtkComponentIface *iface)
{
  g_return_if_fail (iface != NULL);

  /*
   * Use default implementation for contains and get_position
   */
  iface->contains = NULL;
  iface->get_position = NULL;
  iface->add_focus_handler = gail_html_box_add_focus_handler;
  iface->get_extents = gail_html_box_get_extents;
  iface->get_size = NULL;
  iface->grab_focus = gail_html_box_grab_focus;
  iface->remove_focus_handler = gail_html_box_remove_focus_handler;
  iface->set_extents = NULL;
  iface->set_position = NULL;
  iface->set_size = NULL;
}

static guint
gail_html_box_add_focus_handler (AtkComponent    *component,
                                 AtkFocusHandler handler)
{
  return g_signal_connect_closure (component, 
                                   "focus-event",
                                   g_cclosure_new (
						   G_CALLBACK (handler), NULL,
						   (GClosureNotify) NULL),
                                   FALSE);
}

static void
gail_html_box_get_extents (AtkComponent *component,
                           gint         *x,
                           gint         *y,
                           gint         *width,
                           gint         *height,
                           AtkCoordType coord_type)
{
  AtkGObject *atk_gobj;
  HtmlBox *box;
  GObject *g_obj;

  g_return_if_fail (GAIL_IS_HTML_BOX (component));

  atk_gobj = ATK_GOBJECT (component);
  g_obj = atk_gobject_get_object (atk_gobj);
  if (g_obj == NULL)
    return;

  g_return_if_fail (HTML_IS_BOX (g_obj));
  box = HTML_BOX (g_obj);

  *x = html_box_get_absolute_x (box);
  *y = html_box_get_absolute_y (box);
  *width = box->width;
  *height = box->height;

  g_print ("%d %d %d %d\n",
           html_box_get_absolute_x (box),
           html_box_get_absolute_y (box),
           html_box_get_containing_block_width (box),
           html_box_get_containing_block_height (box));
}

static gboolean
gail_html_box_grab_focus (AtkComponent    *component)
{
  return TRUE;
}

static void
gail_html_box_remove_focus_handler (AtkComponent *component,
                                    guint        handler_id)
{
  g_signal_handler_disconnect (ATK_OBJECT (component), handler_id);
}
