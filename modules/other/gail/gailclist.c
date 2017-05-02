/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <stdio.h>

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#include "gailclist.h"
#include "gailclistcell.h"
#include "gailcellparent.h"

/* Copied from gtkclist.c */
/* this defigns the base grid spacing */
#define CELL_SPACING 1

/* added the horizontal space at the beginning and end of a row*/
#define COLUMN_INSET 3


/* gives the top pixel of the given row in context of
 * the clist's voffset */
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
                                    (((row) + 1) * CELL_SPACING) + \
                                    (clist)->voffset)

/* returns the row index from a y pixel location in the
 * context of the clist's voffset */
#define ROW_FROM_YPIXEL(clist, y)  (((y) - (clist)->voffset) / \
                                    ((clist)->row_height + CELL_SPACING))
/* gives the left pixel of the given column in context of
 * the clist's hoffset */
#define COLUMN_LEFT_XPIXEL(clist, colnum)  ((clist)->column[(colnum)].area.x + \
                                            (clist)->hoffset)

/* returns the column index from a x pixel location in the
 * context of the clist's hoffset */
static inline gint
COLUMN_FROM_XPIXEL (GtkCList * clist,
                    gint x)
{
  gint i, cx;

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].visible)
      {
        cx = clist->column[i].area.x + clist->hoffset;

        if (x >= (cx - (COLUMN_INSET + CELL_SPACING)) &&
            x <= (cx + clist->column[i].area.width + COLUMN_INSET))
          return i;
      }

  /* no match */
  return -1;
}

/* returns the top pixel of the given row in the context of
 * the list height */
#define ROW_TOP(clist, row)        (((clist)->row_height + CELL_SPACING) * (row))

/* returns the left pixel of the given column in the context of
 * the list width */
#define COLUMN_LEFT(clist, colnum) ((clist)->column[(colnum)].area.x)

/* returns the total height of the list */
#define LIST_HEIGHT(clist)         (((clist)->row_height * ((clist)->rows)) + \
                                    (CELL_SPACING * ((clist)->rows + 1)))

static inline gint
LIST_WIDTH (GtkCList * clist)
{
  gint last_column;

  for (last_column = clist->columns - 1;
       last_column >= 0 && !clist->column[last_column].visible; last_column--);

  if (last_column >= 0)
    return (clist->column[last_column].area.x +
            clist->column[last_column].area.width +
            COLUMN_INSET + CELL_SPACING);
  return 0;
}

/* returns the GList item for the nth row */
#define ROW_ELEMENT(clist, row) (((row) == (clist)->rows - 1) ? \
                                 (clist)->row_list_end : \
                                 g_list_nth ((clist)->row_list, (row)))

typedef struct _GailCListRow        GailCListRow;
typedef struct _GailCListCellData   GailCListCellData;


static void       gail_clist_class_init            (GailCListClass    *klass);
static void       gail_clist_init                  (GailCList         *clist);
static void       gail_clist_real_initialize       (AtkObject         *obj,
                                                    gpointer          data);
static void       gail_clist_finalize              (GObject           *object);

static gint       gail_clist_get_n_children        (AtkObject         *obj);
static AtkObject* gail_clist_ref_child             (AtkObject         *obj,
                                                    gint              i);
static AtkStateSet* gail_clist_ref_state_set       (AtkObject         *obj);


static void       atk_selection_interface_init     (AtkSelectionIface *iface);
static gboolean   gail_clist_clear_selection       (AtkSelection   *selection);

static AtkObject* gail_clist_ref_selection         (AtkSelection   *selection,
                                                    gint           i);
static gint       gail_clist_get_selection_count   (AtkSelection   *selection);
static gboolean   gail_clist_is_child_selected     (AtkSelection   *selection,
                                                    gint           i);
static gboolean   gail_clist_select_all_selection  (AtkSelection   *selection);

static void       atk_table_interface_init         (AtkTableIface     *iface);
static gint       gail_clist_get_index_at          (AtkTable      *table,
                                                    gint          row,
                                                    gint          column);
static gint       gail_clist_get_column_at_index   (AtkTable      *table,
                                                    gint          index);
static gint       gail_clist_get_row_at_index      (AtkTable      *table,
                                                    gint          index);
static AtkObject* gail_clist_ref_at                (AtkTable      *table,
                                                    gint          row,
                                                    gint          column);
static AtkObject* gail_clist_ref_at_actual         (AtkTable      *table,
                                                    gint          row,
                                                    gint          column);
static AtkObject*
                  gail_clist_get_caption           (AtkTable      *table);

static gint       gail_clist_get_n_columns         (AtkTable      *table);
static gint       gail_clist_get_n_actual_columns  (GtkCList      *clist);

static const gchar*
                  gail_clist_get_column_description(AtkTable      *table,
                                                    gint          column);
static AtkObject*  gail_clist_get_column_header     (AtkTable      *table,
                                                    gint          column);
static gint       gail_clist_get_n_rows            (AtkTable      *table);
static const gchar*
                  gail_clist_get_row_description   (AtkTable      *table,
                                                    gint          row);
static AtkObject*  gail_clist_get_row_header        (AtkTable      *table,
                                                    gint          row);
static AtkObject* gail_clist_get_summary           (AtkTable      *table);
static gboolean   gail_clist_add_row_selection     (AtkTable      *table,
                                                    gint          row);
static gboolean   gail_clist_remove_row_selection  (AtkTable      *table,
                                                    gint          row);
