/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2002 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GAIL_FACTORY_H__
#define _GAIL_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobject.h>

#define GAIL_ACCESSIBLE_FACTORY(type, type_as_function, opt_create_accessible)	\
										\
static GType									\
type_as_function ## _factory_get_accessible_type (void)				\
{										\
  return type;									\
}										\
										\
static AtkObject*								\
type_as_function ## _factory_create_accessible (GObject *obj)			\
{										\
  GtkWidget *widget;								\
  AtkObject *accessible;							\
										\
  g_return_val_if_fail (GTK_IS_WIDGET (obj), NULL);				\
										\
  widget = GTK_WIDGET (obj);							\
										\
  accessible = opt_create_accessible (widget);					\
										\
  return accessible;								\
}										\
										\
static void									\
type_as_function ## _factory_class_init (AtkObjectFactoryClass *klass)		\
{										\
  klass->create_accessible   = type_as_function ## _factory_create_accessible;	\
  klass->get_accessible_type = type_as_function ## _factory_get_accessible_type;\
}										\
										\
static GType									\
type_as_function ## _factory_get_type (void)					\
{										\
  static GType t = 0;								\
										\
  if (!t)									\
  {										\
    char *name;									\
    static const GTypeInfo tinfo =						\
    {										\
      sizeof (AtkObjectFactoryClass),					\
      NULL, NULL, (GClassInitFunc) type_as_function ## _factory_class_init,			\
      NULL, NULL, sizeof (AtkObjectFactory), 0, NULL, NULL			\
    };										\
										\
    name = g_strconcat (g_type_name (type), "Factory", NULL);			\
    t = g_type_register_static (						\
	    ATK_TYPE_OBJECT_FACTORY, name, &tinfo, 0);				\
    g_free (name);								\
  }										\
										\
  return t;									\
}

#define GAIL_WIDGET_SET_FACTORY(widget_type, type_as_function)			\
	atk_registry_set_factory_type (atk_get_default_registry (),		\
				       widget_type,				\
				       type_as_function ## _factory_get_type ())

#endif /* _GAIL_FACTORY_H__ */
