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
#include "gtktable.h"


static void gtk_table_class_init    (GtkTableClass  *klass);
static void gtk_table_init          (GtkTable       *table);
static void gtk_table_finalize      (GtkObject      *object);
static void gtk_table_map           (GtkWidget      *widget);
static void gtk_table_unmap         (GtkWidget      *widget);
static void gtk_table_draw          (GtkWidget      *widget,
				     GdkRectangle   *area);
static gint gtk_table_expose        (GtkWidget      *widget,
				     GdkEventExpose *event);
static void gtk_table_size_request  (GtkWidget      *widget,
				     GtkRequisition *requisition);
static void gtk_table_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);
static void gtk_table_add           (GtkContainer   *container,
				     GtkWidget      *widget);
static void gtk_table_remove        (GtkContainer   *container,
				     GtkWidget      *widget);
static void gtk_table_foreach       (GtkContainer   *container,
				     GtkCallback     callback,
				     gpointer        callback_data);

static void gtk_table_size_request_init  (GtkTable *table);
static void gtk_table_size_request_pass1 (GtkTable *table);
static void gtk_table_size_request_pass2 (GtkTable *table);
static void gtk_table_size_request_pass3 (GtkTable *table);

static void gtk_table_size_allocate_init  (GtkTable *table);
static void gtk_table_size_allocate_pass1 (GtkTable *table);
static void gtk_table_size_allocate_pass2 (GtkTable *table);


static GtkContainerClass *parent_class = NULL;