static gint       gail_clist_get_selected_rows     (AtkTable      *table,
                                                    gint          **rows_selected);
static gboolean   gail_clist_is_row_selected       (AtkTable      *table,
                                                    gint          row);
static gboolean   gail_clist_is_selected           (AtkTable      *table,
                                                    gint          row,
                                                    gint          column);
static void       gail_clist_set_caption           (AtkTable      *table,
                                                    AtkObject     *caption);
static void       gail_clist_set_column_description(AtkTable      *table,
                                                    gint          column,
                                                    const gchar   *description);
static void       gail_clist_set_column_header     (AtkTable      *table,
                                                    gint          column,
                                                    AtkObject     *header);
static void       gail_clist_set_row_description   (AtkTable      *table,
                                                    gint          row,
                                                    const gchar   *description);
static void       gail_clist_set_row_header        (AtkTable      *table,
                                                    gint          row,
                                                    AtkObject     *header);
static void       gail_clist_set_summary           (AtkTable      *table,
                                                    AtkObject     *accessible);

/* gailcellparent.h */

static void       gail_cell_parent_interface_init  (GailCellParentIface *iface);
static void       gail_clist_get_cell_extents      (GailCellParent      *parent,
                                                    GailCell            *cell,
                                                    gint                *x,
                                                    gint                *y,
                                                    gint                *width,
                                                    gint                *height,
                                                    AtkCoordType        coord_type);

static void       gail_clist_get_cell_area         (GailCellParent      *parent,
                                                    GailCell            *cell,
                                                    GdkRectangle        *cell_rect);

static void       gail_clist_select_row_gtk        (GtkCList      *clist,
                                                    int           row,
                                                    int           column,
                                                    GdkEvent      *event,
                                                    gpointer      data);
static void       gail_clist_unselect_row_gtk      (GtkCList      *clist,
                                                    int           row,
                                                    int           column,
                                                    GdkEvent      *event,
                                                    gpointer      data);
static gint       gail_clist_get_visible_column    (AtkTable      *table,
                                                    int           column);
static gint       gail_clist_get_actual_column     (AtkTable      *table,
                                                    int           visible_column);
static void       gail_clist_set_row_data          (AtkTable      *table,
                                                    gint          row,
                                                    const gchar   *description,
                                                    AtkObject     *header,
                                                    gboolean      is_header);
static GailCListRow*
                  gail_clist_get_row_data          (AtkTable      *table,
                                                    gint          row);
static void       gail_clist_get_visible_rect      (GtkCList      *clist,
                                                    GdkRectangle  *clist_rect);
static gboolean   gail_clist_is_cell_visible       (GdkRectangle  *cell_rect,
                                                    GdkRectangle  *visible_rect);
static void       gail_clist_cell_data_new         (GailCList     *clist,
                                                    GailCell      *cell,
                                                    gint          column,
                                                    gint          row);
static void       gail_clist_cell_destroyed        (gpointer      data);
static void       gail_clist_cell_data_remove      (GailCList     *clist,
                                                    GailCell      *cell);
static GailCell*  gail_clist_find_cell             (GailCList     *clist,
                                                    gint          index);
static void       gail_clist_adjustment_changed    (GtkAdjustment *adjustment,
                                                    GtkCList      *clist);

struct _GailCListColumn
{
  gchar *description;
  AtkObject *header;
};

struct _GailCListRow
{
  GtkCListRow *row_data;
  int row_number;
  gchar *description;
  AtkObject *header;
};

struct _GailCListCellData
{
  GtkCell *gtk_cell;
  GailCell *gail_cell;
  int row_number;
  int column_number;
};

G_DEFINE_TYPE_WITH_CODE (GailCList, gail_clist, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TABLE, atk_table_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init)
                         G_IMPLEMENT_INTERFACE (GAIL_TYPE_CELL_PARENT, gail_cell_parent_interface_init))

static void
gail_clist_class_init (GailCListClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  class->get_n_children = gail_clist_get_n_children;
  class->ref_child = gail_clist_ref_child;
  class->ref_state_set = gail_clist_ref_state_set;
  class->initialize = gail_clist_real_initialize;

  gobject_class->finalize = gail_clist_finalize;
}

static void
gail_clist_init (GailCList *clist)
{
}

