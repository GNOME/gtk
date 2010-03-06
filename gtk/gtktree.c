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

#undef GTK_DISABLE_DEPRECATED

#include "config.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtksignal.h"
#include "gtklist.h"

#define GTK_ENABLE_BROKEN
#include "gtktree.h"
#include "gtktreeitem.h"
#include "gtkintl.h"

#include "gtkalias.h"

enum {
  SELECTION_CHANGED,
  SELECT_CHILD,
  UNSELECT_CHILD,
  LAST_SIGNAL
};

static void gtk_tree_class_init      (GtkTreeClass   *klass);
static void gtk_tree_init            (GtkTree        *tree);
static void gtk_tree_destroy         (GtkObject      *object);
static void gtk_tree_map             (GtkWidget      *widget);
static void gtk_tree_parent_set      (GtkWidget      *widget,
				      GtkWidget      *previous_parent);
static void gtk_tree_unmap           (GtkWidget      *widget);
static void gtk_tree_realize         (GtkWidget      *widget);
static gint gtk_tree_motion_notify   (GtkWidget      *widget,
				      GdkEventMotion *event);
static gint gtk_tree_button_press    (GtkWidget      *widget,
				      GdkEventButton *event);
static gint gtk_tree_button_release  (GtkWidget      *widget,
				      GdkEventButton *event);
static void gtk_tree_size_request    (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void gtk_tree_size_allocate   (GtkWidget      *widget,
				      GtkAllocation  *allocation);
static void gtk_tree_add             (GtkContainer   *container,
				      GtkWidget      *widget);
static void gtk_tree_forall          (GtkContainer   *container,
				      gboolean	      include_internals,
				      GtkCallback     callback,
				      gpointer        callback_data);

static void gtk_real_tree_select_child   (GtkTree       *tree,
					  GtkWidget     *child);
static void gtk_real_tree_unselect_child (GtkTree       *tree,
					  GtkWidget     *child);

static GtkType gtk_tree_child_type  (GtkContainer   *container);

static GtkContainerClass *parent_class = NULL;
static guint tree_signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_tree_get_type (void)
{
  static GtkType tree_type = 0;
  
  if (!tree_type)
    {
      static const GtkTypeInfo tree_info =
      {
	"GtkTree",
	sizeof (GtkTree),
	sizeof (GtkTreeClass),
	(GtkClassInitFunc) gtk_tree_class_init,
	(GtkObjectInitFunc) gtk_tree_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      I_("GtkTree");
      tree_type = gtk_type_unique (gtk_container_get_type (), &tree_info);
    }
  
  return tree_type;
}

static void
gtk_tree_class_init (GtkTreeClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  
  parent_class = gtk_type_class (gtk_container_get_type ());
  
  
  object_class->destroy = gtk_tree_destroy;
  
  widget_class->map = gtk_tree_map;
  widget_class->unmap = gtk_tree_unmap;
  widget_class->parent_set = gtk_tree_parent_set;
  widget_class->realize = gtk_tree_realize;
  widget_class->motion_notify_event = gtk_tree_motion_notify;
  widget_class->button_press_event = gtk_tree_button_press;
  widget_class->button_release_event = gtk_tree_button_release;
  widget_class->size_request = gtk_tree_size_request;
  widget_class->size_allocate = gtk_tree_size_allocate;
  
  container_class->add = gtk_tree_add;
  container_class->remove = 
    (void (*)(GtkContainer *, GtkWidget *)) gtk_tree_remove_item;
  container_class->forall = gtk_tree_forall;
  container_class->child_type = gtk_tree_child_type;
  
  class->selection_changed = NULL;
  class->select_child = gtk_real_tree_select_child;
  class->unselect_child = gtk_real_tree_unselect_child;

  tree_signals[SELECTION_CHANGED] =
    gtk_signal_new (I_("selection-changed"),
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeClass, selection_changed),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  tree_signals[SELECT_CHILD] =
    gtk_signal_new (I_("select-child"),
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeClass, select_child),
		    _gtk_marshal_VOID__OBJECT,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_WIDGET);
  tree_signals[UNSELECT_CHILD] =
    gtk_signal_new (I_("unselect-child"),
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeClass, unselect_child),
		    _gtk_marshal_VOID__OBJECT,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_WIDGET);
}