guint
gtk_table_get_type ()
{
  static guint table_type = 0;

  if (!table_type)
    {
      GtkTypeInfo table_info =
      {
	"GtkTable",
	sizeof (GtkTable),
	sizeof (GtkTableClass),
	(GtkClassInitFunc) gtk_table_class_init,
	(GtkObjectInitFunc) gtk_table_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      table_type = gtk_type_unique (gtk_container_get_type (), &table_info);
    }

  return table_type;
}

static void
gtk_table_class_init (GtkTableClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  object_class->finalize = gtk_table_finalize;

  widget_class->map = gtk_table_map;
  widget_class->unmap = gtk_table_unmap;
  widget_class->draw = gtk_table_draw;
  widget_class->expose_event = gtk_table_expose;
  widget_class->size_request = gtk_table_size_request;
  widget_class->size_allocate = gtk_table_size_allocate;

  container_class->add = gtk_table_add;
  container_class->remove = gtk_table_remove;
  container_class->foreach = gtk_table_foreach;
}

static void
gtk_table_init (GtkTable *table)
{
  GTK_WIDGET_SET_FLAGS (table, GTK_NO_WINDOW | GTK_BASIC);

  table->children = NULL;
  table->rows = NULL;
  table->cols = NULL;
  table->nrows = 0;
  table->ncols = 0;
  table->homogeneous = FALSE;
}

static void
gtk_table_init_rows (GtkTable *table, int start, int end)
{
  const int spacing = table->row_spacing;
  int row;

  for (row = start; row < end; row++)
    {
      table->rows[row].requisition = 0;
      table->rows[row].allocation = 0;
      table->rows[row].spacing = spacing;
      table->rows[row].need_expand = 0;
      table->rows[row].need_shrink = 0;
      table->rows[row].expand = 0;
      table->rows[row].shrink = 0;
    }
}

static void
gtk_table_init_cols (GtkTable *table, int start, int end)
{
  const int spacing = table->column_spacing;
  int col;
  
  for (col = start; col < end; col++)
    {
      table->cols[col].requisition = 0;
      table->cols[col].allocation = 0;
      table->cols[col].spacing = spacing;
      table->cols[col].need_expand = 0;
      table->cols[col].need_shrink = 0;
      table->cols[col].expand = 0;
      table->cols[col].shrink = 0;
    }
}

GtkWidget*
gtk_table_new (gint rows,
	       gint columns,
	       gint homogeneous)
{
  GtkTable *table;

  table = gtk_type_new (gtk_table_get_type ());

  table->nrows = rows;
  table->ncols = columns;
  table->homogeneous = (homogeneous ? TRUE : FALSE);
  table->column_spacing = 0;
  table->row_spacing = 0;
  
  table->rows = g_new (GtkTableRowCol, table->nrows);
  table->cols = g_new (GtkTableRowCol, table->ncols);

  gtk_table_init_rows (table, 0, table->nrows);
  gtk_table_init_cols (table, 0, table->ncols);

  return GTK_WIDGET (table);
}

static void
gtk_table_expand_cols (GtkTable *table, int new_max)
{
  table->cols = g_realloc (table->cols, new_max * sizeof (GtkTableRowCol));
  gtk_table_init_cols (table, table->ncols, new_max);
  table->ncols = new_max;
}

static void
gtk_table_expand_rows (GtkTable *table, int new_max)
{
  table->rows = g_realloc (table->rows, new_max * sizeof (GtkTableRowCol));
  gtk_table_init_rows (table, table->nrows, new_max);
  table->nrows = new_max;
}

void
gtk_table_attach (GtkTable  *table,
		  GtkWidget *child,
		  gint       left_attach,
		  gint       right_attach,
		  gint       top_attach,
		  gint       bottom_attach,
		  gint       xoptions,
		  gint       yoptions,
		  gint       xpadding,
		  gint       ypadding)
{
  GtkTableChild *table_child;
  
  g_return_if_fail (table != NULL);
  g_return_if_fail (GTK_IS_TABLE (table));
  g_return_if_fail (child != NULL);

  g_return_if_fail (left_attach >= 0);
  g_return_if_fail (left_attach < right_attach);
  g_return_if_fail (top_attach >= 0);
  g_return_if_fail (top_attach < bottom_attach);

  if (right_attach >= table->ncols)
	  gtk_table_expand_cols (table, right_attach);

  if (bottom_attach >= table->nrows)
	  gtk_table_expand_rows (table, bottom_attach);

  table_child = g_new (GtkTableChild, 1);
  table_child->widget = child;
  table_child->left_attach = left_attach;
  table_child->right_attach = right_attach;
  table_child->top_attach = top_attach;
  table_child->bottom_attach = bottom_attach;
  table_child->xexpand = (xoptions & GTK_EXPAND) != 0;
  table_child->xshrink = (xoptions & GTK_SHRINK) != 0;
  table_child->xfill = (xoptions & GTK_FILL) != 0;
  table_child->xpadding = xpadding;
  table_child->yexpand = (yoptions & GTK_EXPAND) != 0;
  table_child->yshrink = (yoptions & GTK_SHRINK) != 0;
  table_child->yfill = (yoptions & GTK_FILL) != 0;
  table_child->ypadding = ypadding;

  table->children = g_list_prepend (table->children, table_child);

  gtk_widget_set_parent (child, GTK_WIDGET (table));

  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (table)))
    {
      if (GTK_WIDGET_REALIZED (GTK_WIDGET (table)) &&
	  !GTK_WIDGET_REALIZED (child))
	gtk_widget_realize (child);
      
      if (GTK_WIDGET_MAPPED (GTK_WIDGET (table)) &&
	  !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }

  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (table))
    gtk_widget_queue_resize (child);
}

void
gtk_table_attach_defaults (GtkTable  *table,
			   GtkWidget *widget,
			   gint       left_attach,
			   gint       right_attach,
			   gint       top_attach,
			   gint       bottom_attach)
{
  gtk_table_attach (table, widget,
		    left_attach, right_attach,
		    top_attach, bottom_attach,
		    GTK_EXPAND | GTK_FILL,
		    GTK_EXPAND | GTK_FILL,
		    0, 0);
}

void
gtk_table_set_row_spacing (GtkTable *table,
			   gint      row,
			   gint      spacing)
{
  g_return_if_fail (table != NULL);
  g_return_if_fail (GTK_IS_TABLE (table));
  g_return_if_fail ((row >= 0) && (row < (table->nrows - 1)));

  if (table->rows[row].spacing != spacing)
    {
      table->rows[row].spacing = spacing;

      if (GTK_WIDGET_VISIBLE (table))
	gtk_widget_queue_resize (GTK_WIDGET (table));
    }
}