static void
gail_clist_real_initialize (AtkObject *obj,
                            gpointer  data)
{
  GailCList *clist;
  GtkCList *gtk_clist;
  gint i;

  ATK_OBJECT_CLASS (gail_clist_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_TABLE;

  clist = GAIL_CLIST (obj);

  clist->caption = NULL;
  clist->summary = NULL;
  clist->row_data = NULL;
  clist->cell_data = NULL;
  clist->previous_selected_cell = NULL;

  gtk_clist = GTK_CLIST (data);
 
  clist->n_cols = gtk_clist->columns;
  clist->columns = g_new (GailCListColumn, gtk_clist->columns);
  for (i = 0; i < gtk_clist->columns; i++)
    {
      clist->columns[i].description = NULL;
      clist->columns[i].header = NULL;
    }
  /*
   * Set up signal handlers for select-row and unselect-row
   */
  g_signal_connect (gtk_clist,
                    "select-row",
                    G_CALLBACK (gail_clist_select_row_gtk),
                    obj);
  g_signal_connect (gtk_clist,
                    "unselect-row",
                    G_CALLBACK (gail_clist_unselect_row_gtk),
                    obj);
  /*
   * Adjustment callbacks
   */
  if (gtk_clist->hadjustment)
    {
      g_signal_connect (gtk_clist->hadjustment,
                        "value_changed",
                        G_CALLBACK (gail_clist_adjustment_changed),
                        gtk_clist);
    }
  if (gtk_clist->vadjustment)
    {
      g_signal_connect (gtk_clist->vadjustment,
                        "value_changed",
                        G_CALLBACK (gail_clist_adjustment_changed),
                        gtk_clist);
    }
}

static void
gail_clist_finalize (GObject            *object)
{
  GailCList *clist = GAIL_CLIST (object);
  gint i;
  GArray *array;

  if (clist->caption)
    g_object_unref (clist->caption);
  if (clist->summary)
    g_object_unref (clist->summary);

  for (i = 0; i < clist->n_cols; i++)
    {
      g_free (clist->columns[i].description);
      if (clist->columns[i].header)
        g_object_unref (clist->columns[i].header);
    }
  g_free (clist->columns);

  array = clist->row_data;

  if (clist->previous_selected_cell)
    g_object_unref (clist->previous_selected_cell);

  if (array)
    {
      for (i = 0; i < array->len; i++)
        {
          GailCListRow *row_data;

          row_data = g_array_index (array, GailCListRow*, i);

          if (row_data->header)
            g_object_unref (row_data->header);
          g_free (row_data->description);
        }
    }

  if (clist->cell_data)
    {
      GList *temp_list;

      for (temp_list = clist->cell_data; temp_list; temp_list = temp_list->next)
        {
          g_list_free (temp_list->data);
        }
      g_list_free (clist->cell_data);
    }

  G_OBJECT_CLASS (gail_clist_parent_class)->finalize (object);
}

static gint
gail_clist_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  gint row, col;

  g_return_val_if_fail (GAIL_IS_CLIST (obj), 0);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return 0;

  row = gail_clist_get_n_rows (ATK_TABLE (obj));
  col = gail_clist_get_n_actual_columns (GTK_CLIST (widget));
  return (row * col);
}

static AtkObject*
gail_clist_ref_child (AtkObject *obj,
                      gint      i)
{
  GtkWidget *widget;
  gint row, col;
  gint n_columns;

  g_return_val_if_fail (GAIL_IS_CLIST (obj), NULL);
  g_return_val_if_fail (i >= 0, NULL);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  n_columns = gail_clist_get_n_actual_columns (GTK_CLIST (widget));
  if (!n_columns)
    return NULL;

  row = i / n_columns;
  col = i % n_columns;
  return gail_clist_ref_at_actual (ATK_TABLE (obj), row, col);
}

static AtkStateSet*
gail_clist_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gail_clist_parent_class)->ref_state_set (obj);
  widget = GTK_ACCESSIBLE (obj)->widget;

  if (widget != NULL)
    atk_state_set_add_state (state_set, ATK_STATE_MANAGES_DESCENDANTS);

  return state_set;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->clear_selection = gail_clist_clear_selection;
  iface->ref_selection = gail_clist_ref_selection;
  iface->get_selection_count = gail_clist_get_selection_count;
  iface->is_child_selected = gail_clist_is_child_selected;
  iface->select_all_selection = gail_clist_select_all_selection;
}

static gboolean
gail_clist_clear_selection (AtkSelection   *selection)
{
  GtkCList *clist;
  GtkWidget *widget;
  
  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;
  
  clist = GTK_CLIST (widget);
  gtk_clist_unselect_all(clist);
  return TRUE;
}

static AtkObject*
gail_clist_ref_selection (AtkSelection   *selection,
                          gint           i)
{
  gint visible_columns;
  gint selected_row;
  gint selected_column;
  gint *selected_rows;

  if ( i < 0 && i >= gail_clist_get_selection_count (selection))
    return NULL;

  visible_columns = gail_clist_get_n_columns (ATK_TABLE (selection));
  gail_clist_get_selected_rows (ATK_TABLE (selection), &selected_rows);
  selected_row = selected_rows[i / visible_columns];
  g_free (selected_rows);
  selected_column = gail_clist_get_actual_column (ATK_TABLE (selection), 
                                                  i % visible_columns);

  return gail_clist_ref_at (ATK_TABLE (selection), selected_row, 
                            selected_column);
}

static gint
gail_clist_get_selection_count (AtkSelection   *selection)
{
  gint n_rows_selected;

  n_rows_selected = gail_clist_get_selected_rows (ATK_TABLE (selection), NULL);

  if (n_rows_selected > 0)
    /*
     * The number of cells selected is the number of columns
     * times the number of selected rows
     */
    return gail_clist_get_n_columns (ATK_TABLE (selection)) * n_rows_selected;
  return 0;
}

static gboolean
gail_clist_is_child_selected (AtkSelection   *selection,
                              gint           i)
{
  gint row;

  row = atk_table_get_row_at_index (ATK_TABLE (selection), i);

  if (row == 0 && i >= gail_clist_get_n_columns (ATK_TABLE (selection)))
    return FALSE;
  return gail_clist_is_row_selected (ATK_TABLE (selection), row);
}

static gboolean
gail_clist_select_all_selection (AtkSelection   *selection)
{
  GtkCList *clist;
  GtkWidget *widget;
  /* GtkArg arg; */
  
  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  clist = GTK_CLIST (widget);
  gtk_clist_select_all(clist);

  return TRUE;
}

