/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <string.h>
#include "gtkobject.h"
#include "gtktypeutils.h"


typedef struct _GtkTypeNode GtkTypeNode;

struct _GtkTypeNode
{
  GtkType type;
  gint init_class;
  gpointer klass;
  GtkTypeInfo type_info;
  GtkTypeNode *parent;
  GList *children;
};


static void  gtk_type_insert       (guint        parent_type,
				    GtkType      type,
				    GtkTypeInfo *type_info);
static void  gtk_type_class_init   (GtkTypeNode *node);
static void  gtk_type_object_init  (GtkTypeNode *node,
				    gpointer     object);
static guint gtk_type_hash         (GtkType     *key);
static gint  gtk_type_compare      (GtkType     *a,
			            GtkType     *b);
static guint gtk_type_name_hash    (const char  *key);
static gint  gtk_type_name_compare (const char  *a,
				    const char  *b);
static void  gtk_type_init_builtin_types (void);


static int initialize = TRUE;
static GHashTable *type_hash_table = NULL;
static GHashTable *name_hash_table = NULL;


void
gtk_type_init ()
{
  if (initialize)
    {
      g_assert (sizeof (GtkType) >= 4);

      initialize = FALSE;
      type_hash_table = g_hash_table_new ((GHashFunc) gtk_type_hash,
					  (GCompareFunc) gtk_type_compare);
      name_hash_table = g_hash_table_new ((GHashFunc) gtk_type_name_hash,
					  (GCompareFunc) gtk_type_name_compare);
      gtk_type_init_builtin_types ();
    }
}

GtkType
gtk_type_unique (GtkType      parent_type,
		 GtkTypeInfo *type_info)
{
  static guint next_seqno = 0;
  GtkType new_type;

  g_return_val_if_fail (type_info != NULL, 0);

  if (initialize)
    gtk_type_init ();

  next_seqno++;
  if (parent_type == GTK_TYPE_INVALID)
    new_type = next_seqno;
  else
    new_type = GTK_TYPE_MAKE (GTK_FUNDAMENTAL_TYPE (parent_type), next_seqno);
  gtk_type_insert (parent_type, new_type, type_info);

  return new_type;
}

gchar*
gtk_type_name (GtkType type)
{
  GtkTypeNode *node;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);

  if (node)
    return node->type_info.type_name;

  return NULL;
}

GtkType
gtk_type_from_name (const gchar *name)
{
  GtkTypeNode *node;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (name_hash_table, (gpointer) name);

  if (node)
    return node->type;

  return 0;
}

GtkType
gtk_type_parent (GtkType type)
{
  GtkTypeNode *node;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);

  if (node && node->parent)
    return node->parent->type;

  return 0;
}

gpointer
gtk_type_class (GtkType type)
{
  GtkTypeNode *node;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);
  g_return_val_if_fail (node != NULL, NULL);

  if (node->init_class)
    gtk_type_class_init (node);

  return node->klass;
}

gpointer
gtk_type_new (GtkType type)
{
  GtkTypeNode *node;
  gpointer object;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);
  g_return_val_if_fail (node != NULL, NULL);

  object = g_new0 (guchar, node->type_info.object_size);
  ((GtkObject*) object)->klass = gtk_type_class (type);
  gtk_type_object_init (node, object);

  return object;
}

void
gtk_type_describe_heritage (GtkType type)
{
  GtkTypeNode *node;
  gint first;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);
  first = TRUE;

  while (node)
    {
      if (first)
	{
	  first = FALSE;
	  g_print ("is a ");
	}

      if (node->type_info.type_name)
	g_print ("%s\n", node->type_info.type_name);
      else
	g_print ("<unnamed type>\n");

      node = node->parent;
    }
}

void
gtk_type_describe_tree (GtkType type,
			gint  show_size)
{
  static gint indent = 0;
  GtkTypeNode *node;
  GtkTypeNode *child;
  GList *children;
  gint old_indent;
  gint i;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);

  for (i = 0; i < indent; i++)
    g_print (" ");

  if (node->type_info.type_name)
    g_print ("%s", node->type_info.type_name);
  else
    g_print ("<unnamed type>");

  if (show_size)
    g_print (" ( %d bytes )\n", node->type_info.object_size);
  else
    g_print ("\n");

  old_indent = indent;
  indent += 4;

  children = node->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      gtk_type_describe_tree (child->type, show_size);
    }

  indent = old_indent;
}

gint
gtk_type_is_a (GtkType type,
	       GtkType is_a_type)
{
  GtkTypeNode *node;

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);

  while (node)
    {
      if (node->type == is_a_type)
	return TRUE;
      node = node->parent;
    }

  return FALSE;
}

void
gtk_type_get_arg (GtkObject   *object,
		  GtkType      type,
		  GtkArg      *arg,
		  guint	       arg_id)
{
  GtkTypeNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (arg != NULL);

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);

  if (node && node->type_info.arg_get_func)
    (* node->type_info.arg_get_func) (object, arg, arg_id);
  else
    arg->type = GTK_TYPE_INVALID;
}

void
gtk_type_set_arg (GtkObject *object,
		  GtkType    type,
		  GtkArg    *arg,
		  guint	     arg_id)
{
  GtkTypeNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (arg != NULL);

  if (initialize)
    gtk_type_init ();

  node = g_hash_table_lookup (type_hash_table, &type);

  if (node && node->type_info.arg_set_func)
    (* node->type_info.arg_set_func) (object, arg, arg_id);
}

