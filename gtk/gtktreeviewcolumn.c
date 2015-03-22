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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktreeviewcolumn.h"

#include <string.h>

#include "gtktreeview.h"
#include "gtktreeprivate.h"
#include "gtkcelllayout.h"
#include "gtkbutton.h"
#include "deprecated/gtkalignment.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkmarshalers.h"
#include "gtkimage.h"
#include "gtkcellareacontext.h"
#include "gtkcellareabox.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "a11y/gtktreeviewaccessibleprivate.h"


/**
 * SECTION:gtktreeviewcolumn
 * @Short_description: A visible column in a GtkTreeView widget
 * @Title: GtkTreeViewColumn
 * @See_also: #GtkTreeView, #GtkTreeSelection, #GtkTreeModel, #GtkTreeSortable,
 *   #GtkTreeModelSort, #GtkListStore, #GtkTreeStore, #GtkCellRenderer, #GtkCellEditable,
 *   #GtkCellRendererPixbuf, #GtkCellRendererText, #GtkCellRendererToggle,
 *   [GtkTreeView drag-and-drop][gtk3-GtkTreeView-drag-and-drop]
 *
 * The GtkTreeViewColumn object represents a visible column in a #GtkTreeView widget.
 * It allows to set properties of the column header, and functions as a holding pen for
 * the cell renderers which determine how the data in the column is displayed.
 *
 * Please refer to the [tree widget conceptual overview][TreeWidget]
 * for an overview of all the objects and data types related to the tree widget and how
 * they work together.
 */


/* Type methods */
static void gtk_tree_view_column_cell_layout_init              (GtkCellLayoutIface      *iface);

/* GObject methods */
static void gtk_tree_view_column_set_property                  (GObject                 *object,
								guint                    prop_id,
								const GValue            *value,
								GParamSpec              *pspec);
static void gtk_tree_view_column_get_property                  (GObject                 *object,
								guint                    prop_id,
								GValue                  *value,
								GParamSpec              *pspec);
static void gtk_tree_view_column_finalize                      (GObject                 *object);
static void gtk_tree_view_column_dispose                       (GObject                 *object);
static void gtk_tree_view_column_constructed                   (GObject                 *object);

/* GtkCellLayout implementation */
static void       gtk_tree_view_column_ensure_cell_area        (GtkTreeViewColumn      *column,
                                                                GtkCellArea            *cell_area);

static GtkCellArea *gtk_tree_view_column_cell_layout_get_area  (GtkCellLayout           *cell_layout);

/* Button handling code */
static void gtk_tree_view_column_create_button                 (GtkTreeViewColumn       *tree_column);
static void gtk_tree_view_column_update_button                 (GtkTreeViewColumn       *tree_column);

/* Button signal handlers */
static gint gtk_tree_view_column_button_event                  (GtkWidget               *widget,
								GdkEvent                *event,
								gpointer                 data);
static void gtk_tree_view_column_button_clicked                (GtkWidget               *widget,
								gpointer                 data);
static gboolean gtk_tree_view_column_mnemonic_activate         (GtkWidget *widget,
					                        gboolean   group_cycling,
								gpointer   data);

/* Property handlers */
static void gtk_tree_view_model_sort_column_changed            (GtkTreeSortable         *sortable,
								GtkTreeViewColumn       *tree_column);

/* GtkCellArea/GtkCellAreaContext callbacks */
static void gtk_tree_view_column_context_changed               (GtkCellAreaContext      *context,
								GParamSpec              *pspec,
								GtkTreeViewColumn       *tree_column);
static void gtk_tree_view_column_add_editable_callback         (GtkCellArea             *area,
								GtkCellRenderer         *renderer,
								GtkCellEditable         *edit_widget,
								GdkRectangle            *cell_area,
								const gchar             *path_string,
								gpointer                 user_data);
static void gtk_tree_view_column_remove_editable_callback      (GtkCellArea             *area,
								GtkCellRenderer         *renderer,
								GtkCellEditable         *edit_widget,
								gpointer                 user_data);

/* Internal functions */
static void gtk_tree_view_column_sort                          (GtkTreeViewColumn       *tree_column,
								gpointer                 data);
static void gtk_tree_view_column_setup_sort_column_id_callback (GtkTreeViewColumn       *tree_column);
static void gtk_tree_view_column_set_attributesv               (GtkTreeViewColumn       *tree_column,
								GtkCellRenderer         *cell_renderer,
								va_list                  args);

/* GtkBuildable implementation */
static void gtk_tree_view_column_buildable_init                 (GtkBuildableIface     *iface);


struct _GtkTreeViewColumnPrivate 
{
  GtkWidget *tree_view;
  GtkWidget *button;
  GtkWidget *child;
  GtkWidget *arrow;
  GtkWidget *alignment;
  GdkWindow *window;
  gulong property_changed_signal;
  gfloat xalign;

  /* Sizing fields */
  /* see gtk+/doc/tree-column-sizing.txt for more information on them */
  GtkTreeViewColumnSizing column_type;
  gint padding;
  gint x_offset;
  gint width;
  gint fixed_width;
  gint min_width;
  gint max_width;

  /* dragging columns */
  gint drag_x;
  gint drag_y;

  gchar *title;

  /* Sorting */
  gulong      sort_clicked_signal;
  gulong      sort_column_changed_signal;
  gint        sort_column_id;
  GtkSortType sort_order;

  /* Cell area */
  GtkCellArea        *cell_area;
  GtkCellAreaContext *cell_area_context;
  gulong              add_editable_signal;
  gulong              remove_editable_signal;
  gulong              context_changed_signal;

  /* Flags */
  guint visible             : 1;
  guint resizable           : 1;
  guint clickable           : 1;
  guint dirty               : 1;
  guint show_sort_indicator : 1;
  guint maybe_reordered     : 1;
  guint reorderable         : 1;
  guint expand              : 1;
};

enum
{
  PROP_0,
  PROP_VISIBLE,
  PROP_RESIZABLE,
  PROP_X_OFFSET,
  PROP_WIDTH,
  PROP_SPACING,
  PROP_SIZING,
  PROP_FIXED_WIDTH,
  PROP_MIN_WIDTH,
  PROP_MAX_WIDTH,
  PROP_TITLE,
  PROP_EXPAND,
  PROP_CLICKABLE,
  PROP_WIDGET,
  PROP_ALIGNMENT,
  PROP_REORDERABLE,
  PROP_SORT_INDICATOR,
  PROP_SORT_ORDER,
  PROP_SORT_COLUMN_ID,
  PROP_CELL_AREA
};

enum
{
  CLICKED,
  LAST_SIGNAL
};

static guint tree_column_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkTreeViewColumn, gtk_tree_view_column, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (GtkTreeViewColumn)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_tree_view_column_cell_layout_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_tree_view_column_buildable_init))