static GtkType
gtk_tree_child_type (GtkContainer     *container)
{
  return GTK_TYPE_TREE_ITEM;
}

static void
gtk_tree_init (GtkTree *tree)
{
  tree->children = NULL;
  tree->root_tree = tree;
  tree->selection = NULL;
  tree->tree_owner = NULL;
  tree->selection_mode = GTK_SELECTION_SINGLE;
  tree->indent_value = 9;
  tree->current_indent = 0;
  tree->level = 0;
  tree->view_mode = GTK_TREE_VIEW_LINE;
  tree->view_line = TRUE;
}

GtkWidget*
gtk_tree_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_tree_get_type ()));
}

void
gtk_tree_append (GtkTree   *tree,
		 GtkWidget *tree_item)
{
  g_return_if_fail (GTK_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  
  gtk_tree_insert (tree, tree_item, -1);
}

void
gtk_tree_prepend (GtkTree   *tree,
		  GtkWidget *tree_item)
{
  g_return_if_fail (GTK_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  
  gtk_tree_insert (tree, tree_item, 0);
}

void
gtk_tree_insert (GtkTree   *tree,
		 GtkWidget *tree_item,
		 gint       position)
{
  gint nchildren;
  
  g_return_if_fail (GTK_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  
  nchildren = g_list_length (tree->children);
  
  if ((position < 0) || (position > nchildren))
    position = nchildren;
  
  if (position == nchildren)
    tree->children = g_list_append (tree->children, tree_item);
  else
    tree->children = g_list_insert (tree->children, tree_item, position);
  
  gtk_widget_set_parent (tree_item, GTK_WIDGET (tree));
}

static void
gtk_tree_add (GtkContainer *container,
	      GtkWidget    *child)
{
  GtkTree *tree;
  
  g_return_if_fail (GTK_IS_TREE (container));
  g_return_if_fail (GTK_IS_TREE_ITEM (child));
  
  tree = GTK_TREE (container);
  
  tree->children = g_list_append (tree->children, child);
  
  gtk_widget_set_parent (child, GTK_WIDGET (container));
  
  if (!tree->selection && (tree->selection_mode == GTK_SELECTION_BROWSE))
    gtk_tree_select_child (tree, child);
}

static gint
gtk_tree_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  GtkTree *tree;
  GtkWidget *item;
  
  g_return_val_if_fail (GTK_IS_TREE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  tree = GTK_TREE (widget);
  item = gtk_get_event_widget ((GdkEvent*) event);
  
  while (item && !GTK_IS_TREE_ITEM (item))
    item = item->parent;
  
  if (!item || (item->parent != widget))
    return FALSE;
  
  switch(event->button) 
    {
    case 1:
      gtk_tree_select_child (tree, item);
      break;
    case 2:
      if(GTK_TREE_ITEM(item)->subtree) gtk_tree_item_expand(GTK_TREE_ITEM(item));
      break;
    case 3:
      if(GTK_TREE_ITEM(item)->subtree) gtk_tree_item_collapse(GTK_TREE_ITEM(item));
      break;
    }
  
  return TRUE;
}

static gint
gtk_tree_button_release (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkTree *tree;
  GtkWidget *item;
  
  g_return_val_if_fail (GTK_IS_TREE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  tree = GTK_TREE (widget);
  item = gtk_get_event_widget ((GdkEvent*) event);
  
  return TRUE;
}

gint
gtk_tree_child_position (GtkTree   *tree,
			 GtkWidget *child)
{
  GList *children;
  gint pos;
  
  
  g_return_val_if_fail (GTK_IS_TREE (tree), -1);
  g_return_val_if_fail (child != NULL, -1);
  
  pos = 0;
  children = tree->children;
  
  while (children)
    {
      if (child == GTK_WIDGET (children->data)) 
	return pos;
      
      pos += 1;
      children = children->next;
    }
  
  
  return -1;
}

void
gtk_tree_clear_items (GtkTree *tree,
		      gint     start,
		      gint     end)
{
  GtkWidget *widget;
  GList *clear_list;
  GList *tmp_list;
  guint nchildren;
  guint index;
  
  g_return_if_fail (GTK_IS_TREE (tree));
  
  nchildren = g_list_length (tree->children);
  
  if (nchildren > 0)
    {
      if ((end < 0) || (end > nchildren))
	end = nchildren;
      
      if (start >= end)
	return;
      
      tmp_list = g_list_nth (tree->children, start);
      clear_list = NULL;
      index = start;
      while (tmp_list && index <= end)
	{
	  widget = tmp_list->data;
	  tmp_list = tmp_list->next;
	  index++;
	  
	  clear_list = g_list_prepend (clear_list, widget);
	}
      
      gtk_tree_remove_items (tree, clear_list);
    }
}

static void
gtk_tree_destroy (GtkObject *object)
{
  GtkTree *tree;
  GtkWidget *child;
  GList *children;
  
  g_return_if_fail (GTK_IS_TREE (object));
  
  tree = GTK_TREE (object);
  
  children = tree->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      g_object_ref (child);
      gtk_widget_unparent (child);
      gtk_widget_destroy (child);
      g_object_unref (child);
    }
  
  g_list_free (tree->children);
  tree->children = NULL;
  
  if (tree->root_tree == tree)
    {
      GList *node;
      for (node = tree->selection; node; node = node->next)
	g_object_unref (node->data);
      g_list_free (tree->selection);
      tree->selection = NULL;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_tree_forall (GtkContainer *container,
		 gboolean      include_internals,
		 GtkCallback   callback,
		 gpointer      callback_data)
{
  GtkTree *tree;
  GtkWidget *child;
  GList *children;
  
  
  g_return_if_fail (GTK_IS_TREE (container));
  g_return_if_fail (callback != NULL);
  
  tree = GTK_TREE (container);
  children = tree->children;
  
  while (children)
    {
      child = children->data;
      children = children->next;
      
      (* callback) (child, callback_data);
    }
}

static void
gtk_tree_unselect_all (GtkTree *tree)
{
  GList *tmp_list, *selection;
  GtkWidget *tmp_item;
      
  selection = tree->selection;
  tree->selection = NULL;

  tmp_list = selection;
  while (tmp_list)
    {
      tmp_item = selection->data;

      if (tmp_item->parent &&
	  GTK_IS_TREE (tmp_item->parent) &&
	  GTK_TREE (tmp_item->parent)->root_tree == tree)
	gtk_tree_item_deselect (GTK_TREE_ITEM (tmp_item));

      g_object_unref (tmp_item);

      tmp_list = tmp_list->next;
    }

  g_list_free (selection);
}

static void
gtk_tree_parent_set (GtkWidget *widget,
		     GtkWidget *previous_parent)
{
  GtkTree *tree = GTK_TREE (widget);
  GtkWidget *child;
  GList *children;
  
  if (GTK_IS_TREE (widget->parent))
    {
      gtk_tree_unselect_all (tree);
      
      /* set root tree for this tree */
      tree->root_tree = GTK_TREE(widget->parent)->root_tree;
      
      tree->level = GTK_TREE(GTK_WIDGET(tree)->parent)->level+1;
      tree->indent_value = GTK_TREE(GTK_WIDGET(tree)->parent)->indent_value;
      tree->current_indent = GTK_TREE(GTK_WIDGET(tree)->parent)->current_indent + 
	tree->indent_value;
      tree->view_mode = GTK_TREE(GTK_WIDGET(tree)->parent)->view_mode;
      tree->view_line = GTK_TREE(GTK_WIDGET(tree)->parent)->view_line;
    }
  else
    {
      tree->root_tree = tree;
      
      tree->level = 0;
      tree->current_indent = 0;
    }

  children = tree->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (GTK_TREE_ITEM (child)->subtree)
	gtk_tree_parent_set (GTK_TREE_ITEM (child)->subtree, child);
    }
}

static void
gtk_tree_map (GtkWidget *widget)
{
  GtkTree *tree = GTK_TREE (widget);
  GtkWidget *child;
  GList *children;
  
  gtk_widget_set_mapped (widget, TRUE);
  
  children = tree->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (gtk_widget_get_visible (child) &&
	  !gtk_widget_get_mapped (child))
	gtk_widget_map (child);
      
      if (GTK_TREE_ITEM (child)->subtree)
	{
	  child = GTK_WIDGET (GTK_TREE_ITEM (child)->subtree);
	  
	  if (gtk_widget_get_visible (child) && !gtk_widget_get_mapped (child))
	    gtk_widget_map (child);
	}
    }

  gdk_window_show (widget->window);
}

static gint
gtk_tree_motion_notify (GtkWidget      *widget,
			GdkEventMotion *event)
{
  g_return_val_if_fail (GTK_IS_TREE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
#ifdef TREE_DEBUG
  g_message("gtk_tree_motion_notify\n");
#endif /* TREE_DEBUG */
  
  return FALSE;
}

static void
gtk_tree_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  
  g_return_if_fail (GTK_IS_TREE (widget));
  
  gtk_widget_set_realized (widget, TRUE);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  gdk_window_set_background (widget->window, 
			     &widget->style->base[GTK_STATE_NORMAL]);
}

void
gtk_tree_remove_item (GtkTree      *container,
		      GtkWidget    *widget)
{
  GList *item_list;
  
  g_return_if_fail (GTK_IS_TREE (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (container == GTK_TREE (widget->parent));
  
  item_list = g_list_append (NULL, widget);
  
  gtk_tree_remove_items (GTK_TREE (container), item_list);
  
  g_list_free (item_list);
}

/* used by gtk_tree_remove_items to make the function independent of
   order in list of items to remove.
   Sort item by depth in tree */
static gint 
gtk_tree_sort_item_by_depth(GtkWidget* a, GtkWidget* b)
{
  if((GTK_TREE(a->parent)->level) < (GTK_TREE(b->parent)->level))
    return 1;
  if((GTK_TREE(a->parent)->level) > (GTK_TREE(b->parent)->level))
    return -1;
  
  return 0;
}

void
gtk_tree_remove_items (GtkTree *tree,
		       GList   *items)
{
  GtkWidget *widget;
  GList *selected_widgets;
  GList *tmp_list;
  GList *sorted_list;
  GtkTree *real_tree;
  GtkTree *root_tree;
  
  g_return_if_fail (GTK_IS_TREE (tree));
  
#ifdef TREE_DEBUG
  g_message("+ gtk_tree_remove_items [ tree %#x items list %#x ]\n", (int)tree, (int)items);
#endif /* TREE_DEBUG */
  
  /* We may not yet be mapped, so we actively have to find our
   * root tree
   */
  if (tree->root_tree)
    root_tree = tree->root_tree;
  else
    {
      GtkWidget *tmp = GTK_WIDGET (tree);
      while (GTK_IS_TREE (tmp->parent))
	tmp = tmp->parent;
      
      root_tree = GTK_TREE (tmp);
    }
  
  tmp_list = items;
  selected_widgets = NULL;
  sorted_list = NULL;
  widget = NULL;
  
#ifdef TREE_DEBUG
  g_message("* sort list by depth\n");
#endif /* TREE_DEBUG */
  
  while (tmp_list)
    {
      
#ifdef TREE_DEBUG
      g_message ("* item [%#x] depth [%d]\n", 
		 (int)tmp_list->data,
		 (int)GTK_TREE(GTK_WIDGET(tmp_list->data)->parent)->level);
#endif /* TREE_DEBUG */
      
      sorted_list = g_list_insert_sorted(sorted_list,
					 tmp_list->data,
					 (GCompareFunc)gtk_tree_sort_item_by_depth);
      tmp_list = g_list_next(tmp_list);
    }
  
#ifdef TREE_DEBUG
  /* print sorted list */
  g_message("* sorted list result\n");
  tmp_list = sorted_list;
  while(tmp_list)
    {
      g_message("* item [%#x] depth [%d]\n", 
		(int)tmp_list->data,
		(int)GTK_TREE(GTK_WIDGET(tmp_list->data)->parent)->level);
      tmp_list = g_list_next(tmp_list);
    }
#endif /* TREE_DEBUG */
  
#ifdef TREE_DEBUG
  g_message("* scan sorted list\n");
#endif /* TREE_DEBUG */
  
  tmp_list = sorted_list;
  while (tmp_list)
    {
      widget = tmp_list->data;
      tmp_list = tmp_list->next;
      
#ifdef TREE_DEBUG
      g_message("* item [%#x] subtree [%#x]\n", 
		(int)widget, (int)GTK_TREE_ITEM_SUBTREE(widget));
#endif /* TREE_DEBUG */
      
      /* get real owner of this widget */
      real_tree = GTK_TREE(widget->parent);
#ifdef TREE_DEBUG
      g_message("* subtree having this widget [%#x]\n", (int)real_tree);
#endif /* TREE_DEBUG */
      
      
      if (widget->state == GTK_STATE_SELECTED)
	{
	  selected_widgets = g_list_prepend (selected_widgets, widget);
#ifdef TREE_DEBUG
	  g_message("* selected widget - adding it in selected list [%#x]\n",
		    (int)selected_widgets);
#endif /* TREE_DEBUG */
	}
      
      /* remove this item from its real parent */
#ifdef TREE_DEBUG
      g_message("* remove widget from its owner tree\n");
#endif /* TREE_DEBUG */
      real_tree->children = g_list_remove (real_tree->children, widget);
      
      /* remove subtree associate at this item if it exist */      
      if(GTK_TREE_ITEM(widget)->subtree) 
	{
#ifdef TREE_DEBUG
	  g_message("* remove subtree associate at this item [%#x]\n",
		    (int) GTK_TREE_ITEM(widget)->subtree);
#endif /* TREE_DEBUG */
	  if (gtk_widget_get_mapped (GTK_TREE_ITEM(widget)->subtree))
	    gtk_widget_unmap (GTK_TREE_ITEM(widget)->subtree);
	  
	  gtk_widget_unparent (GTK_TREE_ITEM(widget)->subtree);
	  GTK_TREE_ITEM(widget)->subtree = NULL;
	}
      
      /* really remove widget for this item */
#ifdef TREE_DEBUG
      g_message("* unmap and unparent widget [%#x]\n", (int)widget);
#endif /* TREE_DEBUG */
      if (gtk_widget_get_mapped (widget))
	gtk_widget_unmap (widget);
      
      gtk_widget_unparent (widget);
      
      /* delete subtree if there is no children in it */
      if(real_tree->children == NULL && 
	 real_tree != root_tree)
	{
#ifdef TREE_DEBUG
	  g_message("* owner tree don't have children ... destroy it\n");
#endif /* TREE_DEBUG */
	  gtk_tree_item_remove_subtree(GTK_TREE_ITEM(real_tree->tree_owner));
	}
      
#ifdef TREE_DEBUG
      g_message("* next item in list\n");
#endif /* TREE_DEBUG */
    }
  
  if (selected_widgets)
    {
#ifdef TREE_DEBUG
      g_message("* scan selected item list\n");
#endif /* TREE_DEBUG */
      tmp_list = selected_widgets;
      while (tmp_list)
	{
	  widget = tmp_list->data;
	  tmp_list = tmp_list->next;
	  
#ifdef TREE_DEBUG
	  g_message("* widget [%#x] subtree [%#x]\n", 
		    (int)widget, (int)GTK_TREE_ITEM_SUBTREE(widget));
#endif /* TREE_DEBUG */
	  
	  /* remove widget of selection */
	  root_tree->selection = g_list_remove (root_tree->selection, widget);
	  
	  /* unref it to authorize is destruction */
	  g_object_unref (widget);
	}
      
      /* emit only one selection_changed signal */
      gtk_signal_emit (GTK_OBJECT (root_tree), 
		       tree_signals[SELECTION_CHANGED]);
    }
  
#ifdef TREE_DEBUG
  g_message("* free selected_widgets list\n");
#endif /* TREE_DEBUG */
  g_list_free (selected_widgets);
  g_list_free (sorted_list);
  
  if (root_tree->children && !root_tree->selection &&
      (root_tree->selection_mode == GTK_SELECTION_BROWSE))
    {
#ifdef TREE_DEBUG
      g_message("* BROWSE mode, select another item\n");
#endif /* TREE_DEBUG */
      widget = root_tree->children->data;
      gtk_tree_select_child (root_tree, widget);
    }
  
  if (gtk_widget_get_visible (GTK_WIDGET (root_tree)))
    {
#ifdef TREE_DEBUG
      g_message("* query queue resizing for root_tree\n");
#endif /* TREE_DEBUG */      
      gtk_widget_queue_resize (GTK_WIDGET (root_tree));
    }
}

void
gtk_tree_select_child (GtkTree   *tree,
		       GtkWidget *tree_item)
{
  g_return_if_fail (GTK_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  
  gtk_signal_emit (GTK_OBJECT (tree), tree_signals[SELECT_CHILD], tree_item);
}

void
gtk_tree_select_item (GtkTree   *tree,
		      gint       item)
{
  GList *tmp_list;
  
  g_return_if_fail (GTK_IS_TREE (tree));
  
  tmp_list = g_list_nth (tree->children, item);
  if (tmp_list)
    gtk_tree_select_child (tree, GTK_WIDGET (tmp_list->data));
  
}

static void
gtk_tree_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkTree *tree;
  GtkWidget *child, *subtree;
  GtkAllocation child_allocation;
  GList *children;
  
  
  g_return_if_fail (GTK_IS_TREE (widget));
  g_return_if_fail (allocation != NULL);
  
  tree = GTK_TREE (widget);
  
  widget->allocation = *allocation;
  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);
  
  if (tree->children)
    {
      child_allocation.x = GTK_CONTAINER (tree)->border_width;
      child_allocation.y = GTK_CONTAINER (tree)->border_width;
      child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2);
      
      children = tree->children;
      
      while (children)
	{
	  child = children->data;
	  children = children->next;
	  
	  if (gtk_widget_get_visible (child))
	    {
	      GtkRequisition child_requisition;
	      gtk_widget_get_child_requisition (child, &child_requisition);
	      
	      child_allocation.height = child_requisition.height;
	      
	      gtk_widget_size_allocate (child, &child_allocation);
	      
	      child_allocation.y += child_allocation.height;
	      
	      if((subtree = GTK_TREE_ITEM(child)->subtree))
		if(gtk_widget_get_visible (subtree))
		  {
		    child_allocation.height = subtree->requisition.height;
		    gtk_widget_size_allocate (subtree, &child_allocation);
		    child_allocation.y += child_allocation.height;
		  }
	    }
	}
    }
  
}

static void
gtk_tree_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkTree *tree;
  GtkWidget *child, *subtree;
  GList *children;
  GtkRequisition child_requisition;
  
  
  g_return_if_fail (GTK_IS_TREE (widget));
  g_return_if_fail (requisition != NULL);
  
  tree = GTK_TREE (widget);
  requisition->width = 0;
  requisition->height = 0;
  
  children = tree->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (gtk_widget_get_visible (child))
	{
	  gtk_widget_size_request (child, &child_requisition);
	  
	  requisition->width = MAX (requisition->width, child_requisition.width);
	  requisition->height += child_requisition.height;
	  
	  if((subtree = GTK_TREE_ITEM(child)->subtree) &&
	     gtk_widget_get_visible (subtree))
	    {
	      gtk_widget_size_request (subtree, &child_requisition);
	      
	      requisition->width = MAX (requisition->width, 
					child_requisition.width);
	      
	      requisition->height += child_requisition.height;
	    }
	}
    }
  
  requisition->width += GTK_CONTAINER (tree)->border_width * 2;
  requisition->height += GTK_CONTAINER (tree)->border_width * 2;
  
  requisition->width = MAX (requisition->width, 1);
  requisition->height = MAX (requisition->height, 1);
  
}

static void
gtk_tree_unmap (GtkWidget *widget)
{
  
  g_return_if_fail (GTK_IS_TREE (widget));
  
  gtk_widget_set_mapped (widget, FALSE);
  gdk_window_hide (widget->window);
  
}

void
gtk_tree_unselect_child (GtkTree   *tree,
			 GtkWidget *tree_item)
{
  g_return_if_fail (GTK_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  
  gtk_signal_emit (GTK_OBJECT (tree), tree_signals[UNSELECT_CHILD], tree_item);
}

void
gtk_tree_unselect_item (GtkTree *tree,
			gint     item)
{
  GList *tmp_list;
  
  g_return_if_fail (GTK_IS_TREE (tree));
  
  tmp_list = g_list_nth (tree->children, item);
  if (tmp_list)
    gtk_tree_unselect_child (tree, GTK_WIDGET (tmp_list->data));
  
}

static void
gtk_real_tree_select_child (GtkTree   *tree,
			    GtkWidget *child)
{
  GList *selection, *root_selection;
  GList *tmp_list;
  GtkWidget *tmp_item;
  
  g_return_if_fail (GTK_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_ITEM (child));

  root_selection = tree->root_tree->selection;
  
  switch (tree->root_tree->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
      
      selection = root_selection;
      
      /* remove old selection list */
      while (selection)
	{
	  tmp_item = selection->data;
	  
	  if (tmp_item != child)
	    {
	      gtk_tree_item_deselect (GTK_TREE_ITEM (tmp_item));
	      
	      tmp_list = selection;
	      selection = selection->next;
	      
	      root_selection = g_list_remove_link (root_selection, tmp_list);
	      g_object_unref (tmp_item);
	      
	      g_list_free (tmp_list);
	    }
	  else
	    selection = selection->next;
	}
      
      if (child->state == GTK_STATE_NORMAL)
	{
	  gtk_tree_item_select (GTK_TREE_ITEM (child));
	  root_selection = g_list_prepend (root_selection, child);
	  g_object_ref (child);
	}
      else if (child->state == GTK_STATE_SELECTED)
	{
	  gtk_tree_item_deselect (GTK_TREE_ITEM (child));
	  root_selection = g_list_remove (root_selection, child);
	  g_object_unref (child);
	}
      
      tree->root_tree->selection = root_selection;
      
      gtk_signal_emit (GTK_OBJECT (tree->root_tree), 
		       tree_signals[SELECTION_CHANGED]);
      break;
      
      
    case GTK_SELECTION_BROWSE:
      selection = root_selection;
      
      while (selection)
	{
	  tmp_item = selection->data;
	  
	  if (tmp_item != child)
	    {
	      gtk_tree_item_deselect (GTK_TREE_ITEM (tmp_item));
	      
	      tmp_list = selection;
	      selection = selection->next;
	      
	      root_selection = g_list_remove_link (root_selection, tmp_list);
	      g_object_unref (tmp_item);
	      
	      g_list_free (tmp_list);
	    }
	  else
	    selection = selection->next;
	}
      
      tree->root_tree->selection = root_selection;
      
      if (child->state == GTK_STATE_NORMAL)
	{
	  gtk_tree_item_select (GTK_TREE_ITEM (child));
	  root_selection = g_list_prepend (root_selection, child);
	  g_object_ref (child);
	  tree->root_tree->selection = root_selection;
	  gtk_signal_emit (GTK_OBJECT (tree->root_tree), 
			   tree_signals[SELECTION_CHANGED]);
	}
      break;
      
    case GTK_SELECTION_MULTIPLE:
      break;
    }
}

static void
gtk_real_tree_unselect_child (GtkTree   *tree,
			      GtkWidget *child)
{
  g_return_if_fail (GTK_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_ITEM (child));
  
  switch (tree->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:
      if (child->state == GTK_STATE_SELECTED)
	{
	  GtkTree* root_tree = GTK_TREE_ROOT_TREE(tree);
	  gtk_tree_item_deselect (GTK_TREE_ITEM (child));
	  root_tree->selection = g_list_remove (root_tree->selection, child);
	  g_object_unref (child);
	  gtk_signal_emit (GTK_OBJECT (tree->root_tree), 
			   tree_signals[SELECTION_CHANGED]);
	}
      break;
      
    case GTK_SELECTION_MULTIPLE:
      break;
    }
}

void
gtk_tree_set_selection_mode (GtkTree       *tree,
			     GtkSelectionMode mode) 
{
  g_return_if_fail (GTK_IS_TREE (tree));
  
  tree->selection_mode = mode;
}

void
gtk_tree_set_view_mode (GtkTree       *tree,
			GtkTreeViewMode mode) 
{
  g_return_if_fail (GTK_IS_TREE (tree));
  
  tree->view_mode = mode;
}

void
gtk_tree_set_view_lines (GtkTree       *tree,
			 gboolean	flag) 
{
  g_return_if_fail (GTK_IS_TREE (tree));
  
  tree->view_line = flag;
}

#define __GTK_TREE_C__
#include "gtkaliasdef.c"
