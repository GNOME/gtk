/* gtktreeviewcolumn.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#include "gtktreeviewcolumn.h"
#include "gtktreeprivate.h"
#include "gtksignal.h"
#include "gtkbutton.h"
#include "gtkalignment.h"
#include "gtklabel.h"

enum {
  CLICKED,
  LAST_SIGNAL
};


static void gtk_tree_view_column_init            (GtkTreeViewColumn      *tree_column);
static void gtk_tree_view_column_class_init      (GtkTreeViewColumnClass *klass);
static void gtk_tree_view_column_set_attributesv (GtkTreeViewColumn      *tree_column,
						  va_list                 args);
static void gtk_real_tree_column_clicked         (GtkTreeViewColumn      *tree_column);


static GtkObjectClass *parent_class = NULL;
static guint tree_column_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_tree_view_column_get_type (void)
{
  static GtkType tree_column_type = 0;

  if (!tree_column_type)
    {
      static const GTypeInfo tree_column_info =
      {
	sizeof (GtkTreeViewColumnClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_tree_view_column_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkTreeViewColumn),
	0,
	(GInstanceInitFunc) gtk_tree_view_column_init,
      };

      tree_column_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeViewColumn", &tree_column_info, 0);
    }

  return tree_column_type;
}

static void
gtk_tree_view_column_class_init (GtkTreeViewColumnClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  parent_class = g_type_class_peek_parent (class);

  tree_column_signals[CLICKED] =
    gtk_signal_new ("clicked",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeViewColumnClass, clicked),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, tree_column_signals, LAST_SIGNAL);

  class->clicked = gtk_real_tree_column_clicked;
}

static void
gtk_tree_view_column_init (GtkTreeViewColumn *tree_column)
{
  tree_column->button = NULL;
  tree_column->justification = GTK_JUSTIFY_LEFT;
  tree_column->size = 0;
  tree_column->min_width = -1;
  tree_column->max_width = -1;
  tree_column->cell = NULL;
  tree_column->attributes = NULL;
  tree_column->column_type = GTK_TREE_VIEW_COLUMN_AUTOSIZE;
  tree_column->visible = TRUE;
  tree_column->button_active = FALSE;
  tree_column->dirty = TRUE;
}

/* used to make the buttons 'unclickable' */

static gint
gtk_tree_view_passive_func (GtkWidget *widget,
			    GdkEvent  *event,
			    gpointer   data)
{
  g_return_val_if_fail (event != NULL, FALSE);

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      return TRUE;
    default:
      break;
    }
  return FALSE;
}

static void
gtk_real_tree_column_clicked (GtkTreeViewColumn *tree_column)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

}

/**
 * gtk_tree_view_column_new:
 * 
 * Creates a new #GtkTreeViewColumn.
 * 
 * Return value: A newly created #GtkTreeViewColumn.
 **/
GtkObject *
gtk_tree_view_column_new (void)
{
  GtkObject *retval;

  retval = GTK_OBJECT (gtk_type_new (GTK_TYPE_TREE_COLUMN));

  return retval;
}

/**
 * gtk_tree_view_column_new_with_attributes:
 * @title: The title to set the header to.
 * @cell: The #GtkCellRenderer.
 * @Varargs: A NULL terminated list of attributes.
 * 
 * Creates a new #GtkTreeViewColumn with a number of default values.  This is
 * equivalent to calling @gtk_tree_view_column_set_title,
 * @gtk_tree_view_column_set_cell_renderer, and
 * @gtk_tree_view_column_set_attributes on the newly created #GtkTreeViewColumn.
 * 
 * Return value: A newly created #GtkTreeViewColumn.
 **/
GtkObject *
gtk_tree_view_column_new_with_attributes (gchar           *title,
					  GtkCellRenderer *cell,
					  ...)
{
  GtkObject *retval;
  va_list args;

  retval = gtk_tree_view_column_new ();

  gtk_tree_view_column_set_title (GTK_TREE_VIEW_COLUMN (retval), title);
  gtk_tree_view_column_set_cell_renderer (GTK_TREE_VIEW_COLUMN (retval), cell);

  va_start (args, cell);
  gtk_tree_view_column_set_attributesv (GTK_TREE_VIEW_COLUMN (retval),
					args);
  va_end (args);

  return retval;
}