void
gtk_table_set_col_spacing (GtkTable *table,
			   gint      column,
			   gint      spacing)
{
  g_return_if_fail (table != NULL);
  g_return_if_fail (GTK_IS_TABLE (table));
  g_return_if_fail ((column >= 0) && (column < (table->ncols - 1)));

  if (table->cols[column].spacing != spacing)
    {
      table->cols[column].spacing = spacing;

      if (GTK_WIDGET_VISIBLE (table))
	gtk_widget_queue_resize (GTK_WIDGET (table));
    }
}

void
gtk_table_set_row_spacings (GtkTable *table,
			    gint      spacing)
{
  gint row;

  g_return_if_fail (table != NULL);
  g_return_if_fail (GTK_IS_TABLE (table));

  table->row_spacing = spacing;
  for (row = 0; row < table->nrows - 1; row++)
    table->rows[row].spacing = spacing;

  if (GTK_WIDGET_VISIBLE (table))
    gtk_widget_queue_resize (GTK_WIDGET (table));
}

void
gtk_table_set_col_spacings (GtkTable *table,
			    gint      spacing)
{
  gint col;

  g_return_if_fail (table != NULL);
  g_return_if_fail (GTK_IS_TABLE (table));

  table->column_spacing = spacing;
  for (col = 0; col < table->ncols - 1; col++)
    table->cols[col].spacing = spacing;

  if (GTK_WIDGET_VISIBLE (table))
    gtk_widget_queue_resize (GTK_WIDGET (table));
}

void
gtk_table_set_homogeneous (GtkTable *table,
			   gint      homogeneous)
{
  g_return_if_fail (table != NULL);
  g_return_if_fail (GTK_IS_TABLE (table));

  table->homogeneous = (homogeneous != 0);

  if (GTK_WIDGET_VISIBLE (table))
    gtk_widget_queue_resize (GTK_WIDGET (table));
}


static void
gtk_table_finalize (GtkObject *object)
{
  GtkTable *table;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_TABLE (object));

  table = GTK_TABLE (object);

  g_free (table->rows);
  g_free (table->cols);

  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_table_map (GtkWidget *widget)
{
  GtkTable *table;
  GtkTableChild *child;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TABLE (widget));

  table = GTK_TABLE (widget);
  GTK_WIDGET_SET_FLAGS (table, GTK_MAPPED);

  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget) &&
          !GTK_WIDGET_MAPPED (child->widget))
        gtk_widget_map (child->widget);
    }
}

static void
gtk_table_unmap (GtkWidget *widget)
{
  GtkTable *table;
  GtkTableChild *child;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TABLE (widget));

  table = GTK_TABLE (widget);
  GTK_WIDGET_UNSET_FLAGS (table, GTK_MAPPED);

  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget) &&
          GTK_WIDGET_MAPPED (child->widget))
        gtk_widget_unmap (child->widget);
    }
}

static void
gtk_table_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkTable *table;
  GtkTableChild *child;
  GList *children;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TABLE (widget));

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      table = GTK_TABLE (widget);

      children = table->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (gtk_widget_intersect (child->widget, area, &child_area))
	    gtk_widget_draw (child->widget, &child_area);
	}
    }
}

static gint
gtk_table_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkTable *table;
  GtkTableChild *child;
  GList *children;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TABLE (widget), FALSE);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      table = GTK_TABLE (widget);

      child_event = *event;

      children = table->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_NO_WINDOW (child->widget) &&
	      gtk_widget_intersect (child->widget, &event->area, &child_event.area))
	    gtk_widget_event (child->widget, (GdkEvent*) &child_event);
	}
    }

  return FALSE;
}

