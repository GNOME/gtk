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

#include "config.h"
#include "gtktable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

enum
{
  PROP_0,
  PROP_N_ROWS,
  PROP_N_COLUMNS,
  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,
  PROP_HOMOGENEOUS
};

enum
{
  CHILD_PROP_0,
  CHILD_PROP_LEFT_ATTACH,
  CHILD_PROP_RIGHT_ATTACH,
  CHILD_PROP_TOP_ATTACH,
  CHILD_PROP_BOTTOM_ATTACH,
  CHILD_PROP_X_OPTIONS,
  CHILD_PROP_Y_OPTIONS,
  CHILD_PROP_X_PADDING,
  CHILD_PROP_Y_PADDING
};
  
#define GTK_TABLE_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_TABLE, GtkTablePrivate))

typedef struct
{
  GtkUSize xpadding_unit;
  GtkUSize ypadding_unit;
} GtkTableChildPrivate;

typedef struct
{
  GtkUSize spacing_unit;
} GtkTableRowColPrivate;

struct _GtkTablePrivate
{
  GHashTable *child_map;
  GtkTableRowColPrivate *rows;
  GtkTableRowColPrivate *cols;

  /* mirror nrows and ncols from public data here for integrity checks (see ensure_integrity()) */
  guint16 nrows;
  guint16 ncols;

  GtkUSize column_spacing_unit;
  GtkUSize row_spacing_unit;

  gboolean skip_priv;
};

/* the purpose of this function is to detect if an application is
 * munging the public fields in GtkTable in the same ways that some
 * apps are munging GtkBox directly
 *
 *  http://bugzilla.gnome.org/show_bug.cgi?id=546711#c3
 *
 * We check this by mirroring nrows and ncols in private data; if it
 * has changed, someone external (either an app or a subclass) has
 * directly modified these sealed fields. The action we take to
 * remediate this situation is simply to set skip_priv to TRUE; that
 * will make the widget ignore all private data for rows/cols.
 */
static void
ensure_integrity (GtkTable *table)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);

  if (G_UNLIKELY (priv->skip_priv))
    return;

  if (priv->nrows != table->nrows || priv->ncols != table->ncols)
    {
      g_warning ("GtkTable: the application is munging rows/columns directly!");
      priv->skip_priv = TRUE;
    }
}

/* getter/setter functions for private row/column data that respect skip_priv */

static GtkUSize
get_row_spacing (GtkTable *table, int row)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  if (G_UNLIKELY (priv->skip_priv))
    return table->rows[row].spacing;
  else
    return priv->rows[row].spacing_unit;
}

static GtkUSize
get_col_spacing (GtkTable *table, int col)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  if (G_UNLIKELY (priv->skip_priv))
    return table->cols[col].spacing;
  else
    return priv->cols[col].spacing_unit;
}

static void
set_row_spacing (GtkTable *table, int row, GtkUSize spacing_unit)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  table->rows[row].spacing = gtk_widget_size_to_pixel (table, spacing_unit);
  if (G_UNLIKELY (priv->skip_priv))
    return;
  priv->rows[row].spacing_unit = spacing_unit;
}

static void
set_col_spacing (GtkTable *table, int col, GtkUSize spacing_unit)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  table->cols[col].spacing = gtk_widget_size_to_pixel (table, spacing_unit);
  if (G_UNLIKELY (priv->skip_priv))
    return;
  priv->cols[col].spacing_unit = spacing_unit;
}

static void
add_priv_child (GtkTable *table, GtkTableChild *child, GtkTableChildPrivate *priv_child)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  g_hash_table_insert (priv->child_map, child, priv_child);
}

static GtkTableChildPrivate *
get_priv_child (GtkTable *table, GtkTableChild *child)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  GtkTableChildPrivate *priv_child;

  priv_child = g_hash_table_lookup (priv->child_map, child);
  if (G_UNLIKELY (priv_child == NULL))
    g_warning ("GtkTable: No private child for child; the application is munging table->child directly!");

  return priv_child;
}

static void
remove_priv_child (GtkTable *table, GtkTableChild *child)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  if (G_UNLIKELY (!g_hash_table_remove (priv->child_map, child)))
    g_warning ("GtkTable: No private child for child; the application is munging table->child directly!");
}

static void gtk_table_finalize	    (GObject	    *object);
static void gtk_table_size_request  (GtkWidget	    *widget,
				     GtkRequisition *requisition);
static void gtk_table_size_allocate (GtkWidget	    *widget,
				     GtkAllocation  *allocation);
static void gtk_table_add	    (GtkContainer   *container,
				     GtkWidget	    *widget);
static void gtk_table_remove	    (GtkContainer   *container,
				     GtkWidget	    *widget);
static void gtk_table_forall	    (GtkContainer   *container,
				     gboolean	     include_internals,
				     GtkCallback     callback,
				     gpointer	     callback_data);
static void gtk_table_get_property  (GObject         *object,
				     guint            prop_id,
				     GValue          *value,
				     GParamSpec      *pspec);
static void gtk_table_set_property  (GObject         *object,
				     guint            prop_id,
				     const GValue    *value,
				     GParamSpec      *pspec);
static void gtk_table_set_child_property (GtkContainer    *container,
					  GtkWidget       *child,
					  guint            property_id,
					  const GValue    *value,
					  GParamSpec      *pspec);
static void gtk_table_get_child_property (GtkContainer    *container,
					  GtkWidget       *child,
					  guint            property_id,
					  GValue          *value,
					  GParamSpec      *pspec);