/**
 * gtk_tree_view_column_set_cell_renderer:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell: The #GtkCellRenderer, or NULL.
 * 
 * Sets the cell renderer of the @tree_column, and unsets the current
 * attributes.  If there is a cell renderer already set, then it is removed.  If
 * @cell is NULL, then the cell renderer is unset.
 **/
void
gtk_tree_view_column_set_cell_renderer (GtkTreeViewColumn *tree_column,
					GtkCellRenderer   *cell)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  if (cell)
    g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  if (cell)
    g_object_ref (G_OBJECT (cell));

  if (tree_column->cell)
    g_object_unref (G_OBJECT (tree_column->cell));

  tree_column->cell = cell;

  gtk_tree_view_column_set_attributes (tree_column, NULL);
}

/**
 * gtk_tree_view_column_add_attribute:
 * @tree_column: A #GtkTreeViewColumn.
 * @attribute: An attribute on the columns cell
 * @column: The column position on the model to get the attribute from.
 * 
 * Adds an attribute mapping to the list in @tree_column.  The @column is the
 * column position on the model to get a value from, and the @attribute is the
 * parameter on the @tree_column's cell renderer to set that value to.
 **/
void
gtk_tree_view_column_add_attribute (GtkTreeViewColumn *tree_column,
				    gchar             *attribute,
				    gint               column)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  tree_column->attributes = g_slist_prepend (tree_column->attributes,
					     GINT_TO_POINTER (column));
  tree_column->attributes = g_slist_prepend (tree_column->attributes,
					     g_strdup (attribute));
}

static void
gtk_tree_view_column_set_attributesv (GtkTreeViewColumn *tree_column,
				      va_list            args)
{
  GSList *list;
  gchar *attribute;
  gint column;

  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  attribute = va_arg (args, gchar *);

  list = tree_column->attributes;

  while (list && list->next)
    {
      g_free (list->data);
      list = list->next->next;
    }
  g_slist_free (tree_column->attributes);
  tree_column->attributes = NULL;

  while (attribute != NULL)
    {
      column = va_arg (args, gint);
      gtk_tree_view_column_add_attribute (tree_column,
					  attribute,
					  column);
      attribute = va_arg (args, gchar *);
    }
}

/**
 * gtk_tree_view_column_set_attributes:
 * @tree_column: A #GtkTreeViewColumn.
 * @Varargs: A NULL terminated listing of attributes to add.
 * 
 * Adds the attributes in the list the the @tree_column.  The attributes should
 * be in attribute/column order, as in @gtk_tree_view_column_add_attribute.  If
 * the list is empty, then all attributes are removed.
 **/
void
gtk_tree_view_column_set_attributes (GtkTreeViewColumn *tree_column,
				     ...)
{
  va_list args;

  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  va_start (args, tree_column);

  gtk_tree_view_column_set_attributesv (tree_column, args);

  va_end (args);
}

/**
 * gtk_tree_view_column_set_cell_data:
 * @tree_column: A #GtkTreeViewColumn.
 * @tree_model: The #GtkTreeModel to to get the cell renderers attributes from.
 * @iter: The #GtkTreeIter to to get the cell renderer's attributes from.
 * 
 * Sets the cell renderer based on the @tree_model and @tree_node.  That is, for
 * every attribute mapping in @tree_column, it will get a value from the set
 * column on the @tree_node, and use that value to set the attribute on the cell
 * renderer.
 **/
void
gtk_tree_view_column_set_cell_data (GtkTreeViewColumn *tree_column,
				    GtkTreeModel      *tree_model,
				    GtkTreeIter       *iter)
{
  GSList *list;
  GValue value = { 0, };
  GObject *cell;

  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (tree_column->cell != NULL);

  if (tree_column->func && (* tree_column->func) (tree_column,
						  tree_model,
						  iter,
						  tree_column->func_data))
    return;

  cell = (GObject *) tree_column->cell;
  list = tree_column->attributes;

  while (list && list->next)
    {
      gtk_tree_model_get_value (tree_model, iter,
				GPOINTER_TO_INT (list->next->data),
				&value);
      g_object_set_param (cell, (gchar *) list->data, &value);
      g_value_unset (&value);
      list = list->next->next;
    }
}

