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
#include "gtktreeview.h"
#include "gtktreeprivate.h"
#include "gtksignal.h"
#include "gtkbutton.h"
#include "gtkalignment.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkarrow.h"
#include "gtkintl.h"

enum
{
  PROP_0,
  
  PROP_CELL_RENDERER,
  PROP_VISIBLE,
  PROP_SIZING,
  PROP_WIDTH,
  PROP_MIN_WIDTH,
  PROP_MAX_WIDTH,
  PROP_TITLE,
  PROP_CLICKABLE,
  PROP_WIDGET,
  PROP_ALIGNMENT,
  PROP_SORT_INDICATOR,
  PROP_SORT_ORDER
};

enum
{
  CLICKED,
  LAST_SIGNAL
};


static void gtk_tree_view_column_init            (GtkTreeViewColumn      *tree_column);
static void gtk_tree_view_column_class_init      (GtkTreeViewColumnClass *klass);
static void gtk_tree_view_column_set_attributesv (GtkTreeViewColumn      *tree_column,
						  va_list                 args);
static void gtk_real_tree_column_clicked         (GtkTreeViewColumn      *tree_column);
static void gtk_tree_view_column_set_property    (GObject                *object,
						  guint                   prop_id,
						  const GValue           *value,
						  GParamSpec             *pspec);
static void gtk_tree_view_column_get_property    (GObject                *object,
						  guint                   prop_id,
						  GValue                 *value,
						  GParamSpec             *pspec);