static void
atk_table_interface_init (AtkTableIface *iface)
{
  iface->ref_at = gail_clist_ref_at;
  iface->get_index_at = gail_clist_get_index_at;
  iface->get_column_at_index = gail_clist_get_column_at_index;
  iface->get_row_at_index = gail_clist_get_row_at_index;
  iface->get_caption = gail_clist_get_caption;
  iface->get_n_columns = gail_clist_get_n_columns;
  iface->get_column_description = gail_clist_get_column_description;
  iface->get_column_header = gail_clist_get_column_header;
  iface->get_n_rows = gail_clist_get_n_rows;
  iface->get_row_description = gail_clist_get_row_description;
  iface->get_row_header = gail_clist_get_row_header;
  iface->get_summary = gail_clist_get_summary;
  iface->add_row_selection = gail_clist_add_row_selection;
  iface->remove_row_selection = gail_clist_remove_row_selection;
  iface->get_selected_rows = gail_clist_get_selected_rows;
  iface->is_row_selected = gail_clist_is_row_selected;
  iface->is_selected = gail_clist_is_selected;
  iface->set_caption = gail_clist_set_caption;
  iface->set_column_description = gail_clist_set_column_description;
  iface->set_column_header = gail_clist_set_column_header;
  iface->set_row_description = gail_clist_set_row_description;
  iface->set_row_header = gail_clist_set_row_header;
  iface->set_summary = gail_clist_set_summary;
}

static AtkObject*
gail_clist_ref_at (AtkTable *table,
                   gint     row,
                   gint     column)
{
  GtkWidget *widget;
  gint actual_column;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  actual_column = gail_clist_get_actual_column (table, column);
  return gail_clist_ref_at_actual (table, row, actual_column);
}


static AtkObject*
gail_clist_ref_at_actual (AtkTable      *table,
                          gint          row,
                          gint          column)
{
  /*
   * The column number pased to this function is the actual column number
   * whereas the column number passed to gail_clist_ref_at is the
   * visible column number
   */
  GtkCList *clist;
  GtkWidget *widget;
  GtkCellType cellType;
  AtkObject *return_object;
  gint n_rows, n_columns;
  gint index;
  GailCell *cell;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE (table), NULL);
  
  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  clist = GTK_CLIST (widget);
  n_rows = gail_clist_get_n_rows (table); 
  n_columns = gail_clist_get_n_actual_columns (clist); 

  if (row < 0 || row >= n_rows)
    return NULL;
  if (column < 0 || column >= n_columns)
    return NULL;

  /*
   * Check whether the child is cached
   */
  index =  column + row * n_columns;
  cell = gail_clist_find_cell (GAIL_CLIST (table), index);
  if (cell)
    {
      g_object_ref (cell);
      return ATK_OBJECT (cell);
    }
  cellType = gtk_clist_get_cell_type(clist, row, column);
  switch (cellType) 
    {
    case GTK_CELL_TEXT:
    case GTK_CELL_PIXTEXT:
      return_object = gail_clist_cell_new ();
      break;
    case GTK_CELL_PIXMAP:
      return_object = NULL;
      break;
    default:
      /* Don't handle GTK_CELL_EMPTY or GTK_CELL_WIDGET, return NULL */
      return_object = NULL;
      break;
    }
  if (return_object)
    {
      cell = GAIL_CELL (return_object);

      g_return_val_if_fail (ATK_IS_OBJECT (table), NULL);

      gail_cell_initialise (cell, widget, ATK_OBJECT (table),
                            index);
      /*
       * Store the cell in a cache
       */
      gail_clist_cell_data_new (GAIL_CLIST (table), cell, column, row);
      /*
       * If the column is visible, sets the cell's state
       */
      if (clist->column[column].visible)
        {
          GdkRectangle cell_rect, visible_rect;
  
          gail_clist_get_cell_area (GAIL_CELL_PARENT (table), cell, &cell_rect);
          gail_clist_get_visible_rect (clist, &visible_rect);
          gail_cell_add_state (cell, ATK_STATE_VISIBLE, FALSE);
          if (gail_clist_is_cell_visible (&cell_rect, &visible_rect))
            gail_cell_add_state (cell, ATK_STATE_SHOWING, FALSE);
        }
      /*
       * If a row is selected, all cells in the row are selected
       */
      if (gail_clist_is_row_selected (table, row))
        {
          gail_cell_add_state (cell, ATK_STATE_SELECTED, FALSE);
          if (clist->columns == 1)
            gail_cell_add_state (cell, ATK_STATE_FOCUSED, FALSE);
        }
    }

  return return_object; 
}

static gint
gail_clist_get_index_at (AtkTable *table,
                         gint     row,
                         gint     column)
{
  gint n_cols, n_rows;

  n_cols = atk_table_get_n_columns (table);
  n_rows = atk_table_get_n_rows (table);

  g_return_val_if_fail (row < n_rows, 0);
  g_return_val_if_fail (column < n_cols, 0);

  return row * n_cols + column;
}

static gint
gail_clist_get_column_at_index (AtkTable *table,
                                gint     index)
{
  gint n_cols;

  n_cols = atk_table_get_n_columns (table);

  if (n_cols == 0)
    return 0;
  else
    return (gint) (index % n_cols);
}

static gint
gail_clist_get_row_at_index (AtkTable *table,
                             gint     index)
{
  gint n_cols;

  n_cols = atk_table_get_n_columns (table);

  if (n_cols == 0)
    return 0;
  else
    return (gint) (index / n_cols);
}

static AtkObject*
gail_clist_get_caption (AtkTable      *table)
{
  GailCList* obj = GAIL_CLIST (table);

  return obj->caption;
}