static void
gtk_tree_view_column_class_init (GtkTreeViewColumnClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  class->clicked = NULL;

  object_class->constructed = gtk_tree_view_column_constructed;
  object_class->finalize = gtk_tree_view_column_finalize;
  object_class->dispose = gtk_tree_view_column_dispose;
  object_class->set_property = gtk_tree_view_column_set_property;
  object_class->get_property = gtk_tree_view_column_get_property;
  
  tree_column_signals[CLICKED] =
    g_signal_new (I_("clicked"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewColumnClass, clicked),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visible",
                                                        P_("Visible"),
                                                        P_("Whether to display the column"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (object_class,
                                   PROP_RESIZABLE,
                                   g_param_spec_boolean ("resizable",
							 P_("Resizable"),
							 P_("Column is user-resizable"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (object_class,
                                   PROP_X_OFFSET,
                                   g_param_spec_int ("x-offset",
                                                     P_("X position"),
                                                     P_("Current X position of the column"),
                                                     -G_MAXINT,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
						     P_("Width"),
						     P_("Current width of the column"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READABLE));
  g_object_class_install_property (object_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
						     P_("Spacing"),
						     P_("Space which is inserted between cells"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
                                   PROP_SIZING,
                                   g_param_spec_enum ("sizing",
                                                      P_("Sizing"),
                                                      P_("Resize mode of the column"),
                                                      GTK_TYPE_TREE_VIEW_COLUMN_SIZING,
                                                      GTK_TREE_VIEW_COLUMN_GROW_ONLY,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (object_class,
                                   PROP_FIXED_WIDTH,
                                   g_param_spec_int ("fixed-width",
                                                     P_("Fixed Width"),
                                                     P_("Current fixed width of the column"),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_MIN_WIDTH,
                                   g_param_spec_int ("min-width",
                                                     P_("Minimum Width"),
                                                     P_("Minimum allowed width of the column"),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_MAX_WIDTH,
                                   g_param_spec_int ("max-width",
                                                     P_("Maximum Width"),
                                                     P_("Maximum allowed width of the column"),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        P_("Title"),
                                                        P_("Title to appear in column header"),
                                                        "",
                                                        GTK_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_EXPAND,
                                   g_param_spec_boolean ("expand",
							 P_("Expand"),
							 P_("Column gets share of extra width allocated to the widget"),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (object_class,
                                   PROP_CLICKABLE,
                                   g_param_spec_boolean ("clickable",
                                                        P_("Clickable"),
                                                        P_("Whether the header can be clicked"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  

  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        P_("Widget"),
                                                        P_("Widget to put in column header button instead of column title"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ALIGNMENT,
                                   g_param_spec_float ("alignment",
                                                       P_("Alignment"),
                                                       P_("X Alignment of the column header text or widget"),
                                                       0.0, 1.0, 0.0,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
							 P_("Reorderable"),
							 P_("Whether the column can be reordered around the headers"),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_SORT_INDICATOR,
                                   g_param_spec_boolean ("sort-indicator",
                                                        P_("Sort indicator"),
                                                        P_("Whether to show a sort indicator"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_SORT_ORDER,
                                   g_param_spec_enum ("sort-order",
                                                      P_("Sort order"),
                                                      P_("Sort direction the sort indicator should indicate"),
                                                      GTK_TYPE_SORT_TYPE,
                                                      GTK_SORT_ASCENDING,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTreeViewColumn:sort-column-id:
   *
   * Logical sort column ID this column sorts on when selected for sorting. Setting the sort column ID makes the column header
   * clickable. Set to -1 to make the column unsortable.
   *
   * Since: 2.18
   **/
  g_object_class_install_property (object_class,
                                   PROP_SORT_COLUMN_ID,
                                   g_param_spec_int ("sort-column-id",
                                                     P_("Sort column ID"),
                                                     P_("Logical sort column ID this column sorts on when selected for sorting"),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTreeViewColumn:cell-area:
   *
   * The #GtkCellArea used to layout cell renderers for this column.
   *
   * If no area is specified when creating the tree view column with gtk_tree_view_column_new_with_area() 
   * a horizontally oriented #GtkCellAreaBox will be used.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
				   PROP_CELL_AREA,
				   g_param_spec_object ("cell-area",
							P_("Cell Area"),
							P_("The GtkCellArea used to layout cells"),
							GTK_TYPE_CELL_AREA,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gtk_tree_view_column_custom_tag_end (GtkBuildable *buildable,
				     GtkBuilder   *builder,
				     GObject      *child,
				     const gchar  *tagname,
				     gpointer     *data)
{
  /* Just ignore the boolean return from here */
  _gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname, data);
}

static void
gtk_tree_view_column_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = _gtk_cell_layout_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_tree_view_column_custom_tag_end;
}

static void
gtk_tree_view_column_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_tree_view_column_cell_layout_get_area;
}

static void
gtk_tree_view_column_init (GtkTreeViewColumn *tree_column)
{
  GtkTreeViewColumnPrivate *priv;

  tree_column->priv = gtk_tree_view_column_get_instance_private (tree_column);
  priv = tree_column->priv;

  priv->button = NULL;
  priv->xalign = 0.0;
  priv->width = 0;
  priv->padding = 0;
  priv->min_width = -1;
  priv->max_width = -1;
  priv->column_type = GTK_TREE_VIEW_COLUMN_GROW_ONLY;
  priv->visible = TRUE;
  priv->resizable = FALSE;
  priv->expand = FALSE;
  priv->clickable = FALSE;
  priv->dirty = TRUE;
  priv->sort_order = GTK_SORT_ASCENDING;
  priv->show_sort_indicator = FALSE;
  priv->property_changed_signal = 0;
  priv->sort_clicked_signal = 0;
  priv->sort_column_changed_signal = 0;
  priv->sort_column_id = -1;
  priv->reorderable = FALSE;
  priv->maybe_reordered = FALSE;
  priv->fixed_width = -1;
  priv->title = g_strdup ("");
}

static void
gtk_tree_view_column_constructed (GObject *object)
{
  GtkTreeViewColumn *tree_column = GTK_TREE_VIEW_COLUMN (object);

  G_OBJECT_CLASS (gtk_tree_view_column_parent_class)->constructed (object);

  gtk_tree_view_column_ensure_cell_area (tree_column, NULL);
}

static void
gtk_tree_view_column_dispose (GObject *object)
{
  GtkTreeViewColumn        *tree_column = (GtkTreeViewColumn *) object;
  GtkTreeViewColumnPrivate *priv        = tree_column->priv;

  /* Remove this column from its treeview, 
   * in case this column is destroyed before its treeview.
   */ 
  if (priv->tree_view)
    gtk_tree_view_remove_column (GTK_TREE_VIEW (priv->tree_view), tree_column);
    
  if (priv->cell_area_context)
    { 
      g_signal_handler_disconnect (priv->cell_area_context,
				   priv->context_changed_signal);

      g_object_unref (priv->cell_area_context);

      priv->cell_area_context = NULL;
      priv->context_changed_signal = 0;
    }

  if (priv->cell_area)
    {
      g_signal_handler_disconnect (priv->cell_area,
				   priv->add_editable_signal);
      g_signal_handler_disconnect (priv->cell_area,
				   priv->remove_editable_signal);

      g_object_unref (priv->cell_area);
      priv->cell_area = NULL;
      priv->add_editable_signal = 0;
      priv->remove_editable_signal = 0;
    }

  if (priv->child)
    {
      g_object_unref (priv->child);
      priv->child = NULL;
    }

  G_OBJECT_CLASS (gtk_tree_view_column_parent_class)->dispose (object);
}

static void
gtk_tree_view_column_finalize (GObject *object)
{
  GtkTreeViewColumn        *tree_column = (GtkTreeViewColumn *) object;
  GtkTreeViewColumnPrivate *priv        = tree_column->priv;

  g_free (priv->title);

  G_OBJECT_CLASS (gtk_tree_view_column_parent_class)->finalize (object);
}

static void
gtk_tree_view_column_set_property (GObject         *object,
                                   guint            prop_id,
                                   const GValue    *value,
                                   GParamSpec      *pspec)
{
  GtkTreeViewColumn *tree_column;
  GtkCellArea       *area;

  tree_column = GTK_TREE_VIEW_COLUMN (object);

  switch (prop_id)
    {
    case PROP_VISIBLE:
      gtk_tree_view_column_set_visible (tree_column,
                                        g_value_get_boolean (value));
      break;

    case PROP_RESIZABLE:
      gtk_tree_view_column_set_resizable (tree_column,
					  g_value_get_boolean (value));
      break;

    case PROP_SIZING:
      gtk_tree_view_column_set_sizing (tree_column,
                                       g_value_get_enum (value));
      break;

    case PROP_FIXED_WIDTH:
      gtk_tree_view_column_set_fixed_width (tree_column,
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

    case PROP_SPACING:
      gtk_tree_view_column_set_spacing (tree_column,
					g_value_get_int (value));
      break;

    case PROP_TITLE:
      gtk_tree_view_column_set_title (tree_column,
                                      g_value_get_string (value));
      break;

    case PROP_EXPAND:
      gtk_tree_view_column_set_expand (tree_column,
				       g_value_get_boolean (value));
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

    case PROP_REORDERABLE:
      gtk_tree_view_column_set_reorderable (tree_column,
					    g_value_get_boolean (value));
      break;

    case PROP_SORT_INDICATOR:
      gtk_tree_view_column_set_sort_indicator (tree_column,
                                               g_value_get_boolean (value));
      break;

    case PROP_SORT_ORDER:
      gtk_tree_view_column_set_sort_order (tree_column,
                                           g_value_get_enum (value));
      break;
      
    case PROP_SORT_COLUMN_ID:
      gtk_tree_view_column_set_sort_column_id (tree_column,
                                               g_value_get_int (value));
      break;

    case PROP_CELL_AREA:
      /* Construct-only, can only be assigned once */
      area = g_value_get_object (value);

      if (area)
        {
          if (tree_column->priv->cell_area != NULL)
            {
              g_warning ("cell-area has already been set, ignoring construct property");
              g_object_ref_sink (area);
              g_object_unref (area);
            }
          else
            gtk_tree_view_column_ensure_cell_area (tree_column, area);
        }
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
    case PROP_VISIBLE:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_visible (tree_column));
      break;

    case PROP_RESIZABLE:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_resizable (tree_column));
      break;

    case PROP_X_OFFSET:
      g_value_set_int (value,
                       gtk_tree_view_column_get_x_offset (tree_column));
      break;

    case PROP_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_width (tree_column));
      break;

    case PROP_SPACING:
      g_value_set_int (value,
                       gtk_tree_view_column_get_spacing (tree_column));
      break;

    case PROP_SIZING:
      g_value_set_enum (value,
                        gtk_tree_view_column_get_sizing (tree_column));
      break;

    case PROP_FIXED_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_fixed_width (tree_column));
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

    case PROP_EXPAND:
      g_value_set_boolean (value,
                          gtk_tree_view_column_get_expand (tree_column));
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

    case PROP_REORDERABLE:
      g_value_set_boolean (value,
			   gtk_tree_view_column_get_reorderable (tree_column));
      break;

    case PROP_SORT_INDICATOR:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_sort_indicator (tree_column));
      break;

    case PROP_SORT_ORDER:
      g_value_set_enum (value,
                        gtk_tree_view_column_get_sort_order (tree_column));
      break;
      
    case PROP_SORT_COLUMN_ID:
      g_value_set_int (value,
                       gtk_tree_view_column_get_sort_column_id (tree_column));
      break;

    case PROP_CELL_AREA:
      g_value_set_object (value, tree_column->priv->cell_area);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Implementation of GtkCellLayout interface
 */

static void
gtk_tree_view_column_ensure_cell_area (GtkTreeViewColumn *column,
                                       GtkCellArea       *cell_area)
{
  GtkTreeViewColumnPrivate *priv = column->priv;

  if (priv->cell_area)
    return;

  if (cell_area)
    priv->cell_area = cell_area;
  else
    priv->cell_area = gtk_cell_area_box_new ();

  g_object_ref_sink (priv->cell_area);

  priv->add_editable_signal =
    g_signal_connect (priv->cell_area, "add-editable",
                      G_CALLBACK (gtk_tree_view_column_add_editable_callback),
                      column);
  priv->remove_editable_signal =
    g_signal_connect (priv->cell_area, "remove-editable",
                      G_CALLBACK (gtk_tree_view_column_remove_editable_callback),
                      column);

  priv->cell_area_context = gtk_cell_area_create_context (priv->cell_area);

  priv->context_changed_signal =
    g_signal_connect (priv->cell_area_context, "notify",
                      G_CALLBACK (gtk_tree_view_column_context_changed),
                      column);
}

static GtkCellArea *
gtk_tree_view_column_cell_layout_get_area (GtkCellLayout   *cell_layout)
{
  GtkTreeViewColumn        *column = GTK_TREE_VIEW_COLUMN (cell_layout);
  GtkTreeViewColumnPrivate *priv   = column->priv;

  if (G_UNLIKELY (!priv->cell_area))
    gtk_tree_view_column_ensure_cell_area (column, NULL);

  return priv->cell_area;
}

/* Button handling code
 */
static void
gtk_tree_view_column_create_button (GtkTreeViewColumn *tree_column)
{
  GtkTreeViewColumnPrivate *priv = tree_column->priv;
  GtkTreeView *tree_view;
  GtkWidget *child;
  GtkWidget *hbox;

  tree_view = (GtkTreeView *) priv->tree_view;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (priv->button == NULL);

  priv->button = gtk_button_new ();
  if (priv->visible)
    gtk_widget_show (priv->button);
  gtk_widget_add_events (priv->button, GDK_POINTER_MOTION_MASK);

  /* make sure we own a reference to it as well. */
  if (_gtk_tree_view_get_header_window (tree_view))
    gtk_widget_set_parent_window (priv->button, _gtk_tree_view_get_header_window (tree_view));

  gtk_widget_set_parent (priv->button, GTK_WIDGET (tree_view));

  g_signal_connect (priv->button, "event",
		    G_CALLBACK (gtk_tree_view_column_button_event),
		    tree_column);
  g_signal_connect (priv->button, "clicked",
		    G_CALLBACK (gtk_tree_view_column_button_clicked),
		    tree_column);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  priv->alignment = gtk_alignment_new (priv->xalign, 0.5, 0.0, 0.0);
G_GNUC_END_IGNORE_DEPRECATIONS

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  priv->arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);

  if (priv->child)
    child = priv->child;
  else
    {
      child = gtk_label_new (priv->title);
      gtk_widget_show (child);
    }

  g_signal_connect (child, "mnemonic-activate",
		    G_CALLBACK (gtk_tree_view_column_mnemonic_activate),
		    tree_column);

  if (priv->xalign <= 0.5)
    {
      gtk_box_pack_start (GTK_BOX (hbox), priv->alignment, TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), priv->arrow, FALSE, FALSE, 0);
    }
  else
    {
      gtk_box_pack_start (GTK_BOX (hbox), priv->arrow, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), priv->alignment, TRUE, TRUE, 0);
    }

  gtk_container_add (GTK_CONTAINER (priv->alignment), child);
  gtk_container_add (GTK_CONTAINER (priv->button), hbox);

  gtk_widget_show (hbox);
  gtk_widget_show (priv->alignment);
  gtk_tree_view_column_update_button (tree_column);
}

static void 
gtk_tree_view_column_update_button (GtkTreeViewColumn *tree_column)
{
  GtkTreeViewColumnPrivate *priv = tree_column->priv;
  gint sort_column_id = -1;
  GtkWidget *hbox;
  GtkWidget *alignment;
  GtkWidget *arrow;
  GtkWidget *current_child;
  const gchar *icon_name = "missing-image";
  GtkTreeModel *model;

  if (priv->tree_view)
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
  else
    model = NULL;

  /* Create a button if necessary */
  if (priv->visible &&
      priv->button == NULL &&
      priv->tree_view &&
      gtk_widget_get_realized (priv->tree_view))
    gtk_tree_view_column_create_button (tree_column);
  
  if (! priv->button)
    return;

  hbox = gtk_bin_get_child (GTK_BIN (priv->button));
  alignment = priv->alignment;
  arrow = priv->arrow;
  current_child = gtk_bin_get_child (GTK_BIN (alignment));

  /* Set up the actual button */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_alignment_set (GTK_ALIGNMENT (alignment), priv->xalign, 0.5, 0.0, 0.0);
G_GNUC_END_IGNORE_DEPRECATIONS
      
  if (priv->child)
    {
      if (current_child != priv->child)
	{
	  gtk_container_remove (GTK_CONTAINER (alignment),
				current_child);
	  gtk_container_add (GTK_CONTAINER (alignment),
			     priv->child);
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

      if (priv->title)
	gtk_label_set_text_with_mnemonic (GTK_LABEL (current_child),
					  priv->title);
      else
	gtk_label_set_text_with_mnemonic (GTK_LABEL (current_child),
					  "");
    }

  if (GTK_IS_TREE_SORTABLE (model))
    gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (model),
					  &sort_column_id,
					  NULL);

  if (priv->show_sort_indicator)
    {
      gboolean alternative;

      g_object_get (gtk_widget_get_settings (priv->tree_view),
		    "gtk-alternative-sort-arrows", &alternative,
		    NULL);

      switch (priv->sort_order)
        {
	  case GTK_SORT_ASCENDING:
            icon_name = alternative ? "pan-up-symbolic" : "pan-down-symbolic";
	    break;

	  case GTK_SORT_DESCENDING:
            icon_name = alternative ? "pan-down-symbolic" : "pan-up-symbolic";
	    break;

	  default:
	    g_warning (G_STRLOC": bad sort order");
	    break;
	}
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (arrow), icon_name, GTK_ICON_SIZE_BUTTON);

  /* Put arrow on the right if the text is left-or-center justified, and on the
   * left otherwise; do this by packing boxes, so flipping text direction will
   * reverse things
   */
  if (priv->xalign <= 0.5)
    gtk_box_reorder_child (GTK_BOX (hbox), arrow, 1);
  else
    gtk_box_reorder_child (GTK_BOX (hbox), arrow, 0);

  if (priv->show_sort_indicator
      || (GTK_IS_TREE_SORTABLE (model) && priv->sort_column_id >= 0))
    gtk_widget_show (arrow);
  else
    gtk_widget_hide (arrow);

  if (priv->show_sort_indicator)
    gtk_widget_set_opacity (arrow, 1.0);
  else
    gtk_widget_set_opacity (arrow, 0.0);

  /* It's always safe to hide the button.  It isn't always safe to show it, as
   * if you show it before it's realized, it'll get the wrong window. */
  if (priv->button &&
      priv->tree_view != NULL &&
      gtk_widget_get_realized (priv->tree_view))
    {
      if (priv->visible &&
          gdk_window_is_visible (_gtk_tree_view_get_header_window (GTK_TREE_VIEW (priv->tree_view))))
	{
          gtk_widget_show (priv->button);

	  if (priv->window)
	    {
	      if (priv->resizable)
		{
		  gdk_window_show (priv->window);
		  gdk_window_raise (priv->window);
		}
	      else
		{
		  gdk_window_hide (priv->window);
		}
	    }
	}
      else
	{
	  gtk_widget_hide (priv->button);
	  if (priv->window)
	    gdk_window_hide (priv->window);
	}
    }
  
  if (priv->reorderable || priv->clickable)
    {
      gtk_widget_set_can_focus (priv->button, TRUE);
    }
  else
    {
      gtk_widget_set_can_focus (priv->button, FALSE);
      if (gtk_widget_has_focus (priv->button))
	{
	  GtkWidget *toplevel = gtk_widget_get_toplevel (priv->tree_view);
	  if (gtk_widget_is_toplevel (toplevel))
	    {
	      gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
	    }
	}
    }
  /* Queue a resize on the assumption that we always want to catch all changes
   * and columns don't change all that often.
   */
  if (gtk_widget_get_realized (priv->tree_view))
     gtk_widget_queue_resize (priv->tree_view);

}

/* Button signal handlers
 */

static gint
gtk_tree_view_column_button_event (GtkWidget *widget,
				   GdkEvent  *event,
				   gpointer   data)
{
  GtkTreeViewColumn        *column = (GtkTreeViewColumn *) data;
  GtkTreeViewColumnPrivate *priv   = column->priv;

  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS &&
      priv->reorderable &&
      ((GdkEventButton *)event)->button == GDK_BUTTON_PRIMARY)
    {
      priv->maybe_reordered = TRUE;
      priv->drag_x = event->button.x;
      priv->drag_y = event->button.y;
      gtk_widget_grab_focus (widget);
    }

  if (event->type == GDK_BUTTON_RELEASE ||
      event->type == GDK_LEAVE_NOTIFY)
    priv->maybe_reordered = FALSE;
  
  if (event->type == GDK_MOTION_NOTIFY &&
      priv->maybe_reordered &&
      (gtk_drag_check_threshold (widget,
				 priv->drag_x,
				 priv->drag_y,
				 (gint) ((GdkEventMotion *)event)->x,
				 (gint) ((GdkEventMotion *)event)->y)))
    {
      priv->maybe_reordered = FALSE;
      _gtk_tree_view_column_start_drag (GTK_TREE_VIEW (priv->tree_view), column,
                                        event->motion.device);
      return TRUE;
    }

  if (priv->clickable == FALSE)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_RELEASE:
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  return TRUE;
	default:
	  return FALSE;
	}
    }
  return FALSE;
}


static void
gtk_tree_view_column_button_clicked (GtkWidget *widget, gpointer data)
{
  g_signal_emit_by_name (data, "clicked");
}

static gboolean
gtk_tree_view_column_mnemonic_activate (GtkWidget *widget,
					gboolean   group_cycling,
					gpointer   data)
{
  GtkTreeViewColumn        *column = (GtkTreeViewColumn *)data;
  GtkTreeViewColumnPrivate *priv   = column->priv;

  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), FALSE);

  _gtk_tree_view_set_focus_column (GTK_TREE_VIEW (priv->tree_view), column);

  if (priv->clickable)
    gtk_button_clicked (GTK_BUTTON (priv->button));
  else if (gtk_widget_get_can_focus (priv->button))
    gtk_widget_grab_focus (priv->button);
  else
    gtk_widget_grab_focus (priv->tree_view);

  return TRUE;
}

static void
gtk_tree_view_model_sort_column_changed (GtkTreeSortable   *sortable,
					 GtkTreeViewColumn *column)
{
  GtkTreeViewColumnPrivate *priv = column->priv;
  gint sort_column_id;
  GtkSortType order;

  if (gtk_tree_sortable_get_sort_column_id (sortable,
					    &sort_column_id,
					    &order))
    {
      if (sort_column_id == priv->sort_column_id)
	{
	  gtk_tree_view_column_set_sort_indicator (column, TRUE);
	  gtk_tree_view_column_set_sort_order (column, order);
	}
      else
	{
	  gtk_tree_view_column_set_sort_indicator (column, FALSE);
	}
    }
  else
    {
      gtk_tree_view_column_set_sort_indicator (column, FALSE);
    }
}

static void
gtk_tree_view_column_sort (GtkTreeViewColumn *tree_column,
			   gpointer           data)
{
  GtkTreeViewColumnPrivate *priv = tree_column->priv;
  GtkTreeModel *model;
  GtkTreeSortable *sortable;
  gint sort_column_id;
  GtkSortType order;
  gboolean has_sort_column;
  gboolean has_default_sort_func;

  g_return_if_fail (priv->tree_view != NULL);

  model    = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
  sortable = GTK_TREE_SORTABLE (model);

  has_sort_column =
    gtk_tree_sortable_get_sort_column_id (sortable,
					  &sort_column_id,
					  &order);
  has_default_sort_func =
    gtk_tree_sortable_has_default_sort_func (sortable);

  if (has_sort_column &&
      sort_column_id == priv->sort_column_id)
    {
      if (order == GTK_SORT_ASCENDING)
	gtk_tree_sortable_set_sort_column_id (sortable,
					      priv->sort_column_id,
					      GTK_SORT_DESCENDING);
      else if (order == GTK_SORT_DESCENDING && has_default_sort_func)
	gtk_tree_sortable_set_sort_column_id (sortable,
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);
      else
	gtk_tree_sortable_set_sort_column_id (sortable,
					      priv->sort_column_id,
					      GTK_SORT_ASCENDING);
    }
  else
    {
      gtk_tree_sortable_set_sort_column_id (sortable,
					    priv->sort_column_id,
					    GTK_SORT_ASCENDING);
    }
}

static void
gtk_tree_view_column_setup_sort_column_id_callback (GtkTreeViewColumn *tree_column)
{
  GtkTreeViewColumnPrivate *priv = tree_column->priv;
  GtkTreeModel *model;

  if (priv->tree_view == NULL)
    return;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));

  if (model == NULL)
    return;

  if (GTK_IS_TREE_SORTABLE (model) &&
      priv->sort_column_id != -1)
    {
      gint real_sort_column_id;
      GtkSortType real_order;

      if (priv->sort_column_changed_signal == 0)
        priv->sort_column_changed_signal =
	  g_signal_connect (model, "sort-column-changed",
			    G_CALLBACK (gtk_tree_view_model_sort_column_changed),
			    tree_column);

      if (gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (model),
						&real_sort_column_id,
						&real_order) &&
	  (real_sort_column_id == priv->sort_column_id))
	{
	  gtk_tree_view_column_set_sort_indicator (tree_column, TRUE);
	  gtk_tree_view_column_set_sort_order (tree_column, real_order);
 	}
      else 
	{
	  gtk_tree_view_column_set_sort_indicator (tree_column, FALSE);
	}
   }
}

static void
gtk_tree_view_column_context_changed  (GtkCellAreaContext      *context,
				       GParamSpec              *pspec,
				       GtkTreeViewColumn       *tree_column)
{
  /* Here we want the column re-requested if the underlying context was
   * actually reset for any reason, this can happen if the underlying
   * area/cell configuration changes (i.e. cell packing properties
   * or cell spacing and the like) 
   *
   * Note that we block this handler while requesting for sizes
   * so there is no need to check for the new context size being -1,
   * we also block the handler when explicitly resetting the context
   * so as to avoid some infinite stack recursion.
   */
  if (!strcmp (pspec->name, "minimum-width") ||
      !strcmp (pspec->name, "natural-width") ||
      !strcmp (pspec->name, "minimum-height") ||
      !strcmp (pspec->name, "natural-height"))
    _gtk_tree_view_column_cell_set_dirty (tree_column, TRUE);
}

static void
gtk_tree_view_column_add_editable_callback (GtkCellArea       *area,
                                            GtkCellRenderer   *renderer,
                                            GtkCellEditable   *edit_widget,
                                            GdkRectangle      *cell_area,
                                            const gchar       *path_string,
                                            gpointer           user_data)
{
  GtkTreeViewColumn        *column = user_data;
  GtkTreeViewColumnPrivate *priv   = column->priv;
  GtkTreePath              *path;

  if (priv->tree_view)
    {
      path = gtk_tree_path_new_from_string (path_string);
      
      _gtk_tree_view_add_editable (GTK_TREE_VIEW (priv->tree_view),
				   column,
				   path,
				   edit_widget,
				   cell_area);
      
      gtk_tree_path_free (path);
    }
}

static void
gtk_tree_view_column_remove_editable_callback (GtkCellArea     *area,
                                               GtkCellRenderer *renderer,
                                               GtkCellEditable *edit_widget,
                                               gpointer         user_data)
{
  GtkTreeViewColumn        *column = user_data;
  GtkTreeViewColumnPrivate *priv   = column->priv;

  if (priv->tree_view)
    _gtk_tree_view_remove_editable (GTK_TREE_VIEW (priv->tree_view),
				    column,
				    edit_widget);
}

/* Exported Private Functions.
 * These should only be called by gtktreeview.c or gtktreeviewcolumn.c
 */
void
_gtk_tree_view_column_realize_button (GtkTreeViewColumn *column)
{
  GtkTreeViewColumnPrivate *priv = column->priv;
  GtkAllocation allocation;
  GtkTreeView *tree_view;
  GdkWindowAttr attr;
  guint attributes_mask;
  gboolean rtl;

  tree_view = (GtkTreeView *)priv->tree_view;
  rtl       = (gtk_widget_get_direction (priv->tree_view) == GTK_TEXT_DIR_RTL);

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (gtk_widget_get_realized (priv->tree_view));
  g_return_if_fail (priv->button != NULL);

  g_return_if_fail (_gtk_tree_view_get_header_window (tree_view) != NULL);
  gtk_widget_set_parent_window (priv->button, _gtk_tree_view_get_header_window (tree_view));

  attr.window_type = GDK_WINDOW_CHILD;
  attr.wclass = GDK_INPUT_ONLY;
  attr.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
  attr.event_mask = gtk_widget_get_events (GTK_WIDGET (tree_view)) |
                    (GDK_BUTTON_PRESS_MASK |
		     GDK_BUTTON_RELEASE_MASK |
		     GDK_POINTER_MOTION_MASK |
		     GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_CURSOR | GDK_WA_X | GDK_WA_Y;
  attr.cursor = gdk_cursor_new_for_display 
    (gdk_window_get_display (_gtk_tree_view_get_header_window (tree_view)), GDK_SB_H_DOUBLE_ARROW);
  attr.y = 0;
  attr.width = TREE_VIEW_DRAG_WIDTH;
  attr.height = _gtk_tree_view_get_header_height (tree_view);

  gtk_widget_get_allocation (priv->button, &allocation);
  attr.x       = (allocation.x + (rtl ? 0 : allocation.width)) - TREE_VIEW_DRAG_WIDTH / 2;
  priv->window = gdk_window_new (_gtk_tree_view_get_header_window (tree_view),
				 &attr, attributes_mask);
  gtk_widget_register_window (GTK_WIDGET (tree_view), priv->window);

  gtk_tree_view_column_update_button (column);

  g_object_unref (attr.cursor);
}

void
_gtk_tree_view_column_unrealize_button (GtkTreeViewColumn *column)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (column != NULL);

  priv = column->priv;
  g_return_if_fail (priv->window != NULL);

  gtk_widget_unregister_window (GTK_WIDGET (priv->tree_view), priv->window);
  gdk_window_destroy (priv->window);
  priv->window = NULL;
}

void
_gtk_tree_view_column_unset_model (GtkTreeViewColumn *column,
				   GtkTreeModel      *old_model)
{
  GtkTreeViewColumnPrivate *priv = column->priv;

  if (priv->sort_column_changed_signal)
    {
      g_signal_handler_disconnect (old_model,
				   priv->sort_column_changed_signal);
      priv->sort_column_changed_signal = 0;
    }
  gtk_tree_view_column_set_sort_indicator (column, FALSE);
}

void
_gtk_tree_view_column_set_tree_view (GtkTreeViewColumn *column,
				     GtkTreeView       *tree_view)
{
  GtkTreeViewColumnPrivate *priv = column->priv;

  g_assert (priv->tree_view == NULL);

  priv->tree_view = GTK_WIDGET (tree_view);
  gtk_tree_view_column_create_button (column);

  priv->property_changed_signal =
    g_signal_connect_swapped (tree_view,
			      "notify::model",
			      G_CALLBACK (gtk_tree_view_column_setup_sort_column_id_callback),
			      column);

  gtk_tree_view_column_setup_sort_column_id_callback (column);
}

void
_gtk_tree_view_column_unset_tree_view (GtkTreeViewColumn *column)
{
  GtkTreeViewColumnPrivate *priv = column->priv;

  if (priv->tree_view && priv->button)
    {
      gtk_container_remove (GTK_CONTAINER (priv->tree_view), priv->button);
    }
  if (priv->property_changed_signal)
    {
      g_signal_handler_disconnect (priv->tree_view, priv->property_changed_signal);
      priv->property_changed_signal = 0;
    }

  if (priv->sort_column_changed_signal)
    {
      g_signal_handler_disconnect (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view)),
				   priv->sort_column_changed_signal);
      priv->sort_column_changed_signal = 0;
    }

  priv->tree_view = NULL;
  priv->button = NULL;
}

gboolean
_gtk_tree_view_column_has_editable_cell (GtkTreeViewColumn *column)
{
  GtkTreeViewColumnPrivate *priv = column->priv;
  gboolean ret = FALSE;
  GList *list, *cells;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->cell_area));

  for (list = cells; list; list = list->next)
    {
      GtkCellRenderer *cell = list->data;
      GtkCellRendererMode mode;

      g_object_get (cell, "mode", &mode, NULL);

      if (mode == GTK_CELL_RENDERER_MODE_EDITABLE)
        {
          ret = TRUE;
          break;
        }
    }

  g_list_free (cells);

  return ret;
}

/* gets cell being edited */
GtkCellRenderer *
_gtk_tree_view_column_get_edited_cell (GtkTreeViewColumn *column)
{
  GtkTreeViewColumnPrivate *priv = column->priv;

  return gtk_cell_area_get_edited_cell (priv->cell_area);
}

GtkCellRenderer *
_gtk_tree_view_column_get_cell_at_pos (GtkTreeViewColumn *column,
                                       GdkRectangle      *cell_area,
                                       GdkRectangle      *background_area,
                                       gint               x,
                                       gint               y)
{
  GtkCellRenderer *match = NULL;
  GtkTreeViewColumnPrivate *priv = column->priv;

  /* If (x, y) is outside of the background area, immediately return */
  if (x < background_area->x ||
      x > background_area->x + background_area->width ||
      y < background_area->y ||
      y > background_area->y + background_area->height)
    return NULL;

  /* If (x, y) is inside the background area, clamp it to the cell_area
   * so that a cell is still returned.  The main reason for doing this
   * (on the x axis) is for handling clicks in the indentation area
   * (either at the left or right depending on RTL setting).  Another
   * reason is for handling clicks on the area where the focus rectangle
   * is drawn (this is outside of cell area), this manifests itself
   * mainly when a large setting is used for focus-line-width.
   */
  if (x < cell_area->x)
    x = cell_area->x;
  else if (x > cell_area->x + cell_area->width)
    x = cell_area->x + cell_area->width;

  if (y < cell_area->y)
    y = cell_area->y;
  else if (y > cell_area->y + cell_area->height)
    y = cell_area->y + cell_area->height;

  match = gtk_cell_area_get_cell_at_position (priv->cell_area,
                                              priv->cell_area_context,
                                              priv->tree_view,
                                              cell_area,
                                              x, y,
                                              NULL);

  return match;
}

gboolean
_gtk_tree_view_column_is_blank_at_pos (GtkTreeViewColumn *column,
                                       GdkRectangle      *cell_area,
                                       GdkRectangle      *background_area,
                                       gint               x,
                                       gint               y)
{
  GtkCellRenderer *match;
  GdkRectangle cell_alloc, aligned_area, inner_area;
  GtkTreeViewColumnPrivate *priv = column->priv;

  match = _gtk_tree_view_column_get_cell_at_pos (column,
                                                 cell_area,
                                                 background_area,
                                                 x, y);
  if (!match)
    return FALSE;

  gtk_cell_area_get_cell_allocation (priv->cell_area,
                                     priv->cell_area_context,
                                     priv->tree_view,
                                     match,
                                     cell_area,
                                     &cell_alloc);

  gtk_cell_area_inner_cell_area (priv->cell_area, priv->tree_view,
                                 &cell_alloc, &inner_area);
  gtk_cell_renderer_get_aligned_area (match, priv->tree_view, 0,
                                      &inner_area, &aligned_area);

  if (x < aligned_area.x ||
      x > aligned_area.x + aligned_area.width ||
      y < aligned_area.y ||
      y > aligned_area.y + aligned_area.height)
    return TRUE;

  return FALSE;
}

/* Public Functions */


/**
 * gtk_tree_view_column_new:
 * 
 * Creates a new #GtkTreeViewColumn.
 * 
 * Returns: A newly created #GtkTreeViewColumn.
 **/
GtkTreeViewColumn *
gtk_tree_view_column_new (void)
{
  GtkTreeViewColumn *tree_column;

  tree_column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN, NULL);

  return tree_column;
}

/**
 * gtk_tree_view_column_new_with_area:
 * @area: the #GtkCellArea that the newly created column should use to layout cells.
 * 
 * Creates a new #GtkTreeViewColumn using @area to render its cells.
 * 
 * Returns: A newly created #GtkTreeViewColumn.
 *
 * Since: 3.0
 */
GtkTreeViewColumn *
gtk_tree_view_column_new_with_area (GtkCellArea *area)
{
  GtkTreeViewColumn *tree_column;

  tree_column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN, "cell-area", area, NULL);

  return tree_column;
}


/**
 * gtk_tree_view_column_new_with_attributes:
 * @title: The title to set the header to
 * @cell: The #GtkCellRenderer
 * @...: A %NULL-terminated list of attributes
 *
 * Creates a new #GtkTreeViewColumn with a number of default values.
 * This is equivalent to calling gtk_tree_view_column_set_title(),
 * gtk_tree_view_column_pack_start(), and
 * gtk_tree_view_column_set_attributes() on the newly created #GtkTreeViewColumn.
 *
 * Here’s a simple example:
 * |[<!-- language="C" -->
 *  enum { TEXT_COLUMN, COLOR_COLUMN, N_COLUMNS };
 *  ...
 *  {
 *    GtkTreeViewColumn *column;
 *    GtkCellRenderer   *renderer = gtk_cell_renderer_text_new ();
 *  
 *    column = gtk_tree_view_column_new_with_attributes ("Title",
 *                                                       renderer,
 *                                                       "text", TEXT_COLUMN,
 *                                                       "foreground", COLOR_COLUMN,
 *                                                       NULL);
 *  }
 * ]|
 * 
 * Returns: A newly created #GtkTreeViewColumn.
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
  gtk_tree_view_column_pack_start (retval, cell, TRUE);

  va_start (args, cell);
  gtk_tree_view_column_set_attributesv (retval, cell, args);
  va_end (args);

  return retval;
}

/**
 * gtk_tree_view_column_pack_start:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell: The #GtkCellRenderer. 
 * @expand: %TRUE if @cell is to be given extra space allocated to @tree_column.
 *
 * Packs the @cell into the beginning of the column. If @expand is %FALSE, then
 * the @cell is allocated no more space than it needs. Any unused space is divided
 * evenly between cells for which @expand is %TRUE.
 **/
void
gtk_tree_view_column_pack_start (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 gboolean           expand)
{
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tree_column), cell, expand);
}

/**
 * gtk_tree_view_column_pack_end:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell: The #GtkCellRenderer. 
 * @expand: %TRUE if @cell is to be given extra space allocated to @tree_column.
 *
 * Adds the @cell to end of the column. If @expand is %FALSE, then the @cell
 * is allocated no more space than it needs. Any unused space is divided
 * evenly between cells for which @expand is %TRUE.
 **/
void
gtk_tree_view_column_pack_end (GtkTreeViewColumn  *tree_column,
			       GtkCellRenderer    *cell,
			       gboolean            expand)
{
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (tree_column), cell, expand);
}

/**
 * gtk_tree_view_column_clear:
 * @tree_column: A #GtkTreeViewColumn
 * 
 * Unsets all the mappings on all renderers on the @tree_column.
 **/
void
gtk_tree_view_column_clear (GtkTreeViewColumn *tree_column)
{
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (tree_column));
}

