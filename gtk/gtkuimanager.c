/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include <string.h>
#include "gtkuimanager.h"
#include "gtktoolbar.h"
#include "gtkseparatortoolitem.h"
#include "gtkmenushell.h"
#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkseparatormenuitem.h"
#include "gtktearoffmenuitem.h"
#include "gtkintl.h"

#undef DEBUG_UI_MANAGER

typedef enum 
{
  GTK_UI_MANAGER_UNDECIDED,
  GTK_UI_MANAGER_ROOT,
  GTK_UI_MANAGER_MENUBAR,
  GTK_UI_MANAGER_MENU,
  GTK_UI_MANAGER_TOOLBAR,
  GTK_UI_MANAGER_MENU_PLACEHOLDER,
  GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER,
  GTK_UI_MANAGER_POPUP,
  GTK_UI_MANAGER_MENUITEM,
  GTK_UI_MANAGER_TOOLITEM,
  GTK_UI_MANAGER_SEPARATOR,
} GtkUIManagerNodeType;


typedef struct _GtkUIManagerNode  GtkUIManagerNode;

struct _GtkUIManagerNode {
  GtkUIManagerNodeType type;

  const gchar *name;

  GQuark action_name;
  GtkAction *action;
  GtkWidget *proxy;
  GtkWidget *extra; /*GtkMenu for submenus, second separator for placeholders*/

  GList *uifiles;

  guint dirty : 1;
};

#define GTK_UI_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_UI_MANAGER, GtkUIManagerPrivate))

struct _GtkUIManagerPrivate 
{
  GtkAccelGroup *accel_group;

  GNode *root_node;
  GList *action_groups;

  guint last_merge_id;

  guint update_tag;  

  gboolean add_tearoffs;
};

#define NODE_INFO(node) ((GtkUIManagerNode *)node->data)

typedef struct _NodeUIReference NodeUIReference;

struct _NodeUIReference 
{
  guint merge_id;
  GQuark action_quark;
};

static void   gtk_ui_manager_class_init    (GtkUIManagerClass *class);
static void   gtk_ui_manager_init          (GtkUIManager      *self);
static void   gtk_ui_manager_set_property  (GObject         *object,
					    guint            prop_id,
					    const GValue    *value,
					    GParamSpec      *pspec);
static void   gtk_ui_manager_get_property  (GObject         *object,
					    guint            prop_id,
					    GValue          *value,
					    GParamSpec      *pspec);
static void   gtk_ui_manager_queue_update  (GtkUIManager *self);
static void   gtk_ui_manager_dirty_all     (GtkUIManager *self);

static GNode *get_child_node               (GtkUIManager *self, 
					    GNode *parent,
					    const gchar *childname,
					    gint childname_length,
					    GtkUIManagerNodeType node_type,
					    gboolean create, 
					    gboolean top);
static GNode *gtk_ui_manager_get_node      (GtkUIManager *self,
					    const gchar *path,
					    GtkUIManagerNodeType node_type,
					    gboolean create);
static guint gtk_ui_manager_next_merge_id  (GtkUIManager *self);

static void  gtk_ui_manager_node_prepend_ui_reference (GtkUIManagerNode *node,
						       guint merge_id,
						       GQuark action_quark);
static void  gtk_ui_manager_node_remove_ui_reference  (GtkUIManagerNode *node,
						       guint merge_id);
static void  gtk_ui_manager_ensure_update  (GtkUIManager *self);


enum 
{
  ADD_WIDGET,
  REMOVE_WIDGET,
  CHANGED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ADD_TEAROFFS
};

static guint merge_signals[LAST_SIGNAL] = { 0 };

static GMemChunk *merge_node_chunk = NULL;

GType
gtk_ui_manager_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
      {
        sizeof (GtkUIManagerClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_ui_manager_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        
        sizeof (GtkUIManager),
        0, /* n_preallocs */
        (GInstanceInitFunc) gtk_ui_manager_init,
      };

      type = g_type_register_static (G_TYPE_OBJECT,
				     "GtkUIManager",
				     &type_info, 0);
    }
  return type;
}

static void
gtk_ui_manager_class_init (GtkUIManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  if (!merge_node_chunk)
    merge_node_chunk = g_mem_chunk_create (GtkUIManagerNode, 64,
					   G_ALLOC_AND_FREE);

  gobject_class->set_property = gtk_ui_manager_set_property;
  gobject_class->get_property = gtk_ui_manager_get_property;

  /**
   * GtkUIManager:add-tearoffs:
   *
   * The add-tearoffs property controls whether generated menus 
   * have tearoff menu items. 
   *
   * Note that this only affects regular menus. Generated popup 
   * menus never have tearoff menu items.   
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ADD_TEAROFFS,
                                   g_param_spec_boolean ("add_tearoffs",
							 _("Add tearoffs to menus"),
							 _("Whether tearoff menu items should be added to menus"),
                                                         FALSE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  /**
   * GtkUIManager::add-widget:
   * @merge: a #GtkUIManager
   * @widget: the added widget
   *
   * The add_widget signal is emitted for each generated menubar and toolbar.
   * It is not emitted for generated popup menus, which can be obtained by 
   * gtk_ui_manager_get_widget().
   *
   * Since: 2.4
   */
  merge_signals[ADD_WIDGET] =
    g_signal_new ("add_widget",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, add_widget), NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, 
		  GTK_TYPE_WIDGET);

  merge_signals[REMOVE_WIDGET] =
    g_signal_new ("remove_widget",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, remove_widget), NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, 
		  GTK_TYPE_WIDGET);  
  /**
   * GtkUIManager::changed:
   * @merge: a #GtkUIManager
   *
   * The changed signal is emitted whenever the merged UI changes.
   *
   * Since: 2.4
   */
  merge_signals[CHANGED] =
    g_signal_new ("changed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, changed),  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  g_type_class_add_private (gobject_class, sizeof (GtkUIManagerPrivate));
}


static void
gtk_ui_manager_init (GtkUIManager *self)
{
  guint merge_id;
  GNode *node;

  self->private_data = GTK_UI_MANAGER_GET_PRIVATE (self);

  self->private_data->accel_group = gtk_accel_group_new ();

  self->private_data->root_node = NULL;
  self->private_data->action_groups = NULL;

  self->private_data->last_merge_id = 0;
  self->private_data->add_tearoffs = FALSE;


  merge_id = gtk_ui_manager_next_merge_id (self);
  node = get_child_node (self, NULL, "ui", 4,
			GTK_UI_MANAGER_ROOT, TRUE, FALSE);
  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (node), merge_id, 0);
}

