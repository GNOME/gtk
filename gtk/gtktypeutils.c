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


#define	TYPE_NODES_BLOCK_SIZE	(200)

typedef struct _GtkTypeNode GtkTypeNode;

struct _GtkTypeNode
{
  GtkType type;
  GtkTypeInfo type_info;
  guint n_supers : 24;
  guint chunk_alloc_locked : 1;
  GtkType *supers;
  GtkType parent_type;
  gpointer klass;
  GList *children_types;
  GMemChunk *mem_chunk;
};


#define	LOOKUP_TYPE_NODE(node_var, type)	{ \
  if (type > 0) \
  { \
    register GtkType sqn = GTK_TYPE_SEQNO (type); \
    if (sqn < n_type_nodes) \
      node_var = type_nodes + sqn; \
    else \
      node_var = NULL; \
  } \
  else \
    node_var = NULL; \
}

static void  gtk_type_class_init		(GtkTypeNode *node);
static guint gtk_type_name_hash			(const char  *key);
static gint  gtk_type_name_compare		(const char  *a,
						 const char  *b);
static void  gtk_type_init_builtin_types	(void);

static GtkTypeNode *type_nodes = NULL;
static guint        n_type_nodes = 0;
static GHashTable  *type_name_2_type_ht = NULL;
static const gchar *key_enum_vals = "gtk-type-enum-values";
static guint        key_id_enum_vals = 0;


static GtkTypeNode*
gtk_type_node_next_and_invalidate (void)
{
  static guint  n_free_type_nodes = 0;
  register GtkTypeNode  *node;
  register GtkType new_type;

  /* don't keep *any* GtkTypeNode pointers across invokation of this function!!!
   */

  if (n_free_type_nodes == 0)
    {
      register guint i;
      register guint size;
      
      /* nearest pow
       */
      size = n_type_nodes + TYPE_NODES_BLOCK_SIZE;
      size *= sizeof (GtkTypeNode);
      i = 1;
      while (i < size)
	i <<= 1;
      size = i;
      
      type_nodes = g_realloc (type_nodes, size);

      n_free_type_nodes = size / sizeof (GtkTypeNode) - n_type_nodes;
      
      memset (type_nodes + n_type_nodes, 0, n_free_type_nodes * sizeof (GtkTypeNode));
    }

  new_type = n_type_nodes++;
  n_free_type_nodes--;

  LOOKUP_TYPE_NODE (node, new_type);
  if (node)
    node->type = new_type;

  return node;
}

void
gtk_type_init (void)
{
  if (n_type_nodes == 0)
    {
      GtkTypeNode *zero;

      g_assert (sizeof (GtkType) >= 4);

      zero = gtk_type_node_next_and_invalidate ();
      g_assert (zero == NULL);

      type_name_2_type_ht = g_hash_table_new ((GHashFunc) gtk_type_name_hash,
					      (GCompareFunc) gtk_type_name_compare);

      gtk_type_init_builtin_types ();
    }
}

void
gtk_type_set_chunk_alloc (GtkType      type,
			  guint        n_chunks)
{
  GtkTypeNode *node;

  LOOKUP_TYPE_NODE (node, type);
  g_return_if_fail (node != NULL);
  g_return_if_fail (node->chunk_alloc_locked == FALSE);

  if (node->mem_chunk)
    {
      g_mem_chunk_destroy (node->mem_chunk);
      node->mem_chunk = NULL;
    }

  if (n_chunks)
    node->mem_chunk = g_mem_chunk_new (node->type_info.type_name,
				       node->type_info.object_size,
				       node->type_info.object_size * n_chunks,
				       G_ALLOC_AND_FREE);
}