static gint
gail_clist_get_n_columns (AtkTable      *table)
{
  GtkWidget *widget;
  GtkCList *clist;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  clist = GTK_CLIST (widget);

  return gail_clist_get_visible_column (table, 
                                  gail_clist_get_n_actual_columns (clist)); 
}

static gint
gail_clist_get_n_actual_columns (GtkCList *clist)
{
  return clist->columns;
}

static const gchar*
gail_clist_get_column_description (AtkTable      *table,
                                   gint          column)
{
  GailCList *clist = GAIL_CLIST (table);
  GtkWidget *widget;
  gint actual_column;

  if (column < 0 || column >= gail_clist_get_n_columns (table))
    return NULL;

  actual_column = gail_clist_get_actual_column (table, column);
  if (clist->columns[actual_column].description)
    return (clist->columns[actual_column].description);

  widget = GTK_ACCESSIBLE (clist)->widget;
  if (widget == NULL)
    return NULL;

  return gtk_clist_get_column_title (GTK_CLIST (widget), actual_column);
}

static AtkObject*
gail_clist_get_column_header (AtkTable      *table,
                              gint          column)
{
  GailCList *clist = GAIL_CLIST (table);
  GtkWidget *widget;
  GtkWidget *return_widget;
  gint actual_column;

  if (column < 0 || column >= gail_clist_get_n_columns (table))
    return NULL;

  actual_column = gail_clist_get_actual_column (table, column);

  if (clist->columns[actual_column].header)
    return (clist->columns[actual_column].header);

  widget = GTK_ACCESSIBLE (clist)->widget;
  if (widget == NULL)
    return NULL;

  return_widget = gtk_clist_get_column_widget (GTK_CLIST (widget), 
                                               actual_column);
  if (return_widget == NULL)
    return NULL;

  g_return_val_if_fail (GTK_IS_BIN (return_widget), NULL);
  return_widget = gtk_bin_get_child (GTK_BIN(return_widget));

  return gtk_widget_get_accessible (return_widget);
}

static gint
gail_clist_get_n_rows (AtkTable      *table)
{
  GtkWidget *widget;
  GtkCList *clist;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  clist = GTK_CLIST (widget);
  return clist->rows;
}

static const gchar*
gail_clist_get_row_description (AtkTable      *table,
                                gint          row)
{
  GailCListRow* row_data;

  row_data = gail_clist_get_row_data (table, row);
  if (row_data == NULL)
    return NULL;
  return row_data->description;
}

static AtkObject*
gail_clist_get_row_header (AtkTable      *table,
                           gint          row)
{
  GailCListRow* row_data;

  row_data = gail_clist_get_row_data (table, row);
  if (row_data == NULL)
    return NULL;
  return row_data->header;
}

static AtkObject*
gail_clist_get_summary (AtkTable      *table)
{
  GailCList* obj = GAIL_CLIST (table);

  return obj->summary;
}

static gboolean
gail_clist_add_row_selection (AtkTable      *table,
                              gint          row)
{
  GtkWidget *widget;
  GtkCList *clist;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  clist = GTK_CLIST (widget);
  gtk_clist_select_row (clist, row, -1);
  if (gail_clist_is_row_selected (table, row))
    return TRUE;
  
  return FALSE;
}

static gboolean
gail_clist_remove_row_selection (AtkTable      *table,
                                 gint          row)
{
  GtkWidget *widget;
  GtkCList *clist;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  clist = GTK_CLIST (widget);
  if (gail_clist_is_row_selected (table, row))
  {
    gtk_clist_select_row (clist, row, -1);
    return TRUE;
  }
  return FALSE;
}

static gint
gail_clist_get_selected_rows (AtkTable *table,
                              gint     **rows_selected)
{
  GtkWidget *widget;
  GtkCList *clist;
  GList *list;
  gint n_selected;
  gint i;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  clist = GTK_CLIST (widget);
 
  n_selected = g_list_length (clist->selection);

  if (n_selected == 0) 
    return 0;

  if (rows_selected)
    {
      gint *selected_rows;

      selected_rows = (gint*) g_malloc (sizeof (gint) * n_selected);
      list = clist->selection;

      i = 0;
      while (list)
        {
          selected_rows[i++] = GPOINTER_TO_INT (list->data);
          list = list->next;
        }
      *rows_selected = selected_rows;
    }
  return n_selected;
}

static gboolean
gail_clist_is_row_selected (AtkTable      *table,
                            gint          row)
{
  GList *elem;
  GtkWidget *widget;
  GtkCList *clist;
  GtkCListRow *clist_row;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
	return FALSE;

  clist = GTK_CLIST (widget);
      
  if (row < 0 || row >= clist->rows)
    return FALSE;

  elem = ROW_ELEMENT (clist, row);
  if (!elem)
    return FALSE;
  clist_row = elem->data;

  return (clist_row->state == GTK_STATE_SELECTED);
}

static gboolean
gail_clist_is_selected (AtkTable      *table,
                        gint          row,
                        gint          column)
{
  return gail_clist_is_row_selected (table, row);
}