/* Options for manipulating the columns */

/**
 * gtk_tree_view_column_set_visible:
 * @tree_column: A #GtkTreeViewColumn.
 * @visible: TRUE if the @tree_column is visible.
 * 
 * Sets the visibility of @tree_column.
 **/
void
gtk_tree_view_column_set_visible (GtkTreeViewColumn *tree_column,
				  gboolean           visible)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_column->visible == visible)
    return;

  tree_column->visible = visible;

  if (visible)
    {
      gtk_widget_show (tree_column->button);
      gdk_window_show (tree_column->window);
    }
  else
    {
      gtk_widget_hide (tree_column->button);
      gdk_window_hide (tree_column->window);
    }

  if (GTK_WIDGET_REALIZED (tree_column->tree_view))
    {
      _gtk_tree_view_set_size (GTK_TREE_VIEW (tree_column->tree_view), -1, -1);
      gtk_widget_queue_resize (tree_column->tree_view);
    }
}

/**
 * gtk_tree_view_column_get_visible:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns TRUE if @tree_column is visible.
 * 
 * Return value: whether the column is visible or not.  If it is visible, then
 * the tree will show the column.
 **/
gboolean
gtk_tree_view_column_get_visible (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->visible;
}

/**
 * gtk_tree_view_column_set_col_type:
 * @tree_column: A #GtkTreeViewColumn.
 * @type: The #GtkTreeViewColumnType.
 * 
 * Sets the growth behavior of @tree_column to @type.
 **/
void
gtk_tree_view_column_set_col_type (GtkTreeViewColumn     *tree_column,
				   GtkTreeViewColumnType  type)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (type == tree_column->column_type)
    return;

  tree_column->column_type = type;
  switch (type)
    {
    case GTK_TREE_VIEW_COLUMN_AUTOSIZE:
      tree_column->dirty = TRUE;
    case GTK_TREE_VIEW_COLUMN_FIXED:
      gdk_window_hide (tree_column->window);
      break;
    default:
      gdk_window_show (tree_column->window);
      gdk_window_raise (tree_column->window);
      break;
    }

  gtk_widget_queue_resize (tree_column->tree_view);
}

/**
 * gtk_tree_view_column_get_col_type:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current type of @tree_column.
 * 
 * Return value: The type of @tree_column.
 **/
gint
gtk_tree_view_column_get_col_type (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->column_type;
}

/**
 * gtk_tree_view_column_get_size:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current size of the @tree_column in pixels.
 * 
 * Return value: The current size of the @tree_column.
 **/
gint
gtk_tree_view_column_get_size (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->size;
}

/**
 * gtk_tree_view_column_set_size:
 * @tree_column: A #GtkTreeViewColumn.
 * @size: The size to set the @tree_column to.
 * 
 * Sets the size of the column in pixels, unless the the column type is
 * #GTK_TREE_VIEW_COLUMN_AUTOSIZE.  In this case, the value is discarded as the
 * size of the column is based on the calculated width of the column.
 **/
void
gtk_tree_view_column_set_size (GtkTreeViewColumn *tree_column,
			       gint               size)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (size > 0);

  if (tree_column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE ||
      tree_column->size == size)
    return;

  tree_column->size = size;

  if (GTK_WIDGET_REALIZED (tree_column->tree_view))
    gtk_widget_queue_resize (tree_column->tree_view);
}

/**
 * gtk_tree_view_column_set_min_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @min_width: The minimum width of the column in pixels, or -1.
 * 
 * Sets the minimum width of the @tree_column.  If @min_width is -1, then the
 * minimum width is unset.
 **/