static void
gtk_ui_manager_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  GtkUIManager *self = GTK_UI_MANAGER (object);
 
  switch (prop_id)
    {
    case PROP_ADD_TEAROFFS:
      gtk_ui_manager_set_add_tearoffs (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_ui_manager_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  GtkUIManager *self = GTK_UI_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ADD_TEAROFFS:
      g_value_set_boolean (value, self->private_data->add_tearoffs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/**
 * gtk_ui_manager_new:
 * 
 * Creates a new menu merge object.
 * 
 * Return value: a new menu merge object.
 *
 * Since: 2.4
 **/
GtkUIManager*
gtk_ui_manager_new (void)
{
  return g_object_new (GTK_TYPE_UI_MANAGER, NULL);
}


/**
 * gtk_ui_manager_get_add_tearoffs:
 * @self: a #GtkUIManager
 * 
 * Returns whether menus generated by this #GtkUIManager
 * will have tearoff menu items. 
 * 
 * Return value: whether tearoff menu items are added
 *
 * Since: 2.4
 **/
gboolean 
gtk_ui_manager_get_add_tearoffs (GtkUIManager *self)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), FALSE);
  
  return self->private_data->add_tearoffs;
}


/**
 * gtk_ui_manager_set_add_tearoffs:
 * @self: a #GtkUIManager
 * @add_tearoffs: whether tearoff menu items are added
 * 
 * Sets the add_tearoffs property, which controls whether menus 
 * generated by this #GtkUIManager will have tearoff menu items. 
 *
 * Note that this only affects regular menus. Generated popup 
 * menus never have tearoff menu items.
 *
 * Since: 2.4
 **/
void 
gtk_ui_manager_set_add_tearoffs (GtkUIManager *self,
				 gboolean      add_tearoffs)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (self));

  add_tearoffs = add_tearoffs != FALSE;

  if (add_tearoffs != self->private_data->add_tearoffs)
    {
      self->private_data->add_tearoffs = add_tearoffs;
      
      /* dirty all nodes */
      gtk_ui_manager_dirty_all (self);

      g_object_notify (G_OBJECT (self), "add_tearoffs");
    }
}

/**
 * gtk_ui_manager_insert_action_group:
 * @self: a #GtkUIManager object
 * @action_group: the action group to be inserted
 * @pos: the position at which the group will be inserted
 * 
 * Inserts an action group into the list of action groups associated 
 * with @self.
 *
 * Since: 2.4
 **/
void
gtk_ui_manager_insert_action_group (GtkUIManager   *self,
				    GtkActionGroup *action_group, 
				    gint            pos)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (self));
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  g_return_if_fail (g_list_find (self->private_data->action_groups, 
				 action_group) == NULL);

  g_object_ref (action_group);
  self->private_data->action_groups = 
    g_list_insert (self->private_data->action_groups, action_group, pos);

  /* dirty all nodes, as action bindings may change */
  gtk_ui_manager_dirty_all (self);
}

/**
 * gtk_ui_manager_remove_action_group:
 * @self: a #GtkUIManager object
 * @action_group: the action group to be removed
 * 
 * Removes an action group from the list of action groups associated 
 * with @self.
 *
 * Since: 2.4
 **/
void
gtk_ui_manager_remove_action_group (GtkUIManager   *self,
				    GtkActionGroup *action_group)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (self));
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  g_return_if_fail (g_list_find (self->private_data->action_groups, 
				 action_group) != NULL);

  self->private_data->action_groups =
    g_list_remove (self->private_data->action_groups, action_group);
  g_object_unref (action_group);

  /* dirty all nodes, as action bindings may change */
  gtk_ui_manager_dirty_all (self);
}

/**
 * gtk_ui_manager_get_action_groups:
 * @self: a #GtkUIManager object
 * 
 * Returns the list of action groups associated with @self.
 *
 * Return value: a #GList of action groups. The list is owned by GTK+ 
 *   and should not be modified.
 *
 * Since: 2.4
 **/
GList *
gtk_ui_manager_get_action_groups (GtkUIManager   *self)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), NULL);

  return self->private_data->action_groups;
}

/**
 * gtk_ui_manager_get_accel_group:
 * @self: a #GtkUIManager object
 * 
 * Returns the #GtkAccelGroup associated with @self.
 *
 * Return value: the #GtkAccelGroup.
 *
 * Since: 2.4
 **/
GtkAccelGroup *
gtk_ui_manager_get_accel_group (GtkUIManager   *self)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), NULL);

  return self->private_data->accel_group;
}

/**
 * gtk_ui_manager_get_widget:
 * @self: a #GtkUIManager
 * @path: a path
 * 
 * Looks up a widget by following a path. The path consists of the names 
 * specified in the XML description of the UI. separated by '/'. Elements which 
 * don't have a name attribute in the XML (e.g. &lt;popup&gt;) can be addressed 
 * by their XML element name (e.g. "popup"). The root element (&lt;ui&gt;) can 
 * be omitted in the path.
 * 
 * Return value: the widget found by following the path, or %NULL if no widget
 *   was found.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_ui_manager_get_widget (GtkUIManager *self, 
			   const gchar  *path)
{
  GNode *node;

  /* ensure that there are no pending updates before we get the
   * widget */
  gtk_ui_manager_ensure_update (self);

  node = gtk_ui_manager_get_node (self, path, GTK_UI_MANAGER_UNDECIDED, FALSE);

  if (node == NULL)
    return NULL;

  return NODE_INFO (node)->proxy;
}

