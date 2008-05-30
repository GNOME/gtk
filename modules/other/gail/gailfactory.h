/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2002 Sun Microsystems Inc.
 * Copyright (C) 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
 * Copyright (C) 2007 Christian Persch
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
#include <atk/atk.h>

#define _GAIL_IMPLEMENT_FACTORY_CREATE_ACCESSIBLE(GAIL_TYPE, TYPE) \
{ \
  AtkObject *accessible; \
\
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (object, TYPE), NULL); \
\
  accessible = g_object_new (GAIL_TYPE, NULL); \
  atk_object_initialize (accessible, object); \
\
  return accessible; \
}

#define _GAIL_IMPLEMENT_FACTORY_BEGIN(GAIL_TYPE, TypeName, type_name) \
\
static GType \
type_name##_factory_get_accessible_type (void) \
{ \
  return GAIL_TYPE; \
} \
\
static AtkObject* \
type_name##_factory_create_accessible (GObject *object) \
{

#define _GAIL_IMPLEMENT_FACTORY_END(GAIL_TYPE, TypeName, type_name) \
} \
\
static void \
type_name##_factory_class_init (AtkObjectFactoryClass *klass) \
{ \
  klass->create_accessible   = type_name ## _factory_create_accessible;	\
  klass->get_accessible_type = type_name ## _factory_get_accessible_type;\
} \
\
GType \
type_name##_factory_get_type (void) \
{ \
  static volatile gsize g_define_type_id__volatile = 0; \
  if (g_once_init_enter (&g_define_type_id__volatile)) \
    { \
      GType g_define_type_id = \
        g_type_register_static_simple (ATK_TYPE_OBJECT_FACTORY, \
                                       g_intern_static_string (#TypeName "Factory"), \
                                       sizeof (AtkObjectFactoryClass), \
                                       (GClassInitFunc) type_name##_factory_class_init, \
                                       sizeof (AtkObjectFactory), \
                                       (GInstanceInitFunc) NULL, \
                                       (GTypeFlags) 0); \
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id); \
    } \
  return g_define_type_id__volatile; \
}

/* Implements a AtkObjectFactory creating accessibles of type
 * @GAIL_TYPE for objects of type @TYPE.
 */
#define GAIL_IMPLEMENT_FACTORY(GAIL_TYPE, TypeName, type_name, TYPE) \
_GAIL_IMPLEMENT_FACTORY_BEGIN (GAIL_TYPE, TypeName, type_name) \
_GAIL_IMPLEMENT_FACTORY_CREATE_ACCESSIBLE (GAIL_TYPE, TYPE) \
_GAIL_IMPLEMENT_FACTORY_END (GAIL_TYPE, TypeName, type_name)

/* Implements a AtkObjectFactory creating accessibles of type
 * @GAIL_TYPE with creation func @create_accessible.
 */
#define GAIL_IMPLEMENT_FACTORY_WITH_FUNC(GAIL_TYPE, TypeName, type_name, create_accessible) \
_GAIL_IMPLEMENT_FACTORY_BEGIN (GAIL_TYPE, TypeName, type_name) \
{ return create_accessible (GTK_WIDGET (object)); } \
_GAIL_IMPLEMENT_FACTORY_END (GAIL_TYPE, TypeName, type_name)

#define GAIL_IMPLEMENT_FACTORY_WITH_FUNC_DUMMY(GAIL_TYPE, TypeName, type_name, TYPE, create_accessible) \
_GAIL_IMPLEMENT_FACTORY_BEGIN (GAIL_TYPE, TypeName, type_name) \
{ \
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (object, TYPE), NULL);\
  return create_accessible (); \
} \
_GAIL_IMPLEMENT_FACTORY_END (GAIL_TYPE, TypeName, type_name)

#define GAIL_WIDGET_SET_FACTORY(widget_type, type_as_function)			\
	atk_registry_set_factory_type (atk_get_default_registry (),		\
				       widget_type,				\
				       type_as_function ## _factory_get_type ())

#endif /* _GAIL_FACTORY_H__ */