static void
gail_clist_set_caption (AtkTable      *table,
                        AtkObject     *caption)
{
  GailCList* obj = GAIL_CLIST (table);
  AtkPropertyValues values = { NULL };
  AtkObject *old_caption;

  old_caption = obj->caption;
  obj->caption = caption;
  if (obj->caption)
    g_object_ref (obj->caption);

  g_value_init (&values.old_value, G_TYPE_POINTER);
  g_value_set_pointer (&values.old_value, old_caption);
  g_value_init (&values.new_value, G_TYPE_POINTER);
  g_value_set_pointer (&values.new_value, obj->caption);

  values.property_name = "accessible-table-caption";
  g_signal_emit_by_name (table, 
                         "property_change::accessible-table-caption", 
                         &values, NULL);
  if (old_caption)
    g_object_unref (old_caption);
}

static void
gail_clist_set_column_description (AtkTable      *table,
                                   gint          column,
                                   const gchar   *description)
{
  GailCList *clist = GAIL_CLIST (table);
  AtkPropertyValues values = { NULL };
  gint actual_column;

  if (column < 0 || column >= gail_clist_get_n_columns (table))
    return;

  if (description == NULL)
    return;

  actual_column = gail_clist_get_actual_column (table, column);
  g_free (clist->columns[actual_column].description);
  clist->columns[actual_column].description = g_strdup (description);

  g_value_init (&values.new_value, G_TYPE_INT);
  g_value_set_int (&values.new_value, column);

  values.property_name = "accessible-table-column-description";
  g_signal_emit_by_name (table, 
                         "property_change::accessible-table-column-description",
                          &values, NULL);

}

static void
gail_clist_set_column_header (AtkTable      *table,
                              gint          column,
                              AtkObject     *header)
{
  GailCList *clist = GAIL_CLIST (table);
  AtkPropertyValues values = { NULL };
  gint actual_column;

  if (column < 0 || column >= gail_clist_get_n_columns (table))
    return;

  actual_column = gail_clist_get_actual_column (table, column);
  if (clist->columns[actual_column].header)
    g_object_unref (clist->columns[actual_column].header);
  if (header)
    g_object_ref (header);
  clist->columns[actual_column].header = header;

  g_value_init (&values.new_value, G_TYPE_INT);
  g_value_set_int (&values.new_value, column);

  values.property_name = "accessible-table-column-header";
  g_signal_emit_by_name (table, 
                         "property_change::accessible-table-column-header",
                         &values, NULL);
}

static void
gail_clist_set_row_description (AtkTable      *table,
                                gint          row,
                                const gchar   *description)
{
  gail_clist_set_row_data (table, row, description, NULL, FALSE);
}

static void
gail_clist_set_row_header (AtkTable      *table,
                           gint          row,
                           AtkObject     *header)
{
  gail_clist_set_row_data (table, row, NULL, header, TRUE);
}

static void
gail_clist_set_summary (AtkTable      *table,
                        AtkObject     *accessible)
{
  GailCList* obj = GAIL_CLIST (table);
  AtkPropertyValues values = { 0, };
  AtkObject *old_summary;

  old_summary = obj->summary;
  obj->summary = accessible;
  if (obj->summary)
    g_object_ref (obj->summary);

  g_value_init (&values.old_value, G_TYPE_POINTER);
  g_value_set_pointer (&values.old_value, old_summary);
  g_value_init (&values.new_value, G_TYPE_POINTER);
  g_value_set_pointer (&values.new_value, obj->summary);

  values.property_name = "accessible-table-summary";
  g_signal_emit_by_name (table, 
                         "property_change::accessible-table-summary", 
                         &values, NULL);
  if (old_summary)
    g_object_unref (old_summary);
}


static void gail_cell_parent_interface_init (GailCellParentIface *iface)
{
  iface->get_cell_extents = gail_clist_get_cell_extents;
  iface->get_cell_area = gail_clist_get_cell_area;
}

static void
gail_clist_get_cell_extents (GailCellParent *parent,
                             GailCell       *cell,
                             gint           *x,
                             gint           *y,
                             gint           *width,
                             gint           *height,
                             AtkCoordType   coord_type)
{
  GtkWidget* widget;
  GtkCList *clist;
  gint widget_x, widget_y, widget_width, widget_height;
  GdkRectangle cell_rect;
  GdkRectangle visible_rect;

  widget = GTK_ACCESSIBLE (parent)->widget;
  if (widget == NULL)
    return;
  clist = GTK_CLIST (widget);

  atk_component_get_extents (ATK_COMPONENT (parent), &widget_x, &widget_y,
                             &widget_width, &widget_height,
                             coord_type);

  gail_clist_get_cell_area (parent, cell, &cell_rect);
  *width = cell_rect.width;
  *height = cell_rect.height;
  gail_clist_get_visible_rect (clist, &visible_rect);
  if (gail_clist_is_cell_visible (&cell_rect, &visible_rect))
    {
      *x = cell_rect.x + widget_x;
      *y = cell_rect.y + widget_y;
    }
  else
    {
      *x = G_MININT;
      *y = G_MININT;
    }
}

static void
gail_clist_get_cell_area (GailCellParent *parent,
                          GailCell       *cell,
                          GdkRectangle   *cell_rect)
{
  GtkWidget* widget;
  GtkCList *clist;
  gint column, row, n_columns;

  widget = GTK_ACCESSIBLE (parent)->widget;
  if (widget == NULL)
    return;
  clist = GTK_CLIST (widget);

  n_columns = gail_clist_get_n_actual_columns (clist);
  g_return_if_fail (n_columns > 0);
  column = cell->index % n_columns;
  row = cell->index / n_columns; 
  cell_rect->x = COLUMN_LEFT (clist, column);
  cell_rect->y = ROW_TOP (clist, row);
  cell_rect->width = clist->column[column].area.width;
  cell_rect->height = clist->row_height;
}