static GNode *
get_child_node (GtkUIManager        *self, 
		GNode               *parent,
		const gchar         *childname, 
		gint                 childname_length,
		GtkUIManagerNodeType node_type,
		gboolean             create, 
		gboolean             top)
{
  GNode *child = NULL;

  g_return_val_if_fail (parent == NULL ||
			(NODE_INFO (parent)->type != GTK_UI_MANAGER_MENUITEM &&
			 NODE_INFO (parent)->type != GTK_UI_MANAGER_TOOLITEM), 
			NULL);

  if (parent)
    {
      if (childname)
	{
	  for (child = parent->children; child != NULL; child = child->next)
	    {
	      if (strlen (NODE_INFO (child)->name) == childname_length &&
		  !strncmp (NODE_INFO (child)->name, childname, childname_length))
		{
		  /* if undecided about node type, set it */
		  if (NODE_INFO (child)->type == GTK_UI_MANAGER_UNDECIDED)
		    NODE_INFO (child)->type = node_type;
		  
		  /* warn about type mismatch */
		  if (NODE_INFO (child)->type != GTK_UI_MANAGER_UNDECIDED &&
		      node_type != GTK_UI_MANAGER_UNDECIDED &&
		      NODE_INFO (child)->type != node_type)
		    g_warning ("node type doesn't match %d (%s is type %d)",
			       node_type, 
			       NODE_INFO (child)->name,
			       NODE_INFO (child)->type);
		  
		  return child;
		}
	    }
	}
      if (!child && create)
	{
	  GtkUIManagerNode *mnode;
	  
	  mnode = g_chunk_new0 (GtkUIManagerNode, merge_node_chunk);
	  mnode->type = node_type;
	  mnode->name = g_strndup (childname, childname_length);
	  mnode->dirty = TRUE;

	  if (top)
	    child = g_node_prepend_data (parent, mnode);
	  else
	    child = g_node_append_data (parent, mnode);
	}
    }
  else
    {
      /* handle root node */
      if (self->private_data->root_node)
	{
	  child = self->private_data->root_node;
	  if (strncmp (NODE_INFO (child)->name, childname, childname_length) != 0)
	    g_warning ("root node name '%s' doesn't match '%s'",
		       childname, NODE_INFO (child)->name);
	  if (NODE_INFO (child)->type != GTK_UI_MANAGER_ROOT)
	    g_warning ("base element must be of type ROOT");
	}
      else if (create)
	{
	  GtkUIManagerNode *mnode;

	  mnode = g_chunk_new0 (GtkUIManagerNode, merge_node_chunk);
	  mnode->type = node_type;
	  mnode->name = g_strndup (childname, childname_length);
	  mnode->dirty = TRUE;
	  
	  child = self->private_data->root_node = g_node_new (mnode);
	}
    }

  return child;
}

static GNode *
gtk_ui_manager_get_node (GtkUIManager        *self, 
			 const gchar         *path,
			 GtkUIManagerNodeType node_type, 
			 gboolean             create)
{
  const gchar *pos, *end;
  GNode *parent, *node;
  
  end = path + strlen (path);
  pos = path;
  parent = node = NULL;
  while (pos < end)
    {
      const gchar *slash;
      gsize length;

      slash = strchr (pos, '/');
      if (slash)
	length = slash - pos;
      else
	length = strlen (pos);

      node = get_child_node (self, parent, pos, length, GTK_UI_MANAGER_UNDECIDED,
			     create, FALSE);
      if (!node)
	return NULL;
      
      pos += length + 1; /* move past the node name and the slash too */
      parent = node;
    }

  if (NODE_INFO (node)->type == GTK_UI_MANAGER_UNDECIDED)
    NODE_INFO (node)->type = node_type;
  return node;
}

static guint
gtk_ui_manager_next_merge_id (GtkUIManager *self)
{
  self->private_data->last_merge_id++;

  return self->private_data->last_merge_id;
}

static void
gtk_ui_manager_node_prepend_ui_reference (GtkUIManagerNode *node,
					  guint             merge_id, 
					  GQuark            action_quark)
{
  NodeUIReference *reference;

  reference = g_new (NodeUIReference, 1);
  reference->action_quark = action_quark;
  reference->merge_id = merge_id;

  /* Prepend the reference */
  node->uifiles = g_list_prepend (node->uifiles, reference);

  node->dirty = TRUE;
}

static void
gtk_ui_manager_node_remove_ui_reference (GtkUIManagerNode *node,
					 guint             merge_id)
{
  GList *p;
  
  for (p = node->uifiles; p != NULL; p = p->next)
    {
      NodeUIReference *reference = p->data;
      
      if (reference->merge_id == merge_id)
	{
	  node->uifiles = g_list_remove_link (node->uifiles, p);
	  node->dirty = TRUE;
	  g_free (reference);

	  break;
	}
    }
}

/* -------------------- The UI file parser -------------------- */

typedef enum
{
  STATE_START,
  STATE_ROOT,
  STATE_MENU,
  STATE_TOOLBAR,
  STATE_MENUITEM,
  STATE_TOOLITEM,
  STATE_END
} ParseState;

typedef struct _ParseContext ParseContext;
struct _ParseContext
{
  ParseState state;
  ParseState prev_state;

  GtkUIManager *self;

  GNode *current;

  guint merge_id;
};