/**
 * gtk_tree_view_column_add_attribute:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell_renderer: the #GtkCellRenderer to set attributes on
 * @attribute: An attribute on the renderer
 * @column: The column position on the model to get the attribute from.
 * 
 * Adds an attribute mapping to the list in @tree_column.  The @column is the
 * column of the model to get a value from, and the @attribute is the
 * parameter on @cell_renderer to be set from the value. So for example
 * if column 2 of the model contains strings, you could have the
 * “text” attribute of a #GtkCellRendererText get its values from
 * column 2.
 **/
void
gtk_tree_view_column_add_attribute (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer   *cell_renderer,
				    const gchar       *attribute,
				    gint               column)
{
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (tree_column),
                                 cell_renderer, attribute, column);
}

static void
gtk_tree_view_column_set_attributesv (GtkTreeViewColumn *tree_column,
				      GtkCellRenderer   *cell_renderer,
				      va_list            args)
{
  GtkTreeViewColumnPrivate *priv = tree_column->priv;
  gchar *attribute;
  gint column;

  attribute = va_arg (args, gchar *);

  gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (priv->cell_area),
                                    cell_renderer);
  
  while (attribute != NULL)
    {
      column = va_arg (args, gint);
      gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->cell_area),
                                     cell_renderer, attribute, column);
      attribute = va_arg (args, gchar *);
    }
}