static GtkType
gtk_type_create (GtkType      parent_type,
		 gchar	      *type_name,
		 GtkTypeInfo *type_info)
{
  GtkTypeNode *new_node;
  GtkTypeNode *parent;
  guint i;

  if (g_hash_table_lookup (type_name_2_type_ht, type_name))
    {
      g_warning ("gtk_type_create(): type `%s' already exists.", type_name);
      return 0;
    }
  
  if (parent_type)
    {
      GtkTypeNode *tmp_node;
      
      LOOKUP_TYPE_NODE (tmp_node, parent_type);
      if (!tmp_node)
	{
	  g_warning ("gtk_type_create(): unknown parent type `%u'.", parent_type);
	  return 0;
	}
    }

  /* relookup pointers afterwards.
   */
  new_node = gtk_type_node_next_and_invalidate ();

  if (parent_type)
    {
      new_node->type = GTK_TYPE_MAKE (parent_type, new_node->type);
      LOOKUP_TYPE_NODE (parent, parent_type);
    }
  else
    {
      g_assert (new_node->type <= 0xff);
      parent = NULL;
    }

  new_node->type_info = *type_info;
  new_node->type_info.type_name = type_name;
  new_node->n_supers = parent ? parent->n_supers + 1 : 0;
  new_node->chunk_alloc_locked = FALSE;
  new_node->supers = g_new0 (GtkType, new_node->n_supers + 1);
  new_node->parent_type = parent_type;
  new_node->klass = NULL;
  new_node->children_types = NULL;
  new_node->mem_chunk = NULL;

  if (parent)
    parent->children_types = g_list_append (parent->children_types, GUINT_TO_POINTER (new_node->type));

  parent = new_node;
  for (i = 0; i < new_node->n_supers + 1; i++)
    {
      new_node->supers[i] = parent->type;
      LOOKUP_TYPE_NODE (parent, parent->parent_type);
    }
    
  g_hash_table_insert (type_name_2_type_ht, new_node->type_info.type_name, GUINT_TO_POINTER (new_node->type));

  return new_node->type;
}

GtkType
gtk_type_unique (GtkType      parent_type,
		 GtkTypeInfo *type_info)
{
  GtkType new_type;
  gchar *type_name;

  g_return_val_if_fail (type_info != NULL, 0);
  g_return_val_if_fail (type_info->type_name != NULL, 0);
  
  if (n_type_nodes == 0)
    gtk_type_init ();

  type_name = g_strdup (type_info->type_name);

  /* relookup pointers afterwards.
   */
  new_type = gtk_type_create (parent_type, type_name, type_info);

  if (!new_type)
    g_free (type_name);

  return new_type;
}

gchar*
gtk_type_name (GtkType type)
{
  GtkTypeNode *node;

  LOOKUP_TYPE_NODE (node, type);

  if (node)
    return node->type_info.type_name;

  return NULL;
}

GtkType
gtk_type_from_name (const gchar *name)
{
  if (type_name_2_type_ht)
    {
      GtkType type;
      
      type = GPOINTER_TO_UINT (g_hash_table_lookup (type_name_2_type_ht, (gpointer) name));

      return type;
    }

  return 0;
}

GtkType
gtk_type_parent (GtkType type)
{
  GtkTypeNode *node;

  LOOKUP_TYPE_NODE (node, type);
  if (node)
    return node->parent_type;

  return 0;
}

gpointer
gtk_type_parent_class (GtkType type)
{
  GtkTypeNode *node;

  LOOKUP_TYPE_NODE (node, type);
  g_return_val_if_fail (node != NULL, NULL);

  if (node)
    {
      LOOKUP_TYPE_NODE (node, node->parent_type);

      if (node)
	{
	  if (!node->klass)
	    gtk_type_class_init (node);

	  return node->klass;
	}
    }

  return NULL;
}

gpointer
gtk_type_class (GtkType type)
{
  GtkTypeNode *node;

  LOOKUP_TYPE_NODE (node, type);
  g_return_val_if_fail (node != NULL, NULL);

  if (!node->klass)
    gtk_type_class_init (node);

  return node->klass;
}

gpointer
gtk_type_new (GtkType type)
{
  GtkTypeNode *node;
  GtkObject *object;
  gpointer klass;
  guint i;

  LOOKUP_TYPE_NODE (node, type);
  g_return_val_if_fail (node != NULL, NULL);

  klass = gtk_type_class (type);
  node->chunk_alloc_locked = TRUE;
  if (node->mem_chunk)
    {
      object = g_mem_chunk_alloc (node->mem_chunk);
      memset (object, 0, node->type_info.object_size);
    }
  else
    object = g_malloc0 (node->type_info.object_size);
  object->klass = klass;

  for (i = node->n_supers; i > 0; i--)
    {
      GtkTypeNode *pnode;

      LOOKUP_TYPE_NODE (pnode, node->supers[i]);
      if (pnode->type_info.object_init_func)
	(* pnode->type_info.object_init_func) (object);
    }
  if (node->type_info.object_init_func)
    (* node->type_info.object_init_func) (object);

  return object;
}

