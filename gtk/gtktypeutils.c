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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <string.h>
#include "gtktypeutils.h"


#define	TYPE_NODES_BLOCK_SIZE	(35)  /* needs to be > GTK_TYPE_FUNDAMENTAL_MAX */

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
    GtkTypeNode *__node = NULL; \
    GtkType sqn = GTK_TYPE_SEQNO (type); \
    if (sqn > 0) \
      { \
	sqn--; \
	if (sqn < GTK_TYPE_FUNDAMENTAL_MAX) \
	  { \
	    if (sqn < n_ftype_nodes) \
	      __node = type_nodes + sqn; \
	  } \
	else if (sqn < n_type_nodes) \
	  __node = type_nodes + sqn; \
      } \
    node_var = __node; \
}

static void  gtk_type_class_init		(GtkType      node_type);
static void  gtk_type_init_builtin_types	(void);

static GtkTypeNode *type_nodes = NULL;
static guint	    n_type_nodes = 0;
static guint	    n_ftype_nodes = 0;
static GHashTable  *type_name_2_type_ht = NULL;


static GtkTypeNode*
gtk_type_node_next_and_invalidate (GtkType parent_type)
{
  static guint n_free_type_nodes = 0;
  GtkTypeNode *node;
  
  /* don't keep *any* GtkTypeNode pointers across invokation of this function!!!
   */
  
  if (n_free_type_nodes == 0)
    {
      guint i;
      guint size;
      
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
      if (!n_type_nodes)
	{
	  n_type_nodes = GTK_TYPE_FUNDAMENTAL_MAX;
	  n_free_type_nodes -= GTK_TYPE_FUNDAMENTAL_MAX;
	}
    }

  if (!parent_type)
    {
      g_assert (n_ftype_nodes < GTK_TYPE_FUNDAMENTAL_MAX); /* paranoid */

      node = type_nodes + n_ftype_nodes;
      n_ftype_nodes++;
      node->type = n_ftype_nodes;
    }
  else
    {
      node = type_nodes + n_type_nodes;
      n_type_nodes++;
      n_free_type_nodes--;
      node->type = GTK_TYPE_MAKE (parent_type, n_type_nodes);
    }
  
  return node;
}

void
gtk_type_init (void)
{
  if (n_type_nodes == 0)
    {
      g_assert (sizeof (GtkType) >= 4);
      g_assert (TYPE_NODES_BLOCK_SIZE > GTK_TYPE_FUNDAMENTAL_MAX);
      
      type_name_2_type_ht = g_hash_table_new (g_str_hash, g_str_equal);
      
      gtk_type_init_builtin_types ();
    }
}

void
gtk_type_set_chunk_alloc (GtkType      type,
			  guint	       n_chunks)
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
		 const GtkTypeInfo *type_info)
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
  new_node = gtk_type_node_next_and_invalidate (parent_type);
  
  if (parent_type)
    {
      g_assert (GTK_TYPE_SEQNO (new_node->type) > GTK_TYPE_FUNDAMENTAL_MAX);
      LOOKUP_TYPE_NODE (parent, parent_type);
    }
  else
    {
      g_assert (new_node->type <= GTK_TYPE_FUNDAMENTAL_MAX);
      parent = NULL;
    }
  
  new_node->type_info = *type_info;
  new_node->type_info.type_name = type_name;
  /* new_node->type_info.reserved_1 = NULL; */
  new_node->type_info.reserved_2 = NULL;
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
		 const GtkTypeInfo *type_info)
{
  GtkType new_type;
  gchar *type_name;
  
  g_return_val_if_fail (type_info != NULL, 0);
  g_return_val_if_fail (type_info->type_name != NULL, 0);
  
  if (!parent_type && n_ftype_nodes >= GTK_TYPE_FUNDAMENTAL_MAX)
    {
      g_warning ("gtk_type_unique(): maximum amount of fundamental types reached, "
		 "try increasing GTK_TYPE_FUNDAMENTAL_MAX");
      return 0;
    }

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
	    {
	      type = node->type;
	      gtk_type_class_init (type);
	      LOOKUP_TYPE_NODE (node, type);
	    }
	  
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
    {
      type = node->type;
      gtk_type_class_init (type);
      LOOKUP_TYPE_NODE (node, type);
    }
  
  return node->klass;
}