/**
 * gtk_tree_view_column_set_attributes:
 * @tree_column: A #GtkTreeViewColumn
 * @cell_renderer: the #GtkCellRenderer we’re setting the attributes of
 * @...: A %NULL-terminated list of attributes
 *
 * Sets the attributes in the list as the attributes of @tree_column.
 * The attributes should be in attribute/column order, as in
 * gtk_tree_view_column_add_attribute(). All existing attributes
 * are removed, and replaced with the new attributes.
 */
void
gtk_tree_view_column_set_attributes (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell_renderer,
				     ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell_renderer));

  va_start (args, cell_renderer);
  gtk_tree_view_column_set_attributesv (tree_column, cell_renderer, args);
  va_end (args);
}


/**
 * gtk_tree_view_column_set_cell_data_func:
 * @tree_column: A #GtkTreeViewColumn
 * @cell_renderer: A #GtkCellRenderer
 * @func: (allow-none): The #GtkTreeCellDataFunc to use. 
 * @func_data: (closure): The user data for @func.
 * @destroy: The destroy notification for @func_data
 * 
 * Sets the #GtkTreeCellDataFunc to use for the column.  This
 * function is used instead of the standard attributes mapping for
 * setting the column value, and should set the value of @tree_column's
 * cell renderer as appropriate.  @func may be %NULL to remove an
 * older one.
 **/
