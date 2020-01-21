/* ATK -  Accessibility Toolkit
 * Copyright (C) 2009 Novell, Inc.
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

#include "atk.h"
#include "atkplug.h"

/**
 * SECTION:atkplug
 * @Short_description: Toplevel for embedding into other processes
 * @Title: AtkPlug
 * @See_also: #AtkPlug
 *
 * See #AtkSocket
 *
 */

static void atk_component_interface_init (AtkComponentIface *iface);

typedef struct {
  AtkObject *child;
} AtkPlugPrivate;

static gint AtkPlug_private_offset;

G_DEFINE_TYPE_WITH_CODE (AtkPlug, atk_plug, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init)
                         G_ADD_PRIVATE(AtkPlug))

static AtkObject*
atk_plug_ref_child (AtkObject *obj, int i)
{
  AtkPlugPrivate *private = atk_plug_get_instance_private (ATK_PLUG (obj));
  AtkObject *child;

  if (i != 0)
    return NULL;

  child = private->child;

  if (child == NULL)
    return NULL;

  return g_object_ref (child);
}

static int
atk_plug_get_n_children (AtkObject *obj)
{
  AtkPlugPrivate *private = atk_plug_get_instance_private (ATK_PLUG (obj));

  if (private->child == NULL)
    return 0;

  return 1;
}

static AtkStateSet*
atk_plug_ref_state_set (AtkObject *obj)
{
  AtkPlugPrivate *private = atk_plug_get_instance_private (ATK_PLUG (obj));
  AtkObject *child;

  child = private->child;

  if (child == NULL)
    return NULL;

  return atk_object_ref_state_set (child);
}

static void
atk_plug_init (AtkPlug* obj)
{
  AtkObject *accessible = ATK_OBJECT (obj);

  accessible->role = ATK_ROLE_FILLER;
  accessible->layer = ATK_LAYER_WIDGET;
}

static void
atk_plug_class_init (AtkPlugClass* klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  if (AtkPlug_private_offset != 0)
    g_type_class_adjust_private_offset (klass, &AtkPlug_private_offset);

  klass->get_object_id = NULL;

  class->get_n_children = atk_plug_get_n_children;
  class->ref_child      = atk_plug_ref_child;
  class->ref_state_set  = atk_plug_ref_state_set;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
}

/**
 * atk_plug_new:
 *
 * Creates a new #AtkPlug instance.
 *
 * Returns: (transfer full): the newly created #AtkPlug
 *
 * Since: 1.30
 */
AtkObject *
atk_plug_new (void)
{
  return g_object_new (ATK_TYPE_PLUG, NULL);
}

/**
 * atk_plug_set_child:
 * @plug: an #AtkPlug to be set as accessible parent of @child.
 * @child: an #AtkObject to be set as accessible child of @plug.
 *
 * Sets @child as accessible child of @plug and @plug as accessible parent of
 * @child. @child can be NULL.
 *
 * In some cases, one can not use the AtkPlug type directly as accessible
 * object for the toplevel widget of the application. For instance in the gtk
 * case, GtkPlugAccessible can not inherit both from GtkWindowAccessible and
 * from AtkPlug. In such a case, one can create, in addition to the standard
 * accessible object for the toplevel widget, an AtkPlug object, and make the
 * former the child of the latter by calling atk_plug_set_child().
 *
 * Since: 2.35.0
 */
void
atk_plug_set_child (AtkPlug *plug, AtkObject *child)
{
  AtkPlugPrivate *private = atk_plug_get_instance_private (plug);

  if (private->child)
    atk_object_set_parent (private->child, NULL);

  private->child = child;

  if (child)
    atk_object_set_parent (child, ATK_OBJECT(plug));
}

/**
 * atk_plug_get_id:
 * @plug: an #AtkPlug
 *
 * Gets the unique ID of an #AtkPlug object, which can be used to
 * embed inside of an #AtkSocket using atk_socket_embed().
 *
 * Internally, this calls a class function that should be registered
 * by the IPC layer (usually at-spi2-atk). The implementor of an
 * #AtkPlug object should call this function (after atk-bridge is
 * loaded) and pass the value to the process implementing the
 * #AtkSocket, so it could embed the plug.
 *
 * Returns: the unique ID for the plug
 *
 * Since: 1.30
 **/
gchar*
atk_plug_get_id (AtkPlug* plug)
{
  AtkPlugClass *klass;

  g_return_val_if_fail (ATK_IS_PLUG (plug), NULL);

  klass = g_type_class_peek (ATK_TYPE_PLUG);

  if (klass && klass->get_object_id)
    return (klass->get_object_id) (plug);
  else
    return NULL;
}