static void
gtk_table_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkTable *table;
  gint row, col;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TABLE (widget));
  g_return_if_fail (requisition != NULL);

  table = GTK_TABLE (widget);

  requisition->width = 0;
  requisition->height = 0;

  gtk_table_size_request_init (table);
  gtk_table_size_request_pass1 (table);
  gtk_table_size_request_pass2 (table);
  gtk_table_size_request_pass3 (table);
  gtk_table_size_request_pass2 (table);

  for (col = 0; col < table->ncols; col++)
    requisition->width += table->cols[col].requisition;
  for (col = 0; col < table->ncols - 1; col++)
    requisition->width += table->cols[col].spacing;

  for (row = 0; row < table->nrows; row++)
    requisition->height += table->rows[row].requisition;
  for (row = 0; row < table->nrows - 1; row++)
    requisition->height += table->rows[row].spacing;

  requisition->width += GTK_CONTAINER (table)->border_width * 2;
  requisition->height += GTK_CONTAINER (table)->border_width * 2;
}

static void
gtk_table_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkTable *table;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TABLE (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  table = GTK_TABLE (widget);

  gtk_table_size_allocate_init (table);
  gtk_table_size_allocate_pass1 (table);
  gtk_table_size_allocate_pass2 (table);
}

static void
gtk_table_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TABLE (container));
  g_return_if_fail (widget != NULL);

  gtk_table_attach_defaults (GTK_TABLE (container), widget, 0, 1, 0, 1);
}

static void
gtk_table_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkTable *table;
  GtkTableChild *child;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TABLE (container));
  g_return_if_fail (widget != NULL);

  table = GTK_TABLE (container);
  children = table->children;

  while (children)
    {
      child = children->data;
      children = children->next;

      if (child->widget == widget)
        {
	  gtk_widget_unparent (widget);

          table->children = g_list_remove (table->children, child);
          g_free (child);

          if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
            gtk_widget_queue_resize (GTK_WIDGET (container));
          break;
        }
    }
}

static void
gtk_table_foreach (GtkContainer *container,
		   GtkCallback     callback,
		   gpointer        callback_data)
{
  GtkTable *table;
  GtkTableChild *child;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TABLE (container));
  g_return_if_fail (callback != NULL);

  table = GTK_TABLE (container);
  children = table->children;

  while (children)
    {
      child = children->data;
      children = children->next;

      (* callback) (child->widget, callback_data);
    }
}

static void
gtk_table_size_request_init (GtkTable *table)
{
  GtkTableChild *child;
  GList *children;
  gint row, col;

  for (row = 0; row < table->nrows; row++)
    table->rows[row].requisition = 0;
  for (col = 0; col < table->ncols; col++)
    table->cols[col].requisition = 0;

  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	gtk_widget_size_request (child->widget, &child->widget->requisition);
    }
}

static void
gtk_table_size_request_pass1 (GtkTable *table)
{
  GtkTableChild *child;
  GList *children;
  gint width;
  gint height;

  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        {
          /* Child spans a single column.
           */
          if (child->left_attach == (child->right_attach - 1))
            {
              width = child->widget->requisition.width + child->xpadding * 2;
              table->cols[child->left_attach].requisition = MAX (table->cols[child->left_attach].requisition, width);
            }

          /* Child spans a single row.
           */
          if (child->top_attach == (child->bottom_attach - 1))
            {
              height = child->widget->requisition.height + child->ypadding * 2;
              table->rows[child->top_attach].requisition = MAX (table->rows[child->top_attach].requisition, height);
            }
        }
    }
}

static void
gtk_table_size_request_pass2 (GtkTable *table)
{
  gint max_width;
  gint max_height;
  gint row, col;

  if (table->homogeneous)
    {
      max_width = 0;
      max_height = 0;

      for (col = 0; col < table->ncols; col++)
        max_width = MAX (max_width, table->cols[col].requisition);
      for (row = 0; row < table->nrows; row++)
        max_height = MAX (max_height, table->rows[row].requisition);

      for (col = 0; col < table->ncols; col++)
        table->cols[col].requisition = max_width;
      for (row = 0; row < table->nrows; row++)
        table->rows[row].requisition = max_height;
    }
}