void
gtk_tree_view_column_set_cell_data_func (GtkTreeViewColumn   *tree_column,
					 GtkCellRenderer     *cell_renderer,
					 GtkTreeCellDataFunc  func,
					 gpointer             func_data,
					 GDestroyNotify       destroy)
{
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (tree_column),
                                      cell_renderer,
                                      (GtkCellLayoutDataFunc)func,
                                      func_data, destroy);
}


/**
 * gtk_tree_view_column_clear_attributes:
 * @tree_column: a #GtkTreeViewColumn
 * @cell_renderer: a #GtkCellRenderer to clear the attribute mapping on.
 * 
 * Clears all existing attributes previously set with
 * gtk_tree_view_column_set_attributes().
 **/
void
gtk_tree_view_column_clear_attributes (GtkTreeViewColumn *tree_column,
				       GtkCellRenderer   *cell_renderer)
{
  gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (tree_column),
                                    cell_renderer);
}

/**
 * gtk_tree_view_column_set_spacing:
 * @tree_column: A #GtkTreeViewColumn.
 * @spacing: distance between cell renderers in pixels.
 * 
 * Sets the spacing field of @tree_column, which is the number of pixels to
 * place between cell renderers packed into it.
 **/
void
gtk_tree_view_column_set_spacing (GtkTreeViewColumn *tree_column,
				  gint               spacing)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (spacing >= 0);

  priv = tree_column->priv;

  if (gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (priv->cell_area)) != spacing)
    {
      gtk_cell_area_box_set_spacing (GTK_CELL_AREA_BOX (priv->cell_area), spacing);
      if (priv->tree_view)
        _gtk_tree_view_column_cell_set_dirty (tree_column, TRUE);
      g_object_notify (G_OBJECT (tree_column), "spacing");
    }
}

/**
 * gtk_tree_view_column_get_spacing:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the spacing of @tree_column.
 * 
 * Returns: the spacing of @tree_column.
 **/
gint
gtk_tree_view_column_get_spacing (GtkTreeViewColumn *tree_column)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  priv = tree_column->priv;

  return gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (priv->cell_area));
}

/* Options for manipulating the columns */

/**
 * gtk_tree_view_column_set_visible:
 * @tree_column: A #GtkTreeViewColumn.
 * @visible: %TRUE if the @tree_column is visible.
 * 
 * Sets the visibility of @tree_column.
 */
void
gtk_tree_view_column_set_visible (GtkTreeViewColumn *tree_column,
                                  gboolean           visible)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv    = tree_column->priv;
  visible = !! visible;

  if (priv->visible == visible)
    return;

  priv->visible = visible;

  if (priv->visible)
    _gtk_tree_view_column_cell_set_dirty (tree_column, TRUE);

  if (priv->tree_view)
    {
      _gtk_tree_view_reset_header_styles (GTK_TREE_VIEW (priv->tree_view));
      _gtk_tree_view_accessible_toggle_visibility (GTK_TREE_VIEW (priv->tree_view),
                                                   tree_column);
    }

  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "visible");
}