static void
start_element_handler (GMarkupParseContext *context,
		       const gchar         *element_name,
		       const gchar        **attribute_names,
		       const gchar        **attribute_values,
		       gpointer             user_data,
		       GError             **error)
{
  ParseContext *ctx = user_data;
  GtkUIManager *self = ctx->self;

  gint i;
  const gchar *node_name;
  const gchar *action;
  GQuark action_quark;
  gboolean top;

  gboolean raise_error = TRUE;
  gchar *error_attr = NULL;

  node_name = NULL;
  action = NULL;
  action_quark = 0;
  top = FALSE;
  for (i = 0; attribute_names[i] != NULL; i++)
    {
      if (!strcmp (attribute_names[i], "name"))
	{
	  node_name = attribute_values[i];
	}
      else if (!strcmp (attribute_names[i], "action"))
	{
	  action = attribute_values[i];
	  action_quark = g_quark_from_string (attribute_values[i]);
	}
      else if (!strcmp (attribute_names[i], "pos"))
	{
	  top = !strcmp (attribute_values[i], "top");
	}
    }

  /* Work out a name for this node.  Either the name attribute, or
   * the action, or the element name */
  if (node_name == NULL) 
    {
      if (action != NULL)
	node_name = action;
      else 
	node_name = element_name;
    }

  switch (element_name[0])
    {
    case 'u':
      if (ctx->state == STATE_START && !strcmp (element_name, "ui"))
	{
	  ctx->state = STATE_ROOT;
	  ctx->current = self->private_data->root_node;
	  raise_error = FALSE;

	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (ctx->current),
						    ctx->merge_id, action_quark);
	}
      break;
    case 'm':
      if (ctx->state == STATE_ROOT && !strcmp (element_name, "menubar"))
	{
	  ctx->state = STATE_MENU;
	  ctx->current = get_child_node (self, ctx->current,
					 node_name, strlen (node_name),
					 GTK_UI_MANAGER_MENUBAR,
					 TRUE, FALSE);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;

	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (ctx->current),
						    ctx->merge_id, action_quark);
	  NODE_INFO (ctx->current)->dirty = TRUE;

	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_MENU && !strcmp (element_name, "menu"))
	{
	  ctx->current = get_child_node (self, ctx->current,
					 node_name, strlen (node_name),
					 GTK_UI_MANAGER_MENU,
					 TRUE, top);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;

	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (ctx->current),
						    ctx->merge_id, action_quark);
	  NODE_INFO (ctx->current)->dirty = TRUE;
	  
	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_MENU && !strcmp (element_name, "menuitem"))
	{
	  GNode *node;

	  ctx->state = STATE_MENUITEM;
	  node = get_child_node (self, ctx->current,
				 node_name, strlen (node_name),
				 GTK_UI_MANAGER_MENUITEM,
				 TRUE, top);
	  if (NODE_INFO (node)->action_name == 0)
	    NODE_INFO (node)->action_name = action_quark;
	  
	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (node),
						    ctx->merge_id, action_quark);
	  NODE_INFO (node)->dirty = TRUE;
	  
	  raise_error = FALSE;
	}
      break;
    case 'p':
      if (ctx->state == STATE_ROOT && !strcmp (element_name, "popup"))
	{
	  ctx->state = STATE_MENU;
	  ctx->current = get_child_node (self, ctx->current,
					 node_name, strlen (node_name),
					 GTK_UI_MANAGER_POPUP,
					 TRUE, FALSE);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;
	  
	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (ctx->current),
						    ctx->merge_id, action_quark);
	  NODE_INFO (ctx->current)->dirty = TRUE;
	  
	  raise_error = FALSE;
	}
      else if ((ctx->state == STATE_MENU || ctx->state == STATE_TOOLBAR) &&
	       !strcmp (element_name, "placeholder"))
	{
	  if (ctx->state == STATE_TOOLBAR)
	    ctx->current = get_child_node (self, ctx->current,
					   node_name, strlen (node_name),
					   GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER,
					   TRUE, top);
	  else
	    ctx->current = get_child_node (self, ctx->current,
					   node_name, strlen (node_name),
					   GTK_UI_MANAGER_MENU_PLACEHOLDER,
					   TRUE, top);
	  
	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (ctx->current),
						    ctx->merge_id, action_quark);
	  NODE_INFO (ctx->current)->dirty = TRUE;
	  
	  raise_error = FALSE;
	}
      break;
    case 's':
      if ((ctx->state == STATE_MENU || ctx->state == STATE_TOOLBAR) &&
	  !strcmp (element_name, "separator"))
	{
	  GNode *node;

	  if (ctx->state == STATE_TOOLBAR)
	    ctx->state = STATE_TOOLITEM;
	  else
	    ctx->state = STATE_MENUITEM;
	  node = get_child_node (self, ctx->current,
				 node_name, strlen (node_name),
				 GTK_UI_MANAGER_SEPARATOR,
				 TRUE, top);
	  if (NODE_INFO (node)->action_name == 0)
	    NODE_INFO (node)->action_name = action_quark;

	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (node),
						    ctx->merge_id, action_quark);
	  NODE_INFO (node)->dirty = TRUE;
	  
	  raise_error = FALSE;
	}
      break;
    case 't':
      if (ctx->state == STATE_ROOT && !strcmp (element_name, "toolbar"))
	{
	  ctx->state = STATE_TOOLBAR;
	  ctx->current = get_child_node (self, ctx->current,
					 node_name, strlen (node_name),
					 GTK_UI_MANAGER_TOOLBAR,
					 TRUE, FALSE);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;
	  
	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (ctx->current),
						    ctx->merge_id, action_quark);
	  NODE_INFO (ctx->current)->dirty = TRUE;
	  
	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_TOOLBAR && !strcmp (element_name, "toolitem"))
	{
	  GNode *node;

	  ctx->state = STATE_TOOLITEM;
	  node = get_child_node (self, ctx->current,
				node_name, strlen (node_name),
				 GTK_UI_MANAGER_TOOLITEM,
				 TRUE, top);
	  if (NODE_INFO (node)->action_name == 0)
	    NODE_INFO (node)->action_name = action_quark;
	  
	  gtk_ui_manager_node_prepend_ui_reference (NODE_INFO (node),
						    ctx->merge_id, action_quark);
	  NODE_INFO (node)->dirty = TRUE;

	  raise_error = FALSE;
	}
      break;
    default:
      break;
    }
  if (raise_error)
    {
      gint line_number, char_number;
 
      g_markup_parse_context_get_position (context,
					   &line_number, &char_number);
      if (error_attr)
	g_set_error (error,
		     G_MARKUP_ERROR,
		     G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
		     _("Unknown attribute '%s' on line %d char %d"),
		     error_attr,
		     line_number, char_number);
      else
	g_set_error (error,
		     G_MARKUP_ERROR,
		     G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		     _("Unknown tag '%s' on line %d char %d"),
		     element_name,
		     line_number, char_number);
    }
}

static void
end_element_handler (GMarkupParseContext *context,
		     const gchar         *element_name,
		     gpointer             user_data,
		     GError             **error)
{
  ParseContext *ctx = user_data;
  GtkUIManager *self = ctx->self;

  switch (ctx->state)
    {
    case STATE_START:
      g_warning ("shouldn't get any end tags in start state");
      /* should we GError here? */
      break;
    case STATE_ROOT:
      if (ctx->current != self->private_data->root_node)
	g_warning ("we are in STATE_ROOT, but the current node isn't the root");
      ctx->current = NULL;
      ctx->state = STATE_END;
      break;
    case STATE_MENU:
      ctx->current = ctx->current->parent;
      if (NODE_INFO (ctx->current)->type == GTK_UI_MANAGER_ROOT) 
	ctx->state = STATE_ROOT;
      /* else, stay in same state */
      break;
    case STATE_TOOLBAR:
      ctx->current = ctx->current->parent;
      /* we conditionalise this test, in case we are closing off a
       * placeholder */
      if (NODE_INFO (ctx->current)->type == GTK_UI_MANAGER_ROOT)
	ctx->state = STATE_ROOT;
      /* else, stay in STATE_TOOLBAR state */
      break;
    case STATE_MENUITEM:
      ctx->state = STATE_MENU;
      break;
    case STATE_TOOLITEM:
      ctx->state = STATE_TOOLBAR;
      break;
    case STATE_END:
      g_warning ("shouldn't get any end tags at this point");
      /* should do an error here */
      break;
    }
}