static void
gail_clist_select_row_gtk (GtkCList *clist,
                           gint      row,
                           gint      column,
                           GdkEvent *event,
                           gpointer data)
{
  GailCList *gail_clist;
  GList *temp_list;
  AtkObject *selected_cell;

  gail_clist = GAIL_CLIST (data);

  for (temp_list = gail_clist->cell_data; temp_list; temp_list = temp_list->next)
    {
      GailCListCellData *cell_data;

      cell_data = (GailCListCellData *) (temp_list->data);

      if (row == cell_data->row_number)
        {
          /*
           * Row is selected
           */
          gail_cell_add_state (cell_data->gail_cell, ATK_STATE_SELECTED, TRUE);
        }
    }
  if (clist->columns == 1)
    {
      selected_cell = gail_clist_ref_at (ATK_TABLE (data), row, 1);
      if (selected_cell)
        {
          if (gail_clist->previous_selected_cell)
            g_object_unref (gail_clist->previous_selected_cell);
          gail_clist->previous_selected_cell = selected_cell;
          gail_cell_add_state (GAIL_CELL (selected_cell), ATK_STATE_FOCUSED, FALSE);
          g_signal_emit_by_name (gail_clist,
                                 "active-descendant-changed",
                                  selected_cell);
       }
    }

  g_signal_emit_by_name (gail_clist, "selection_changed");
}

static void
gail_clist_unselect_row_gtk (GtkCList *clist,
                             gint      row,
                             gint      column,
                             GdkEvent *event,
                             gpointer data)
{
  GailCList *gail_clist;
  GList *temp_list;

  gail_clist = GAIL_CLIST (data);

  for (temp_list = gail_clist->cell_data; temp_list; temp_list = temp_list->next)
    {
      GailCListCellData *cell_data;

      cell_data = (GailCListCellData *) (temp_list->data);

      if (row == cell_data->row_number)
        {
          /*
           * Row is unselected
           */
          gail_cell_add_state (cell_data->gail_cell, ATK_STATE_FOCUSED, FALSE);
          gail_cell_remove_state (cell_data->gail_cell, ATK_STATE_SELECTED, TRUE);
       }
    }

  g_signal_emit_by_name (gail_clist, "selection_changed");
}

/*
 * This function determines the number of visible columns
 * up to and including the specified column
 */
static gint
gail_clist_get_visible_column (AtkTable *table,
                               int      column)
{
  GtkWidget *widget;
  GtkCList *clist;
  gint i;
  gint vis_columns;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  clist = GTK_CLIST (widget);
  for (i = 0, vis_columns = 0; i < column; i++)
    if (clist->column[i].visible)
      vis_columns++;

  return vis_columns;  
}

static gint
gail_clist_get_actual_column (AtkTable *table,
                              int      visible_column)
{
  GtkWidget *widget;
  GtkCList *clist;
  gint i;
  gint vis_columns;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  clist = GTK_CLIST (widget);
  for (i = 0, vis_columns = 0; i < clist->columns; i++)
    {
      if (clist->column[i].visible)
        {
          if (visible_column == vis_columns)
            return i;
          vis_columns++;
        }
    }
  return 0;  
}

static void
gail_clist_set_row_data (AtkTable      *table,
                         gint          row,
                         const gchar   *description,
                         AtkObject     *header,
                         gboolean      is_header)
{
  GtkWidget *widget;
  GtkCList *gtk_clist;
  GailCList *gail_clist;
  GArray *array;
  GailCListRow* row_data;
  gint i;
  gboolean found = FALSE;
  AtkPropertyValues values = { NULL };
  gchar *signal_name;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  gtk_clist = GTK_CLIST (widget);
  if (row < 0 || row >= gtk_clist->rows)
    return;

  gail_clist = GAIL_CLIST (table);

  if (gail_clist->row_data == NULL)
    gail_clist->row_data = g_array_sized_new (FALSE, TRUE, 
                                              sizeof (GailCListRow *), 0);

  array = gail_clist->row_data;

  for (i = 0; i < array->len; i++)
    {
      row_data = g_array_index (array, GailCListRow*, i);

      if (row == row_data->row_number)
        {
          found = TRUE;
          if (is_header)
            {
              if (row_data->header)
                g_object_unref (row_data->header);
              row_data->header = header;
              if (row_data->header)
                g_object_ref (row_data->header);
            }
          else
            {
              g_free (row_data->description);
              row_data->description = g_strdup (row_data->description);
            }
          break;
        }
    } 
  if (!found)
    {
      GList *elem;

      elem = ROW_ELEMENT (gtk_clist, row);
      g_return_if_fail (elem != NULL);

      row_data = g_new (GailCListRow, 1);
      row_data->row_number = row;
      row_data->row_data = elem->data;
      if (is_header)
        {
          row_data->header = header;
          if (row_data->header)
            g_object_ref (row_data->header);
          row_data->description = NULL;
        }
      else
        {
          row_data->description = g_strdup (row_data->description);
          row_data->header = NULL;
        }
      g_array_append_val (array, row_data);
    }

  g_value_init (&values.new_value, G_TYPE_INT);
  g_value_set_int (&values.new_value, row);

  if (is_header)
    {
      values.property_name = "accessible-table-row-header";
      signal_name = "property_change::accessible-table-row-header";
    }
  else
    {
      values.property_name = "accessible-table-row-description";
      signal_name = "property_change::accessible-table-row-description";
    }
  g_signal_emit_by_name (table, 
                         signal_name,
                         &values, NULL);

}