static void
gtk_table_size_request_pass3 (GtkTable *table)
{
  GtkTableChild *child;
  GList *children;
  gint width, height;
  gint row, col;
  gint extra;

  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        {
          /* Child spans multiple columns.
           */
          if (child->left_attach != (child->right_attach - 1))
            {
              /* Check and see if there is already enough space
               *  for the child.
               */
              width = 0;
              for (col = child->left_attach; col < child->right_attach; col++)
                {
                  width += table->cols[col].requisition;
                  if ((col + 1) < child->right_attach)
                    width += table->cols[col].spacing;
                }

              /* If we need to request more space for this child to fill
               *  its requisition, then divide up the needed space evenly
               *  amongst the columns it spans.
               */
              if (width < child->widget->requisition.width + child->xpadding * 2)
                {
                  width = child->widget->requisition.width + child->xpadding * 2 - width;

                  for (col = child->left_attach; col < child->right_attach; col++)
                    {
                      extra = width / (child->right_attach - col);
		      table->cols[col].requisition += extra;
                      width -= extra;
                    }
                }
            }

          /* Child spans multiple rows.
           */
          if (child->top_attach != (child->bottom_attach - 1))
            {
              /* Check and see if there is already enough space
               *  for the child.
               */
              height = 0;
              for (row = child->top_attach; row < child->bottom_attach; row++)
                {
                  height += table->rows[row].requisition;
                  if ((row + 1) < child->bottom_attach)
                    height += table->rows[row].spacing;
                }

              /* If we need to request more space for this child to fill
               *  its requisition, then divide up the needed space evenly
               *  amongst the columns it spans.
               */
              if (height < child->widget->requisition.height + child->ypadding * 2)
                {
                  height = child->widget->requisition.height + child->ypadding * 2 - height;

                  for (row = child->top_attach; row < child->bottom_attach; row++)
                    {
                      extra = height / (child->bottom_attach - row);
		      table->rows[row].requisition += extra;
                      height -= extra;
                    }
                }
            }
        }
    }
}

static void
gtk_table_size_allocate_init (GtkTable *table)
{
  GtkTableChild *child;
  GList *children;
  gint row, col;
  gint has_expand;
  gint has_shrink;

  /* Initialize the rows and cols.
   *  By default, rows and cols do not expand and do shrink.
   *  Those values are modified by the children that occupy
   *  the rows and cols.
   */
  for (col = 0; col < table->ncols; col++)
    {
      table->cols[col].allocation = table->cols[col].requisition;
      table->cols[col].need_expand = FALSE;
      table->cols[col].need_shrink = TRUE;
      table->cols[col].expand = FALSE;
      table->cols[col].shrink = TRUE;
    }
  for (row = 0; row < table->nrows; row++)
    {
      table->rows[row].allocation = table->rows[row].requisition;
      table->rows[row].need_expand = FALSE;
      table->rows[row].need_shrink = TRUE;
      table->rows[row].expand = FALSE;
      table->rows[row].shrink = TRUE;
    }

  /* Loop over all the children and adjust the row and col values
   *  based on whether the children want to be allowed to expand
   *  or shrink. This loop handles children that occupy a single
   *  row or column.
   */
  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        {
          if (child->left_attach == (child->right_attach - 1))
            {
              if (child->xexpand)
                table->cols[child->left_attach].expand = TRUE;

              if (!child->xshrink)
                table->cols[child->left_attach].shrink = FALSE;
            }

          if (child->top_attach == (child->bottom_attach - 1))
            {
              if (child->yexpand)
                table->rows[child->top_attach].expand = TRUE;

              if (!child->yshrink)
                table->rows[child->top_attach].shrink = FALSE;
            }
        }
    }

  /* Loop over all the children again and this time handle children
   *  which span multiple rows or columns.
   */
  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        {
          if (child->left_attach != (child->right_attach - 1))
            {
              if (child->xexpand)
                {
                  has_expand = FALSE;
                  for (col = child->left_attach; col < child->right_attach; col++)
                    if (table->cols[col].expand)
                      {
                        has_expand = TRUE;
                        break;
                      }

                  if (!has_expand)
                    for (col = child->left_attach; col < child->right_attach; col++)
                      table->cols[col].need_expand = TRUE;
                }

              if (!child->xshrink)
                {
                  has_shrink = TRUE;
                  for (col = child->left_attach; col < child->right_attach; col++)
                    if (!table->cols[col].shrink)
                      {
                        has_shrink = FALSE;
                        break;
                      }

                  if (has_shrink)
                    for (col = child->left_attach; col < child->right_attach; col++)
                      table->cols[col].need_shrink = FALSE;
                }
            }

          if (child->top_attach != (child->bottom_attach - 1))
            {
              if (child->yexpand)
                {
                  has_expand = FALSE;
                  for (row = child->top_attach; row < child->bottom_attach; row++)
                    if (table->rows[row].expand)
                      {
                        has_expand = TRUE;
                        break;
                      }

                  if (!has_expand)
                    for (row = child->top_attach; row < child->bottom_attach; row++)
                      table->rows[row].need_expand = TRUE;
                }

              if (!child->yshrink)
                {
                  has_shrink = TRUE;
                  for (row = child->top_attach; row < child->bottom_attach; row++)
                    if (!table->rows[row].shrink)
                      {
                        has_shrink = FALSE;
                        break;
                      }

                  if (has_shrink)
                    for (row = child->top_attach; row < child->bottom_attach; row++)
                      table->rows[row].need_shrink = FALSE;
                }
            }
        }
    }

  /* Loop over the columns and set the expand and shrink values
   *  if the column can be expanded or shrunk.
   */
  for (col = 0; col < table->ncols; col++)
    {
      if (table->cols[col].need_expand)
        table->cols[col].expand = TRUE;
      if (!table->cols[col].need_shrink)
        table->cols[col].shrink = FALSE;
    }

  /* Loop over the rows and set the expand and shrink values
   *  if the row can be expanded or shrunk.
   */
  for (row = 0; row < table->nrows; row++)
    {
      if (table->rows[row].need_expand)
        table->rows[row].expand = TRUE;
      if (!table->rows[row].need_shrink)
        table->rows[row].shrink = FALSE;
    }
}