static void
cleanup (GMarkupParseContext *context,
	 GError              *error,
	 gpointer             user_data)
{
  ParseContext *ctx = user_data;
  GtkUIManager *self = ctx->self;

  ctx->current = NULL;
  /* should also walk through the tree and get rid of nodes related to
   * this UI file's tag */

  gtk_ui_manager_remove_ui (self, ctx->merge_id);
}

static GMarkupParser ui_parser = {
  start_element_handler,
  end_element_handler,
  NULL,
  NULL,
  cleanup
};


static gboolean
xml_isspace (char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static guint
add_ui_from_string (GtkUIManager *self,
		    const gchar  *buffer, 
		    gssize        length,
		    gboolean      needs_root,
		    GError      **error)
{
  ParseContext ctx = { 0 };
  GMarkupParseContext *context;

  ctx.state = STATE_START;
  ctx.self = self;
  ctx.current = NULL;
  ctx.merge_id = gtk_ui_manager_next_merge_id (self);

  context = g_markup_parse_context_new (&ui_parser, 0, &ctx, NULL);

  if (needs_root)
    if (!g_markup_parse_context_parse (context, "<ui>", -1, error))
      goto error;

  if (!g_markup_parse_context_parse (context, buffer, length, error))
    goto error;

  if (needs_root)
    if (!g_markup_parse_context_parse (context, "</ui>", -1, error))
      goto error;

  if (!g_markup_parse_context_end_parse (context, error))
    goto error;

  g_markup_parse_context_free (context);

  gtk_ui_manager_queue_update (self);

  g_signal_emit (self, merge_signals[CHANGED], 0);

  return ctx.merge_id;

 error:

  g_markup_parse_context_free (context);

  return 0;
}

/**
 * gtk_ui_manager_add_ui_from_string:
 * @self: a #GtkUIManager object
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: return location for an error
 * 
 * Parses a string containing a UI description and merge it with the current
 * contents of @self. FIXME: describe the XML format.
 * 
 * Return value: The merge id for the merged UI. The merge id can be used
 *   to unmerge the UI with gtk_ui_manager_remove_ui(). If an error occurred,
 *   the return value is 0.
 *
 * Since: 2.4
 **/
guint
gtk_ui_manager_add_ui_from_string (GtkUIManager *self,
				   const gchar  *buffer, 
				   gssize        length,
				   GError      **error)
{
  gboolean needs_root = TRUE;
  const gchar *p;
  const gchar *end;

  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), FALSE);
  g_return_val_if_fail (buffer != NULL, FALSE);

  if (length < 0)
    length = strlen (buffer);

  p = buffer;
  end = buffer + length;
  while (p != end && xml_isspace (*p))
    ++p;

  if (end - p >= 4 && strncmp (p, "<ui>", 4) == 0)
    needs_root = FALSE;
  
  return add_ui_from_string (self, buffer, length, needs_root, error);
}

/**
 * gtk_ui_manager_add_ui_from_file:
 * @self: a #GtkUIManager object
 * @filename: the name of the file to parse 
 * @error: return location for an error
 * 
 * Parses a file containing a UI description and merge it with the current
 * contents of @self. See gtk_ui_manager_add_ui_from_file().
 * 
 * Return value: The merge id for the merged UI. The merge id can be used
 *   to unmerge the UI with gtk_ui_manager_remove_ui(). If an error occurred,
 *   the return value is 0.
 *
 * Since: 2.4
 **/
guint
gtk_ui_manager_add_ui_from_file (GtkUIManager *self,
				 const gchar  *filename,
				 GError      **error)
{
  gchar *buffer;
  gint length;
  guint res;

  if (!g_file_get_contents (filename, &buffer, &length, error))
    return 0;

  res = add_ui_from_string (self, buffer, length, FALSE, error);
  g_free (buffer);

  return res;
}

static gboolean
remove_ui (GNode   *node, 
	   gpointer user_data)
{
  guint merge_id = GPOINTER_TO_UINT (user_data);

  gtk_ui_manager_node_remove_ui_reference (NODE_INFO (node), merge_id);

  return FALSE; /* continue */
}

/**
 * gtk_ui_manager_remove_ui:
 * @self: a #GtkUIManager object
 * @merge_id: a merge id as returned by gtk_ui_manager_add_ui_from_string()
 * 
 * Unmerges the part of @self<!-- -->s content identified by @merge_id.
 *
 * Since: 2.4
 **/
void
gtk_ui_manager_remove_ui (GtkUIManager *self, 
			  guint         merge_id)
{
  g_node_traverse (self->private_data->root_node, G_POST_ORDER, G_TRAVERSE_ALL, -1,
		   remove_ui, GUINT_TO_POINTER (merge_id));

  gtk_ui_manager_queue_update (self);

  g_signal_emit (self, merge_signals[CHANGED], 0);
}

/* -------------------- Updates -------------------- */


static GtkAction *
get_action_by_name (GtkUIManager *merge, 
		    const char   *action_name)
{
  GList *tmp;

  if (!action_name)
    return NULL;
  
  /* lookup name */
  for (tmp = merge->private_data->action_groups; tmp != NULL; tmp = tmp->next)
    {
      GtkActionGroup *action_group = tmp->data;
      GtkAction *action;
      
      action = gtk_action_group_get_action (action_group, action_name);

      if (action)
	return action;
    }

  return NULL;
}