static GailCListRow*
gail_clist_get_row_data (AtkTable      *table,
                         gint          row)
{
  GtkWidget *widget;
  GtkCList *clist;
  GailCList *obj;
  GArray *array;
  GailCListRow* row_data;
  gint i;

  widget = GTK_ACCESSIBLE (table)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  clist = GTK_CLIST (widget);
  if (row < 0 || row >= clist->rows)
    return NULL;

  obj = GAIL_CLIST (table);

  if (obj->row_data == NULL)
    return NULL;

  array = obj->row_data;

  for (i = 0; i < array->len; i++)
    {
      row_data = g_array_index (array, GailCListRow*, i);

      if (row == row_data->row_number)
        return row_data;
    }
 
  return NULL;
}

static void
gail_clist_get_visible_rect (GtkCList      *clist,
                             GdkRectangle  *clist_rect)
{
  clist_rect->x = - clist->hoffset;
  clist_rect->y = - clist->voffset;
  clist_rect->width = clist->clist_window_width;
  clist_rect->height = clist->clist_window_height;
}

static gboolean
gail_clist_is_cell_visible (GdkRectangle  *cell_rect,
                            GdkRectangle  *visible_rect)
{
  /*
   * A cell is reported as visible if any part of the cell is visible
   */
  if (((cell_rect->x + cell_rect->width) < visible_rect->x) ||
     ((cell_rect->y + cell_rect->height) < visible_rect->y) ||
     (cell_rect->x > (visible_rect->x + visible_rect->width)) ||
     (cell_rect->y > (visible_rect->y + visible_rect->height)))
    return FALSE;
  else
    return TRUE;
}

static void
gail_clist_cell_data_new (GailCList     *clist,
                          GailCell      *cell,
                          gint          column,
                          gint          row)
{
  GList *elem;
  GailCListCellData *cell_data;
  GtkCList *gtk_clist;
  GtkCListRow *clist_row;

  gtk_clist = GTK_CLIST (GTK_ACCESSIBLE (clist)->widget);
  elem = g_list_nth (gtk_clist->row_list, row);
  g_return_if_fail (elem != NULL);
  clist_row = (GtkCListRow *) elem->data;
  cell_data = g_new (GailCListCellData, 1);
  cell_data->gail_cell = cell;
  cell_data->gtk_cell = &(clist_row->cell[column]);
  cell_data->column_number = column;
  cell_data->row_number = row;
  clist->cell_data = g_list_append (clist->cell_data, cell_data);

  g_object_weak_ref (G_OBJECT (cell),
                     (GWeakNotify) gail_clist_cell_destroyed,
                     cell);
}

static void
gail_clist_cell_destroyed (gpointer      data)
{
  GailCell *cell = GAIL_CELL (data);
  AtkObject* parent;

  parent = atk_object_get_parent (ATK_OBJECT (cell));

  gail_clist_cell_data_remove (GAIL_CLIST (parent), cell);
}

static void
gail_clist_cell_data_remove (GailCList *clist,
                             GailCell  *cell)
{
  GList *temp_list;

  for (temp_list = clist->cell_data; temp_list; temp_list = temp_list->next)
    {
      GailCListCellData *cell_data;

      cell_data = (GailCListCellData *) temp_list->data;
      if (cell_data->gail_cell == cell)
        {
          clist->cell_data = g_list_remove_link (clist->cell_data, temp_list);
          g_free (cell_data);
          return;
        }
    }
  g_warning ("No cell removed in gail_clist_cell_data_remove\n");
}

static GailCell*
gail_clist_find_cell (GailCList     *clist,
                      gint          index)
{
  GList *temp_list;
  gint n_cols;

  n_cols = clist->n_cols;

  for (temp_list = clist->cell_data; temp_list; temp_list = temp_list->next)
    {
      GailCListCellData *cell_data;
      gint real_index;

      cell_data = (GailCListCellData *) (temp_list->data);

      real_index = cell_data->column_number + n_cols * cell_data->row_number;
      if (real_index == index)
        return cell_data->gail_cell;
    }
  return NULL;
}

static void
gail_clist_adjustment_changed (GtkAdjustment *adjustment,
                               GtkCList      *clist)
{
  AtkObject *atk_obj;
  GdkRectangle visible_rect;
  GdkRectangle cell_rect;
  GailCList* obj;
  GList *temp_list;

  /*
   * The scrollbars have changed
   */
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (clist));
  obj = GAIL_CLIST (atk_obj);

  /* Get the currently visible area */
  gail_clist_get_visible_rect (clist, &visible_rect);

  /* loop over the cells and report if they are visible or not. */
  /* Must loop through them all */
  for (temp_list = obj->cell_data; temp_list; temp_list = temp_list->next)
    {
      GailCell *cell;
      GailCListCellData *cell_data;

      cell_data = (GailCListCellData *) (temp_list->data);
      cell = cell_data->gail_cell;

      gail_clist_get_cell_area (GAIL_CELL_PARENT (atk_obj), 
                                cell, &cell_rect);
      if (gail_clist_is_cell_visible (&cell_rect, &visible_rect))
        gail_cell_add_state (cell, ATK_STATE_SHOWING, TRUE);
      else
        gail_cell_remove_state (cell, ATK_STATE_SHOWING, TRUE);
    }
  g_signal_emit_by_name (atk_obj, "visible_data_changed");
}