/**
 * gtk_tree_view_column_get_visible:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns %TRUE if @tree_column is visible.
 * 
 * Returns: whether the column is visible or not.  If it is visible, then
 * the tree will show the column.
 **/
gboolean
gtk_tree_view_column_get_visible (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->priv->visible;
}

/**
 * gtk_tree_view_column_set_resizable:
 * @tree_column: A #GtkTreeViewColumn
 * @resizable: %TRUE, if the column can be resized
 * 
 * If @resizable is %TRUE, then the user can explicitly resize the column by
 * grabbing the outer edge of the column button.  If resizable is %TRUE and
 * sizing mode of the column is #GTK_TREE_VIEW_COLUMN_AUTOSIZE, then the sizing
 * mode is changed to #GTK_TREE_VIEW_COLUMN_GROW_ONLY.
 **/
void
gtk_tree_view_column_set_resizable (GtkTreeViewColumn *tree_column,
				    gboolean           resizable)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv      = tree_column->priv;
  resizable = !! resizable;

  if (priv->resizable == resizable)
    return;

  priv->resizable = resizable;

  if (resizable && priv->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
    gtk_tree_view_column_set_sizing (tree_column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

  gtk_tree_view_column_update_button (tree_column);

  g_object_notify (G_OBJECT (tree_column), "resizable");
}

/**
 * gtk_tree_view_column_get_resizable:
 * @tree_column: A #GtkTreeViewColumn
 * 
 * Returns %TRUE if the @tree_column can be resized by the end user.
 * 
 * Returns: %TRUE, if the @tree_column can be resized.
 **/
gboolean
gtk_tree_view_column_get_resizable (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->priv->resizable;
}


/**
 * gtk_tree_view_column_set_sizing:
 * @tree_column: A #GtkTreeViewColumn.
 * @type: The #GtkTreeViewColumnSizing.
 * 
 * Sets the growth behavior of @tree_column to @type.
 **/
void
gtk_tree_view_column_set_sizing (GtkTreeViewColumn       *tree_column,
                                 GtkTreeViewColumnSizing  type)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv      = tree_column->priv;

  if (type == priv->column_type)
    return;

  if (type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
    gtk_tree_view_column_set_resizable (tree_column, FALSE);

  priv->column_type = type;

  gtk_tree_view_column_update_button (tree_column);

  g_object_notify (G_OBJECT (tree_column), "sizing");
}

/**
 * gtk_tree_view_column_get_sizing:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current type of @tree_column.
 * 
 * Returns: The type of @tree_column.
 **/
GtkTreeViewColumnSizing
gtk_tree_view_column_get_sizing (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->priv->column_type;
}

/**
 * gtk_tree_view_column_get_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current size of @tree_column in pixels.
 * 
 * Returns: The current width of @tree_column.
 **/
gint
gtk_tree_view_column_get_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->priv->width;
}

/**
 * gtk_tree_view_column_get_x_offset:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current X offset of @tree_column in pixels.
 * 
 * Returns: The current X offset of @tree_column.
 *
 * Since: 3.2
 */
gint
gtk_tree_view_column_get_x_offset (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->priv->x_offset;
}

gint
_gtk_tree_view_column_request_width (GtkTreeViewColumn *tree_column)
{
  GtkTreeViewColumnPrivate *priv;
  gint real_requested_width;

  priv = tree_column->priv;

  if (priv->fixed_width != -1)
    {
      real_requested_width = priv->fixed_width;
    }
  else if (gtk_tree_view_get_headers_visible (GTK_TREE_VIEW (priv->tree_view)))
    {
      gint button_request;
      gint requested_width;

      gtk_cell_area_context_get_preferred_width (priv->cell_area_context, &requested_width, NULL);
      requested_width += priv->padding;

      gtk_widget_get_preferred_width (priv->button, &button_request, NULL);
      real_requested_width = MAX (requested_width, button_request);
    }
  else
    {
      gint requested_width;

      gtk_cell_area_context_get_preferred_width (priv->cell_area_context, &requested_width, NULL);
      requested_width += priv->padding;

      real_requested_width = requested_width;
      if (real_requested_width < 0)
        real_requested_width = 0;
    }

  if (priv->min_width != -1)
    real_requested_width = MAX (real_requested_width, priv->min_width);

  if (priv->max_width != -1)
    real_requested_width = MIN (real_requested_width, priv->max_width);

  return real_requested_width;
}

void
_gtk_tree_view_column_allocate (GtkTreeViewColumn *tree_column,
				int                x_offset,
				int                width)
{
  GtkTreeViewColumnPrivate *priv;
  gboolean                  rtl;
  GtkAllocation             allocation = { 0, 0, 0, 0 };

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  if (priv->width != width)
    gtk_widget_queue_draw (priv->tree_view);

  priv->x_offset = x_offset;
  priv->width = width;

  gtk_cell_area_context_allocate (priv->cell_area_context, priv->width - priv->padding, -1);

  if (gtk_tree_view_get_headers_visible (GTK_TREE_VIEW (priv->tree_view)))
    {
      allocation.x      = x_offset;
      allocation.y      = 0;
      allocation.width  = width;
      allocation.height = _gtk_tree_view_get_header_height (GTK_TREE_VIEW (priv->tree_view));

      gtk_widget_size_allocate (priv->button, &allocation);
    }

  if (priv->window)
    {
      rtl = (gtk_widget_get_direction (priv->tree_view) == GTK_TEXT_DIR_RTL);
      gdk_window_move_resize (priv->window,
			      allocation.x + (rtl ? 0 : allocation.width) - TREE_VIEW_DRAG_WIDTH/2,
			      allocation.y,
			      TREE_VIEW_DRAG_WIDTH, allocation.height);
    }

  g_object_notify (G_OBJECT (tree_column), "x-offset");
  g_object_notify (G_OBJECT (tree_column), "width");
}

/**
 * gtk_tree_view_column_set_fixed_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @fixed_width: The new fixed width, in pixels, or -1.
 *
 * If @fixed_width is not -1, sets the fixed width of @tree_column; otherwise
 * unsets it.  The effective value of @fixed_width is clamped between the
 * minumum and maximum width of the column; however, the value stored in the
 * “fixed-width” property is not clamped.  If the column sizing is 
 * #GTK_TREE_VIEW_COLUMN_GROW_ONLY or #GTK_TREE_VIEW_COLUMN_AUTOSIZE, setting a
 * fixed width overrides the automatically calculated width.  Note that
 * @fixed_width is only a hint to GTK+; the width actually allocated to the
 * column may be greater or less than requested.
 *
 * Along with “expand”, the “fixed-width” property changes when the column is
 * resized by the user.
 **/
void
gtk_tree_view_column_set_fixed_width (GtkTreeViewColumn *tree_column,
				      gint               fixed_width)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (fixed_width >= -1);

  priv = tree_column->priv;

  if (priv->fixed_width != fixed_width)
    {
      priv->fixed_width = fixed_width;
      if (priv->visible &&
          priv->tree_view != NULL &&
          gtk_widget_get_realized (priv->tree_view))
        gtk_widget_queue_resize (priv->tree_view);

      g_object_notify (G_OBJECT (tree_column), "fixed-width");
    }
}

/**
 * gtk_tree_view_column_get_fixed_width:
 * @tree_column: A #GtkTreeViewColumn.
 *
 * Gets the fixed width of the column.  This may not be the actual displayed
 * width of the column; for that, use gtk_tree_view_column_get_width().
 *
 * Returns: The fixed width of the column.
 **/
gint
gtk_tree_view_column_get_fixed_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->priv->fixed_width;
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
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (min_width >= -1);

  priv = tree_column->priv;

  if (min_width == priv->min_width)
    return;

  if (priv->visible &&
      priv->tree_view != NULL &&
      gtk_widget_get_realized (priv->tree_view))
    {
      if (min_width > priv->width)
	gtk_widget_queue_resize (priv->tree_view);
    }

  priv->min_width = min_width;
  g_object_freeze_notify (G_OBJECT (tree_column));
  if (priv->max_width != -1 && priv->max_width < min_width)
    {
      priv->max_width = min_width;
      g_object_notify (G_OBJECT (tree_column), "max-width");
    }
  g_object_notify (G_OBJECT (tree_column), "min-width");
  g_object_thaw_notify (G_OBJECT (tree_column));

  if (priv->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE && priv->tree_view)
    _gtk_tree_view_column_autosize (GTK_TREE_VIEW (priv->tree_view),
				    tree_column);
}

/**
 * gtk_tree_view_column_get_min_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the minimum width in pixels of the @tree_column, or -1 if no minimum
 * width is set.
 * 
 * Returns: The minimum width of the @tree_column.
 **/
gint
gtk_tree_view_column_get_min_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), -1);

  return tree_column->priv->min_width;
}

/**
 * gtk_tree_view_column_set_max_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @max_width: The maximum width of the column in pixels, or -1.
 * 
 * Sets the maximum width of the @tree_column.  If @max_width is -1, then the
 * maximum width is unset.  Note, the column can actually be wider than max
 * width if it’s the last column in a view.  In this case, the column expands to
 * fill any extra space.
 **/
void
gtk_tree_view_column_set_max_width (GtkTreeViewColumn *tree_column,
				    gint               max_width)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (max_width >= -1);

  priv = tree_column->priv;

  if (max_width == priv->max_width)
    return;

  if (priv->visible &&
      priv->tree_view != NULL &&
      gtk_widget_get_realized (priv->tree_view))
    {
      if (max_width != -1 && max_width < priv->width)
	gtk_widget_queue_resize (priv->tree_view);
    }

  priv->max_width = max_width;
  g_object_freeze_notify (G_OBJECT (tree_column));
  if (max_width != -1 && max_width < priv->min_width)
    {
      priv->min_width = max_width;
      g_object_notify (G_OBJECT (tree_column), "min-width");
    }
  g_object_notify (G_OBJECT (tree_column), "max-width");
  g_object_thaw_notify (G_OBJECT (tree_column));

  if (priv->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE && priv->tree_view)
    _gtk_tree_view_column_autosize (GTK_TREE_VIEW (priv->tree_view),
				    tree_column);
}

/**
 * gtk_tree_view_column_get_max_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the maximum width in pixels of the @tree_column, or -1 if no maximum
 * width is set.
 * 
 * Returns: The maximum width of the @tree_column.
 **/