static void
gtk_table_size_allocate_pass1 (GtkTable *table)
{
  gint real_width;
  gint real_height;
  gint width, height;
  gint row, col;
  gint nexpand;
  gint nshrink;
  gint extra;

  /* If we were allocated more space than we requested
   *  then we have to expand any expandable rows and columns
   *  to fill in the extra space.
   */

  real_width = GTK_WIDGET (table)->allocation.width - GTK_CONTAINER (table)->border_width * 2;
  real_height = GTK_WIDGET (table)->allocation.height - GTK_CONTAINER (table)->border_width * 2;

  if (table->homogeneous)
    {
      nexpand = 0;
      for (col = 0; col < table->ncols; col++)
        if (table->cols[col].expand)
          {
            nexpand += 1;
            break;
          }

      if (nexpand > 0)
        {
          width = real_width;

          for (col = 0; col < table->ncols - 1; col++)
            width -= table->cols[col].spacing;

          for (col = 0; col < table->ncols; col++)
            {
              extra = width / (table->ncols - col);
	      table->cols[col].allocation = MAX (1, extra);
              width -= extra;
            }
        }
    }
  else
    {
      width = 0;
      nexpand = 0;
      nshrink = 0;

      for (col = 0; col < table->ncols; col++)
        {
          width += table->cols[col].requisition;
          if (table->cols[col].expand)
            nexpand += 1;
          if (table->cols[col].shrink)
            nshrink += 1;
        }
      for (col = 0; col < table->ncols - 1; col++)
        width += table->cols[col].spacing;

      /* Check to see if we were allocated more width than we requested.
       */
      if ((width < real_width) && (nexpand >= 1))
        {
          width = real_width - width;

          for (col = 0; col < table->ncols; col++)
            if (table->cols[col].expand)
              {
                extra = width / nexpand;
		table->cols[col].allocation += extra;

                width -= extra;
                nexpand -= 1;
              }
        }

      /* Check to see if we were allocated less width than we requested.
       */
      if ((width > real_width) && (nshrink >= 1))
        {
          width = width - real_width;

          for (col = 0; col < table->ncols; col++)
            if (table->cols[col].shrink)
              {
                extra = width / nshrink;
		table->cols[col].allocation = MAX (1, table->cols[col].allocation - extra);

                width -= extra;
                nshrink -= 1;
              }
        }
    }

  if (table->homogeneous)
    {
      nexpand = 0;
      for (row = 0; row < table->nrows; row++)
        if (table->rows[row].expand)
          {
            nexpand += 1;
            break;
          }

      if (nexpand > 0)
        {
          height = real_height;

          for (row = 0; row < table->nrows - 1; row++)
            height -= table->rows[row].spacing;


          for (row = 0; row < table->nrows; row++)
            {
              extra = height / (table->nrows - row);
	      table->rows[row].allocation = MAX (1, extra);
              height -= extra;
            }
        }
    }
  else
    {
      height = 0;
      nexpand = 0;
      nshrink = 0;

      for (row = 0; row < table->nrows; row++)
        {
          height += table->rows[row].requisition;
          if (table->rows[row].expand)
            nexpand += 1;
          if (table->rows[row].shrink)
            nshrink += 1;
        }
      for (row = 0; row < table->nrows - 1; row++)
        height += table->rows[row].spacing;

      /* Check to see if we were allocated more height than we requested.
       */
      if ((height < real_height) && (nexpand >= 1))
        {
          height = real_height - height;

          for (row = 0; row < table->nrows; row++)
            if (table->rows[row].expand)
              {
                extra = height / nexpand;
		table->rows[row].allocation += extra;

                height -= extra;
                nexpand -= 1;
              }
        }

      /* Check to see if we were allocated less height than we requested.
       */
      if ((height > real_height) && (nshrink >= 1))
        {
          height = height - real_height;

          for (row = 0; row < table->nrows; row++)
            if (table->rows[row].shrink)
              {
                extra = height / nshrink;
		table->rows[row].allocation = MAX (1, table->rows[row].allocation - extra);

                height -= extra;
                nshrink -= 1;
              }
        }
    }
}