gpointer
gtk_type_new (GtkType type)
{
  GtkTypeNode *node;
  GtkTypeObject *tobject;
  gpointer klass;
  
  LOOKUP_TYPE_NODE (node, type);
  g_return_val_if_fail (node != NULL, NULL);
  
  klass = node->klass;
  if (!klass)
    {
      klass = gtk_type_class (type);
      LOOKUP_TYPE_NODE (node, type);
    }
  node->chunk_alloc_locked = TRUE;

  if (node->mem_chunk)
    tobject = g_mem_chunk_alloc0 (node->mem_chunk);
  else
    tobject = g_malloc0 (node->type_info.object_size);
  
  /* we need to call the base classes' object_init_func for derived
   * objects with the object's ->klass field still pointing to the
   * corresponding base class, otherwise overridden class functions
   * could get called with partly-initialized objects. the real object
   * class is passed as second argment to the initializers.
   */
  if (node->n_supers)
    {
      guint i;
      GtkType *supers;
      GtkTypeNode *pnode;

      supers = node->supers;
      for (i = node->n_supers; i > 0; i--)
	{
	  LOOKUP_TYPE_NODE (pnode, supers[i]);
	  if (pnode->type_info.object_init_func)
	    {
	      tobject->klass = pnode->klass;
	      pnode->type_info.object_init_func (tobject, klass);
	    }
	}
      LOOKUP_TYPE_NODE (node, type);
    }
  tobject->klass = klass;
  if (node->type_info.object_init_func)
    {
      node->type_info.object_init_func (tobject, klass);
      tobject->klass = klass;
    }
  
  return tobject;
}

void
gtk_type_free (GtkType	    type,
	       gpointer	    mem)
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

GList*
gtk_type_children_types (GtkType type)
{
  GtkTypeNode *node;
  
  LOOKUP_TYPE_NODE (node, type);
  if (node)
    return node->children_types;
  
  return NULL;
}

void
gtk_type_describe_heritage (GtkType type)
{
  GtkTypeNode *node;
  gchar *is_a = "";
  
  LOOKUP_TYPE_NODE (node, type);
  
  while (node)
    {
      if (node->type_info.type_name)
	g_message ("%s%s",
		   is_a,
		   node->type_info.type_name);
      else
	g_message ("%s<unnamed type>",
		   is_a);
      is_a = "is a ";
      
      LOOKUP_TYPE_NODE (node, node->parent_type);
    }
}

void
gtk_type_describe_tree (GtkType	 type,
			gboolean show_size)
{
  GtkTypeNode *node;
  
  LOOKUP_TYPE_NODE (node, type);
  
  if (node)
    {
      static gint indent = 0;
      GList *list;
      guint old_indent;
      guint i;
      GString *gstring;

      gstring = g_string_new ("");
      
      for (i = 0; i < indent; i++)
	g_string_append_c (gstring, ' ');
      
      if (node->type_info.type_name)
	g_string_append (gstring, node->type_info.type_name);
      else
	g_string_append (gstring, "<unnamed type>");
      
      if (show_size)
	g_string_sprintfa (gstring, " (%d bytes)", node->type_info.object_size);

      g_message ("%s", gstring->str);
      g_string_free (gstring, TRUE);
      
      old_indent = indent;
      indent += 4;
      
      for (list = node->children_types; list; list = list->next)
	gtk_type_describe_tree (GPOINTER_TO_UINT (list->data), show_size);
      
      indent = old_indent;
    }
}