static gboolean
find_menu_position (GNode      *node, 
		    GtkWidget **menushell_p, 
		    gint       *pos_p)
{
  GtkWidget *menushell;
  gint pos;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (NODE_INFO (node)->type == GTK_UI_MANAGER_MENU ||
			NODE_INFO (node)->type == GTK_UI_MANAGER_POPUP ||
			NODE_INFO (node)->type == GTK_UI_MANAGER_MENU_PLACEHOLDER ||
			NODE_INFO (node)->type == GTK_UI_MANAGER_MENUITEM ||
			NODE_INFO (node)->type == GTK_UI_MANAGER_SEPARATOR,
			FALSE);

  /* first sibling -- look at parent */
  if (node->prev == NULL)
    {
      GNode *parent;
      GList *siblings;

      parent = node->parent;
      switch (NODE_INFO (parent)->type)
	{
	case GTK_UI_MANAGER_MENUBAR:
	case GTK_UI_MANAGER_POPUP:
	  menushell = NODE_INFO (parent)->proxy;
	  pos = 0;
	  break;
	case GTK_UI_MANAGER_MENU:
	  menushell = NODE_INFO (parent)->proxy;
	  if (GTK_IS_MENU_ITEM (menushell))
	    menushell = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menushell));
	  siblings = gtk_container_get_children (GTK_CONTAINER (menushell));
	  if (siblings != NULL && GTK_IS_TEAROFF_MENU_ITEM (siblings->data))
	    pos = 1;
	  else
	    pos = 0;
	  break;
	case GTK_UI_MANAGER_MENU_PLACEHOLDER:
	  menushell = gtk_widget_get_parent (NODE_INFO (parent)->proxy);
	  g_return_val_if_fail (GTK_IS_MENU_SHELL (menushell), FALSE);
	  pos = g_list_index (GTK_MENU_SHELL (menushell)->children,
			      NODE_INFO (parent)->proxy) + 1;
	  break;
	default:
	  g_warning("%s: bad parent node type %d", G_STRLOC,
		    NODE_INFO (parent)->type);
	  return FALSE;
	}
    }
  else
    {
      GtkWidget *prev_child;
      GNode *sibling;

      sibling = node->prev;
      if (NODE_INFO (sibling)->type == GTK_UI_MANAGER_MENU_PLACEHOLDER)
	prev_child = NODE_INFO (sibling)->extra; /* second Separator */
      else
	prev_child = NODE_INFO (sibling)->proxy;

      g_return_val_if_fail (GTK_IS_WIDGET (prev_child), FALSE);
      menushell = gtk_widget_get_parent (prev_child);
      g_return_val_if_fail (GTK_IS_MENU_SHELL (menushell), FALSE);

      pos = g_list_index (GTK_MENU_SHELL (menushell)->children, prev_child) + 1;
    }

  if (menushell_p)
    *menushell_p = menushell;
  if (pos_p)
    *pos_p = pos;

  return TRUE;
}

static gboolean
find_toolbar_position (GNode      *node, 
		       GtkWidget **toolbar_p, 
		       gint       *pos_p)
{
  GtkWidget *toolbar;
  gint pos;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (NODE_INFO (node)->type == GTK_UI_MANAGER_TOOLBAR ||
			NODE_INFO (node)->type == GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER ||
			NODE_INFO (node)->type == GTK_UI_MANAGER_TOOLITEM ||
			NODE_INFO (node)->type == GTK_UI_MANAGER_SEPARATOR,
			FALSE);

  /* first sibling -- look at parent */
  if (node->prev == NULL)
    {
      GNode *parent;

      parent = node->parent;
      switch (NODE_INFO (parent)->type)
	{
	case GTK_UI_MANAGER_TOOLBAR:
	  toolbar = NODE_INFO (parent)->proxy;
	  pos = 0;
	  break;
	case GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER:
	  toolbar = gtk_widget_get_parent (NODE_INFO (parent)->proxy);
	  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);
	  pos = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar),
					    GTK_TOOL_ITEM (NODE_INFO (parent)->proxy)) + 1;
	  break;
	default:
	  g_warning ("%s: bad parent node type %d", G_STRLOC,
		     NODE_INFO (parent)->type);
	  return FALSE;
	}
    }
  else
    {
      GtkWidget *prev_child;
      GNode *sibling;

      sibling = node->prev;
      if (NODE_INFO (sibling)->type == GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER)
	prev_child = NODE_INFO (sibling)->extra; /* second Separator */
      else
	prev_child = NODE_INFO (sibling)->proxy;

      g_return_val_if_fail (GTK_IS_WIDGET (prev_child), FALSE);
      toolbar = gtk_widget_get_parent (prev_child);
      g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

      pos = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar),
					GTK_TOOL_ITEM (prev_child)) + 1;
    }

  if (toolbar_p)
    *toolbar_p = toolbar;
  if (pos_p)
    *pos_p = pos;

  return TRUE;
}