void
gtk_type_free (GtkType      type,
	       gpointer     mem)
{
  GtkTypeNode *node;

  g_return_if_fail (mem != NULL);
  LOOKUP_TYPE_NODE (node, type);
  g_return_if_fail (node != NULL);

  if (node->mem_chunk)
    g_mem_chunk_free (node->mem_chunk, mem);
  else
    g_free (mem);
}

void
gtk_type_describe_heritage (GtkType type)
{
  GtkTypeNode *node;
  gint first;

  LOOKUP_TYPE_NODE (node, type);
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

      LOOKUP_TYPE_NODE (node, node->parent_type);
    }
}

void
gtk_type_describe_tree (GtkType type,
			gint  show_size)
{
  GtkTypeNode *node;
  
  LOOKUP_TYPE_NODE (node, type);
  
  if (node)
    {
      static gint indent = 0;
      GList *list;
      guint old_indent;
      guint i;
      
      for (i = 0; i < indent; i++)
	g_print (" ");
      
      if (node->type_info.type_name)
	g_print ("%s", node->type_info.type_name);
      else
	g_print ("(no-name)");
      
      if (show_size)
	g_print (" ( %d bytes )\n", node->type_info.object_size);
      else
	g_print ("\n");
      
      old_indent = indent;
      indent += 4;
      
      for (list = node->children_types; list; list = list->next)
	gtk_type_describe_tree (GPOINTER_TO_UINT (list->data), show_size);
      
      indent = old_indent;
    }
}