static void gtk_tree_view_column_finalize        (GObject                *object);


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
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  parent_class = g_type_class_peek_parent (class);

  class->clicked = gtk_real_tree_column_clicked;

  object_class->finalize = gtk_tree_view_column_finalize;
  object_class->set_property = gtk_tree_view_column_set_property;
  object_class->get_property = gtk_tree_view_column_get_property;
  
  tree_column_signals[CLICKED] =
    g_signal_newc ("clicked",
                   GTK_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET (GtkTreeViewColumnClass, clicked),
		   NULL, NULL,
		   gtk_marshal_VOID__VOID,
                   GTK_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_CELL_RENDERER,
                                   g_param_spec_object ("cell_renderer",
                                                        _("Cell renderer"),
                                                        _("Cell renderer object to use for rendering the cell"),
                                                        GTK_TYPE_CELL_RENDERER,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visible",
                                                        _("Visible"),
                                                        _("Whether to display the colomn"),
                                                         TRUE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_SIZING,
                                   g_param_spec_enum ("sizing",
                                                      _("Sizing"),
                                                      _("Resize mode of the column"),
                                                      GTK_TYPE_TREE_VIEW_COLUMN_SIZING,
                                                      GTK_TREE_VIEW_COLUMN_AUTOSIZE,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
                                                     _("Width"),
                                                     _("Current width of the column"),
                                                     1,
                                                     G_MAXINT,
                                                     1, /* not useful */
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_MIN_WIDTH,
                                   g_param_spec_int ("min_width",
                                                     _("Minimum Width"),
                                                     _("Minimum allowed width of the column"),
                                                     -1,
                                                     G_MAXINT,
                                                     1,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_MAX_WIDTH,
                                   g_param_spec_int ("max_width",
                                                     _("Maximum Width"),
                                                     _("Maximum allowed width of the column"),
                                                     -1,
                                                     G_MAXINT,
                                                     G_MAXINT,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        _("Title"),
                                                        _("Title to appear in column header"),
                                                        "",
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_CLICKABLE,
                                   g_param_spec_boolean ("clickable",
                                                        _("Clickable"),
                                                        _("Whether the header can be clicked"),
                                                         TRUE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
  

  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        _("Widget"),
                                                        _("Widget to put in column header button instead of column title"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ALIGNMENT,
                                   g_param_spec_float ("alignment",
                                                       _("Alignment"),
                                                       _("Alignment of the column header text or widget"),
                                                       0.0,
                                                       1.0,
                                                       0.5,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SORT_INDICATOR,
                                   g_param_spec_boolean ("sort_indicator",
                                                        _("Sort indicator"),
                                                        _("Whether to show a sort indicator"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SORT_ORDER,
                                   g_param_spec_enum ("sort_order",
                                                      _("Sort order"),
                                                      _("Sort direction the sort indicator should indicate"),
                                                      GTK_TYPE_TREE_SORT_ORDER,
                                                      GTK_TREE_SORT_ASCENDING,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
}

static void
gtk_tree_view_column_init (GtkTreeViewColumn *tree_column)
{
  /* FIXME remove on port to GtkObject */
  gtk_object_ref (GTK_OBJECT (tree_column));
  gtk_object_sink (GTK_OBJECT (tree_column));
  
  tree_column->button = NULL;
  tree_column->xalign = 0.0;
  tree_column->width = 1;
  tree_column->min_width = -1;
  tree_column->max_width = -1;
  tree_column->cell = NULL;
  tree_column->attributes = NULL;
  tree_column->column_type = GTK_TREE_VIEW_COLUMN_AUTOSIZE;
  tree_column->visible = TRUE;
  tree_column->button_active = FALSE;
  tree_column->dirty = TRUE;
  tree_column->sort_order = GTK_TREE_SORT_ASCENDING;
  tree_column->show_sort_indicator = FALSE;
}

static void
gtk_tree_view_column_set_property (GObject         *object,
                                   guint            prop_id,
                                   const GValue    *value,
                                   GParamSpec      *pspec)
{
  GtkTreeViewColumn *tree_column;

  tree_column = GTK_TREE_VIEW_COLUMN (object);

  switch (prop_id)
    {
    case PROP_CELL_RENDERER:
      gtk_tree_view_column_set_cell_renderer (tree_column,
                                              (GtkCellRenderer*)
                                              g_value_get_object (value));
      break;

    case PROP_VISIBLE:
      gtk_tree_view_column_set_visible (tree_column,
                                        g_value_get_boolean (value));
      break;

    case PROP_SIZING:
      gtk_tree_view_column_set_sizing (tree_column,
                                       g_value_get_enum (value));
      break;

    case PROP_WIDTH:
      gtk_tree_view_column_set_width (tree_column,
                                      g_value_get_int (value));
      break;

    case PROP_MIN_WIDTH:
      gtk_tree_view_column_set_min_width (tree_column,
                                          g_value_get_int (value));
      break;

    case PROP_MAX_WIDTH:
      gtk_tree_view_column_set_max_width (tree_column,
                                          g_value_get_int (value));
      break;

    case PROP_TITLE:
      gtk_tree_view_column_set_title (tree_column,
                                      g_value_get_string (value));
      break;

    case PROP_CLICKABLE:
      gtk_tree_view_column_set_clickable (tree_column,
                                          g_value_get_boolean (value));
      break;

    case PROP_WIDGET:
      gtk_tree_view_column_set_widget (tree_column,
                                       (GtkWidget*) g_value_get_object (value));
      break;

    case PROP_ALIGNMENT:
      gtk_tree_view_column_set_alignment (tree_column,
                                          g_value_get_float (value));
      break;

    case PROP_SORT_INDICATOR:
      gtk_tree_view_column_set_sort_indicator (tree_column,
                                               g_value_get_boolean (value));
      break;

    case PROP_SORT_ORDER:
      gtk_tree_view_column_set_sort_order (tree_column,
                                           g_value_get_enum (value));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_view_column_get_property (GObject         *object,
                                   guint            prop_id,
                                   GValue          *value,
                                   GParamSpec      *pspec)
{
  GtkTreeViewColumn *tree_column;

  tree_column = GTK_TREE_VIEW_COLUMN (object);

  switch (prop_id)
    {
    case PROP_CELL_RENDERER:
      g_value_set_object (value,
                          (GObject*) gtk_tree_view_column_get_cell_renderer (tree_column));
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_visible (tree_column));
      break;

    case PROP_SIZING:
      g_value_set_enum (value,
                        gtk_tree_view_column_get_sizing (tree_column));
      break;

    case PROP_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_width (tree_column));
      break;

    case PROP_MIN_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_min_width (tree_column));
      break;

    case PROP_MAX_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_max_width (tree_column));
      break;

    case PROP_TITLE:
      g_value_set_string (value,
                          gtk_tree_view_column_get_title (tree_column));
      break;

    case PROP_CLICKABLE:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_clickable (tree_column));
      break;

    case PROP_WIDGET:
      g_value_set_object (value,
                          (GObject*) gtk_tree_view_column_get_widget (tree_column));
      break;

    case PROP_ALIGNMENT:
      g_value_set_float (value,
                         gtk_tree_view_column_get_alignment (tree_column));
      break;

    case PROP_SORT_INDICATOR:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_sort_indicator (tree_column));
      break;

    case PROP_SORT_ORDER:
      g_value_set_enum (value,
                        gtk_tree_view_column_get_sort_order (tree_column));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_view_column_finalize (GObject *object)
{
  GtkTreeViewColumn *tree_column = (GtkTreeViewColumn *) object;

  if (tree_column->func_data && tree_column->destroy)
    (tree_column->destroy) (tree_column->func_data);

  g_slist_free (tree_column->attributes);
  g_free (tree_column->title);
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


static void
gtk_tree_view_column_button_clicked (GtkWidget *widget, gpointer data)
{
  g_signal_emit_by_name (G_OBJECT (data), "clicked");
}

void
_gtk_tree_view_column_create_button (GtkTreeViewColumn *column)
{
  GtkTreeView *tree_view;
  GtkWidget *child;
  GtkWidget *hbox;

  tree_view = (GtkTreeView *) column->tree_view;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column->button == NULL);

  gtk_widget_push_composite_child ();
  column->button = gtk_button_new ();
  gtk_widget_pop_composite_child ();

  /* make sure we own a reference to it as well. */
  gtk_widget_set_parent (column->button, GTK_WIDGET (tree_view));
  
  gtk_signal_connect (GTK_OBJECT (column->button), "clicked",
		      (GtkSignalFunc) gtk_tree_view_column_button_clicked,
		      (gpointer) column);

  column->alignment = gtk_alignment_new (column->xalign, 0.5, 0.0, 0.0);

  hbox = gtk_hbox_new (FALSE, 2);
  column->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_IN);

  if (column->child)
    child = column->child;
  else
    {
      child = gtk_label_new (column->title);
      gtk_widget_show (child);
    }

  if (column->xalign <= 0.5)
    gtk_box_pack_end (GTK_BOX (hbox), column->arrow, FALSE, FALSE, 0);
  else
    gtk_box_pack_start (GTK_BOX (hbox), column->arrow, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), column->alignment, TRUE, TRUE, 0);
        
  gtk_container_add (GTK_CONTAINER (column->alignment), child);
  gtk_container_add (GTK_CONTAINER (column->button), hbox);

  if (column->visible)
    gtk_widget_show (column->button);

  gtk_widget_show (hbox);
  gtk_widget_show (column->alignment);
}

void
_gtk_tree_view_column_realize_button (GtkTreeViewColumn *column)
{
  GtkTreeView *tree_view;
  GdkWindowAttr attr;
  guint attributes_mask;

  tree_view = (GtkTreeView *)column->tree_view;

  g_return_if_fail (tree_view != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_WIDGET_REALIZED (tree_view));
  g_return_if_fail (tree_view->priv->header_window != NULL);
  g_return_if_fail (column->button != NULL);

  attr.window_type = GDK_WINDOW_CHILD;
  attr.wclass = GDK_INPUT_ONLY;
  attr.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
  attr.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
  attr.event_mask = gtk_widget_get_events (GTK_WIDGET (tree_view));
  attr.event_mask = (GDK_BUTTON_PRESS_MASK |
		     GDK_BUTTON_RELEASE_MASK |
		     GDK_POINTER_MOTION_MASK |
		     GDK_POINTER_MOTION_HINT_MASK |
		     GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_CURSOR | GDK_WA_X | GDK_WA_Y;
  attr.cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
  tree_view->priv->cursor_drag = attr.cursor;

  attr.y = 0;
  attr.width = TREE_VIEW_DRAG_WIDTH;
  attr.height = tree_view->priv->header_height;

  gtk_widget_set_parent_window (column->button, tree_view->priv->header_window);

  attr.x = (column->button->allocation.x + column->button->allocation.width) - 3;
          
  column->window = gdk_window_new (tree_view->priv->header_window,
				   &attr, attributes_mask);
  gdk_window_set_user_data (column->window, tree_view);
}

void
_gtk_tree_view_column_unrealize_button (GtkTreeViewColumn *column)
{
  g_return_if_fail (column != NULL);
  g_return_if_fail (column->window != NULL);

  gdk_window_set_user_data (column->window, NULL);
  gdk_window_destroy (column->window);
  column->window = NULL;
}

void
_gtk_tree_view_column_set_tree_view (GtkTreeViewColumn *column,
				     GtkTreeView       *tree_view)
{
  column->tree_view = GTK_WIDGET (tree_view);
}

void
_gtk_tree_view_column_unset_tree_view (GtkTreeViewColumn *column)
{
  if (column->tree_view && column->button)
    {
      gtk_container_remove (GTK_CONTAINER (column->tree_view), column->button);
    }

  column->tree_view = NULL;
  column->button = NULL;
}



/* Public Functions */


/**
 * gtk_tree_view_column_new:
 * 
 * Creates a new #GtkTreeViewColumn.
 * 
 * Return value: A newly created #GtkTreeViewColumn.
 **/
GtkTreeViewColumn *
gtk_tree_view_column_new (void)
{
  GtkTreeViewColumn *retval;

  retval = GTK_TREE_VIEW_COLUMN (gtk_type_new (GTK_TYPE_TREE_VIEW_COLUMN));

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
GtkTreeViewColumn *
gtk_tree_view_column_new_with_attributes (const gchar     *title,
					  GtkCellRenderer *cell,
					  ...)
{
  GtkTreeViewColumn *retval;
  va_list args;

  retval = gtk_tree_view_column_new ();

  gtk_tree_view_column_set_title (retval, title);
  gtk_tree_view_column_set_cell_renderer (retval, cell);

  va_start (args, cell);
  gtk_tree_view_column_set_attributesv (retval, args);
  va_end (args);

  return retval;
}

/**
 * gtk_tree_view_column_set_cell_renderer:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell: The #GtkCellRenderer, or NULL.
 * 
 * Sets the cell renderer of the @tree_column.  If there is a cell
 * renderer already set, then it is removed.  If @cell is NULL, then
 * the cell renderer is unset.
 **/
void
gtk_tree_view_column_set_cell_renderer (GtkTreeViewColumn *tree_column,
					GtkCellRenderer   *cell)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (cell)
    g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  if (cell)
    g_object_ref (G_OBJECT (cell));

  if (tree_column->cell)
    g_object_unref (G_OBJECT (tree_column->cell));

  tree_column->cell = cell;

  g_object_notify (G_OBJECT (tree_column), "cell_renderer");
}

/**
 * gtk_tree_view_column_get_cell_renderer:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set with gtk_tree_view_column_set_cell_renderer().
 * 
 * Return value: cell renderer for the column, or %NULL if unset
 **/
GtkCellRenderer*
gtk_tree_view_column_get_cell_renderer (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);
  
  return tree_column->cell;
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
				    const gchar       *attribute,
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
  gchar *attribute;
  gint column;

  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  attribute = va_arg (args, gchar *);

  gtk_tree_view_column_clear_attributes (tree_column);
  
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
 * @Varargs: A NULL terminated listing of attributes.
 * 
 * Sets the attributes in the list as the attributes of @tree_column.
 * The attributes should be in attribute/column order, as in
 * @gtk_tree_view_column_add_attribute. All existing attributes
 * are removed, and replaced with the new attributes.
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
 * gtk_tree_view_column_set_cell_data_func:
 * @tree_column: A #GtkTreeViewColumn
 * @func: The #GtkTreeViewColumnFunc to use. 
 * @func_data: The user data for @func.
 * @destroy: The destroy notification for @func_data
 * 
 * Sets the #GtkTreeViewColumnFunc to use for the column.  This function is used
 * instead of the standard attributes mapping for setting the column value, and
 * should set the value of @tree_column 's cell renderer as appropriate.  @func
 * may be NULL to remove an older one.
 **/
void
gtk_tree_view_column_set_cell_data_func (GtkTreeViewColumn *tree_column,
					 GtkCellDataFunc    func,
					 gpointer           func_data,
					 GtkDestroyNotify   destroy)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_column->func_data && tree_column->destroy)
    (tree_column->destroy) (tree_column->func_data);

  tree_column->func = func;
  tree_column->func_data = func_data;
  tree_column->destroy = destroy;
}


/**
 * gtk_tree_view_column_clear_attributes:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Clears all existing attributes previously set with
 * gtk_tree_view_column_set_attributes().
 **/
void
gtk_tree_view_column_clear_attributes (GtkTreeViewColumn *tree_column)
{
  GSList *list;
  
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  list = tree_column->attributes;

  while (list && list->next)
    {
      g_free (list->data);
      list = list->next->next;
    }
  g_slist_free (tree_column->attributes);
  tree_column->attributes = NULL;
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

  if (tree_model == NULL)
    return;

  cell = (GObject *) tree_column->cell;
  list = tree_column->attributes;

  g_object_freeze_notify (cell);

  while (list && list->next)
    {
      gtk_tree_model_get_value (tree_model, iter,
				GPOINTER_TO_INT (list->next->data),
				&value);
      g_object_set_property (cell, (gchar *) list->data, &value);
      g_value_unset (&value);
      list = list->next->next;

    }

  if (tree_column->func)
    (* tree_column->func) (tree_column, tree_column->cell, tree_model, iter, tree_column->func_data);

  g_object_thaw_notify (cell);
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

  visible = !! visible;
  
  if (tree_column->visible == visible)
    return;

  tree_column->visible = visible;

  if (tree_column->tree_view != NULL)
    {
      if (visible)
	{
	  gtk_widget_show (tree_column->button);
	  if (GTK_WIDGET_REALIZED (tree_column->tree_view) && tree_column->window)
	    gdk_window_show (tree_column->window);
	}
      else
	{
	  gtk_widget_hide (tree_column->button);
	  if (GTK_WIDGET_REALIZED (tree_column->tree_view) && tree_column->window)
	    gdk_window_hide (tree_column->window);
	}

      if (GTK_WIDGET_REALIZED (tree_column->tree_view))
	_gtk_tree_view_update_size (GTK_TREE_VIEW (tree_column->tree_view));
    }

  g_object_notify (G_OBJECT (tree_column), "visible");
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
 * gtk_tree_view_column_set_sizing:
 * @tree_column: A #GtkTreeViewColumn.
 * @type: The #GtkTreeViewColumnSizing.
 * 
 * Sets the growth behavior of @tree_column to @type.
 **/
void
gtk_tree_view_column_set_sizing (GtkTreeViewColumn     *tree_column,
                                 GtkTreeViewColumnSizing  type)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (type == tree_column->column_type)
    return;

  tree_column->column_type = type;

  g_object_notify (G_OBJECT (tree_column), "sizing");

  if (tree_column->tree_view == NULL)
    return;

  switch (type)
    {
    case GTK_TREE_VIEW_COLUMN_AUTOSIZE:
      tree_column->dirty = TRUE;
    case GTK_TREE_VIEW_COLUMN_FIXED:
      if (GTK_WIDGET_REALIZED (tree_column->tree_view))
        gdk_window_hide (tree_column->window);
      break;
    default:
      if (GTK_WIDGET_REALIZED (tree_column->tree_view))
        {
          gdk_window_show (tree_column->window);
          gdk_window_raise (tree_column->window);
        }
      break;
    }

  gtk_widget_queue_resize (tree_column->tree_view);
}

/**
 * gtk_tree_view_column_get_sizing:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current type of @tree_column.
 * 
 * Return value: The type of @tree_column.
 **/
gint
gtk_tree_view_column_get_sizing (GtkTreeViewColumn *tree_column)
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

  return tree_column->width;
}

/**
 * gtk_tree_view_column_set_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @size: The size to set the @tree_column to.
 * 
 * Sets the size of the column in pixels, unless the the column type is
 * #GTK_TREE_VIEW_COLUMN_AUTOSIZE.  In this case, the value is discarded as the
 * size of the column is based on the calculated width of the column. The
 * width is clamped to the min/max width for the column.
 **/
void
gtk_tree_view_column_set_width (GtkTreeViewColumn *tree_column,
                                gint               size)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (size > 0);
  
  if (tree_column->min_width != -1 &&
      size <= tree_column->min_width)
    size = tree_column->min_width;
  else if (tree_column->max_width != -1 &&
           size > tree_column->max_width)
    size = tree_column->max_width;
  
  if (tree_column->width == size)
    return;
  
  tree_column->width = size;

  
  g_object_notify (G_OBJECT (tree_column), "width");

  if (tree_column->tree_view == NULL)
    return;

  if (GTK_WIDGET_REALIZED (tree_column->tree_view))
    gtk_widget_queue_resize (tree_column->tree_view);
}

/**
 * gtk_tree_view_column_get_width:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set by gtk_tree_view_column_set_width().
 * 
 * Return value: the width of the column
 **/
gint
gtk_tree_view_column_get_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->width;
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
      ((tree_column->min_width > tree_column->width) ||
       (tree_column->min_width == -1 &&
	tree_column->button->requisition.width > tree_column->width) ||
       (min_width > tree_column->width) ||
       (min_width == -1 &&
	tree_column->button->requisition.width > tree_column->width)))
    gtk_widget_queue_resize (tree_column->tree_view);

  if (tree_column->max_width != -1 &&
      tree_column->max_width < real_min_width)
    tree_column->max_width = real_min_width;

  tree_column->min_width = min_width;

  g_object_notify (G_OBJECT (tree_column), "min_width");
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
      ((tree_column->max_width < tree_column->width) ||
       (max_width != -1 && max_width < tree_column->width)))
    gtk_widget_queue_resize (tree_column->tree_view);

  tree_column->max_width = max_width;

  if (real_min_width > max_width)
    tree_column->min_width = max_width;

  g_object_notify (G_OBJECT (tree_column), "max_width");
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
 * gtk_tree_view_column_clicked:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Emits the "clicked" signal on the column.
 **/
void
gtk_tree_view_column_clicked (GtkTreeViewColumn *tree_column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  gtk_signal_emit (GTK_OBJECT (tree_column), tree_column_signals[CLICKED]);
}

static void 
update_button_contents (GtkTreeViewColumn *tree_column)
{
  if (tree_column->button)
    {
      GtkWidget *hbox = GTK_BIN (tree_column->button)->child;
      GtkWidget *alignment = tree_column->alignment;
      GtkWidget *arrow = tree_column->arrow;
      GtkWidget *current_child = GTK_BIN (alignment)->child;

      gtk_alignment_set (GTK_ALIGNMENT (alignment), tree_column->xalign,
                         0.5, 0.0, 0.0);
      
      if (tree_column->child)
        {
          if (current_child != tree_column->child)
            {
              gtk_container_remove (GTK_CONTAINER (alignment),
                                    current_child);

              gtk_container_add (GTK_CONTAINER (alignment),
                                 tree_column->child);
            }
        }
      else 
        {
          if (current_child == NULL)
            {
              current_child = gtk_label_new (NULL);

              gtk_widget_show (current_child);
              
              gtk_container_add (GTK_CONTAINER (alignment),
                                 current_child);
            }

          g_return_if_fail (GTK_IS_LABEL (current_child));

          if (tree_column->title)
            gtk_label_set_text (GTK_LABEL (current_child),
                                tree_column->title);
          else
            gtk_label_set_text (GTK_LABEL (current_child),
                                "");
        }

      switch (tree_column->sort_order)
        {
        case GTK_TREE_SORT_ASCENDING:
          gtk_arrow_set (GTK_ARROW (arrow),
                         GTK_ARROW_DOWN,
                         GTK_SHADOW_IN);
          break;

        case GTK_TREE_SORT_DESCENDING:
          gtk_arrow_set (GTK_ARROW (arrow),
                         GTK_ARROW_UP,
                         GTK_SHADOW_IN);
          break;
          
        default:
          g_warning (G_STRLOC": bad sort order");
          break;
        }

      /* Put arrow on the right if the text is left-or-center justified,
       * and on the left otherwise; do this by packing boxes, so flipping
       * text direction will reverse things
       */
      gtk_widget_ref (arrow);
      gtk_container_remove (GTK_CONTAINER (hbox), arrow);

      if (tree_column->xalign <= 0.5)
        {
          gtk_box_pack_end (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
        }
      else
        {
          gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
          /* move it to the front */
          gtk_box_reorder_child (GTK_BOX (hbox), arrow, 0);
        }

      gtk_widget_unref (arrow);

      if (tree_column->show_sort_indicator)
        gtk_widget_show (arrow);
      else
        gtk_widget_hide (arrow);
    }
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
				const gchar       *title)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  g_free (tree_column->title);
  if (title)
    tree_column->title = g_strdup (title);
  else
    tree_column->title = NULL;

  update_button_contents (tree_column);

  g_object_notify (G_OBJECT (tree_column), "title");
}

/**
 * gtk_tree_view_column_get_title:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the title of the widget.  This value should not be modified.
 * 
 * Return value: the title of the column.
 **/
G_CONST_RETURN gchar *
gtk_tree_view_column_get_title (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (tree_column != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->title;
}

/**
 * gtk_tree_view_column_set_clickable:
 * @tree_column: A #GtkTreeViewColumn.
 * @active: TRUE if the header is active.
 * 
 * Sets the header to be active if @active is TRUE.  When the header is active,
 * then it can take keyboard focus, and can be clicked.
 **/
void
gtk_tree_view_column_set_clickable (GtkTreeViewColumn *tree_column,
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

  g_object_notify (G_OBJECT (tree_column), "clickable");
}

/**
 * gtk_tree_view_column_get_clickable:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Returns %TRUE if the user can click on the header for the column.
 * 
 * Return value: whether the user can click the column header
 **/
gboolean
gtk_tree_view_column_get_clickable (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->button_active;
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
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (widget)
    {
      gtk_object_ref (GTK_OBJECT (widget));
      gtk_object_sink (GTK_OBJECT (widget));
    }

  if (tree_column->child)      
    gtk_object_unref (GTK_OBJECT (tree_column->child));

  tree_column->child = widget;
  
  update_button_contents (tree_column);

  g_object_notify (G_OBJECT (tree_column), "widget");
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

  return tree_column->child;
}

/**
 * gtk_tree_view_column_set_alignment:
 * @tree_column: A #GtkTreeViewColumn.
 * @xalign: alignment (0.0 for left, 0.5 for center, 1.0 for right)
 * 
 * Sets the alignment of the title or custom widget inside the column header.
 **/
void
gtk_tree_view_column_set_alignment (GtkTreeViewColumn *tree_column,
                                    gfloat             xalign)
{
  g_return_if_fail (tree_column != NULL);
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_column->xalign == xalign)
    return;

  tree_column->xalign = xalign;

  update_button_contents (tree_column);

  g_object_notify (G_OBJECT (tree_column), "alignment");
}

gfloat
gtk_tree_view_column_get_alignment (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0.5);

  return tree_column->xalign;
}