static void
update_node (GtkUIManager *self, 
	     GNode        *node,
	     gboolean      add_tearoffs)
{
  GtkUIManagerNode *info;
  GNode *child;
  GtkAction *action;
#ifdef DEBUG_UI_MANAGER
  GList *tmp;
#endif
  
  g_return_if_fail (node != NULL);
  g_return_if_fail (NODE_INFO (node) != NULL);

  info = NODE_INFO (node);

#ifdef DEBUG_UI_MANAGER
  g_print ("update_node name=%s dirty=%d (", info->name, info->dirty);
  for (tmp = info->uifiles; tmp != NULL; tmp = tmp->next)
    {
      NodeUIReference *ref = tmp->data;
      g_print("%s:%u", g_quark_to_string (ref->action_quark), ref->merge_id);
      if (tmp->next)
	g_print (", ");
    }
  g_print (")\n");
#endif

  if (info->dirty)
    {
      const gchar *action_name;
      NodeUIReference *ref;

      if (info->uifiles == NULL) {
	/* We may need to remove this node.
	 * This must be done in post order
	 */
	goto recurse_children;
      }

      ref = info->uifiles->data;
      action_name = g_quark_to_string (ref->action_quark);
      action = get_action_by_name (self, action_name);

      info->dirty = FALSE;

      /* Check if the node doesn't have an action and must have an action */
      if (action == NULL &&
	  info->type != GTK_UI_MANAGER_MENUBAR &&
	  info->type != GTK_UI_MANAGER_TOOLBAR &&
	  info->type != GTK_UI_MANAGER_SEPARATOR &&
	  info->type != GTK_UI_MANAGER_MENU_PLACEHOLDER &&
	  info->type != GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER)
	{
	  /* FIXME: Should we warn here? */
	  goto recurse_children;
	}

      /* If the widget already has a proxy and the action hasn't changed, then
       * we only have to update the tearoff menu items.
       */
      if (info->proxy != NULL && action == info->action)
	{
	  if (info->type == GTK_UI_MANAGER_MENU) 
	    {
	      GtkWidget *menu;
	      GList *siblings;

	      menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (info->proxy));
	      siblings = gtk_container_get_children (GTK_CONTAINER (menu));
	      if (siblings != NULL && GTK_IS_TEAROFF_MENU_ITEM (siblings->data))
		g_object_set (G_OBJECT (siblings->data), "visible", add_tearoffs, 0);
	    }

	  goto recurse_children;
	}
      
      if (info->action)
	g_object_unref (info->action);
      info->action = action;
      if (info->action)
	g_object_ref (info->action);

      switch (info->type)
	{
	case GTK_UI_MANAGER_MENUBAR:
	  if (info->proxy == NULL)
	    {
	      info->proxy = gtk_menu_bar_new ();
	      gtk_widget_show (info->proxy);
	      g_signal_emit (self, merge_signals[ADD_WIDGET], 0, info->proxy);
	    }
	  break;
	case GTK_UI_MANAGER_POPUP:
	  if (info->proxy == NULL) 
	    {
	      info->proxy = gtk_menu_new ();
	      gtk_menu_set_accel_group (GTK_MENU (info->proxy), 
					self->private_data->accel_group);
	    }
	  break;
	case GTK_UI_MANAGER_MENU:
	  {
	    GtkWidget *prev_submenu = NULL;
	    GtkWidget *menu;
	    GList *siblings;
	    /* remove the proxy if it is of the wrong type ... */
	    if (info->proxy &&  G_OBJECT_TYPE (info->proxy) !=
		GTK_ACTION_GET_CLASS (info->action)->menu_item_type)
	      {
		prev_submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (info->proxy));
		if (prev_submenu)
		  {
		    g_object_ref (prev_submenu);
		    gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy), NULL);
		  }
		gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				      info->proxy);
		info->proxy = NULL;
	      }
	    /* create proxy if needed ... */
	    if (info->proxy == NULL)
	      {
		GtkWidget *menushell;
		gint pos;
		
		if (find_menu_position (node, &menushell, &pos))
		  {
		    info->proxy = gtk_action_create_menu_item (info->action);
		    menu = gtk_menu_new ();
		    GtkWidget *tearoff = gtk_tearoff_menu_item_new ();
		    gtk_menu_shell_append (GTK_MENU_SHELL (menu), tearoff);
		    gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy), menu);
		    gtk_menu_set_accel_group (GTK_MENU (menu), self->private_data->accel_group);
		    gtk_menu_shell_insert (GTK_MENU_SHELL (menushell), info->proxy, pos);
		  }
	      }
	    else
	      gtk_action_connect_proxy (info->action, info->proxy);
	    if (prev_submenu)
	      {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy),
					   prev_submenu);
		g_object_unref (prev_submenu);
	      }
	    menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (info->proxy));
	    siblings = gtk_container_get_children (GTK_CONTAINER (menu));
	    if (siblings != NULL && GTK_IS_TEAROFF_MENU_ITEM (siblings->data))
	      g_object_set (G_OBJECT (siblings->data), "visible", add_tearoffs, 0);
	  }
	  break;
	case GTK_UI_MANAGER_UNDECIDED:
	  g_warning ("found 'undecided node!");
	  break;
	case GTK_UI_MANAGER_ROOT:
	  break;
	case GTK_UI_MANAGER_TOOLBAR:
	  if (info->proxy == NULL)
	    {
	      info->proxy = gtk_toolbar_new ();
	      gtk_widget_show (info->proxy);
	      g_signal_emit (self, merge_signals[ADD_WIDGET], 0, info->proxy);
	    }
	  break;
	case GTK_UI_MANAGER_MENU_PLACEHOLDER:
	  /* create menu items for placeholders if necessary ... */
	  if (!GTK_IS_SEPARATOR_MENU_ITEM (info->proxy) ||
	      !GTK_IS_SEPARATOR_MENU_ITEM (info->extra))
	    {
	      if (info->proxy)
		gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				      info->proxy);
	      if (info->extra)
		gtk_container_remove (GTK_CONTAINER (info->extra->parent),
				      info->extra);
	      info->proxy = NULL;
	      info->extra = NULL;
	    }
	  if (info->proxy == NULL)
	    {
	      GtkWidget *menushell;
	      gint pos;

	      if (find_menu_position (node, &menushell, &pos))
		{
		  NODE_INFO (node)->proxy = gtk_separator_menu_item_new ();
		  gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
					NODE_INFO (node)->proxy, pos);

		  NODE_INFO (node)->extra = gtk_separator_menu_item_new ();
		  gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
					 NODE_INFO (node)->extra, pos+1);
		}
	    }
	  break;
	case GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER:
	  /* create toolbar items for placeholders if necessary ... */
	  if (!GTK_IS_SEPARATOR_TOOL_ITEM (info->proxy) ||
	      !GTK_IS_SEPARATOR_TOOL_ITEM (info->extra))
	    {
	      if (info->proxy)
		gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				      info->proxy);
	      if (info->extra)
		gtk_container_remove (GTK_CONTAINER (info->extra->parent),
				      info->extra);
	      info->proxy = NULL;
	      info->extra = NULL;
	    }
	  if (info->proxy == NULL)
	    {
	      GtkWidget *toolbar;
	      gint pos;

	      if (find_toolbar_position (node, &toolbar, &pos))
		{
		  GtkToolItem *item;

		  item = gtk_separator_tool_item_new ();
		  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, pos);
		  NODE_INFO(node)->proxy = GTK_WIDGET (item);

		  item = gtk_separator_tool_item_new ();
		  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, pos+1);
		  NODE_INFO (node)->extra = GTK_WIDGET (item);
		}
	    }
	  break;
	case GTK_UI_MANAGER_MENUITEM:
	  /* remove the proxy if it is of the wrong type ... */
	  if (info->proxy &&  G_OBJECT_TYPE (info->proxy) !=
	      GTK_ACTION_GET_CLASS (info->action)->menu_item_type)
	    {
	      gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				    info->proxy);
	      info->proxy = NULL;
	    }
	  /* create proxy if needed ... */
	  if (info->proxy == NULL)
	    {
	      GtkWidget *menushell;
	      gint pos;

	      if (find_menu_position (node, &menushell, &pos))
		{
		  info->proxy = gtk_action_create_menu_item (info->action);

		  gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
					 info->proxy, pos);
		}
	    }
	  else
	    {
	      gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy), NULL);
	      gtk_action_connect_proxy (info->action, info->proxy);
	    }
	  break;
	case GTK_UI_MANAGER_TOOLITEM:
	  /* remove the proxy if it is of the wrong type ... */
	  if (info->proxy &&  G_OBJECT_TYPE (info->proxy) !=
	      GTK_ACTION_GET_CLASS (info->action)->toolbar_item_type)
	    {
	      gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				    info->proxy);
	      info->proxy = NULL;
	    }
	  /* create proxy if needed ... */
	  if (info->proxy == NULL)
	    {
	      GtkWidget *toolbar;
	      gint pos;

	      if (find_toolbar_position (node, &toolbar, &pos))
		{
		  info->proxy = gtk_action_create_tool_item (info->action);

		  gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
					        GTK_TOOL_ITEM (info->proxy), pos);
		}
	    }
	  else
	    {
	      gtk_action_connect_proxy (info->action, info->proxy);
	    }
	  break;
	case GTK_UI_MANAGER_SEPARATOR:
	  if (NODE_INFO (node->parent)->type == GTK_UI_MANAGER_TOOLBAR ||
	      NODE_INFO (node->parent)->type == GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER)
	    {
	      GtkWidget *toolbar;
	      gint pos;

	      if (GTK_IS_SEPARATOR_TOOL_ITEM (info->proxy))
		{
		  gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
					info->proxy);
		  info->proxy = NULL;
		}

	      if (find_toolbar_position (node, &toolbar, &pos))
		{
		  GtkToolItem *item = gtk_separator_tool_item_new ();
		  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, pos);
		  info->proxy = GTK_WIDGET (item);
		  gtk_widget_show (info->proxy);
		}
	    }
	  else
	    {
	      GtkWidget *menushell;
	      gint pos;

	      if (GTK_IS_SEPARATOR_MENU_ITEM (info->proxy))
		{
		  gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
					info->proxy);
		  info->proxy = NULL;
		}

	      if (find_menu_position (node, &menushell, &pos))
		{
		  info->proxy = gtk_separator_menu_item_new ();
		  gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
					 info->proxy, pos);
		  gtk_widget_show (info->proxy);
		}
	    }
	  break;
	}

      /* if this node has a widget, but it is the wrong type, remove it */
    }

 recurse_children:
  /* process children */
  child = node->children;
  while (child)
    {
      GNode *current;

      current = child;
      child = current->next;
      update_node (self, current, add_tearoffs && (info->type != GTK_UI_MANAGER_POPUP));
    }

  /* handle cleanup of dead nodes */
  if (node->children == NULL && info->uifiles == NULL)
    {
      if (info->proxy)
	gtk_widget_destroy (info->proxy);
      if ((info->type == GTK_UI_MANAGER_MENU_PLACEHOLDER ||
	   info->type == GTK_UI_MANAGER_TOOLBAR_PLACEHOLDER) &&
	  info->extra)
	gtk_widget_destroy (info->extra);
      g_chunk_free (info, merge_node_chunk);
      g_node_destroy (node);
    }
}