gint
gtk_tree_view_column_get_max_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), -1);

  return tree_column->priv->max_width;
}

/**
 * gtk_tree_view_column_clicked:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Emits the “clicked” signal on the column.  This function will only work if
 * @tree_column is clickable.
 **/
void
gtk_tree_view_column_clicked (GtkTreeViewColumn *tree_column)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  if (priv->visible &&
      priv->button &&
      priv->clickable)
    gtk_button_clicked (GTK_BUTTON (priv->button));
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
  GtkTreeViewColumnPrivate *priv;
  gchar                    *new_title;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  new_title = g_strdup (title);
  g_free (priv->title);
  priv->title = new_title;

  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "title");
}

/**
 * gtk_tree_view_column_get_title:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the title of the widget.
 * 
 * Returns: the title of the column. This string should not be
 * modified or freed.
 **/
const gchar *
gtk_tree_view_column_get_title (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->priv->title;
}

/**
 * gtk_tree_view_column_set_expand:
 * @tree_column: A #GtkTreeViewColumn.
 * @expand: %TRUE if the column should expand to fill available space.
 *
 * Sets the column to take available extra space.  This space is shared equally
 * amongst all columns that have the expand set to %TRUE.  If no column has this
 * option set, then the last column gets all extra space.  By default, every
 * column is created with this %FALSE.
 *
 * Along with “fixed-width”, the “expand” property changes when the column is
 * resized by the user.
 *
 * Since: 2.4
 **/
void
gtk_tree_view_column_set_expand (GtkTreeViewColumn *tree_column,
				 gboolean           expand)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  expand = expand?TRUE:FALSE;
  if (priv->expand == expand)
    return;
  priv->expand = expand;

  if (priv->visible &&
      priv->tree_view != NULL &&
      gtk_widget_get_realized (priv->tree_view))
    {
      gtk_widget_queue_resize (priv->tree_view);
    }

  g_object_notify (G_OBJECT (tree_column), "expand");
}

/**
 * gtk_tree_view_column_get_expand:
 * @tree_column: A #GtkTreeViewColumn.
 *
 * Returns %TRUE if the column expands to fill available space.
 *
 * Returns: %TRUE if the column expands to fill available space.
 *
 * Since: 2.4
 **/
gboolean
gtk_tree_view_column_get_expand (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->priv->expand;
}

/**
 * gtk_tree_view_column_set_clickable:
 * @tree_column: A #GtkTreeViewColumn.
 * @clickable: %TRUE if the header is active.
 * 
 * Sets the header to be active if @clickable is %TRUE.  When the header is
 * active, then it can take keyboard focus, and can be clicked.
 **/
void
gtk_tree_view_column_set_clickable (GtkTreeViewColumn *tree_column,
                                    gboolean           clickable)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  clickable = !! clickable;
  if (priv->clickable == clickable)
    return;

  priv->clickable = clickable;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "clickable");
}

/**
 * gtk_tree_view_column_get_clickable:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Returns %TRUE if the user can click on the header for the column.
 * 
 * Returns: %TRUE if user can click the column header.
 **/
gboolean
gtk_tree_view_column_get_clickable (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->priv->clickable;
}

/**
 * gtk_tree_view_column_set_widget:
 * @tree_column: A #GtkTreeViewColumn.
 * @widget: (allow-none): A child #GtkWidget, or %NULL.
 *
 * Sets the widget in the header to be @widget.  If widget is %NULL, then the
 * header button is set with a #GtkLabel set to the title of @tree_column.
 **/
void
gtk_tree_view_column_set_widget (GtkTreeViewColumn *tree_column,
				 GtkWidget         *widget)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  priv = tree_column->priv;

  if (widget)
    g_object_ref_sink (widget);

  if (priv->child)      
    g_object_unref (priv->child);

  priv->child = widget;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "widget");
}

/**
 * gtk_tree_view_column_get_widget:
 * @tree_column: A #GtkTreeViewColumn.
 *
 * Returns the #GtkWidget in the button on the column header.
 * If a custom widget has not been set then %NULL is returned.
 *
 * Returns: (transfer none): The #GtkWidget in the column
 *     header, or %NULL
 **/
GtkWidget *
gtk_tree_view_column_get_widget (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->priv->child;
}

/**
 * gtk_tree_view_column_set_alignment:
 * @tree_column: A #GtkTreeViewColumn.
 * @xalign: The alignment, which is between [0.0 and 1.0] inclusive.
 * 
 * Sets the alignment of the title or custom widget inside the column header.
 * The alignment determines its location inside the button -- 0.0 for left, 0.5
 * for center, 1.0 for right.
 **/
void
gtk_tree_view_column_set_alignment (GtkTreeViewColumn *tree_column,
                                    gfloat             xalign)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  xalign = CLAMP (xalign, 0.0, 1.0);

  if (priv->xalign == xalign)
    return;

  priv->xalign = xalign;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "alignment");
}

/**
 * gtk_tree_view_column_get_alignment:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current x alignment of @tree_column.  This value can range
 * between 0.0 and 1.0.
 * 
 * Returns: The current alignent of @tree_column.
 **/
gfloat
gtk_tree_view_column_get_alignment (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0.5);

  return tree_column->priv->xalign;
}

/**
 * gtk_tree_view_column_set_reorderable:
 * @tree_column: A #GtkTreeViewColumn
 * @reorderable: %TRUE, if the column can be reordered.
 * 
 * If @reorderable is %TRUE, then the column can be reordered by the end user
 * dragging the header.
 **/
void
gtk_tree_view_column_set_reorderable (GtkTreeViewColumn *tree_column,
				      gboolean           reorderable)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  /*  if (reorderable)
      gtk_tree_view_column_set_clickable (tree_column, TRUE);*/

  if (priv->reorderable == (reorderable?TRUE:FALSE))
    return;

  priv->reorderable = (reorderable?TRUE:FALSE);
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "reorderable");
}

/**
 * gtk_tree_view_column_get_reorderable:
 * @tree_column: A #GtkTreeViewColumn
 * 
 * Returns %TRUE if the @tree_column can be reordered by the user.
 * 
 * Returns: %TRUE if the @tree_column can be reordered by the user.
 **/
gboolean
gtk_tree_view_column_get_reorderable (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->priv->reorderable;
}


/**
 * gtk_tree_view_column_set_sort_column_id:
 * @tree_column: a #GtkTreeViewColumn
 * @sort_column_id: The @sort_column_id of the model to sort on.
 *
 * Sets the logical @sort_column_id that this column sorts on when this column 
 * is selected for sorting.  Doing so makes the column header clickable.
 **/
void
gtk_tree_view_column_set_sort_column_id (GtkTreeViewColumn *tree_column,
					 gint               sort_column_id)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (sort_column_id >= -1);

  priv = tree_column->priv;

  if (priv->sort_column_id == sort_column_id)
    return;

  priv->sort_column_id = sort_column_id;

  /* Handle unsetting the id */
  if (sort_column_id == -1)
    {
      GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));

      if (priv->sort_clicked_signal)
	{
	  g_signal_handler_disconnect (tree_column, priv->sort_clicked_signal);
	  priv->sort_clicked_signal = 0;
	}

      if (priv->sort_column_changed_signal)
	{
	  g_signal_handler_disconnect (model, priv->sort_column_changed_signal);
	  priv->sort_column_changed_signal = 0;
	}

      gtk_tree_view_column_set_sort_order (tree_column, GTK_SORT_ASCENDING);
      gtk_tree_view_column_set_sort_indicator (tree_column, FALSE);
      gtk_tree_view_column_set_clickable (tree_column, FALSE);
      g_object_notify (G_OBJECT (tree_column), "sort-column-id");
      return;
    }

  gtk_tree_view_column_set_clickable (tree_column, TRUE);

  if (! priv->sort_clicked_signal)
    priv->sort_clicked_signal = g_signal_connect (tree_column,
						  "clicked",
						  G_CALLBACK (gtk_tree_view_column_sort),
						  NULL);

  gtk_tree_view_column_setup_sort_column_id_callback (tree_column);
  g_object_notify (G_OBJECT (tree_column), "sort-column-id");
}

/**
 * gtk_tree_view_column_get_sort_column_id:
 * @tree_column: a #GtkTreeViewColumn
 *
 * Gets the logical @sort_column_id that the model sorts on when this
 * column is selected for sorting.
 * See gtk_tree_view_column_set_sort_column_id().
 *
 * Returns: the current @sort_column_id for this column, or -1 if
 *               this column can’t be used for sorting.
 **/
gint
gtk_tree_view_column_get_sort_column_id (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->priv->sort_column_id;
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

  if (setting == tree_column->priv->show_sort_indicator)
    return;

  tree_column->priv->show_sort_indicator = setting;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "sort-indicator");
}

/**
 * gtk_tree_view_column_get_sort_indicator:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set by gtk_tree_view_column_set_sort_indicator().
 * 
 * Returns: whether the sort indicator arrow is displayed
 **/
gboolean
gtk_tree_view_column_get_sort_indicator  (GtkTreeViewColumn     *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->priv->show_sort_indicator;
}

/**
 * gtk_tree_view_column_set_sort_order:
 * @tree_column: a #GtkTreeViewColumn
 * @order: sort order that the sort indicator should indicate
 *
 * Changes the appearance of the sort indicator. 
 * 
 * This does not actually sort the model.  Use
 * gtk_tree_view_column_set_sort_column_id() if you want automatic sorting
 * support.  This function is primarily for custom sorting behavior, and should
 * be used in conjunction with gtk_tree_sortable_set_sort_column_id() to do
 * that. For custom models, the mechanism will vary. 
 * 
 * The sort indicator changes direction to indicate normal sort or reverse sort.
 * Note that you must have the sort indicator enabled to see anything when 
 * calling this function; see gtk_tree_view_column_set_sort_indicator().
 **/
void
gtk_tree_view_column_set_sort_order      (GtkTreeViewColumn     *tree_column,
                                          GtkSortType            order)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (order == tree_column->priv->sort_order)
    return;

  tree_column->priv->sort_order = order;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "sort-order");
}

/**
 * gtk_tree_view_column_get_sort_order:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set by gtk_tree_view_column_set_sort_order().
 * 
 * Returns: the sort order the sort indicator is indicating
 **/
GtkSortType
gtk_tree_view_column_get_sort_order      (GtkTreeViewColumn     *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->priv->sort_order;
}