void
gtk_tree_view_column_set_min_width (GtkTreeViewColumn *tree_column,
				    gint               min_width)
{
  gint real_min_width;

  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (min_width >= -1);

  if (min_width == tree_column->min_width)
    return;

  real_min_width = (tree_column->min_width == -1) ?
    tree_column->button->requisition.width : tree_column->min_width;

  /* We want to queue a resize if the either the old min_size or the
   * new min_size determined the size of the column */
  if (GTK_WIDGET_REALIZED (tree_column->tree_view) &&
      ((tree_column->min_width > tree_column->size) ||
       (tree_column->min_width == -1 &&
	tree_column->button->requisition.width > tree_column->size) ||
       (min_width > tree_column->size) ||
       (min_width == -1 &&
	tree_column->button->requisition.width > tree_column->size)))
    gtk_widget_queue_resize (tree_column->tree_view);

  if (tree_column->max_width != -1 &&
      tree_column->max_width < real_min_width)
    tree_column->max_width = real_min_width;

  tree_column->min_width = min_width;
}

/**
 * gtk_tree_view_column_get_min_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the minimum width in pixels of the @tree_column, or -1 if no minimum
 * width is set.
 * 
 * Return value: The minimum width of the @tree_column.
 **/
gint
gtk_tree_view_column_get_min_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), -1);

  return tree_column->min_width;
}

/**
 * gtk_tree_view_column_set_max_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @max_width: The maximum width of the column in pixels, or -1.
 * 
 * Sets the maximum width of the @tree_column.  If @max_width is -1, then the
 * maximum width is unset.
 **/
void
gtk_tree_view_column_set_max_width (GtkTreeViewColumn *tree_column,
				    gint               max_width)
{
  gint real_min_width;

  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (max_width >= -1);

  if (max_width == tree_column->max_width)
    return;

  real_min_width = tree_column->min_width == -1 ?
    tree_column->button->requisition.width : tree_column->min_width;

  if (GTK_WIDGET_REALIZED (tree_column->tree_view) &&
      ((tree_column->max_width < tree_column->size) ||
       (max_width != -1 && max_width < tree_column->size)))
    gtk_widget_queue_resize (tree_column->tree_view);

  tree_column->max_width = max_width;

  if (real_min_width > max_width)
    tree_column->min_width = max_width;
}

/**
 * gtk_tree_view_column_get_max_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the maximum width in pixels of the @tree_column, or -1 if no maximum
 * width is set.
 * 
 * Return value: The maximum width of the @tree_column.
 **/
gint
gtk_tree_view_column_get_max_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), -1);

  return tree_column->max_width;
}

/**
 * gtk_tree_view_column_set_title:
 * @tree_column: A #GtkTreeViewColumn.
 * @title: The title of the @tree_column.
 * 
 * Sets the title of the @tree_column.  If a custom widget has been set, then
 * this value is ignored.
 **/
void
gtk_tree_view_column_set_title (GtkTreeViewColumn *tree_column,
				gchar         *title)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  g_free (tree_column->title);
  if (title)
    tree_column->title = g_strdup (title);
  else
    tree_column->title = NULL;

  /* Hmmm.  This is a little ugly... */
  if (tree_column->button)
    {
      if (GTK_BIN (tree_column->button)->child &&
	  GTK_IS_ALIGNMENT (GTK_BIN (tree_column->button)->child))
	{
	  if (GTK_BIN (GTK_BIN (tree_column->button)->child)->child &&
	      GTK_IS_LABEL (GTK_BIN (GTK_BIN (tree_column->button)->child)->child))
	    {
	      gtk_label_set_text (GTK_LABEL (GTK_BIN (GTK_BIN (tree_column->button)->child)->child),
                                  tree_column->title);
	    }
	}
    }
}

/**
 * gtk_tree_view_column_get_title:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the title of the widget.  This value should not be modified.
 * 
 * Return value: the title of the column.
 **/
gchar *
gtk_tree_view_column_get_title (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->title;
}

/**
 * gtk_tree_view_column_set_header_active:
 * @tree_column: A #GtkTreeViewColumn.
 * @active: TRUE if the header is active.
 * 
 * Sets the header to be active if @active is TRUE.  When the header is active,
 * then it can take keyboard focus, and can be clicked.
 **/