gint
gtk_type_is_a (GtkType type,
	       GtkType is_a_type)
{
  if (type == is_a_type)
    return TRUE;
  else
    {
      register GtkTypeNode *node;

      LOOKUP_TYPE_NODE (node, type);
      if (node)
	{
	  register GtkTypeNode *a_node;
	  
	  LOOKUP_TYPE_NODE (a_node, is_a_type);
	  if (a_node)
	    {
	      if (a_node->n_supers <= node->n_supers)
		return node->supers[node->n_supers - a_node->n_supers] == is_a_type;
	    }
	}
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

  LOOKUP_TYPE_NODE (node, type);

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

  LOOKUP_TYPE_NODE (node, type);

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

GtkEnumValue*
gtk_enum_get_values (GtkType      enum_type)
{
  if (gtk_type_is_a (enum_type, GTK_TYPE_ENUM) ||
      gtk_type_is_a (enum_type, GTK_TYPE_FLAGS))
    return g_dataset_id_get_data (gtk_type_name (enum_type), key_id_enum_vals);

  g_warning ("gtk_enum_get_values(): type `%s' is not derived from `enum' or `flags'",
	     gtk_type_name (enum_type));

  return NULL;
}

void
gtk_enum_set_values (GtkType       enum_type,
		     GtkEnumValue *values)
{
  if (!key_id_enum_vals)
    key_id_enum_vals = g_dataset_force_id (key_enum_vals);
    
  if (gtk_type_is_a (enum_type, GTK_TYPE_ENUM) ||
      gtk_type_is_a (enum_type, GTK_TYPE_FLAGS))
    {
      gchar *type_name;

      type_name = gtk_type_name (enum_type);
      if (g_dataset_id_get_data (type_name, key_id_enum_vals))
	g_warning ("gtk_enum_set_values(): enum values for `%s' are already set",
		   type_name);
      else
	g_dataset_id_set_data (type_name,
			       key_id_enum_vals,
			       values);
      return;
    }

  g_warning ("gtk_enum_set_values(): type `%s' is not derived from `enum' or `flags'",
	     gtk_type_name (enum_type));
}

static void
gtk_type_class_init (GtkTypeNode *node)
{
  if (!node->klass && node->type_info.class_size)
    {
      node->klass = g_malloc0 (node->type_info.class_size);

      if (node->parent_type)
	{
	  GtkTypeNode *parent;

	  LOOKUP_TYPE_NODE (parent, node->parent_type);
	  if (!parent->klass)
	    gtk_type_class_init (parent);

	  if (parent->klass)
	    memcpy (node->klass, parent->klass, parent->type_info.class_size);
	}

      if (gtk_type_is_a (node->type, GTK_TYPE_OBJECT))
	{
	  GtkObjectClass *object_class;

	  /* FIXME: this initialization needs to be done through
	   * a function pointer someday.
	   */
	  g_assert (node->type_info.class_size >= sizeof (GtkObjectClass));
	  
	  object_class = node->klass;
	  object_class->type = node->type;
	  object_class->signals = NULL;
	  object_class->nsignals = 0;
	  object_class->n_args = 0;
	}
      
      if (node->type_info.class_init_func)
	(* node->type_info.class_init_func) (node->klass);
    }
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

static inline GtkType
gtk_type_register_builtin (gchar   *name,
			   GtkType parent)
{
  GtkTypeInfo info;

  info.type_name = name;
  info.object_size = info.class_size = 0;
  info.class_init_func = NULL;
  info.object_init_func = NULL;
  info.arg_set_func = NULL;
  info.arg_get_func = NULL;

  return gtk_type_create (parent, name, &info);
}

extern void gtk_object_init_type (void);

#include "makeenums.h"			/* include for various places
					 * with enum definitions
					 */
#include "gtktypebuiltins_vars.c"	/* type variable declarations
					 */
#include "gtktypebuiltins_evals.c"	/* enum value definition arrays
					 */

static void
gtk_type_init_builtin_types (void)
{
  /* GTK_TYPE_INVALID has typeid 0.  The first type id returned by
   * gtk_type_unique is 1, which is GTK_TYPE_NONE.  And so on.
   */

  struct {
    GtkType type_id;
    gchar *name;
  } fundamental_info[] = {
    { GTK_TYPE_NONE,		"void" },
    { GTK_TYPE_CHAR,		"char" },
    { GTK_TYPE_BOOL,		"bool" },
    { GTK_TYPE_INT,		"int" },
    { GTK_TYPE_UINT,		"uint" },
    { GTK_TYPE_LONG,		"long" },
    { GTK_TYPE_ULONG,		"ulong" },
    { GTK_TYPE_FLOAT,		"float" },
    { GTK_TYPE_DOUBLE,		"double" },
    { GTK_TYPE_STRING,		"string" },
    { GTK_TYPE_ENUM,		"enum" },
    { GTK_TYPE_FLAGS,		"flags" },
    { GTK_TYPE_BOXED,		"boxed" },
    { GTK_TYPE_FOREIGN,		"foreign" },
    { GTK_TYPE_CALLBACK,	"callback" },
    { GTK_TYPE_ARGS,		"args" },
    
    { GTK_TYPE_POINTER,		"pointer" },
    { GTK_TYPE_SIGNAL,		"signal" },
    { GTK_TYPE_C_CALLBACK,	"c_callback" }
  };
  struct {
    gchar *type_name;
    GtkType *type_id;
    GtkType parent;
    GtkEnumValue *values;
  } builtin_info[GTK_TYPE_NUM_BUILTINS + 1] = {
#include "gtktypebuiltins_ids.c"	/* type entries */
    { NULL }
  };
  guint i;
  
  for (i = 0; i < sizeof (fundamental_info) / sizeof (fundamental_info[0]); i++)
    {
      GtkType type_id;

      type_id = gtk_type_register_builtin (fundamental_info[i].name,
				      GTK_TYPE_INVALID);

      g_assert (type_id == fundamental_info[i].type_id);
    }
  
  gtk_object_init_type ();
  
  for (i = 0; i < GTK_TYPE_NUM_BUILTINS; i++)
    {
      GtkType type_id;

      g_assert (builtin_info[i].type_name != NULL);

      type_id =
	gtk_type_register_builtin (builtin_info[i].type_name,
				   builtin_info[i].parent);

      g_assert (type_id != GTK_TYPE_INVALID);

      (*builtin_info[i].type_id) = type_id;

      if (builtin_info[i].values)
	{
	  g_assert (gtk_type_is_a (type_id, GTK_TYPE_ENUM) ||
		    gtk_type_is_a (type_id, GTK_TYPE_FLAGS));

	  gtk_enum_set_values (type_id, builtin_info[i].values);
	}
    }
}