/**
 * gtk_tree_view_column_cell_set_cell_data:
 * @tree_column: A #GtkTreeViewColumn.
 * @tree_model: The #GtkTreeModel to to get the cell renderers attributes from.
 * @iter: The #GtkTreeIter to to get the cell renderer’s attributes from.
 * @is_expander: %TRUE, if the row has children
 * @is_expanded: %TRUE, if the row has visible children
 * 
 * Sets the cell renderer based on the @tree_model and @iter.  That is, for
 * every attribute mapping in @tree_column, it will get a value from the set
 * column on the @iter, and use that value to set the attribute on the cell
 * renderer.  This is used primarily by the #GtkTreeView.
 **/
void
gtk_tree_view_column_cell_set_cell_data (GtkTreeViewColumn *tree_column,
					 GtkTreeModel      *tree_model,
					 GtkTreeIter       *iter,
					 gboolean           is_expander,
					 gboolean           is_expanded)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_model == NULL)
    return;

  gtk_cell_area_apply_attributes (tree_column->priv->cell_area, tree_model, iter,
                                  is_expander, is_expanded);
}

/**
 * gtk_tree_view_column_cell_get_size:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell_area: (allow-none): The area a cell in the column will be allocated, or %NULL
 * @x_offset: (out) (optional): location to return x offset of a cell relative to @cell_area, or %NULL
 * @y_offset: (out) (optional): location to return y offset of a cell relative to @cell_area, or %NULL
 * @width: (out) (optional): location to return width needed to render a cell, or %NULL
 * @height: (out) (optional): location to return height needed to render a cell, or %NULL
 * 
 * Obtains the width and height needed to render the column.  This is used
 * primarily by the #GtkTreeView.
 **/
void
gtk_tree_view_column_cell_get_size (GtkTreeViewColumn  *tree_column,
				    const GdkRectangle *cell_area,
				    gint               *x_offset,
				    gint               *y_offset,
				    gint               *width,
				    gint               *height)
{
  GtkTreeViewColumnPrivate *priv;
  gint min_width = 0, min_height = 0;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  priv = tree_column->priv;

  g_signal_handler_block (priv->cell_area_context, 
			  priv->context_changed_signal);

  gtk_cell_area_get_preferred_width (priv->cell_area,
                                     priv->cell_area_context,
                                     priv->tree_view,
                                     NULL, NULL);

  gtk_cell_area_context_get_preferred_width (priv->cell_area_context, &min_width, NULL);

  gtk_cell_area_get_preferred_height_for_width (priv->cell_area,
                                                priv->cell_area_context,
                                                priv->tree_view,
                                                min_width,
                                                &min_height,
                                                NULL);

  g_signal_handler_unblock (priv->cell_area_context, 
			    priv->context_changed_signal);


  if (height)
    * height = min_height;
  if (width)
    * width = min_width;

}

/**
 * gtk_tree_view_column_cell_render:
 * @tree_column: A #GtkTreeViewColumn.
 * @cr: cairo context to draw to
 * @background_area: entire cell area (including tree expanders and maybe padding on the sides)
 * @cell_area: area normally rendered by a cell renderer
 * @flags: flags that affect rendering
 * 
 * Renders the cell contained by #tree_column. This is used primarily by the
 * #GtkTreeView.
 **/
void
_gtk_tree_view_column_cell_render (GtkTreeViewColumn  *tree_column,
				   cairo_t            *cr,
				   const GdkRectangle *background_area,
				   const GdkRectangle *cell_area,
				   guint               flags,
                                   gboolean            draw_focus)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (background_area != NULL);
  g_return_if_fail (cell_area != NULL);

  priv = tree_column->priv;

  cairo_save (cr);

  gtk_cell_area_render (priv->cell_area, priv->cell_area_context,
                        priv->tree_view, cr,
                        background_area, cell_area, flags,
                        draw_focus);

  cairo_restore (cr);
}

gboolean
_gtk_tree_view_column_cell_event (GtkTreeViewColumn  *tree_column,
				  GdkEvent           *event,
				  const GdkRectangle *cell_area,
				  guint               flags)
{
  GtkTreeViewColumnPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  priv = tree_column->priv;

  return gtk_cell_area_event (priv->cell_area,
                              priv->cell_area_context,
                              priv->tree_view,
                              event,
                              cell_area,
                              flags);
}

/**
 * gtk_tree_view_column_cell_is_visible:
 * @tree_column: A #GtkTreeViewColumn
 * 
 * Returns %TRUE if any of the cells packed into the @tree_column are visible.
 * For this to be meaningful, you must first initialize the cells with
 * gtk_tree_view_column_cell_set_cell_data()
 * 
 * Returns: %TRUE, if any of the cells packed into the @tree_column are currently visible
 **/
gboolean
gtk_tree_view_column_cell_is_visible (GtkTreeViewColumn *tree_column)
{
  GList *list;
  GList *cells;
  GtkTreeViewColumnPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  priv = tree_column->priv;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->cell_area));
  for (list = cells; list; list = list->next)
    {
      if (gtk_cell_renderer_get_visible (list->data))
        {
          g_list_free (cells);
          return TRUE;
        }
    }

  g_list_free (cells);

  return FALSE;
}

/**
 * gtk_tree_view_column_focus_cell:
 * @tree_column: A #GtkTreeViewColumn
 * @cell: A #GtkCellRenderer
 *
 * Sets the current keyboard focus to be at @cell, if the column contains
 * 2 or more editable and activatable cells.
 *
 * Since: 2.2
 **/
void
gtk_tree_view_column_focus_cell (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  gtk_cell_area_set_focus_cell (tree_column->priv->cell_area, cell);
}

void
_gtk_tree_view_column_cell_set_dirty (GtkTreeViewColumn *tree_column,
				      gboolean           install_handler)
{
  GtkTreeViewColumnPrivate *priv = tree_column->priv;

  priv->dirty = TRUE;
  priv->padding = 0;
  priv->width = 0;

  /* Issue a manual reset on the context to have all
   * sizes re-requested for the context.
   */
  g_signal_handler_block (priv->cell_area_context, 
			  priv->context_changed_signal);
  gtk_cell_area_context_reset (priv->cell_area_context);
  g_signal_handler_unblock (priv->cell_area_context, 
			    priv->context_changed_signal);

  if (priv->tree_view &&
      gtk_widget_get_realized (priv->tree_view))
    {
      _gtk_tree_view_install_mark_rows_col_dirty (GTK_TREE_VIEW (priv->tree_view), install_handler);
      gtk_widget_queue_resize (priv->tree_view);
    }
}

gboolean
_gtk_tree_view_column_cell_get_dirty (GtkTreeViewColumn  *tree_column)
{
  return tree_column->priv->dirty;
}

/**
 * gtk_tree_view_column_cell_get_position:
 * @tree_column: a #GtkTreeViewColumn
 * @cell_renderer: a #GtkCellRenderer
 * @x_offset: (out) (allow-none): return location for the horizontal
 *            position of @cell within @tree_column, may be %NULL
 * @width: (out) (allow-none): return location for the width of @cell,
 *         may be %NULL
 *
 * Obtains the horizontal position and size of a cell in a column. If the
 * cell is not found in the column, @start_pos and @width are not changed and
 * %FALSE is returned.
 * 
 * Returns: %TRUE if @cell belongs to @tree_column.
 */
gboolean
gtk_tree_view_column_cell_get_position (GtkTreeViewColumn *tree_column,
					GtkCellRenderer   *cell_renderer,
					gint              *x_offset,
					gint              *width)
{
  GtkTreeViewColumnPrivate *priv;
  GdkRectangle cell_area;
  GdkRectangle allocation;

  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell_renderer), FALSE);

  priv = tree_column->priv;

  if (! gtk_cell_area_has_renderer (priv->cell_area, cell_renderer))
    return FALSE;

  gtk_tree_view_get_background_area (GTK_TREE_VIEW (priv->tree_view),
                                     NULL, tree_column, &cell_area);

  gtk_cell_area_get_cell_allocation (priv->cell_area,
                                     priv->cell_area_context,
                                     priv->tree_view,
                                     cell_renderer,
                                     &cell_area,
                                     &allocation);

  if (x_offset)
    *x_offset = allocation.x - cell_area.x;

  if (width)
    *width = allocation.width;

  return TRUE;
}

/**
 * gtk_tree_view_column_queue_resize:
 * @tree_column: A #GtkTreeViewColumn
 *
 * Flags the column, and the cell renderers added to this column, to have
 * their sizes renegotiated.
 *
 * Since: 2.8
 **/
void
gtk_tree_view_column_queue_resize (GtkTreeViewColumn *tree_column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_column->priv->tree_view)
    _gtk_tree_view_column_cell_set_dirty (tree_column, TRUE);
}

/**
 * gtk_tree_view_column_get_tree_view:
 * @tree_column: A #GtkTreeViewColumn
 *
 * Returns the #GtkTreeView wherein @tree_column has been inserted.
 * If @column is currently not inserted in any tree view, %NULL is
 * returned.
 *
 * Returns: (transfer none): The tree view wherein @column has
 *     been inserted if any, %NULL otherwise.
 *
 * Since: 2.12
 */
GtkWidget *
gtk_tree_view_column_get_tree_view (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->priv->tree_view;
}

/**
 * gtk_tree_view_column_get_button:
 * @tree_column: A #GtkTreeViewColumn
 *
 * Returns the button used in the treeview column header
 *
 * Returns: (transfer none): The button for the column header.
 *
 * Since: 3.0
 */
GtkWidget *
gtk_tree_view_column_get_button (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->priv->button;
}

GdkWindow *
_gtk_tree_view_column_get_window (GtkTreeViewColumn  *column)
{
  return column->priv->window;
}

void
_gtk_tree_view_column_push_padding (GtkTreeViewColumn  *column,
				    gint                padding)
{
  column->priv->padding = MAX (column->priv->padding, padding);
}

gint
_gtk_tree_view_column_get_requested_width (GtkTreeViewColumn  *column)
{
  gint requested_width;

  gtk_cell_area_context_get_preferred_width (column->priv->cell_area_context, &requested_width, NULL);

  return requested_width + column->priv->padding;
}

gint
_gtk_tree_view_column_get_drag_x (GtkTreeViewColumn  *column)
{
  return column->priv->drag_x;
}

GtkCellAreaContext *
_gtk_tree_view_column_get_context (GtkTreeViewColumn  *column)
{
  return column->priv->cell_area_context;
}
