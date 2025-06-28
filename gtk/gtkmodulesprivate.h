/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

char ** _gtk_get_module_path          (const char   *type);

/* A version of G_DEFINE_TYPE_WITH_CODE that appends "Builtin"
 * to the type name. This helps with transition from modules
 * to builtin modules for people who build gtk themselves.
 */
#define GTK_DEFINE_BUILTIN_MODULE_TYPE_WITH_CODE(TypeName, type_name, TYPE_PARENT, _C_) \
static void     type_name##_init              (TypeName        *self); \
static void     type_name##_class_init        (TypeName##Class *klass); \
static GType    type_name##_get_type_once     (void); \
static void *   type_name##_parent_class = NULL; \
static int      TypeName##_private_offset; \
\
static void     type_name##_class_intern_init (gpointer klass) \
{ \
  type_name##_parent_class = g_type_class_peek_parent (klass); \
  if (TypeName##_private_offset != 0) \
    g_type_class_adjust_private_offset (klass, &TypeName##_private_offset); \
  type_name##_class_init ((TypeName##Class*) klass); \
} \
\
G_GNUC_UNUSED \
static inline gpointer \
type_name##_get_instance_private (TypeName *self) \
{ \
  return (G_STRUCT_MEMBER_P (self, TypeName##_private_offset)); \
} \
\
GType \
type_name##_get_type (void) \
{ \
  static _g_type_once_init_type static_g_define_type_id = 0; \
\
  if (_g_type_once_init_enter (&static_g_define_type_id)) \
    { \
      GType g_define_type_id = type_name##_get_type_once (); \
      _g_type_once_init_leave (&static_g_define_type_id, g_define_type_id); \
    }                                   \
  return static_g_define_type_id; \
} \
\
G_NO_INLINE \
static GType \
type_name##_get_type_once (void) \
{ \
  GType g_define_type_id = \
        g_type_register_static_simple (TYPE_PARENT, \
                                       g_intern_static_string (#TypeName "Builtin"), \
                                       sizeof (TypeName##Class), \
                                       (GClassInitFunc)(void (*)(void)) type_name##_class_intern_init, \
                                       sizeof (TypeName), \
                                       (GInstanceInitFunc)(void (*)(void)) type_name##_init, \
                                       (GTypeFlags) 0); \
    { \
  { _C_; } \
    } \
  return g_define_type_id; \
}

G_END_DECLS
