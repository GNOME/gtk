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
#include <string.h>
#include "gtkobject.h"
#include "gtktypeutils.h"
#include "gtkcontainer.h"


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

static void  gtk_type_class_init		(GtkType      node_type);
static guint gtk_type_name_hash			(const char  *key);
static gint  gtk_type_name_compare		(const char  *a,
						 const char  *b);
static void  gtk_type_init_builtin_types	(void);

static GtkTypeNode *type_nodes = NULL;
static guint	    n_type_nodes = 0;
static GHashTable  *type_name_2_type_ht = NULL;


static GtkTypeNode*
gtk_type_node_next_and_invalidate (void)
{
  static guint	n_free_type_nodes = 0;
  register GtkTypeNode	*node;
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
  
  /* we need to call the base classes' object_init_func for derived
   * objects with the object's ->klass field still pointing to the
   * corresponding base class, otherwise overridden class functions
   * could get called with partly-initialized objects.
   */
  for (i = node->n_supers; i > 0; i--)
    {
      GtkTypeNode *pnode;
      
      LOOKUP_TYPE_NODE (pnode, node->supers[i]);
      if (pnode->type_info.object_init_func)
	{
	  object->klass = pnode->klass;
	  pnode->type_info.object_init_func (object);
	}
    }
  object->klass = klass;
  if (node->type_info.object_init_func)
    node->type_info.object_init_func (object);
  
  return object;
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
  gboolean first;
  
  LOOKUP_TYPE_NODE (node, type);
  first = TRUE;
  
  while (node)
    {
      if (first)
	{
	  first = FALSE;
	  g_message ("is a ");
	}
      
      if (node->type_info.type_name)
	g_message ("%s\n", node->type_info.type_name);
      else
	g_message ("<unnamed type>\n");
      
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
      
      for (i = 0; i < indent; i++)
	g_message (" ");
      
      if (node->type_info.type_name)
	g_message ("%s", node->type_info.type_name);
      else
	g_message ("(no-name)");
      
      if (show_size)
	g_message (" ( %d bytes )\n", node->type_info.object_size);
      else
	g_message ("\n");
      
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

static void
gtk_type_class_init (GtkType type)
{
  GtkTypeNode *node;

  /* we need to relookup nodes everytime we called an external function */
  LOOKUP_TYPE_NODE (node, type);
  
  if (!node->klass && node->type_info.class_size)
    {
      GtkObjectClass *object_class;
      GtkTypeNode *base_node;
      GSList *slist;
      
      g_assert (node->type_info.class_size >= sizeof (GtkObjectClass));
      
      node->klass = g_malloc0 (node->type_info.class_size);
      
      if (node->parent_type)
	{
	  GtkTypeNode *parent;
	  
	  LOOKUP_TYPE_NODE (parent, node->parent_type);
	  if (!parent->klass)
	    {
	      gtk_type_class_init (parent->type);
	      LOOKUP_TYPE_NODE (node, type);
	      LOOKUP_TYPE_NODE (parent, node->parent_type);
	    }
	  
	  if (parent->klass)
	    memcpy (node->klass, parent->klass, parent->type_info.class_size);
	}
      
      object_class = node->klass;
      object_class->type = node->type;
      
      /* stack all base class initialization functions, so we
       * call them in ascending order.
       */
      base_node = node;
      slist = NULL;
      while (base_node)
	{
	  if (base_node->type_info.base_class_init_func)
	    slist = g_slist_prepend (slist, base_node->type_info.base_class_init_func);
	  LOOKUP_TYPE_NODE (base_node, base_node->parent_type);
	}
      if (slist)
	{
	  GSList *walk;
	  
	  for (walk = slist; walk; walk = walk->next)
	    {
	      register GtkClassInitFunc base_class_init;
	      
	      base_class_init = walk->data;
	      base_class_init (node->klass);
	      LOOKUP_TYPE_NODE (node, type);
	    }
	  g_slist_free (slist);
	}
      
      if (node->type_info.class_init_func)
	node->type_info.class_init_func (node->klass);
    }
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
      while (vals)
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

static inline GtkType
gtk_type_register_intern (gchar	       *name,
			  GtkType	parent,
			  GtkEnumValue *values)
{
  GtkType type_id;
  GtkTypeInfo info;
  
  info.type_name = name;
  info.object_size = 0;
  info.class_size = 0;
  info.class_init_func = NULL;
  info.object_init_func = NULL;
  info.reserved_1 = values;
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
      
      /* relookup pointers afterwards.
       */
      type_id = gtk_type_register_intern (fundamental_info[i].name, GTK_TYPE_INVALID, NULL);
      
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
      
      g_assert (type_id != GTK_TYPE_INVALID);
      
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