gboolean
gtk_type_is_a (GtkType type,
	       GtkType is_a_type)
{
  if (type == is_a_type)
    return TRUE;
  else
    {
      GtkTypeNode *node;
      
      LOOKUP_TYPE_NODE (node, type);
      if (node)
	{
	  GtkTypeNode *a_node;
	  
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

static void
gtk_type_class_init (GtkType type)
{
  GtkTypeNode *node;

  /* we need to relookup nodes everytime we called an external function */
  LOOKUP_TYPE_NODE (node, type);
  
  if (!node->klass && node->type_info.class_size)
    {
      GtkTypeClass *type_class;
      GtkTypeNode *base_node;
      GSList *slist;
      
      if (node->type_info.class_size < sizeof (GtkTypeClass))
	g_warning ("The `%s' class is too small to inherit from GtkTypeClass",
		   node->type_info.type_name);
      
      node->klass = g_malloc0 (node->type_info.class_size);
      
      if (node->parent_type)
	{
	  GtkTypeNode *parent;
	  
	  LOOKUP_TYPE_NODE (parent, node->parent_type);
	  
	  if (node->type_info.class_size < parent->type_info.class_size)
	    g_warning ("The `%s' class is smaller than its parent class `%s'",
		       node->type_info.type_name,
		       parent->type_info.type_name);
	  
	  if (!parent->klass)
	    {
	      gtk_type_class_init (parent->type);
	      LOOKUP_TYPE_NODE (node, type);
	      LOOKUP_TYPE_NODE (parent, node->parent_type);
	    }
	  
	  if (parent->klass)
	    memcpy (node->klass, parent->klass, parent->type_info.class_size);
	}
      
      type_class = node->klass;
      type_class->type = node->type;
      
      /* stack all base class initialization functions, so we
       * call them in ascending order.
       */
      base_node = node;
      slist = NULL;
      while (base_node)
	{
	  if (base_node->type_info.base_class_init_func)
	    slist = g_slist_prepend (slist, (gpointer) base_node->type_info.base_class_init_func);
	  LOOKUP_TYPE_NODE (base_node, base_node->parent_type);
	}
      if (slist)
	{
	  GSList *walk;
	  
	  for (walk = slist; walk; walk = walk->next)
	    {
	      GtkClassInitFunc base_class_init;
	      
	      base_class_init = (GtkClassInitFunc) walk->data;
	      base_class_init (node->klass);
	      LOOKUP_TYPE_NODE (node, type);
	    }
	  g_slist_free (slist);
	}
      
      if (node->type_info.class_init_func)
	node->type_info.class_init_func (node->klass);
    }
}

static inline gchar*
gtk_type_descriptive_name (GtkType type)
{
  gchar *name;

  name = gtk_type_name (type);
  if (!name)
    name = "(unknown)";

  return name;
}

GtkTypeObject*
gtk_type_check_object_cast (GtkTypeObject  *type_object,
			    GtkType         cast_type)
{
  if (!type_object)
    {
      g_warning ("invalid cast from (NULL) pointer to `%s'",
		 gtk_type_descriptive_name (cast_type));
      return type_object;
    }
  if (!type_object->klass)
    {
      g_warning ("invalid unclassed pointer in cast to `%s'",
		 gtk_type_descriptive_name (cast_type));
      return type_object;
    }
  /* currently, GTK_TYPE_OBJECT is the lowest fundamental type
   * dominator for types that introduce classes.
   */
  if (type_object->klass->type < GTK_TYPE_OBJECT)
    {
      g_warning ("invalid class type `%s' in cast to `%s'",
		 gtk_type_descriptive_name (type_object->klass->type),
		 gtk_type_descriptive_name (cast_type));
      return type_object;
    }
  if (!gtk_type_is_a (type_object->klass->type, cast_type))
    {
      g_warning ("invalid cast from `%s' to `%s'",
		 gtk_type_descriptive_name (type_object->klass->type),
		 gtk_type_descriptive_name (cast_type));
      return type_object;
    }

  return type_object;
}

GtkTypeClass*
gtk_type_check_class_cast (GtkTypeClass   *klass,
			   GtkType         cast_type)
{
  if (!klass)
    {
      g_warning ("invalid class cast from (NULL) pointer to `%s'",
		 gtk_type_descriptive_name (cast_type));
      return klass;
    }
  /* currently, GTK_TYPE_OBJECT is the lowest fundamental type
   * dominator for types that introduce classes.
   */
  if (klass->type < GTK_TYPE_OBJECT)
    {
      g_warning ("invalid class type `%s' in class cast to `%s'",
		 gtk_type_descriptive_name (klass->type),
		 gtk_type_descriptive_name (cast_type));
      return klass;
    }
  if (!gtk_type_is_a (klass->type, cast_type))
    {
      g_warning ("invalid class cast from `%s' to `%s'",
		 gtk_type_descriptive_name (klass->type),
		 gtk_type_descriptive_name (cast_type));
      return klass;
    }

  return klass;
}

GtkEnumValue*
gtk_type_enum_get_values (GtkType      enum_type)
{
  if (GTK_FUNDAMENTAL_TYPE (enum_type) == GTK_TYPE_ENUM ||
      GTK_FUNDAMENTAL_TYPE (enum_type) == GTK_TYPE_FLAGS)
    {
      GtkTypeNode *node;
      
      LOOKUP_TYPE_NODE (node, enum_type);
      if (node)
	return (GtkEnumValue*) node->type_info.reserved_1;
    }
  
  g_warning ("gtk_type_enum_get_values(): type `%s' is not derived from `GtkEnum' or `GtkFlags'",
	     gtk_type_name (enum_type));
  
  return NULL;
}

GtkFlagValue*
gtk_type_flags_get_values (GtkType	  flags_type)
{
  return gtk_type_enum_get_values (flags_type);
}

GtkEnumValue*
gtk_type_enum_find_value (GtkType        enum_type,
			  const gchar    *value_name)
{
  g_return_val_if_fail (value_name != NULL, NULL);
  
  if (GTK_FUNDAMENTAL_TYPE (enum_type) == GTK_TYPE_ENUM ||
      GTK_FUNDAMENTAL_TYPE (enum_type) == GTK_TYPE_FLAGS)
    {
      GtkEnumValue *vals;

      vals = gtk_type_enum_get_values (enum_type);
      if (vals)
	while (vals->value_name)
	  {
	    if (strcmp (vals->value_name, value_name) == 0 ||
		strcmp (vals->value_nick, value_name) == 0)
	      return vals;
	    vals++;
	  }
    }
  else
    g_warning ("gtk_type_enum_find_value(): type `%s' is not derived from `GtkEnum' or `GtkFlags'",
	       gtk_type_name (enum_type));
  
  return NULL;
}

GtkFlagValue*
gtk_type_flags_find_value (GtkType         flag_type,
			   const gchar    *value_name)
{
  g_return_val_if_fail (value_name != NULL, NULL);

  return gtk_type_enum_find_value (flag_type, value_name);
}

typedef struct _GtkTypeVarargType GtkTypeVarargType;
struct _GtkTypeVarargType
{
  GtkType foreign_type;
  GtkType varargs_type;
};

static GtkTypeVarargType *vararg_types = NULL;
static guint              n_vararg_types = 0;

void
gtk_type_set_varargs_type (GtkType        foreign_type,
			   GtkType        varargs_type)
{
  g_return_if_fail (foreign_type == GTK_FUNDAMENTAL_TYPE (foreign_type));
  g_return_if_fail (foreign_type > GTK_TYPE_FUNDAMENTAL_LAST);

  if (!((varargs_type >= GTK_TYPE_STRUCTURED_FIRST &&
	 varargs_type <= GTK_TYPE_STRUCTURED_LAST) ||
	(varargs_type >= GTK_TYPE_FLAT_FIRST &&
	 varargs_type <= GTK_TYPE_FLAT_LAST) ||
	varargs_type == GTK_TYPE_NONE))
    {
      g_warning ("invalid varargs type `%s' for fundamental type `%s'",
		 gtk_type_name (varargs_type),
		 gtk_type_name (foreign_type));
      return;
    }
  if (gtk_type_get_varargs_type (foreign_type))
    {
      g_warning ("varargs type is already registered for fundamental type `%s'",
		 gtk_type_name (foreign_type));
      return;
    }

  n_vararg_types++;
  vararg_types = g_realloc (vararg_types, sizeof (GtkTypeVarargType) * n_vararg_types);

  vararg_types[n_vararg_types - 1].foreign_type = foreign_type;
  vararg_types[n_vararg_types - 1].varargs_type = varargs_type;
}

GtkType
gtk_type_get_varargs_type (GtkType foreign_type)
{
  GtkType type;
  guint i;

  type = GTK_FUNDAMENTAL_TYPE (foreign_type);
  if (type <= GTK_TYPE_FUNDAMENTAL_LAST)
    return type;

  for (i = 0; i < n_vararg_types; i++)
    if (vararg_types[i].foreign_type == type)
      return vararg_types[i].varargs_type;

  return 0;
}

static inline GtkType
gtk_type_register_intern (gchar	             *name,
			  GtkType  	      parent,
			  const GtkEnumValue *values)
{
  GtkType type_id;
  GtkTypeInfo info;
  
  info.type_name = name;
  info.object_size = 0;
  info.class_size = 0;
  info.class_init_func = NULL;
  info.object_init_func = NULL;
  info.reserved_1 = (gpointer) values;
  info.reserved_2 = NULL;
  
  /* relookup pointers afterwards.
   */
  type_id = gtk_type_create (parent, name, &info);
  
  if (type_id && values)
    {
      guint i;
      
      /* check for proper type consistency and NULL termination
       * of value array
       */
      g_assert (GTK_FUNDAMENTAL_TYPE (type_id) == GTK_TYPE_ENUM ||
		GTK_FUNDAMENTAL_TYPE (type_id) == GTK_TYPE_FLAGS);
      
      i = 0;
      while (values[i].value_name)
	i++;
      
      g_assert (values[i].value_name == NULL && values[i].value_nick == NULL);
    }
  
  return type_id;
}

GtkType
gtk_type_register_enum (const gchar    *type_name,
			GtkEnumValue   *values)
{
  GtkType type_id;
  gchar *name;
  
  g_return_val_if_fail (type_name != NULL, 0);
  
  name = g_strdup (type_name);
  
  /* relookup pointers afterwards.
   */
  type_id = gtk_type_register_intern (name, GTK_TYPE_ENUM, values);
  
  if (!type_id)
    g_free (name);
  
  return type_id;
}

GtkType
gtk_type_register_flags (const gchar	*type_name,
			 GtkFlagValue	*values)
{
  GtkType type_id;
  gchar *name;
  
  g_return_val_if_fail (type_name != NULL, 0);
  
  name = g_strdup (type_name);
  
  /* relookup pointers afterwards.
   */
  type_id = gtk_type_register_intern (name, GTK_TYPE_FLAGS, values);
  
  if (!type_id)
    g_free (name);
  
  return type_id;
}

GtkTypeQuery*
gtk_type_query (GtkType type)
{
  GtkTypeNode *node;
  
  LOOKUP_TYPE_NODE (node, type);
  if (node)
    {
      GtkTypeQuery *query;

      query = g_new0 (GtkTypeQuery, 1);
      query->type = type;
      query->type_name = node->type_info.type_name;
      query->object_size = node->type_info.object_size;
      query->class_size = node->type_info.class_size;

      return query;
    }
  
  return NULL;
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
  
  static const struct {
    GtkType type_id;
    gchar *name;
  } fundamental_info[] = {
    { GTK_TYPE_NONE,		"void" },
    { GTK_TYPE_CHAR,		"gchar" },
    { GTK_TYPE_UCHAR,		"guchar" },
    { GTK_TYPE_BOOL,		"gboolean" },
    { GTK_TYPE_INT,		"gint" },
    { GTK_TYPE_UINT,		"guint" },
    { GTK_TYPE_LONG,		"glong" },
    { GTK_TYPE_ULONG,		"gulong" },
    { GTK_TYPE_FLOAT,		"gfloat" },
    { GTK_TYPE_DOUBLE,		"gdouble" },
    { GTK_TYPE_STRING,		"GtkString" },
    { GTK_TYPE_ENUM,		"GtkEnum" },
    { GTK_TYPE_FLAGS,		"GtkFlags" },
    { GTK_TYPE_BOXED,		"GtkBoxed" },
    { GTK_TYPE_POINTER,		"gpointer" },
    
    { GTK_TYPE_SIGNAL,		"GtkSignal" },
    { GTK_TYPE_ARGS,		"GtkArgs" },
    { GTK_TYPE_CALLBACK,	"GtkCallback" },
    { GTK_TYPE_C_CALLBACK,	"GtkCCallback" },
    { GTK_TYPE_FOREIGN,		"GtkForeign" },
  };
  static struct {
    gchar *type_name;
    GtkType *type_id;
    GtkType parent;
    const GtkEnumValue *values;
  } builtin_info[GTK_TYPE_NUM_BUILTINS + 1] = {
#include "gtktypebuiltins_ids.c"	/* type entries */
    { NULL }
  };
  guint i;
  
  for (i = 0; i < sizeof (fundamental_info) / sizeof (fundamental_info[0]); i++)
    {
      GtkType type_id;
      
      /* relookup pointers afterwards.
       */
      type_id = gtk_type_register_intern (fundamental_info[i].name, 0, NULL);
      
      g_assert (type_id == fundamental_info[i].type_id);
    }
  
  gtk_object_init_type ();
  
  for (i = 0; i < GTK_TYPE_NUM_BUILTINS; i++)
    {
      GtkType type_id;
      
      g_assert (builtin_info[i].type_name != NULL);
      
      /* relookup pointers afterwards.
       */
      type_id = gtk_type_register_intern (builtin_info[i].type_name,
					  builtin_info[i].parent,
					  builtin_info[i].values);
      
      g_assert (GTK_TYPE_SEQNO (type_id) > GTK_TYPE_FUNDAMENTAL_MAX);
      
      (*builtin_info[i].type_id) = type_id;
    }
}

GtkType
gtk_identifier_get_type (void)
{
  static GtkType identifier_type = 0;
  
  if (!identifier_type)
    identifier_type = gtk_type_register_intern ("GtkIdentifier", GTK_TYPE_STRING, NULL);
  
  return identifier_type;
}
