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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtktypeutils.h"
#include "gtkobject.h"


/* --- functions --- */
GtkType
gtk_type_unique (GtkType            parent_type,
		 const GtkTypeInfo *gtkinfo)
{
  GTypeInfo tinfo = { 0, };

  g_return_val_if_fail (GTK_TYPE_IS_OBJECT (parent_type), 0);
  g_return_val_if_fail (gtkinfo != NULL, 0);
  g_return_val_if_fail (gtkinfo->type_name != NULL, 0);
  g_return_val_if_fail (g_type_from_name (gtkinfo->type_name) == 0, 0);

  tinfo.class_size = gtkinfo->class_size;
  tinfo.base_init = gtkinfo->base_class_init_func;
  tinfo.base_finalize = NULL;
  tinfo.class_init = (GClassInitFunc) gtkinfo->class_init_func;
  tinfo.class_finalize = NULL;
  tinfo.class_data = NULL;
  tinfo.instance_size = gtkinfo->object_size;
  tinfo.n_preallocs = 0;
  tinfo.instance_init = gtkinfo->object_init_func;

  return g_type_register_static (parent_type, gtkinfo->type_name, &tinfo, 0);
}

gpointer
gtk_type_class (GtkType type)
{
  static GQuark quark_static_class = 0;
  gpointer class;

  if (!G_TYPE_IS_ENUM (type) && !G_TYPE_IS_FLAGS (type))
    g_return_val_if_fail (GTK_TYPE_IS_OBJECT (type), NULL);

  /* ok, this is a bit ugly, GLib reference counts classes,
   * and gtk_type_class() used to always return static classes.
   * while we coud be faster with just peeking the glib class
   * for the normal code path, we can't be sure that that
   * class stays around (someone else might be holding the
   * reference count and is going to drop it later). so to
   * ensure that Gtk actually holds a static reference count
   * on the class, we use GType qdata to store referenced
   * classes, and only return those.
   */

  class = g_type_get_qdata (type, quark_static_class);
  if (!class)
    {
      if (!quark_static_class)
	quark_static_class = g_quark_from_static_string ("GtkStaticTypeClass");

      class = g_type_class_ref (type);
      g_assert (class != NULL);
      g_type_set_qdata (type, quark_static_class, class);
    }

  return class;
}

gpointer
gtk_type_new (GtkType type)
{
  gpointer object;

  g_return_val_if_fail (GTK_TYPE_IS_OBJECT (type), NULL);

  object = g_type_create_instance (type);

  return object;
}

/* includes for various places
 * with enum definitions
 */
#include "makeenums.h"
/* type variable declarations
 */
#include "gtktypebuiltins_vars.c"
GType GTK_TYPE_IDENTIFIER = 0;
#include "gtktypebuiltins_evals.c"	  /* enum value definition arrays */

/* Hack to communicate with GLib object debugging for now
 */
#ifdef G_OS_WIN32
#define IMPORT __declspec(dllimport)
#else
#define IMPORT
#endif
extern IMPORT gboolean glib_debug_objects;

#include <gtk.h>	/* for gtktypebuiltins_ids.c */
#include <gdk.h>	/* gtktypebuiltins_ids.c */

void
gtk_type_init (void)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      static const struct {
	GtkType type_id;
	gchar *name;
      } fundamental_info[] = {
	{ GTK_TYPE_SIGNAL,	"GtkSignal" },
      };
      static struct {
	gchar              *type_name;
	GtkType            *type_id;
	GtkType             parent;
	gconstpointer	    pointer1;
	gpointer	    pointer2;
      } builtin_info[GTK_TYPE_N_BUILTINS + 1] = {
#include "gtktypebuiltins_ids.c"	/* type entries */
	{ NULL }
      };
      GTypeFundamentalInfo finfo = { 0, };
      GTypeInfo tinfo = { 0, };
      GtkType type_id;
      guint i;

      initialized = TRUE;

      glib_debug_objects = (gtk_debug_flags & GTK_DEBUG_OBJECTS) != 0;
      
      /* initialize GLib type system
       */
      g_type_init ();
      
      /* GTK_TYPE_OBJECT
       */
      gtk_object_get_type ();

      /* compatibility fundamentals
       */
      for (i = 0; i < sizeof (fundamental_info) / sizeof (fundamental_info[0]); i++)
	{
	  type_id = g_type_register_fundamental (fundamental_info[i].type_id,
						 fundamental_info[i].name,
						 &tinfo, &finfo, 0);
	  g_assert (type_id == fundamental_info[i].type_id);
	}

      /* GTK_TYPE_IDENTIFIER
       */
      GTK_TYPE_IDENTIFIER = g_type_register_static (G_TYPE_STRING, "GtkIdentifier", &tinfo, 0);

      /* enums and flags
       */
      for (i = 0; i < GTK_TYPE_N_BUILTINS; i++)
	{
	  GtkType type_id = 0;

	  if (builtin_info[i].parent == G_TYPE_ENUM)
	    type_id = g_enum_register_static (builtin_info[i].type_name, builtin_info[i].pointer1);
	  else if (builtin_info[i].parent == G_TYPE_FLAGS)
	    type_id = g_flags_register_static (builtin_info[i].type_name, builtin_info[i].pointer1);
	  else if (builtin_info[i].parent == GTK_TYPE_BOXED)
	    {
	      if (builtin_info[i].pointer1 && builtin_info[i].pointer2)
		type_id = g_boxed_type_register_static (builtin_info[i].type_name,
							builtin_info[i].pointer1,
							builtin_info[i].pointer2);
	      else
		type_id = g_type_register_static (GTK_TYPE_BOXED, builtin_info[i].type_name, &tinfo, 0);
	    }
	  else
	    g_assert_not_reached ();

	  *builtin_info[i].type_id = type_id;
	}
    }
}

GtkEnumValue*
gtk_type_enum_get_values (GtkType enum_type)
{
  GEnumClass *class;

  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), NULL);
  
  class = gtk_type_class (enum_type);
  
  return class->values;
}

GtkFlagValue*
gtk_type_flags_get_values (GtkType flags_type)
{
  GFlagsClass *class;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), NULL);

  class = gtk_type_class (flags_type);

  return class->values;
}

GtkEnumValue*
gtk_type_enum_find_value (GtkType      enum_type,
			  const gchar *value_name)
{
  GtkEnumValue *value;
  GEnumClass *class;

  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), NULL);
  g_return_val_if_fail (value_name != NULL, NULL);

  class = gtk_type_class (enum_type);
  value = g_enum_get_value_by_name (class, value_name);
  if (!value)
    value = g_enum_get_value_by_nick (class, value_name);

  return value;
}

GtkFlagValue*
gtk_type_flags_find_value (GtkType      flags_type,
			   const gchar *value_name)
{
  GtkFlagValue *value;
  GFlagsClass *class;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), NULL);
  g_return_val_if_fail (value_name != NULL, NULL);

  class = gtk_type_class (flags_type);
  value = g_flags_get_value_by_name (class, value_name);
  if (!value)
    value = g_flags_get_value_by_nick (class, value_name);

  return value;
}