void
gtk_tree_view_column_set_header_active (GtkTreeViewColumn *tree_column,
					gboolean           active)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (!tree_column->button)
    return;

  if (tree_column->button_active == active)
    return;

  tree_column->button_active = active;
  if (active)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_column->button),
				     (GtkSignalFunc) gtk_tree_view_passive_func,
				     NULL);

      GTK_WIDGET_SET_FLAGS (tree_column->button, GTK_CAN_FOCUS);

      if (GTK_WIDGET_VISIBLE (tree_column->tree_view))
	gtk_widget_queue_draw (tree_column->button);
    }
  else
    {
      gtk_signal_connect (GTK_OBJECT (tree_column->button),
			  "event",
			  (GtkSignalFunc) gtk_tree_view_passive_func,
			  NULL);

      GTK_WIDGET_UNSET_FLAGS (tree_column->button, GTK_CAN_FOCUS);

      if (GTK_WIDGET_VISIBLE (tree_column->tree_view))
	gtk_widget_queue_draw (tree_column->button);
    }
}

/**
 * gtk_tree_view_column_set_widget:
 * @tree_column: A #GtkTreeViewColumn.
 * @widget: A child #GtkWidget, or NULL.
 * 
 * Sets the widget in the header to be @widget.  If widget is NULL, then the
 * header button is set with a #GtkLabel set to the title of @tree_column.
 **/
void
gtk_tree_view_column_set_widget (GtkTreeViewColumn *tree_column,
				 GtkWidget         *widget)
{
  /* FIXME: Implement this function. */
#if 0
  gint new_button = 0;
  GtkWidget *old_widget;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (column < 0 || column >= tree_view->priv->columns)
    return;

  /* if the column button doesn't currently exist,
   * it has to be created first */
  if (!column->button)
    {
      column_button_create (tree_view, column);
      new_button = 1;
    }

  column_title_new (clist, column, NULL);

  /* remove and destroy the old widget */
  old_widget = GTK_BIN (clist->column[column].button)->child;
  if (old_widget)
    gtk_container_remove (GTK_CONTAINER (clist->column[column].button),
			  old_widget);

  /* add and show the widget */
  if (widget)
    {
      gtk_container_add (GTK_CONTAINER (clist->column[column].button), widget);
      gtk_widget_show (widget);
    }

  /* if this button didn't previously exist, then the
   * column button positions have to be re-computed */
  if (GTK_WIDGET_VISIBLE (clist) && new_button)
    size_allocate_title_buttons (clist);
#endif
}

/**
 * gtk_tree_view_column_get_widget:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the #GtkWidget in the button in the column header.  If a custom
 * widget has not been set, then this will be a #GtkAlignment with a #GtkLabel
 * in it.
 * 
 * Return value: The #GtkWidget in the column header.
 **/
GtkWidget *
gtk_tree_view_column_get_widget (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  if (tree_column->button)
    return GTK_BUTTON (tree_column->button)->child;

  return NULL;
}

/**
 * gtk_tree_view_column_set_justification:
 * @tree_column: A #GtkTreeViewColumn.
 * @justification: The justification of the title.
 * 
 * Sets the justification of the title inside the column header.  If a custom
 * widget has been set, then this value is discarded.
 **/
void
gtk_tree_view_column_set_justification (GtkTreeViewColumn *tree_column,
					GtkJustification   justification)
{
  GtkWidget *alignment;

  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_column->justification == justification)
    return;

  tree_column->justification = justification;

  /* change the alignment of the button title if it's not a
   * custom widget */
  alignment = GTK_BIN (tree_column->button)->child;

  if (GTK_IS_ALIGNMENT (alignment))
    {
      switch (tree_column->justification)
	{
	case GTK_JUSTIFY_LEFT:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.0, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_RIGHT:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 1.0, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_CENTER:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.5, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_FILL:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.5, 0.5, 0.0, 0.0);
	  break;

	default:
	  break;
	}
    }
}