static gboolean
do_updates (GtkUIManager *self)
{
  /* this function needs to check through the tree for dirty nodes.
   * For such nodes, it needs to do the following:
   *
   * 1) check if they are referenced by any loaded UI files anymore.
   *    In which case, the proxy widget should be destroyed, unless
   *    there are any subnodes.
   *
   * 2) lookup the action for this node again.  If it is different to
   *    the current one (or if no previous action has been looked up),
   *    the proxy is reconnected to the new action (or a new proxy widget
   *    is created and added to the parent container).
   */
  update_node (self, self->private_data->root_node, 
	       self->private_data->add_tearoffs);

  self->private_data->update_tag = 0;

  return FALSE;
}

static void
gtk_ui_manager_queue_update (GtkUIManager *self)
{
  if (self->private_data->update_tag != 0)
    return;

  self->private_data->update_tag = g_idle_add ((GSourceFunc)do_updates, self);
}

static void
gtk_ui_manager_ensure_update (GtkUIManager *self)
{
  if (self->private_data->update_tag != 0)
    {
      g_source_remove (self->private_data->update_tag);
      do_updates (self);
    }
}

static gboolean
dirty_traverse_func (GNode   *node, 
		     gpointer data)
{
  NODE_INFO (node)->dirty = TRUE;
  return FALSE;
}

static void
gtk_ui_manager_dirty_all (GtkUIManager *self)
{
  g_node_traverse (self->private_data->root_node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		   dirty_traverse_func, NULL);
  gtk_ui_manager_queue_update (self);
}

static const gchar *open_tag_format[] = {
  "%*s<UNDECIDED>\n",
  "%*s<ui>\n",
  "%*s<menubar name=\"%s\">\n",  
  "%*s<menu name='%s' action=\"%s\">\n",
  "%*s<toolbar name=\"%s\">\n",
  "%*s<placeholder name=\"%s\">\n",
  "%*s<placeholder name=\"%s\">\n",
  "%*s<popup name='%s' action=\"%s\">\n",
  "%*s<menuitem name=\"%s\" action=\"%s\"/>\n", 
  "%*s<toolitem name=\"%s\" action=\"%s\"/>\n", 
  "%*s<separator/>\n",
};

static const gchar *close_tag_format[] = {
  "%*s</UNDECIDED>\n",
  "%*s</ui>\n",
  "%*s</menubar>\n",
  "%*s</menu>\n",
  "%*s</toolbar>\n",
  "%*s</placeholder>\n",
  "%*s</placeholder>\n",
  "%*s</popup>\n",
  "",
  "",
  "",
};

static void
print_node (GtkUIManager *self, 
	    GNode        *node, 
	    gint          indent_level,
	    GString      *buffer)
{
  GtkUIManagerNode *mnode;
  GNode *child;

  mnode = node->data;

  g_string_append_printf (buffer, open_tag_format[mnode->type],
			  indent_level, "",
			  mnode->name, 
			  g_quark_to_string (mnode->action_name));

  for (child = node->children; child != NULL; child = child->next) 
    print_node (self, child, indent_level + 2, buffer);

  g_string_append_printf (buffer, close_tag_format[mnode->type],
			  indent_level, "");

}

/**
 * gtk_ui_manager_get_ui:
 * @self: a #GtkUIManager
 * 
 * Creates an XML representation of the merged ui.
 * 
 * Return value: A newly allocated string containing an XML representation of 
 * the merged ui.
 *
 * Since: 2.4
 **/
gchar*
gtk_ui_manager_get_ui (GtkUIManager   *self)
{
  GString *buffer;

  buffer = g_string_new (NULL);

  gtk_ui_manager_ensure_update (self); 
 
  print_node (self, self->private_data->root_node, 0, buffer);  

  return g_string_free (buffer, FALSE);
}