static GType gtk_table_child_type   (GtkContainer   *container);

static void gtk_table_unit_changed (GtkWidget *widget);

static void gtk_table_size_request_init	 (GtkTable *table);
static void gtk_table_size_request_pass1 (GtkTable *table);
static void gtk_table_size_request_pass2 (GtkTable *table);
static void gtk_table_size_request_pass3 (GtkTable *table);

static void gtk_table_size_allocate_init  (GtkTable *table);
static void gtk_table_size_allocate_pass1 (GtkTable *table);
static void gtk_table_size_allocate_pass2 (GtkTable *table);


G_DEFINE_TYPE (GtkTable, gtk_table, GTK_TYPE_CONTAINER)

static void
gtk_table_class_init (GtkTableClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  
  gobject_class->finalize = gtk_table_finalize;

  gobject_class->get_property = gtk_table_get_property;
  gobject_class->set_property = gtk_table_set_property;
  
  widget_class->size_request = gtk_table_size_request;
  widget_class->size_allocate = gtk_table_size_allocate;
  widget_class->unit_changed = gtk_table_unit_changed;
  
  container_class->add = gtk_table_add;
  container_class->remove = gtk_table_remove;
  container_class->forall = gtk_table_forall;
  container_class->child_type = gtk_table_child_type;
  container_class->set_child_property = gtk_table_set_child_property;
  container_class->get_child_property = gtk_table_get_child_property;
  

  g_object_class_install_property (gobject_class,
                                   PROP_N_ROWS,
                                   g_param_spec_uint ("n-rows",
						     P_("Rows"),
						     P_("The number of rows in the table"),
						     1,
						     65535,
						     1,
						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_N_COLUMNS,
                                   g_param_spec_uint ("n-columns",
						     P_("Columns"),
						     P_("The number of columns in the table"),
						     1,
						     65535,
						     1,
						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ROW_SPACING,
                                   gtk_param_spec_usize ("row-spacing",
                                                         P_("Row spacing"),
                                                         P_("The amount of space between two consecutive rows"),
                                                         0, G_MAXINT, 0,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_COLUMN_SPACING,
                                   gtk_param_spec_usize ("column-spacing",
                                                         P_("Column spacing"),
                                                         P_("The amount of space between two consecutive columns"),
                                                         0, G_MAXINT, 0,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_HOMOGENEOUS,
                                   g_param_spec_boolean ("homogeneous",
							 P_("Homogeneous"),
							 P_("If TRUE, the table cells are all the same width/height"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_LEFT_ATTACH,
					      g_param_spec_uint ("left-attach", 
								 P_("Left attachment"), 
								 P_("The column number to attach the left side of the child to"),
								 0, 65535, 0,
								 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_RIGHT_ATTACH,
					      g_param_spec_uint ("right-attach", 
								 P_("Right attachment"), 
								 P_("The column number to attach the right side of a child widget to"),
								 1, 65535, 1,
								 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TOP_ATTACH,
					      g_param_spec_uint ("top-attach", 
								 P_("Top attachment"), 
								 P_("The row number to attach the top of a child widget to"),
								 0, 65535, 0,
								 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_BOTTOM_ATTACH,
					      g_param_spec_uint ("bottom-attach",
								 P_("Bottom attachment"), 
								 P_("The row number to attach the bottom of the child to"),
								 1, 65535, 1,
								 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_X_OPTIONS,
					      g_param_spec_flags ("x-options", 
								  P_("Horizontal options"), 
								  P_("Options specifying the horizontal behaviour of the child"),
								  GTK_TYPE_ATTACH_OPTIONS, GTK_EXPAND | GTK_FILL,
								  GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_Y_OPTIONS,
					      g_param_spec_flags ("y-options", 
								  P_("Vertical options"), 
								  P_("Options specifying the vertical behaviour of the child"),
								  GTK_TYPE_ATTACH_OPTIONS, GTK_EXPAND | GTK_FILL,
								  GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_X_PADDING,
					      gtk_param_spec_usize ("x-padding", 
                                                                    P_("Horizontal padding"), 
                                                                    P_("Extra space to put between the child and its left and right neighbors, in pixels"),
                                                                    0, G_MAXINT, 0,
                                                                    GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_Y_PADDING,
					      gtk_param_spec_usize ("y-padding", 
                                                                    P_("Vertical padding"), 
                                                                    P_("Extra space to put between the child and its upper and lower neighbors, in pixels"),
                                                                    0, G_MAXINT, 0,
                                                                    GTK_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (GtkTablePrivate));
}

static GType
gtk_table_child_type (GtkContainer   *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_table_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkTable *table;
  GtkTablePrivate *priv;

  table = GTK_TABLE (object);
  priv = GTK_TABLE_GET_PRIVATE (table);

  ensure_integrity (table);

  switch (prop_id)
    {
    case PROP_N_ROWS:
      g_value_set_uint (value, table->nrows);
      break;
    case PROP_N_COLUMNS:
      g_value_set_uint (value, table->ncols);
      break;
    case PROP_ROW_SPACING:
      gtk_value_set_usize (value, priv->row_spacing_unit, table);
      break;
    case PROP_COLUMN_SPACING:
      gtk_value_set_usize (value, priv->column_spacing_unit, table);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, table->homogeneous);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_table_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkTable *table;

  table = GTK_TABLE (object);

  ensure_integrity (table);

  switch (prop_id)
    {
    case PROP_N_ROWS:
      gtk_table_resize (table, g_value_get_uint (value), table->ncols);
      break;
    case PROP_N_COLUMNS:
      gtk_table_resize (table, table->nrows, g_value_get_uint (value));
      break;
    case PROP_ROW_SPACING:
      gtk_table_set_row_spacings (table, gtk_value_get_usize (value));
      break;
    case PROP_COLUMN_SPACING:
      gtk_table_set_col_spacings (table, gtk_value_get_usize (value));
      break;
    case PROP_HOMOGENEOUS:
      gtk_table_set_homogeneous (table, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_table_set_child_property (GtkContainer    *container,
			      GtkWidget       *child,
			      guint            property_id,
			      const GValue    *value,
			      GParamSpec      *pspec)
{
  GtkTable *table = GTK_TABLE (container);
  GtkTableChild *table_child;
  GtkTableChildPrivate *priv_table_child;
  GList *list;

  ensure_integrity (table);

  table_child = NULL;
  for (list = table->children; list; list = list->next)
    {
      table_child = list->data;

      if (table_child->widget == child)
	break;
    }
  if (!list)
    {
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  priv_table_child = get_priv_child (table, table_child);

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      table_child->left_attach = g_value_get_uint (value);
      if (table_child->right_attach <= table_child->left_attach)
	table_child->right_attach = table_child->left_attach + 1;
      if (table_child->right_attach >= table->ncols)
	gtk_table_resize (table, table->nrows, table_child->right_attach);
      break;
    case CHILD_PROP_RIGHT_ATTACH:
      table_child->right_attach = g_value_get_uint (value);
      if (table_child->right_attach <= table_child->left_attach)
	table_child->left_attach = table_child->right_attach - 1;
      if (table_child->right_attach >= table->ncols)
	gtk_table_resize (table, table->nrows, table_child->right_attach);
      break;
    case CHILD_PROP_TOP_ATTACH:
      table_child->top_attach = g_value_get_uint (value);
      if (table_child->bottom_attach <= table_child->top_attach)
	table_child->bottom_attach = table_child->top_attach + 1;
      if (table_child->bottom_attach >= table->nrows)
	gtk_table_resize (table, table_child->bottom_attach, table->ncols);
      break;
    case CHILD_PROP_BOTTOM_ATTACH:
      table_child->bottom_attach = g_value_get_uint (value);
      if (table_child->bottom_attach <= table_child->top_attach)
	table_child->top_attach = table_child->bottom_attach - 1;
      if (table_child->bottom_attach >= table->nrows)
	gtk_table_resize (table, table_child->bottom_attach, table->ncols);
      break;
    case CHILD_PROP_X_OPTIONS:
      table_child->xexpand = (g_value_get_flags (value) & GTK_EXPAND) != 0;
      table_child->xshrink = (g_value_get_flags (value) & GTK_SHRINK) != 0;
      table_child->xfill = (g_value_get_flags (value) & GTK_FILL) != 0;
      break;
    case CHILD_PROP_Y_OPTIONS:
      table_child->yexpand = (g_value_get_flags (value) & GTK_EXPAND) != 0;
      table_child->yshrink = (g_value_get_flags (value) & GTK_SHRINK) != 0;
      table_child->yfill = (g_value_get_flags (value) & GTK_FILL) != 0;
      break;
    case CHILD_PROP_X_PADDING:
      if (priv_table_child != NULL)
        priv_table_child->xpadding_unit = gtk_value_get_usize (value);
      table_child->xpadding = gtk_widget_size_to_pixel (table, gtk_value_get_usize (value));
      break;
    case CHILD_PROP_Y_PADDING:
      if (priv_table_child != NULL)
        priv_table_child->ypadding_unit = gtk_value_get_usize (value);
      table_child->ypadding = gtk_widget_size_to_pixel (table, gtk_value_get_usize (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (table))
    gtk_widget_queue_resize (child);
}

static void
gtk_table_get_child_property (GtkContainer    *container,
			      GtkWidget       *child,
			      guint            property_id,
			      GValue          *value,
			      GParamSpec      *pspec)
{
  GtkTable *table = GTK_TABLE (container);
  GtkTableChild *table_child;
  GtkTableChildPrivate *priv_table_child;
  GList *list;

  ensure_integrity (table);

  table_child = NULL;
  for (list = table->children; list; list = list->next)
    {
      table_child = list->data;

      if (table_child->widget == child)
	break;
    }
  if (!list)
    {
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  priv_table_child = get_priv_child (table, table_child);

  switch (property_id)
    {
    case CHILD_PROP_LEFT_ATTACH:
      g_value_set_uint (value, table_child->left_attach);
      break;
    case CHILD_PROP_RIGHT_ATTACH:
      g_value_set_uint (value, table_child->right_attach);
      break;
    case CHILD_PROP_TOP_ATTACH:
      g_value_set_uint (value, table_child->top_attach);
      break;
    case CHILD_PROP_BOTTOM_ATTACH:
      g_value_set_uint (value, table_child->bottom_attach);
      break;
    case CHILD_PROP_X_OPTIONS:
      g_value_set_flags (value, (table_child->xexpand * GTK_EXPAND |
				 table_child->xshrink * GTK_SHRINK |
				 table_child->xfill * GTK_FILL));
      break;
    case CHILD_PROP_Y_OPTIONS:
      g_value_set_flags (value, (table_child->yexpand * GTK_EXPAND |
				 table_child->yshrink * GTK_SHRINK |
				 table_child->yfill * GTK_FILL));
      break;
    case CHILD_PROP_X_PADDING:
      if (priv_table_child != NULL)
        gtk_value_set_usize (value, priv_table_child->xpadding_unit, table);
      else
        gtk_value_set_usize (value, table_child->xpadding, table);
      break;
    case CHILD_PROP_Y_PADDING:
      if (priv_table_child != NULL)
        gtk_value_set_usize (value, priv_table_child->ypadding_unit, table);
      else
        gtk_value_set_usize (value, table_child->ypadding, table);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_table_init (GtkTable *table)
{
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);

  GTK_WIDGET_SET_FLAGS (table, GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (table), FALSE);
  
  table->children = NULL;
  table->rows = NULL;
  table->cols = NULL;
  table->nrows = 0;
  table->ncols = 0;
  table->column_spacing = 0;
  table->row_spacing = 0;
  table->homogeneous = FALSE;

  priv->column_spacing_unit = 0;
  priv->row_spacing_unit = 0;
  priv->child_map = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           g_free);
  priv->rows = NULL;
  priv->cols = NULL;
  priv->nrows = 0;
  priv->ncols = 0;

  gtk_table_resize (table, 1, 1);
}

GtkWidget*
gtk_table_new (guint	rows,
	       guint	columns,
	       gboolean homogeneous)
{
  GtkTable *table;

  if (rows == 0)
    rows = 1;
  if (columns == 0)
    columns = 1;
  
  table = g_object_new (GTK_TYPE_TABLE, NULL);
  
  table->homogeneous = (homogeneous ? TRUE : FALSE);

  gtk_table_resize (table, rows, columns);
  
  return GTK_WIDGET (table);
}

void
gtk_table_resize (GtkTable *table,
		  guint     n_rows,
		  guint     n_cols)
{
  GtkTablePrivate *priv;

  g_return_if_fail (GTK_IS_TABLE (table));
  g_return_if_fail (n_rows > 0 && n_rows <= 65535);
  g_return_if_fail (n_cols > 0 && n_cols <= 65535);

  ensure_integrity (table);

  priv = GTK_TABLE_GET_PRIVATE (table);

  n_rows = MAX (n_rows, 1);
  n_cols = MAX (n_cols, 1);

  if (n_rows != table->nrows ||
      n_cols != table->ncols)
    {
      GList *list;
      
      for (list = table->children; list; list = list->next)
	{
	  GtkTableChild *child;
	  
	  child = list->data;
	  
	  n_rows = MAX (n_rows, child->bottom_attach);
	  n_cols = MAX (n_cols, child->right_attach);
	}
      
      if (n_rows != table->nrows)
	{
	  guint i;

	  i = table->nrows;
	  table->nrows = n_rows;
	  table->rows = g_realloc (table->rows, table->nrows * sizeof (GtkTableRowCol));
	  priv->nrows = n_rows;
	  priv->rows = g_realloc (priv->rows, table->nrows * sizeof (GtkTableRowColPrivate));
	  
	  for (; i < table->nrows; i++)
	    {
	      table->rows[i].requisition = 0;
	      table->rows[i].allocation = 0;
	      table->rows[i].need_expand = 0;
	      table->rows[i].need_shrink = 0;
	      table->rows[i].expand = 0;
	      table->rows[i].shrink = 0;
              set_row_spacing (table, i, priv->row_spacing_unit);
	    }

	  g_object_notify (G_OBJECT (table), "n-rows");
	}

      if (n_cols != table->ncols)
	{
	  guint i;

	  i = table->ncols;
	  table->ncols = n_cols;
	  table->cols = g_realloc (table->cols, table->ncols * sizeof (GtkTableRowCol));
	  priv->ncols = n_cols;
	  priv->cols = g_realloc (priv->cols, table->ncols * sizeof (GtkTableRowColPrivate));
	  
	  for (; i < table->ncols; i++)
	    {
	      table->cols[i].requisition = 0;
	      table->cols[i].allocation = 0;
	      table->cols[i].need_expand = 0;
	      table->cols[i].need_shrink = 0;
	      table->cols[i].expand = 0;
	      table->cols[i].shrink = 0;
              set_col_spacing (table, i, priv->column_spacing_unit);
	    }

	  g_object_notify (G_OBJECT (table), "n-columns");
	}
    }
}

void
gtk_table_attach (GtkTable	  *table,
		  GtkWidget	  *child,
		  guint		   left_attach,
		  guint		   right_attach,
		  guint		   top_attach,
		  guint		   bottom_attach,
		  GtkAttachOptions xoptions,
		  GtkAttachOptions yoptions,
		  GtkUSize         xpadding,
		  GtkUSize         ypadding)
{
  GtkTableChild *table_child;
  GtkTableChildPrivate *priv_table_child;
  
  g_return_if_fail (GTK_IS_TABLE (table));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == NULL);
  
  /* g_return_if_fail (left_attach >= 0); */
  g_return_if_fail (left_attach < right_attach);
  /* g_return_if_fail (top_attach >= 0); */
  g_return_if_fail (top_attach < bottom_attach);
  
  ensure_integrity (table);

  if (right_attach >= table->ncols)
    gtk_table_resize (table, table->nrows, right_attach);
  
  if (bottom_attach >= table->nrows)
    gtk_table_resize (table, bottom_attach, table->ncols);
  
  table_child = g_new (GtkTableChild, 1);
  table_child->widget = child;
  table_child->left_attach = left_attach;
  table_child->right_attach = right_attach;
  table_child->top_attach = top_attach;
  table_child->bottom_attach = bottom_attach;
  table_child->xexpand = (xoptions & GTK_EXPAND) != 0;
  table_child->xshrink = (xoptions & GTK_SHRINK) != 0;
  table_child->xfill = (xoptions & GTK_FILL) != 0;
  table_child->xpadding = gtk_widget_size_to_pixel (table, xpadding);
  table_child->yexpand = (yoptions & GTK_EXPAND) != 0;
  table_child->yshrink = (yoptions & GTK_SHRINK) != 0;
  table_child->yfill = (yoptions & GTK_FILL) != 0;
  table_child->ypadding = gtk_widget_size_to_pixel (table, ypadding);

  priv_table_child = g_new (GtkTableChildPrivate, 1);
  priv_table_child->xpadding_unit = xpadding;
  priv_table_child->ypadding_unit = ypadding;
  
  table->children = g_list_prepend (table->children, table_child);
  add_priv_child (table, table_child, priv_table_child);
  
  gtk_widget_set_parent (child, GTK_WIDGET (table));
}

void
gtk_table_attach_defaults (GtkTable  *table,
			   GtkWidget *widget,
			   guint      left_attach,
			   guint      right_attach,
			   guint      top_attach,
			   guint      bottom_attach)
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
			   guint     row,
			   GtkUSize  spacing)
{
  g_return_if_fail (GTK_IS_TABLE (table));
  g_return_if_fail (row < table->nrows);

  ensure_integrity (table);

  if (table->rows[row].spacing != gtk_widget_size_to_pixel (table, spacing))
    {
      set_row_spacing (table, row, spacing);
      
      if (GTK_WIDGET_VISIBLE (table))
	gtk_widget_queue_resize (GTK_WIDGET (table));
    }
}

/**
 * gtk_table_get_row_spacing:
 * @table: a #GtkTable
 * @row: a row in the table, 0 indicates the first row
 *
 * Gets the amount of space between row @row, and
 * row @row + 1. See gtk_table_set_row_spacing().
 *
 * Return value: the row spacing
 **/
guint
gtk_table_get_row_spacing (GtkTable *table,
			   guint     row)
{
  g_return_val_if_fail (GTK_IS_TABLE (table), 0);
  g_return_val_if_fail (row < table->nrows - 1, 0);

  ensure_integrity (table);

  return table->rows[row].spacing;
}

/**
 * gtk_table_get_row_spacing_unit:
 * @table: a #GtkTable
 * @row: a row in the table, 0 indicates the first row
 *
 * Like gtk_table_get_row_spacing() but preserves the unit.
 *
 * Return value: the column spacing
 *
 * Since: 2.14
 **/
GtkUSize
gtk_table_get_row_spacing_unit (GtkTable *table,
                                guint     row)
{
  g_return_val_if_fail (GTK_IS_TABLE (table), 0);
  g_return_val_if_fail (row < table->nrows - 1, 0);

  ensure_integrity (table);

  return get_row_spacing (table, row);
}

void
gtk_table_set_col_spacing (GtkTable *table,
			   guint     column,
			   GtkUSize  spacing)
{
  g_return_if_fail (GTK_IS_TABLE (table));
  g_return_if_fail (column < table->ncols);

  ensure_integrity (table);

  if (table->cols[column].spacing != gtk_widget_size_to_pixel (table, spacing))
    {
      set_col_spacing (table, column, spacing);
      
      if (GTK_WIDGET_VISIBLE (table))
	gtk_widget_queue_resize (GTK_WIDGET (table));
    }
}

/**
 * gtk_table_get_col_spacing:
 * @table: a #GtkTable
 * @column: a column in the table, 0 indicates the first column
 *
 * Gets the amount of space between column @col, and
 * column @col + 1. See gtk_table_set_col_spacing().
 *
 * Return value: the column spacing
 **/
guint
gtk_table_get_col_spacing (GtkTable *table,
			   guint     column)
{
  g_return_val_if_fail (GTK_IS_TABLE (table), 0);
  g_return_val_if_fail (column < table->ncols, 0);

  ensure_integrity (table);

  return table->cols[column].spacing;
}

/**
 * gtk_table_get_col_spacing_unit:
 * @table: a #GtkTable
 * @column: a column in the table, 0 indicates the first column
 *
 * Like gtk_table_get_col_spacing() but preserves the unit.
 *
 * Return value: the column spacing
 *
 * Since: 2.14
 **/
GtkUSize
gtk_table_get_col_spacing_unit (GtkTable *table,
                                guint     column)
{
  g_return_val_if_fail (GTK_IS_TABLE (table), 0);
  g_return_val_if_fail (column < table->ncols, 0);

  ensure_integrity (table);

  return get_col_spacing (table, column);
}

void
gtk_table_set_row_spacings (GtkTable *table,
			    GtkUSize  spacing)
{
  guint row;
  GtkTablePrivate *priv;
  
  g_return_if_fail (GTK_IS_TABLE (table));

  priv = GTK_TABLE_GET_PRIVATE (table);

  ensure_integrity (table);
  
  table->row_spacing = gtk_widget_size_to_pixel (table, spacing);
  priv->row_spacing_unit = spacing;
  for (row = 0; row < table->nrows; row++)
    set_row_spacing (table, row, spacing);
  
  if (GTK_WIDGET_VISIBLE (table))
    gtk_widget_queue_resize (GTK_WIDGET (table));

  g_object_notify (G_OBJECT (table), "row-spacing");
}

/**
 * gtk_table_get_default_row_spacing:
 * @table: a #GtkTable
 *
 * Gets the default row spacing for the table. This is
 * the spacing that will be used for newly added rows.
 * (See gtk_table_set_row_spacings())
 *
 * Return value: the default row spacing
 **/
guint
gtk_table_get_default_row_spacing (GtkTable *table)
{
  g_return_val_if_fail (GTK_IS_TABLE (table), 0);

  ensure_integrity (table);

  return table->row_spacing;
}

/**
 * gtk_table_get_default_row_spacing_unit:
 * @table: a #GtkTable
 *
 * Like gtk_table_get_default_row_spacing() but preserves the unit.
 *
 * Return value: the default row spacing
 *
 * Since: 2.14
 **/
GtkUSize
gtk_table_get_default_row_spacing_unit (GtkTable *table)
{
  GtkTablePrivate *priv;

  g_return_val_if_fail (GTK_IS_TABLE (table), 0);

  priv = GTK_TABLE_GET_PRIVATE (table);

  ensure_integrity (table);

  return priv->row_spacing_unit;
}

void
gtk_table_set_col_spacings (GtkTable *table,
			    GtkUSize  spacing)
{
  GtkTablePrivate *priv;
  guint col;
  
  g_return_if_fail (GTK_IS_TABLE (table));

  priv = GTK_TABLE_GET_PRIVATE (table);

  ensure_integrity (table);
  
  table->column_spacing = gtk_widget_size_to_pixel (table, spacing);
  priv->column_spacing_unit = spacing;
  for (col = 0; col < table->ncols; col++)
    set_col_spacing (table, col, spacing);
  
  if (GTK_WIDGET_VISIBLE (table))
    gtk_widget_queue_resize (GTK_WIDGET (table));

  g_object_notify (G_OBJECT (table), "column-spacing");
}

/**
 * gtk_table_get_default_col_spacing:
 * @table: a #GtkTable
 *
 * Gets the default column spacing for the table. This is
 * the spacing that will be used for newly added columns.
 * (See gtk_table_set_col_spacings())
 *
 * Return value: the default column spacing
 **/
guint
gtk_table_get_default_col_spacing (GtkTable *table)
{
  g_return_val_if_fail (GTK_IS_TABLE (table), 0);

  ensure_integrity (table);

  return table->column_spacing;
}

/**
 * gtk_table_get_default_col_spacing_unit:
 * @table: a #GtkTable
 *
 * Like gtk_table_get_default_col_spacing() but preserves the unit.
 *
 * Return value: the default column spacing
 *
 * Since: 2.14
 **/
GtkUSize
gtk_table_get_default_col_spacing_unit (GtkTable *table)
{
  GtkTablePrivate *priv;

  g_return_val_if_fail (GTK_IS_TABLE (table), 0);

  ensure_integrity (table);

  priv = GTK_TABLE_GET_PRIVATE (table);

  return priv->column_spacing_unit;
}

void
gtk_table_set_homogeneous (GtkTable *table,
			   gboolean  homogeneous)
{
  g_return_if_fail (GTK_IS_TABLE (table));

  ensure_integrity (table);

  homogeneous = (homogeneous != 0);
  if (homogeneous != table->homogeneous)
    {
      table->homogeneous = homogeneous;
      
      if (GTK_WIDGET_VISIBLE (table))
	gtk_widget_queue_resize (GTK_WIDGET (table));

      g_object_notify (G_OBJECT (table), "homogeneous");
    }
}

/**
 * gtk_table_get_homogeneous:
 * @table: a #GtkTable
 *
 * Returns whether the table cells are all constrained to the same
 * width and height. (See gtk_table_set_homogenous ())
 *
 * Return value: %TRUE if the cells are all constrained to the same size
 **/
gboolean
gtk_table_get_homogeneous (GtkTable *table)
{
  g_return_val_if_fail (GTK_IS_TABLE (table), FALSE);

  ensure_integrity (table);

  return table->homogeneous;
}

static void
gtk_table_finalize (GObject *object)
{
  GtkTable *table = GTK_TABLE (object);
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);

  g_free (table->rows);
  g_free (table->cols);
  g_free (priv->rows);
  g_free (priv->cols);

  /* TODO: should check that hash table is empty */
  g_hash_table_unref (priv->child_map);
  
  G_OBJECT_CLASS (gtk_table_parent_class)->finalize (object);
}

static void
gtk_table_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkTable *table = GTK_TABLE (widget);
  gint row, col;

  ensure_integrity (table);

  requisition->width = 0;
  requisition->height = 0;
  
  gtk_table_size_request_init (table);
  gtk_table_size_request_pass1 (table);
  gtk_table_size_request_pass2 (table);
  gtk_table_size_request_pass3 (table);
  gtk_table_size_request_pass2 (table);
  
  for (col = 0; col < table->ncols; col++)
    requisition->width += table->cols[col].requisition;
  for (col = 0; col + 1 < table->ncols; col++)
    requisition->width += table->cols[col].spacing;
  
  for (row = 0; row < table->nrows; row++)
    requisition->height += table->rows[row].requisition;
  for (row = 0; row + 1 < table->nrows; row++)
    requisition->height += table->rows[row].spacing;
  
  requisition->width += GTK_CONTAINER (table)->border_width * 2;
  requisition->height += GTK_CONTAINER (table)->border_width * 2;
}

static void
gtk_table_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkTable *table = GTK_TABLE (widget);

  ensure_integrity (table);

  widget->allocation = *allocation;

  gtk_table_size_allocate_init (table);
  gtk_table_size_allocate_pass1 (table);
  gtk_table_size_allocate_pass2 (table);
}

static void
gtk_table_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  gtk_table_attach_defaults (GTK_TABLE (container), widget, 0, 1, 0, 1);
}

static void
gtk_table_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkTable *table = GTK_TABLE (container);
  GtkTableChild *child;
  GList *children;

  ensure_integrity (table);

  children = table->children;
  
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (child->widget == widget)
	{
	  gboolean was_visible = GTK_WIDGET_VISIBLE (widget);
	  
	  gtk_widget_unparent (widget);
	  
          remove_priv_child (table, child);

	  table->children = g_list_remove (table->children, child);
	  g_free (child);
	  
	  if (was_visible && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));
	  break;
	}
    }
}

static void
gtk_table_forall (GtkContainer *container,
		  gboolean	include_internals,
		  GtkCallback	callback,
		  gpointer	callback_data)
{
  GtkTable *table = GTK_TABLE (container);
  GtkTableChild *child;
  GList *children;

  ensure_integrity (table);

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
    {
      table->rows[row].requisition = 0;
      table->rows[row].expand = FALSE;
    }
  for (col = 0; col < table->ncols; col++)
    {
      table->cols[col].requisition = 0;
      table->cols[col].expand = FALSE;
    }
  
  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (GTK_WIDGET_VISIBLE (child->widget))
	gtk_widget_size_request (child->widget, NULL);

      if (child->left_attach == (child->right_attach - 1) && child->xexpand)
	table->cols[child->left_attach].expand = TRUE;
      
      if (child->top_attach == (child->bottom_attach - 1) && child->yexpand)
	table->rows[child->top_attach].expand = TRUE;
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
	  GtkRequisition child_requisition;
	  gtk_widget_get_child_requisition (child->widget, &child_requisition);

	  /* Child spans a single column.
	   */
	  if (child->left_attach == (child->right_attach - 1))
	    {
	      width = child_requisition.width + child->xpadding * 2;
	      table->cols[child->left_attach].requisition = MAX (table->cols[child->left_attach].requisition, width);
	    }
	  
	  /* Child spans a single row.
	   */
	  if (child->top_attach == (child->bottom_attach - 1))
	    {
	      height = child_requisition.height + child->ypadding * 2;
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
	      GtkRequisition child_requisition;

	      gtk_widget_get_child_requisition (child->widget, &child_requisition);
	      
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
	       *  its requisition, then divide up the needed space amongst the
	       *  columns it spans, favoring expandable columns if any.
	       */
	      if (width < child_requisition.width + child->xpadding * 2)
		{
		  gint n_expand = 0;
		  gboolean force_expand = FALSE;
		  
		  width = child_requisition.width + child->xpadding * 2 - width;

		  for (col = child->left_attach; col < child->right_attach; col++)
		    if (table->cols[col].expand)
		      n_expand++;

		  if (n_expand == 0)
		    {
		      n_expand = (child->right_attach - child->left_attach);
		      force_expand = TRUE;
		    }
		    
		  for (col = child->left_attach; col < child->right_attach; col++)
		    if (force_expand || table->cols[col].expand)
		      {
			extra = width / n_expand;
			table->cols[col].requisition += extra;
			width -= extra;
			n_expand--;
		      }
		}
	    }
	  
	  /* Child spans multiple rows.
	   */
	  if (child->top_attach != (child->bottom_attach - 1))
	    {
	      GtkRequisition child_requisition;

	      gtk_widget_get_child_requisition (child->widget, &child_requisition);

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
	       *  its requisition, then divide up the needed space amongst the
	       *  rows it spans, favoring expandable rows if any.
	       */
	      if (height < child_requisition.height + child->ypadding * 2)
		{
		  gint n_expand = 0;
		  gboolean force_expand = FALSE;
		  
		  height = child_requisition.height + child->ypadding * 2 - height;
		  
		  for (row = child->top_attach; row < child->bottom_attach; row++)
		    {
		      if (table->rows[row].expand)
			n_expand++;
		    }

		  if (n_expand == 0)
		    {
		      n_expand = (child->bottom_attach - child->top_attach);
		      force_expand = TRUE;
		    }
		    
		  for (row = child->top_attach; row < child->bottom_attach; row++)
		    if (force_expand || table->rows[row].expand)
		      {
			extra = height / n_expand;
			table->rows[row].requisition += extra;
			height -= extra;
			n_expand--;
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
      table->cols[col].empty = TRUE;
    }
  for (row = 0; row < table->nrows; row++)
    {
      table->rows[row].allocation = table->rows[row].requisition;
      table->rows[row].need_expand = FALSE;
      table->rows[row].need_shrink = TRUE;
      table->rows[row].expand = FALSE;
      table->rows[row].shrink = TRUE;
      table->rows[row].empty = TRUE;
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
	      
	      table->cols[child->left_attach].empty = FALSE;
	    }
	  
	  if (child->top_attach == (child->bottom_attach - 1))
	    {
	      if (child->yexpand)
		table->rows[child->top_attach].expand = TRUE;
	      
	      if (!child->yshrink)
		table->rows[child->top_attach].shrink = FALSE;

	      table->rows[child->top_attach].empty = FALSE;
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
	      for (col = child->left_attach; col < child->right_attach; col++)
		table->cols[col].empty = FALSE;

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
	      for (row = child->top_attach; row < child->bottom_attach; row++)
		table->rows[row].empty = FALSE;

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
      if (table->cols[col].empty)
	{
	  table->cols[col].expand = FALSE;
	  table->cols[col].shrink = FALSE;
	}
      else
	{
	  if (table->cols[col].need_expand)
	    table->cols[col].expand = TRUE;
	  if (!table->cols[col].need_shrink)
	    table->cols[col].shrink = FALSE;
	}
    }
  
  /* Loop over the rows and set the expand and shrink values
   *  if the row can be expanded or shrunk.
   */
  for (row = 0; row < table->nrows; row++)
    {
      if (table->rows[row].empty)
	{
	  table->rows[row].expand = FALSE;
	  table->rows[row].shrink = FALSE;
	}
      else
	{
	  if (table->rows[row].need_expand)
	    table->rows[row].expand = TRUE;
	  if (!table->rows[row].need_shrink)
	    table->rows[row].shrink = FALSE;
	}
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
      if (!table->children)
	nexpand = 1;
      else
	{
	  nexpand = 0;
	  for (col = 0; col < table->ncols; col++)
	    if (table->cols[col].expand)
	      {
		nexpand += 1;
		break;
	      }
	}
      if (nexpand)
	{
	  width = real_width;
	  for (col = 0; col + 1 < table->ncols; col++)
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
      for (col = 0; col + 1 < table->ncols; col++)
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
      
      /* Check to see if we were allocated less width than we requested,
       * then shrink until we fit the size give.
       */
      if (width > real_width)
	{
	  gint total_nshrink = nshrink;

	  extra = width - real_width;
	  while (total_nshrink > 0 && extra > 0)
	    {
	      nshrink = total_nshrink;
	      for (col = 0; col < table->ncols; col++)
		if (table->cols[col].shrink)
		  {
		    gint allocation = table->cols[col].allocation;

		    table->cols[col].allocation = MAX (1, (gint) table->cols[col].allocation - extra / nshrink);
		    extra -= allocation - table->cols[col].allocation;
		    nshrink -= 1;
		    if (table->cols[col].allocation < 2)
		      {
			total_nshrink -= 1;
			table->cols[col].shrink = FALSE;
		      }
		  }
	    }
	}
    }
  
  if (table->homogeneous)
    {
      if (!table->children)
	nexpand = 1;
      else
	{
	  nexpand = 0;
	  for (row = 0; row < table->nrows; row++)
	    if (table->rows[row].expand)
	      {
		nexpand += 1;
		break;
	      }
	}
      if (nexpand)
	{
	  height = real_height;
	  
	  for (row = 0; row + 1 < table->nrows; row++)
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
      for (row = 0; row + 1 < table->nrows; row++)
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
       * then shrink until we fit the size give.
       */
      if (height > real_height)
	{
	  gint total_nshrink = nshrink;
	  
	  extra = height - real_height;
	  while (total_nshrink > 0 && extra > 0)
	    {
	      nshrink = total_nshrink;
	      for (row = 0; row < table->nrows; row++)
		if (table->rows[row].shrink)
		  {
		    gint allocation = table->rows[row].allocation;
		    
		    table->rows[row].allocation = MAX (1, (gint) table->rows[row].allocation - extra / nshrink);
		    extra -= allocation - table->rows[row].allocation;
		    nshrink -= 1;
		    if (table->rows[row].allocation < 2)
		      {
			total_nshrink -= 1;
			table->rows[row].shrink = FALSE;
		      }
		  }
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
  GtkWidget *widget = GTK_WIDGET (table);
  
  children = table->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  GtkRequisition child_requisition;
	  gtk_widget_get_child_requisition (child->widget, &child_requisition);

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
	      allocation.width = child_requisition.width;
	      allocation.x = x + (max_width - allocation.width) / 2;
	    }
	  
	  if (child->yfill)
	    {
	      allocation.height = MAX (1, max_height - child->ypadding * 2);
	      allocation.y = y + (max_height - allocation.height) / 2;
	    }
	  else
	    {
	      allocation.height = child_requisition.height;
	      allocation.y = y + (max_height - allocation.height) / 2;
	    }

	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	    allocation.x = widget->allocation.x + widget->allocation.width
	      - (allocation.x - widget->allocation.x) - allocation.width;
	  
	  gtk_widget_size_allocate (child->widget, &allocation);
	}
    }
}

static void
gtk_table_unit_changed (GtkWidget *widget)
{
  GtkTable *table = GTK_TABLE (widget);
  GtkTablePrivate *priv = GTK_TABLE_GET_PRIVATE (table);
  GtkTableChild *child;
  GtkTableChildPrivate *priv_child;
  GList *children;
  guint row, col;

  /* must chain up */
  if (GTK_WIDGET_CLASS (gtk_table_parent_class)->unit_changed != NULL)
    GTK_WIDGET_CLASS (gtk_table_parent_class)->unit_changed (widget);

  table->column_spacing = gtk_widget_size_to_pixel (table, priv->column_spacing_unit);
  table->row_spacing = gtk_widget_size_to_pixel (table, priv->row_spacing_unit);

  children = table->children;
  while (children != NULL)
    {
      child = children->data;
      children = children->next;

      priv_child = get_priv_child (table, child);

      if (priv_child != NULL)
        {
          child->xpadding = gtk_widget_size_to_pixel (table, priv_child->xpadding_unit);
          child->ypadding = gtk_widget_size_to_pixel (table, priv_child->ypadding_unit);
        }
    }

  if (!priv->skip_priv)
    {
      for (row = 0; row < table->nrows; row++)
        table->rows[row].spacing = gtk_widget_size_to_pixel (table, priv->rows[row].spacing_unit);
      for (col = 0; col < table->ncols; col++)
        table->cols[col].spacing = gtk_widget_size_to_pixel (table, priv->cols[col].spacing_unit);
    }
}

#define __GTK_TABLE_C__
#include "gtkaliasdef.c"