/**
 * gtk_tree_view_column_set_sort_indicator:
 * @tree_column: a #GtkTreeViewColumn
 * @setting: %TRUE to display an indicator that the column is sorted
 *
 * Call this function with a @setting of %TRUE to display an arrow in
 * the header button indicating the column is sorted. Call
 * gtk_tree_view_column_set_sort_order() to change the direction of
 * the arrow.
 * 
 **/
void
gtk_tree_view_column_set_sort_indicator (GtkTreeViewColumn     *tree_column,
                                         gboolean               setting)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  setting = setting != FALSE;

  if (setting != tree_column->show_sort_indicator)
    {
      tree_column->show_sort_indicator = setting;

      update_button_contents (tree_column);
      
      g_object_notify (G_OBJECT (tree_column), "sort_indicator");
      if (GTK_WIDGET_REALIZED (tree_column->tree_view))
	gtk_widget_queue_draw (tree_column->tree_view);
    }
}

/**
 * gtk_tree_view_column_get_sort_indicator:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set by gtk_tree_view_column_set_sort_indicator().
 * 
 * Return value: whether the sort indicator arrow is displayed
 **/
gboolean
gtk_tree_view_column_get_sort_indicator  (GtkTreeViewColumn     *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->show_sort_indicator;
}

/**
 * gtk_tree_view_column_set_sort_order:
 * @tree_column: a #GtkTreeViewColumn
 * @order: sort order that the sort indicator should indicate
 *
 * Changes the appearance of the sort indicator. (This <emphasis>does
 * not</emphasis> actually sort the model - for the models shipped
 * with GTK+, use at gtk_tree_sortable_set_sort_column() to do
 * that. For custom models, the mechanism will vary.) The sort
 * indicator changes direction to indicate normal sort or reverse
 * sort. Note that you must have the sort indicator enabled to see
 * anything when calling this function; see
 * gtk_tree_view_column_set_sort_indicator().
 * 
 **/
void
gtk_tree_view_column_set_sort_order      (GtkTreeViewColumn     *tree_column,
                                          GtkTreeSortOrder       order)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (order != tree_column->sort_order)
    {
      tree_column->sort_order = order;

      update_button_contents (tree_column);
      
      g_object_notify (G_OBJECT (tree_column), "sort_order");
    }
}

/**
 * gtk_tree_view_column_get_sort_order:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set by gtk_tree_view_column_set_sort_order().
 * 
 * Return value: the sort order the sort indicator is indicating
 **/
GtkTreeSortOrder
gtk_tree_view_column_get_sort_order      (GtkTreeViewColumn     *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->sort_order;
}