GtkArg*
gtk_arg_copy (GtkArg         *src_arg,
	      GtkArg         *dest_arg)
{
  g_return_val_if_fail (src_arg != NULL, NULL);

  if (!dest_arg)
    {
      dest_arg = g_new0 (GtkArg, 1);
      dest_arg->name = src_arg->name;
    }

  dest_arg->type = src_arg->type;
  dest_arg->d = src_arg->d;

  if (src_arg->type == GTK_TYPE_STRING)
    dest_arg->d.string_data = g_strdup (src_arg->d.string_data);

  return dest_arg;
}

static void
gtk_type_insert (GtkType        parent_type,
		 GtkType        type,
		 GtkTypeInfo   *type_info)
{
  GtkTypeNode *node;
  GtkTypeNode *parent;

  parent = g_hash_table_lookup (type_hash_table, &parent_type);

  node = g_new (GtkTypeNode, 1);
  node->type = type;
  node->init_class = TRUE;
  node->klass = NULL;
  node->type_info = *type_info;
  node->parent = parent;
  node->children = NULL;

  if (node->parent)
    node->parent->children = g_list_append (node->parent->children, node);

  g_hash_table_insert (type_hash_table, &node->type, node);
  g_hash_table_insert (name_hash_table, node->type_info.type_name, node);
}

static void
gtk_type_class_init (GtkTypeNode *node)
{
  GtkObjectClass *object_class;

  if (node->init_class)
    {
      node->init_class = FALSE;
      node->klass = g_new0 (guchar, node->type_info.class_size);

      if (node->parent)
	{
	  if (node->parent->init_class)
	    gtk_type_class_init (node->parent);

	  memcpy (node->klass, node->parent->klass, node->parent->type_info.class_size);
	}

      object_class = node->klass;
      object_class->type = node->type;
      object_class->signals = NULL;
      object_class->nsignals = 0;
      object_class->n_args = 0;

      if (node->type_info.class_init_func)
	(* node->type_info.class_init_func) (node->klass);
    }
}

static void
gtk_type_object_init (GtkTypeNode *node,
		      gpointer     object)
{
  if (node->parent)
    gtk_type_object_init (node->parent, object);

  if (node->type_info.object_init_func)
    (* node->type_info.object_init_func) (object);
}

static guint
gtk_type_hash (GtkType *key)
{
  return GTK_TYPE_SEQNO (*key);
}

static gint
gtk_type_compare (GtkType *a,
		  GtkType *b)
{
  g_return_val_if_fail(a != NULL && b != NULL, 0);
  return (*a == *b);
}

static guint
gtk_type_name_hash (const char *key)
{
  guint result;

  result = 0;
  while (*key)
    result += (result << 3) + *key++;

  return result;
}

static gint
gtk_type_name_compare (const char *a,
		       const char *b)
{
  return (strcmp (a, b) == 0);
}

static GtkType
gtk_type_register_builtin (char   *name,
			   GtkType parent)
{
  GtkTypeInfo info;

  info.type_name = name;
  info.object_size = info.class_size = 0;
  info.class_init_func = NULL;
  info.object_init_func = NULL;
  info.arg_set_func = NULL;
  info.arg_get_func = NULL;

  return gtk_type_unique (parent, &info);
}

extern void gtk_object_init_type (void);

GtkType gtk_type_builtins[GTK_TYPE_NUM_BUILTINS];

static void
gtk_type_init_builtin_types ()
{
  /* GTK_TYPE_INVALID has typeid 0.  The first type id returned by
     gtk_type_unique is 1, which is GTK_TYPE_NONE.  And so on. */

  static struct {
    GtkType enum_id;
    gchar *name;
  } fundamental_info[] = {
    { GTK_TYPE_NONE, "void" },
    { GTK_TYPE_CHAR, "char" },
    { GTK_TYPE_BOOL, "bool" },
    { GTK_TYPE_INT, "int" },
    { GTK_TYPE_UINT, "uint" },
    { GTK_TYPE_LONG, "long" },
    { GTK_TYPE_ULONG, "ulong" },
    { GTK_TYPE_FLOAT, "float" },
    { GTK_TYPE_DOUBLE, "double" },
    { GTK_TYPE_STRING, "string" },
    { GTK_TYPE_ENUM, "enum" },
    { GTK_TYPE_FLAGS, "flags" },
    { GTK_TYPE_BOXED, "boxed" },
    { GTK_TYPE_FOREIGN, "foreign" },
    { GTK_TYPE_CALLBACK, "callback" },
    { GTK_TYPE_ARGS, "args" },

    { GTK_TYPE_POINTER, "pointer" },
    { GTK_TYPE_SIGNAL, "signal" },
    { GTK_TYPE_C_CALLBACK, "c_callback" }
  };

  static struct {
    char *name;
    GtkType parent;
  } builtin_info[] = {
#include "gtktypebuiltins.c"
    { NULL }
  };

  int i;

  for (i = 0; i < sizeof (fundamental_info)/sizeof(fundamental_info[0]); i++)
    {
      GtkType id;
      id = gtk_type_register_builtin (fundamental_info[i].name,
				      GTK_TYPE_INVALID);
      g_assert (id == fundamental_info[i].enum_id);
    }

  gtk_object_init_type ();

  for (i = 0; builtin_info[i].name; i++)
    {
      gtk_type_builtins[i] =
	gtk_type_register_builtin (builtin_info[i].name,
				   builtin_info[i].parent);
    }
}