static void
gtk_table_size_allocate_pass2 (GtkTable *table)
{
  GtkTableChild *child;
  GList *children;
  gint max_width;
  gint max_height;
  gint x, y;
  gint row, col;
  GtkAllocation allocation;

  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        {
          x = GTK_WIDGET (table)->allocation.x + GTK_CONTAINER (table)->border_width;
          y = GTK_WIDGET (table)->allocation.y + GTK_CONTAINER (table)->border_width;
          max_width = 0;
          max_height = 0;

          for (col = 0; col < child->left_attach; col++)
            {
              x += table->cols[col].allocation;
              x += table->cols[col].spacing;
            }

          for (col = child->left_attach; col < child->right_attach; col++)
            {
              max_width += table->cols[col].allocation;
              if ((col + 1) < child->right_attach)
                max_width += table->cols[col].spacing;
            }

          for (row = 0; row < child->top_attach; row++)
            {
              y += table->rows[row].allocation;
              y += table->rows[row].spacing;
            }

          for (row = child->top_attach; row < child->bottom_attach; row++)
            {
              max_height += table->rows[row].allocation;
              if ((row + 1) < child->bottom_attach)
                max_height += table->rows[row].spacing;
            }

          if (child->xfill)
            {
              allocation.width = MAX (1, max_width - child->xpadding * 2);
              allocation.x = x + (max_width - allocation.width) / 2;
            }
          else
            {
              allocation.width = child->widget->requisition.width;
              allocation.x = x + (max_width - allocation.width) / 2;
            }

          if (child->yfill)
            {
              allocation.height = MAX (1, max_height - child->ypadding * 2);
              allocation.y = y + (max_height - allocation.height) / 2;
            }
          else
            {
              allocation.height = child->widget->requisition.height;
              allocation.y = y + (max_height - allocation.height) / 2;
            }

	  gtk_widget_size_allocate (child->widget, &allocation);
        }
    }
}

