/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald, 
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>  
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdlib.h>
#include "../config.h"
#include "gtkclist.h"

/* the number rows memchunk expands at a time */
#define CLIST_OPTIMUM_SIZE 512

/* the width of the column resize windows */
#define DRAG_WIDTH  6

/* minimum allowed width of a column */
#define COLUMN_MIN_WIDTH 5

/* this defigns the base grid spacing */
#define CELL_SPACING 1

/* added the horizontal space at the beginning and end of a row*/
#define COLUMN_INSET 3

/* scrollbar spacing class macro */
#define SCROLLBAR_SPACING(w) (GTK_CLIST_CLASS (GTK_OBJECT (w)->klass)->scrollbar_spacing)

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
#define COLUMN_LEFT_XPIXEL(clist, column)  ((clist)->column[(column)].area.x + \
					    (clist)->hoffset)

/* returns the column index from a x pixel location in the 
 * context of the clist's hoffset */
static inline gint
COLUMN_FROM_XPIXEL (GtkCList * clist,
		    gint x)
{
  gint i, cx;

  for (i = 0; i < clist->columns; i++)
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
#define COLUMN_LEFT(clist, column) ((clist)->column[(column)].area.x)

/* returns the total height of the list */
#define LIST_HEIGHT(clist)         (((clist)->row_height * ((clist)->rows)) + \
				    (CELL_SPACING * ((clist)->rows + 1)))

/* returns the total width of the list */
#define LIST_WIDTH(clist)          ((clist)->column[(clist)->columns - 1].area.x + \
				    (clist)->column[(clist)->columns - 1].area.width + \
				    COLUMN_INSET + CELL_SPACING)


/* Signals */
enum
{
  SELECT_ROW,
  UNSELECT_ROW,
  CLICK_COLUMN,
  LAST_SIGNAL
};

typedef void (*GtkCListSignal1) (GtkObject * object,
				 gint arg1,
				 gint arg2,
				 GdkEventButton * arg3,
				 gpointer data);

typedef void (*GtkCListSignal2) (GtkObject * object,
				 gint arg1,
				 gpointer data);


/* GtkCList Methods */
static void gtk_clist_class_init (GtkCListClass * klass);
static void gtk_clist_init (GtkCList * clist);

/* GtkObject Methods */
static void gtk_clist_destroy (GtkObject * object);
static void gtk_clist_finalize (GtkObject * object);


/* GtkWidget Methods */
static void gtk_clist_realize (GtkWidget * widget);
static void gtk_clist_unrealize (GtkWidget * widget);
static void gtk_clist_map (GtkWidget * widget);
static void gtk_clist_unmap (GtkWidget * widget);
static void gtk_clist_draw (GtkWidget * widget,
			    GdkRectangle * area);
static gint gtk_clist_expose (GtkWidget * widget,
			      GdkEventExpose * event);
static gint gtk_clist_button_press (GtkWidget * widget,
				    GdkEventButton * event);
static gint gtk_clist_button_release (GtkWidget * widget,
				      GdkEventButton * event);
static gint gtk_clist_motion (GtkWidget * widget, 
			      GdkEventMotion * event);

static void gtk_clist_size_request (GtkWidget * widget,
				    GtkRequisition * requisition);
static void gtk_clist_size_allocate (GtkWidget * widget,
				     GtkAllocation * allocation);

/* GtkContainer Methods */
static void gtk_clist_foreach (GtkContainer * container,
			       GtkCallback callback,
			       gpointer callback_data);

/* Drawing */
static void draw_row (GtkCList * clist,
		      GdkRectangle * area,
		      gint row,
		      GtkCListRow * clist_row);
static void draw_rows (GtkCList * clist,
		       GdkRectangle * area);

/* Size Allocation */
static void size_allocate_title_buttons (GtkCList * clist);
static void size_allocate_columns (GtkCList * clist);

/* Selection */
static void real_select_row (GtkCList * clist,
			     gint row,
			     gint column,
			     GdkEventButton * event);
static void real_unselect_row (GtkCList * clist,
			       gint row,
			       gint column,
			       GdkEventButton * event);
static gint get_selection_info (GtkCList * clist,
				gint x,
				gint y,
				gint * row,
				gint * column);

/* Resize Columns */
static void draw_xor_line (GtkCList * clist);
static gint new_column_width (GtkCList * clist,
			      gint column,
			      gint * x,
			      gint * visible);
static void resize_column (GtkCList * clist,
			   gint column,
			   gint width);

/* Buttons */
static void column_button_create (GtkCList * clist,
				  gint column);
static void column_button_clicked (GtkWidget * widget,
				   gpointer data);

/* Scrollbars */
static void create_scrollbars (GtkCList * clist);
static void adjust_scrollbars (GtkCList * clist);
static void check_exposures   (GtkCList * clist);
static void vadjustment_changed (GtkAdjustment * adjustment,
				 gpointer data);
static void vadjustment_value_changed (GtkAdjustment * adjustment,
				       gpointer data);
static void hadjustment_changed (GtkAdjustment * adjustment,
				 gpointer data);
static void hadjustment_value_changed (GtkAdjustment * adjustment,
				       gpointer data);


/* Memory Allocation/Distruction Routines */
static GtkCListColumn *columns_new (GtkCList * clist);

static void column_title_new (GtkCList * clist,
			      gint column,
			      gchar * title);
static void columns_delete (GtkCList * clist);

static GtkCListRow *row_new (GtkCList * clist);

static void row_delete (GtkCList * clist,
			GtkCListRow * clist_row);
static void cell_empty (GtkCList * clist,
			GtkCListRow * clist_row,
			gint column);
static void cell_set_text (GtkCList * clist,
			   GtkCListRow * clist_row,
			   gint column,
			   gchar * text);
static void cell_set_pixmap (GtkCList * clist,
			     GtkCListRow * clist_row,
			     gint column,
			     GdkPixmap * pixmap,
			     GdkBitmap * mask);
static void cell_set_pixtext (GtkCList * clist,
			      GtkCListRow * clist_row,
			      gint column,
			      gchar * text,
			      guint8 spacing,
			      GdkPixmap * pixmap,
			      GdkBitmap * mask);

/* Signals */
static void gtk_clist_marshal_signal_1 (GtkObject * object,
					GtkSignalFunc func,
					gpointer func_data,
					GtkArg * args);
static void gtk_clist_marshal_signal_2 (GtkObject * object,
					GtkSignalFunc func,
					gpointer func_data,
					GtkArg * args);

/* Fill in data after widget is realized and has style */

static void add_style_data (GtkCList * clist);

static GtkContainerClass *parent_class = NULL;
static gint clist_signals[LAST_SIGNAL] = {0};


guint
gtk_clist_get_type ()
{
  static guint clist_type = 0;

  if (!clist_type)
    {
      GtkTypeInfo clist_info =
      {
	"GtkCList",
	sizeof (GtkCList),
	sizeof (GtkCListClass),
	(GtkClassInitFunc) gtk_clist_class_init,
	(GtkObjectInitFunc) gtk_clist_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      clist_type = gtk_type_unique (gtk_container_get_type (), &clist_info);
    }

  return clist_type;
}

static void
gtk_clist_class_init (GtkCListClass * klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  parent_class = gtk_type_class (gtk_container_get_type ());

  clist_signals[SELECT_ROW] =
    gtk_signal_new ("select_row",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, select_row),
		    gtk_clist_marshal_signal_1,
	    GTK_TYPE_NONE, 3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_POINTER);
  clist_signals[UNSELECT_ROW] =
    gtk_signal_new ("unselect_row",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, unselect_row),
		    gtk_clist_marshal_signal_1,
	    GTK_TYPE_NONE, 3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_POINTER);
  clist_signals[CLICK_COLUMN] =
    gtk_signal_new ("click_column",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, click_column),
		    gtk_clist_marshal_signal_2,
		    GTK_TYPE_NONE, 1, GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, clist_signals, LAST_SIGNAL);

  object_class->destroy = gtk_clist_destroy;
  object_class->finalize = gtk_clist_finalize;

  widget_class->realize = gtk_clist_realize;
  widget_class->unrealize = gtk_clist_unrealize;
  widget_class->map = gtk_clist_map;
  widget_class->unmap = gtk_clist_unmap;
  widget_class->draw = gtk_clist_draw;
  widget_class->button_press_event = gtk_clist_button_press;
  widget_class->button_release_event = gtk_clist_button_release;
  widget_class->motion_notify_event = gtk_clist_motion;
  widget_class->expose_event = gtk_clist_expose;
  widget_class->size_request = gtk_clist_size_request;
  widget_class->size_allocate = gtk_clist_size_allocate;

  container_class->add = NULL;
  container_class->remove = NULL;
  container_class->foreach = gtk_clist_foreach;

  klass->select_row = real_select_row;
  klass->unselect_row = real_unselect_row;
  klass->click_column = NULL;

  klass->scrollbar_spacing = 5;
}

static void
gtk_clist_marshal_signal_1 (GtkObject * object,
			    GtkSignalFunc func,
			    gpointer func_data,
			    GtkArg * args)
{
  GtkCListSignal1 rfunc;

  rfunc = (GtkCListSignal1) func;

  (*rfunc) (object, GTK_VALUE_INT (args[0]),
	    GTK_VALUE_INT (args[1]),
	    GTK_VALUE_POINTER (args[2]),
	    func_data);
}

static void
gtk_clist_marshal_signal_2 (GtkObject * object,
			    GtkSignalFunc func,
			    gpointer func_data,
			    GtkArg * args)
{
  GtkCListSignal2 rfunc;

  rfunc = (GtkCListSignal2) func;

  (*rfunc) (object, GTK_VALUE_INT (args[0]),
	    func_data);
}

static void
gtk_clist_init (GtkCList * clist)
{
  clist->flags = 0;

  GTK_WIDGET_UNSET_FLAGS (clist, GTK_NO_WINDOW);
  GTK_CLIST_SET_FLAGS (clist, CLIST_FROZEN);

  clist->rows = 0;
  clist->row_center_offset = 0;
  clist->row_height = 0;
  clist->row_list = NULL;
  clist->row_list_end = NULL;

  clist->columns = 0;

  clist->title_window = NULL;
  clist->column_title_area.x = 0;
  clist->column_title_area.y = 0;
  clist->column_title_area.width = 0;
  clist->column_title_area.height = 0;

  clist->clist_window = NULL;
  clist->clist_window_width = 0;
  clist->clist_window_height = 0;

  clist->hoffset = 0;
  clist->voffset = 0;

  clist->shadow_type = GTK_SHADOW_IN;
  clist->hscrollbar_policy = GTK_POLICY_ALWAYS;
  clist->vscrollbar_policy = GTK_POLICY_ALWAYS;

  clist->cursor_drag = NULL;
  clist->xor_gc = NULL;
  clist->fg_gc = NULL;
  clist->bg_gc = NULL;
  clist->x_drag = 0;

  clist->selection_mode = GTK_SELECTION_SINGLE;
  clist->selection = NULL;
}

/* Constructors */
void
gtk_clist_construct (GtkCList * clist,
		     gint columns,
		     gchar *titles[])
{
  int i;

  /* initalize memory chunks */
  clist->row_mem_chunk = g_mem_chunk_new ("clist row mem chunk",
					  sizeof (GtkCListRow),
					  sizeof (GtkCListRow) * CLIST_OPTIMUM_SIZE, 
					  G_ALLOC_AND_FREE);

  clist->cell_mem_chunk = g_mem_chunk_new ("clist cell mem chunk",
					   sizeof (GtkCell) * columns,
					   sizeof (GtkCell) * columns * CLIST_OPTIMUM_SIZE, 
					   G_ALLOC_AND_FREE);

  /* set number of columns, allocate memory */
  clist->columns = columns;
  clist->column = columns_new (clist);

  /* there needs to be at least one column button 
   * because there is alot of code that will break if it
   * isn't there*/
  column_button_create (clist, 0);

  /* create scrollbars */
  create_scrollbars (clist);

  if (titles)
    {
      GTK_CLIST_SET_FLAGS (clist, CLIST_SHOW_TITLES);
      for (i = 0; i < columns; i++)
	gtk_clist_set_column_title (clist, i, titles[i]);
    }
  else
    {
      GTK_CLIST_UNSET_FLAGS (clist, CLIST_SHOW_TITLES);
    }
}

/*
 * GTKCLIST PUBLIC INTERFACE
 *   gtk_clist_new_with_titles
 *   gtk_clist_new
 */
GtkWidget *
gtk_clist_new_with_titles (gint columns,
			   gchar * titles[])
{
  GtkCList *clist;
  GtkWidget *widget;

 if (titles == NULL)
    return NULL;

  widget = gtk_clist_new (columns);
  if (!widget)
    return NULL;
  else
    clist = GTK_CLIST (widget);

  gtk_clist_construct (clist, columns, titles);
  return widget;
}

GtkWidget *
gtk_clist_new (gint columns)
{
  GtkCList *clist;

  if (columns < 1)
    return NULL;

  clist = gtk_type_new (gtk_clist_get_type ());
  gtk_clist_construct (clist, columns, NULL);
  return GTK_WIDGET (clist);
}

void
gtk_clist_set_border (GtkCList * clist,
		      GtkShadowType border)
{
  g_return_if_fail (clist != NULL);

  clist->shadow_type = border;

  if (GTK_WIDGET_VISIBLE (clist))
    gtk_widget_queue_resize (GTK_WIDGET (clist));
}

void
gtk_clist_set_selection_mode (GtkCList * clist,
			      GtkSelectionMode mode)
{
  g_return_if_fail (clist != NULL);

  clist->selection_mode = mode;
}

void
gtk_clist_freeze (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);

  GTK_CLIST_SET_FLAGS (clist, CLIST_FROZEN);
}

void
gtk_clist_thaw (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);

  GTK_CLIST_UNSET_FLAGS (clist, CLIST_FROZEN);

  adjust_scrollbars (clist);
  draw_rows (clist, NULL);
}

void
gtk_clist_column_titles_show (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);

  if (!GTK_CLIST_SHOW_TITLES (clist))
    {
      GTK_CLIST_SET_FLAGS (clist, CLIST_SHOW_TITLES);
      gdk_window_show (clist->title_window);
      gtk_widget_queue_resize (GTK_WIDGET (clist));
    }
}

void 
gtk_clist_column_titles_hide (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);

  if (GTK_CLIST_SHOW_TITLES (clist))
    {
      GTK_CLIST_UNSET_FLAGS (clist, CLIST_SHOW_TITLES);
      gdk_window_hide (clist->title_window);
      gtk_widget_queue_resize (GTK_WIDGET (clist));
    }
}

void
gtk_clist_column_title_active (GtkCList * clist,
			       gint column)
{
  g_return_if_fail (clist != NULL);

  if (column < 0 || column >= clist->columns)
    return;

  if (!GTK_WIDGET_SENSITIVE (clist->column[column].button) ||
      !GTK_WIDGET_CAN_FOCUS (clist->column[column].button))
    {
      GTK_WIDGET_SET_FLAGS (clist->column[column].button, GTK_SENSITIVE | GTK_CAN_FOCUS);
      if (GTK_WIDGET_VISIBLE (clist))
	gtk_widget_queue_draw (clist->column[column].button);
    }
}

void
gtk_clist_column_title_passive (GtkCList * clist,
				gint column)
{
  g_return_if_fail (clist != NULL);

  if (column < 0 || column >= clist->columns)
    return;

  if (GTK_WIDGET_SENSITIVE (clist->column[column].button) ||
      GTK_WIDGET_CAN_FOCUS (clist->column[column].button))
    {
      GTK_WIDGET_UNSET_FLAGS (clist->column[column].button, GTK_SENSITIVE | GTK_CAN_FOCUS);
      if (GTK_WIDGET_VISIBLE (clist))
	gtk_widget_queue_draw (clist->column[column].button);
    }
}

void
gtk_clist_column_titles_active (GtkCList * clist)
{
  gint i;

  g_return_if_fail (clist != NULL);

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      gtk_clist_column_title_active (clist, i);
}

void
gtk_clist_column_titles_passive (GtkCList * clist)
{
  gint i;

  g_return_if_fail (clist != NULL);

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      gtk_clist_column_title_passive (clist, i);
}

void
gtk_clist_set_column_title (GtkCList * clist,
			    gint column,
			    gchar * title)
{
  gint new_button = 0;
  GtkWidget *old_widget;
  GtkWidget *alignment = NULL;
  GtkWidget *label;

  g_return_if_fail (clist != NULL);

  if (column < 0 || column >= clist->columns)
    return;

  /* if the column button doesn't currently exist,
   * it has to be created first */
  if (!clist->column[column].button)
    {
      column_button_create (clist, column);
      new_button = 1;
    }

  column_title_new (clist, column, title);

  /* remove and destroy the old widget */
  old_widget = GTK_BUTTON (clist->column[column].button)->child;
  if (old_widget)
    {
      gtk_container_remove (GTK_CONTAINER (clist->column[column].button), old_widget);
      gtk_widget_destroy (old_widget);
    }

  /* create new alignment based no column justification */
  switch (clist->column[column].justification)
    {
    case GTK_JUSTIFY_LEFT:
      alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_RIGHT:
      alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_CENTER:
      alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_FILL:
      alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      break;
    }

  label = gtk_label_new (clist->column[column].title);
  gtk_container_add (GTK_CONTAINER (alignment), label);
  gtk_container_add (GTK_CONTAINER (clist->column[column].button), alignment);
  gtk_widget_show (label);
  gtk_widget_show (alignment);

  /* if this button didn't previously exist, then the
   * column button positions have to be re-computed */
  if (GTK_WIDGET_VISIBLE (clist) && new_button)
    size_allocate_title_buttons (clist);
}

void
gtk_clist_set_column_widget (GtkCList * clist,
			     gint column,
			     GtkWidget * widget)
{
  gint new_button = 0;
  GtkWidget *old_widget;

  g_return_if_fail (clist != NULL);

  if (column < 0 || column >= clist->columns)
    return;

  /* if the column button doesn't currently exist,
   * it has to be created first */
  if (!clist->column[column].button)
    {
      column_button_create (clist, column);
      new_button = 1;
    }

  column_title_new (clist, column, NULL);

  /* remove and destroy the old widget */
  old_widget = GTK_BUTTON (clist->column[column].button)->child;
  if (old_widget)
    {
      gtk_container_remove (GTK_CONTAINER (clist->column[column].button), old_widget);
      gtk_widget_destroy (old_widget);
    }

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
}

void
gtk_clist_set_column_justification (GtkCList * clist,
				    gint column,
				    GtkJustification justification)
{
  GtkWidget *alignment;

  g_return_if_fail (clist != NULL);

  if (column < 0 || column >= clist->columns)
    return;

  clist->column[column].justification = justification;

  /* change the alinment of the button title if it's not a
   * custom widget */
  if (clist->column[column].title)
    {
      alignment = GTK_BUTTON (clist->column[column].button)->child;

      switch (clist->column[column].justification)
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

  if (!GTK_CLIST_FROZEN (clist))
    draw_rows (clist, NULL);
}

void
gtk_clist_set_column_width (GtkCList * clist,
			    gint column,
			    gint width)
{
  g_return_if_fail (clist != NULL);

  if (column < 0 || column >= clist->columns)
    return;

  clist->column[column].width = width;
  clist->column[column].width_set = TRUE;

  /* FIXME: this is quite expensive to do if the widget hasn't
   *        been size_allocated yet, and pointless. Should
   *        a flag be kept
   */
  size_allocate_columns (clist);
  size_allocate_title_buttons (clist);

  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);
      draw_rows (clist, NULL);
    }
}

void
gtk_clist_set_row_height (GtkCList * clist,
			  gint height)
{
  gint text_height;

  g_return_if_fail (clist != NULL);

  if (height > 0)
    clist->row_height = height;
  else
    return;

  GTK_CLIST_SET_FLAGS (clist, CLIST_ROW_HEIGHT_SET);
  
  if (GTK_WIDGET_REALIZED (clist))
    {
      text_height = height - (GTK_WIDGET (clist)->style->font->ascent +
			      GTK_WIDGET (clist) ->style->font->descent + 1);
      clist->row_center_offset = (text_height / 2) + GTK_WIDGET (clist)->style->font->ascent + 1.5;
    }
      
  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);
      draw_rows (clist, NULL);
    }
}

void
gtk_clist_moveto (GtkCList * clist,
		  gint row,
		  gint column,
		  gfloat row_align,
		  gfloat col_align)
{
  gint x, y;

  g_return_if_fail (clist != NULL);

  if (row < -1 || row >= clist->rows)
    return;
  if (column < -1 || column >= clist->columns)
    return;

  /* adjust vertical scrollbar */
  if (row >= 0)
    {
      x = ROW_TOP (clist, row) - (row_align * (clist->clist_window_height - 
					       (clist->row_height + 2 * CELL_SPACING)));
      
      if (x < 0)
	GTK_RANGE (clist->vscrollbar)->adjustment->value = 0.0;
      else if (x > LIST_HEIGHT (clist) - clist->clist_window_height)
	GTK_RANGE (clist->vscrollbar)->adjustment->value = LIST_HEIGHT (clist) - 
	  clist->clist_window_height;
      else
	GTK_RANGE (clist->vscrollbar)->adjustment->value = x;
      
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->vscrollbar)->adjustment), 
			       "value_changed");
    } 
     
  /* adjust horizontal scrollbar */
  if (column >= 0)
    {
      y = COLUMN_LEFT (clist, column) - (col_align * (clist->clist_window_width - 
						      clist->column[column].area.width + 
						      2 * (CELL_SPACING + COLUMN_INSET)));
      
      if (y < 0)
	GTK_RANGE (clist->hscrollbar)->adjustment->value = 0.0;
      else if (y > LIST_WIDTH (clist) - clist->clist_window_width)
	GTK_RANGE (clist->hscrollbar)->adjustment->value = LIST_WIDTH (clist) - 
	  clist->clist_window_width;
      else
	GTK_RANGE (clist->hscrollbar)->adjustment->value = y;
      
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->hscrollbar)->adjustment), 
			       "value_changed");
    }
}

GtkCellType 
gtk_clist_get_cell_type (GtkCList * clist,
			 gint row,
			 gint column)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, -1);

  if (row < 0 || row >= clist->rows)
    return -1;
  if (column < 0 || column >= clist->columns)
    return -1;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  return clist_row->cell[column].type;
}

void
gtk_clist_set_text (GtkCList * clist,
		    gint row,
		    gint column,
		    gchar * text)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  /* if text is null, then the cell is empty */
  if (text)
    cell_set_text (clist, clist_row, column, text);
  else
    cell_empty (clist, clist_row, column);

  /* redraw the list if it's not frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row))
	draw_row (clist, NULL, row, clist_row);
    }
}

gint
gtk_clist_get_text (GtkCList * clist,
		    gint row,
		    gint column,
		    gchar ** text)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, 0);

  if (row < 0 || row >= clist->rows)
    return 0;
  if (column < 0 || column >= clist->columns)
    return 0;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->cell[column].type != GTK_CELL_TEXT)
    return 0;

  if (text)
    *text = GTK_CELL_TEXT (clist_row->cell[column])->text;

  return 1;
}

void
gtk_clist_set_pixmap (GtkCList * clist,
		      gint row,
		      gint column,
		      GdkPixmap * pixmap,
		      GdkBitmap * mask)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  
  gdk_pixmap_ref (pixmap);
  gdk_pixmap_ref (mask);
  cell_set_pixmap (clist, clist_row, column, pixmap, mask);

  /* redraw the list if it's not frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row))
	draw_row (clist, NULL, row, clist_row);
    }
}

gint
gtk_clist_get_pixmap (GtkCList * clist,
		      gint row,
		      gint column,
		      GdkPixmap ** pixmap,
		      GdkBitmap ** mask)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, 0);

  if (row < 0 || row >= clist->rows)
    return 0;
  if (column < 0 || column >= clist->columns)
    return 0;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->cell[column].type != GTK_CELL_PIXMAP)
    return 0;

  if (pixmap)
    *pixmap = GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap;
  if (mask)
    *mask = GTK_CELL_PIXMAP (clist_row->cell[column])->mask;

  return 1;
}

void
gtk_clist_set_pixtext (GtkCList * clist,
		       gint row,
		       gint column,
		       gchar * text,
		       guint8 spacing,
		       GdkPixmap * pixmap,
		       GdkBitmap * mask)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  
  gdk_pixmap_ref (pixmap);
  gdk_pixmap_ref (mask);
  cell_set_pixtext (clist, clist_row, column, text, spacing, pixmap, mask);

  /* redraw the list if it's not frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row))
	draw_row (clist, NULL, row, clist_row);
    }
}

gint
gtk_clist_get_pixtext (GtkCList * clist,
		       gint row,
		       gint column,
		       gchar ** text,
		       guint8 * spacing,
		       GdkPixmap ** pixmap,
		       GdkBitmap ** mask)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, 0);

  if (row < 0 || row >= clist->rows)
    return 0;
  if (column < 0 || column >= clist->columns)
    return 0;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->cell[column].type != GTK_CELL_PIXTEXT)
    return 0;

  if (text)
    *text = GTK_CELL_PIXTEXT (clist_row->cell[column])->text;
  if (spacing)
    *spacing = GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing;
  if (pixmap)
    *pixmap = GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap;
  if (mask)
    *mask = GTK_CELL_PIXTEXT (clist_row->cell[column])->mask;

  return 1;
}

void
gtk_clist_set_foreground (GtkCList * clist,
			  gint row,
			  GdkColor * color)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (color != NULL);

  if (row < 0 || row >= clist->rows)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  clist_row->foreground = *color;
  clist_row->fg_set = TRUE;

  if (!GTK_CLIST_FROZEN (clist) && gtk_clist_row_is_visible (clist, row))
    draw_row (clist, NULL, row, clist_row);
}

void gtk_clist_set_background (GtkCList * clist,
			       gint row,
			       GdkColor * color)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (color != NULL);

  if (row < 0 || row >= clist->rows)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  clist_row->background = *color;
  clist_row->bg_set = TRUE;

  if (!GTK_CLIST_FROZEN (clist) && gtk_clist_row_is_visible (clist, row))
    draw_row (clist, NULL, row, clist_row);
}

void
gtk_clist_set_shift (GtkCList * clist,
		     gint row,
		     gint column,
		     gint vertical,
		     gint horizontal)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  clist_row->cell[column].vertical = vertical;
  clist_row->cell[column].horizontal = horizontal;

  if (!GTK_CLIST_FROZEN (clist) && gtk_clist_row_is_visible (clist, row))
    draw_row (clist, NULL, row, clist_row);
}

gint
gtk_clist_append (GtkCList * clist,
		  gchar * text[])
{
  gint i;
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, -1);

  clist_row = row_new (clist);
  clist->rows++;

  /* set the text in the row's columns */
  if (text)
    for (i = 0; i < clist->columns; i++)
      if (text[i])
        cell_set_text (clist, clist_row, i, text[i]);

  /* keeps track of the end of the list so the list 
   * doesn't have to be traversed every time a item is added */
  if (!clist->row_list)
    {
      clist->row_list = g_list_append (clist->row_list, clist_row);
      clist->row_list_end = clist->row_list;

      /* check the selection mode to see if we should select
       * the first row automaticly */
      switch (clist->selection_mode)
	{
	case GTK_SELECTION_BROWSE:
	  gtk_clist_select_row (clist, 0, -1);
	  break;

	default:
	  break;
	}
    }
  else
    clist->row_list_end = (g_list_append (clist->row_list_end, clist_row))->next;
  
  /* redraw the list if it's not frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);

      if (gtk_clist_row_is_visible (clist, clist->rows - 1))
	draw_rows (clist, NULL);
    }

  /* return index of the row */
  return clist->rows - 1;
}

void
gtk_clist_insert (GtkCList * clist,
		  gint row,
		  gchar * text[])
{
  gint i;
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (text != NULL);

  /* return if out of bounds */
  if (row < 0 || row > clist->rows)
    return;

  if (clist->rows == 0)
    gtk_clist_append (clist, text);
  else
    {
      /* create the row */
      clist_row = row_new (clist);

      /* set the text in the row's columns */
      if (text)
	for (i = 0; i < clist->columns; i++)
	  if (text[i])
	    cell_set_text (clist, clist_row, i, text[i]);
      
      /* reset the row end pointer if we're inserting at the
       * end of the list */
      if (row == clist->rows)
	clist->row_list_end = (g_list_append (clist->row_list_end, clist_row))->next;
      else
	clist->row_list = g_list_insert (clist->row_list, clist_row, row);

      clist->rows++;
    }

  /* redraw the list if it isn't frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);

      if (gtk_clist_row_is_visible (clist, row))
	draw_rows (clist, NULL);
    }
}

void
gtk_clist_remove (GtkCList * clist,
		  gint row)
{
  gint was_visible;
  GList *list;
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  /* return if out of bounds */
  if (row < 0 || row > (clist->rows - 1))
    return;

  was_visible = gtk_clist_row_is_visible (clist, row);

  /* get the row we're going to delete */
  list = g_list_nth (clist->row_list, row);
  clist_row = list->data;

  /* reset the row end pointer if we're removing at the
   * end of the list */
  if (row == clist->rows - 1)
    clist->row_list_end = list->prev;

  clist->row_list = g_list_remove (clist->row_list, clist_row);
  clist->rows--;

  /* redraw the row if it isn't frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);

      if (was_visible)
	draw_rows (clist, NULL);
    }

  if (clist_row->state == GTK_STATE_SELECTED)
    {
      switch (clist->selection_mode)
	{
	case GTK_SELECTION_BROWSE:
	  if (row >= clist->rows)
	    row--;
	  gtk_clist_select_row (clist, row, -1);
	  break;
	  
	default:
	  break;
	}

      /* remove from selection list */
      clist->selection = g_list_remove (clist->selection, clist_row);
    }

  row_delete (clist, clist_row);
}

void
gtk_clist_clear (GtkCList * clist)
{
  GList *list;
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  /* remove all the rows */
  list = clist->row_list;
  while (list)
    {
      clist_row = list->data;
      list = list->next;

      row_delete (clist, clist_row);
    }
  g_list_free (clist->row_list);

  /* free up the selection list */
  g_list_free (clist->selection);

  clist->row_list = NULL;
  clist->row_list_end = NULL;
  clist->selection = NULL;
  clist->voffset = 0;
  clist->rows = 0;

  /* zero-out the scrollbars */
  if (clist->vscrollbar)
    {
      GTK_RANGE (clist->vscrollbar)->adjustment->value = 0.0;
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->vscrollbar)->adjustment), "changed");
      
      if (!GTK_CLIST_FROZEN (clist))
	{
	  adjust_scrollbars (clist);
	  draw_rows (clist, NULL);
	}
    }
}

void
gtk_clist_set_row_data (GtkCList * clist,
			gint row,
			gpointer data)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  if (row < 0 || row > (clist->rows - 1))
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  clist_row->data = data;

  /*
   * re-send the selected signal if data is changed/added
   * so the application can respond to the new data -- 
   * this could be questionable behavior
   */
  if (clist_row->state == GTK_STATE_SELECTED)
    gtk_clist_select_row (clist, 0, 0);
}

gpointer
gtk_clist_get_row_data (GtkCList * clist,
			gint row)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, NULL);

  if (row < 0 || row > (clist->rows - 1))
    return NULL;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  return clist_row->data;
}

gint
gtk_clist_find_row_from_data (GtkCList * clist,
			      gpointer data)
{
  GList *list;
  gint n;

  g_return_val_if_fail (clist != NULL, -1);
  g_return_val_if_fail (GTK_IS_CLIST (clist), -1);

  if (clist->rows < 1)
    return -1; /* is this an optimization or just worthless? */

  n = 0;
  list = clist->row_list;
  while (list)
    {
      GtkCListRow *clist_row;

      clist_row = list->data;
      if (clist_row->data == data)
        break;
      n++;
      list = list->next;
    }

  if (list)
    return n;

  return -1;
}

void
gtk_clist_select_row (GtkCList * clist,
		      gint row,
		      gint column)
{
  g_return_if_fail (clist != NULL);

  if (row < 0 || row >= clist->rows)
    return;

  if (column < -1 || column >= clist->columns)
    return;

  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[SELECT_ROW], row, column, NULL);
}

void
gtk_clist_unselect_row (GtkCList * clist,
			gint row,
			gint column)
{
  g_return_if_fail (clist != NULL);

  if (row < 0 || row >= clist->rows)
    return;

  if (column < -1 || column >= clist->columns)
    return;

  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW], row, column, NULL);
}

gint
gtk_clist_row_is_visible (GtkCList * clist,
			 gint row)
{
  g_return_val_if_fail (clist != NULL, 0);

  if (row < 0 || row >= clist->rows)
    return 0;

  if (clist->row_height == 0)
    return 0;

  if (row < ROW_FROM_YPIXEL (clist, 0))
    return 0;

  if (row > ROW_FROM_YPIXEL (clist, clist->clist_window_height))
    return 0;

  return 1;
}

GtkAdjustment *
gtk_clist_get_vadjustment (GtkCList * clist)
{
  g_return_val_if_fail (clist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CLIST (clist), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (clist->vscrollbar));
}

GtkAdjustment *
gtk_clist_get_hadjustment (GtkCList * clist)
{
  g_return_val_if_fail (clist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CLIST (clist), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (clist->hscrollbar));
}

void
gtk_clist_set_policy (GtkCList * clist,
		      GtkPolicyType vscrollbar_policy,
		      GtkPolicyType hscrollbar_policy)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (clist->vscrollbar_policy != vscrollbar_policy)
    {
      clist->vscrollbar_policy = vscrollbar_policy;

      if (GTK_WIDGET (clist)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (clist));
    }

  if (clist->hscrollbar_policy != hscrollbar_policy)
    {
      clist->hscrollbar_policy = hscrollbar_policy;

      if (GTK_WIDGET (clist)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (clist));
    }
}

/*
 * GTKOBJECT
 *   gtk_clist_destroy
 *   gtk_clist_finalize
 */
static void
gtk_clist_destroy (GtkObject * object)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CLIST (object));

  clist = GTK_CLIST (object);

  /* freeze the list */
  GTK_CLIST_SET_FLAGS (clist, CLIST_FROZEN);

  /* get rid of all the rows */
  gtk_clist_clear (clist);

  /* Since we don't have a _remove method, unparent the children
   * instead of destroying them so the focus will be unset properly.
   * (For other containers, the _remove method takes care of the
   * unparent) The destroy will happen when the refcount drops
   * to zero.
   */

  /* destroy the scrollbars */
  if (clist->vscrollbar)
    {
      gtk_widget_unparent (clist->vscrollbar);
      clist->vscrollbar = NULL;
    }
  if (clist->hscrollbar)
    {
      gtk_widget_unparent (clist->hscrollbar);
      clist->hscrollbar = NULL;
    }

  /* destroy the column buttons */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      {
	gtk_widget_unparent (clist->column[i].button);
	clist->column[i].button = NULL;
      }

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_clist_finalize (GtkObject * object)
{
  GtkCList *clist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CLIST (object));

  clist = GTK_CLIST (object);

  columns_delete (clist);

  g_mem_chunk_destroy (clist->cell_mem_chunk);
  g_mem_chunk_destroy (clist->row_mem_chunk);

  if (GTK_OBJECT_CLASS (parent_class)->finalize)
    (*GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/*
 * GTKWIDGET
 *   gtk_clist_realize
 *   gtk_clist_unrealize
 *   gtk_clist_map
 *   gtk_clist_unmap
 *   gtk_clist_draw
 *   gtk_clist_expose
 *   gtk_clist_button_press
 *   gtk_clist_button_release
 *   gtk_clist_button_motion
 *   gtk_clist_size_request
 *   gtk_clist_size_allocate
 */
static void
gtk_clist_realize (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGCValues values;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;


  /* main window */
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, clist);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  /* column-title window */
  clist->title_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (clist->title_window, clist);

  gtk_style_set_background (widget->style, clist->title_window, GTK_STATE_SELECTED);
  gdk_window_show (clist->title_window);

  /* set things up so column buttons are drawn in title window */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      gtk_widget_set_parent_window (clist->column[i].button, clist->title_window);

  /* clist-window */
  clist->clist_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (clist->clist_window, clist);

  gdk_window_set_background (clist->clist_window, &widget->style->bg[GTK_STATE_PRELIGHT]);
  gdk_window_show (clist->clist_window);
  gdk_window_get_size (clist->clist_window, &clist->clist_window_width,
		       &clist->clist_window_height);

  /* create resize windows */
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = (GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_POINTER_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK);
  attributes.cursor = clist->cursor_drag = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
  attributes_mask = GDK_WA_CURSOR;
  
  for (i = 0; i < clist->columns; i++)
    {
      clist->column[i].window = gdk_window_new (clist->title_window, &attributes, attributes_mask);
      gdk_window_set_user_data (clist->column[i].window, clist);
      gdk_window_show (clist->column[i].window);
    }

  /* GCs */
  clist->fg_gc = gdk_gc_new (widget->window);
  clist->bg_gc = gdk_gc_new (widget->window);
  
  /* We'll use this gc to do scrolling as well */
  gdk_gc_set_exposures (clist->fg_gc, TRUE);

  values.foreground = widget->style->white;
  values.function = GDK_XOR;
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  clist->xor_gc = gdk_gc_new_with_values (widget->window,
					  &values,
					  GDK_GC_FOREGROUND |
					  GDK_GC_FUNCTION |
					  GDK_GC_SUBWINDOW);

  add_style_data (clist);
}

static void
gtk_clist_unrealize (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  GTK_CLIST_SET_FLAGS (clist, CLIST_FROZEN);

  gdk_cursor_destroy (clist->cursor_drag);
  gdk_gc_destroy (clist->xor_gc);
  gdk_gc_destroy (clist->fg_gc);
  gdk_gc_destroy (clist->bg_gc);

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].window)
      {
	gdk_window_set_user_data (clist->column[i].window, NULL);
	gdk_window_destroy (clist->column[i].window);
	clist->column[i].window = NULL;
      }

  gdk_window_set_user_data (clist->clist_window, NULL);
  gdk_window_destroy (clist->clist_window);
  clist->clist_window = NULL;

  gdk_window_set_user_data (clist->title_window, NULL);
  gdk_window_destroy (clist->title_window);
  clist->title_window = NULL;

  clist->cursor_drag = NULL;
  clist->xor_gc = NULL;
  clist->fg_gc = NULL;
  clist->bg_gc = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_clist_map (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  if (!GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

      gdk_window_show (widget->window);
      gdk_window_show (clist->title_window);
      gdk_window_show (clist->clist_window);

      /* map column buttons */
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].button &&
	    GTK_WIDGET_VISIBLE (clist->column[i].button) &&
	    !GTK_WIDGET_MAPPED (clist->column[i].button))
	  gtk_widget_map (clist->column[i].button);
      
      /* map resize windows AFTER column buttons (above) */
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].window && clist->column[i].button)
	  gdk_window_show (clist->column[i].window);
       
      /* map vscrollbars */
      if (GTK_WIDGET_VISIBLE (clist->vscrollbar) &&
	  !GTK_WIDGET_MAPPED (clist->vscrollbar))
	gtk_widget_map (clist->vscrollbar);

      if (GTK_WIDGET_VISIBLE (clist->hscrollbar) &&
	  !GTK_WIDGET_MAPPED (clist->hscrollbar))
	gtk_widget_map (clist->hscrollbar);

      /* unfreeze the list */
      GTK_CLIST_UNSET_FLAGS (clist, CLIST_FROZEN);
    }
}

static void
gtk_clist_unmap (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  if (GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].window)
	  gdk_window_hide (clist->column[i].window);

      gdk_window_hide (clist->clist_window);
      gdk_window_hide (clist->title_window);
      gdk_window_hide (widget->window);

      /* unmap scrollbars */
      if (GTK_WIDGET_MAPPED (clist->vscrollbar))
	gtk_widget_unmap (clist->vscrollbar);

      if (GTK_WIDGET_MAPPED (clist->hscrollbar))
	gtk_widget_unmap (clist->hscrollbar);

      /* unmap column buttons */
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].button &&
	    GTK_WIDGET_MAPPED (clist->column[i].button))
	  gtk_widget_unmap (clist->column[i].button);

      /* freeze the list */
      GTK_CLIST_SET_FLAGS (clist, CLIST_FROZEN);
    }
}

static void
gtk_clist_draw (GtkWidget * widget,
		GdkRectangle * area)
{
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      clist = GTK_CLIST (widget);

      gdk_window_clear_area (widget->window,
			     area->x, area->y,
			     area->width, area->height);

      /* draw list shadow/border */
      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, clist->shadow_type,
		       0, 0, 
		       clist->clist_window_width + (2 * widget->style->klass->xthickness),
		       clist->clist_window_height + (2 * widget->style->klass->ythickness) +
		       clist->column_title_area.height);

      gdk_window_clear_area (clist->clist_window,
			     0, 0, -1, -1);

      draw_rows (clist, NULL);
    }
}

static gint
gtk_clist_expose (GtkWidget * widget,
		  GdkEventExpose * event)
{
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      clist = GTK_CLIST (widget);

      /* draw border */
      if (event->window == widget->window)
	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, clist->shadow_type,
			 0, 0,
			 clist->clist_window_width + (2 * widget->style->klass->xthickness),
			 clist->clist_window_height + (2 * widget->style->klass->ythickness) +
			 clist->column_title_area.height);

      /* exposure events on the list */
      if (event->window == clist->clist_window)
	draw_rows (clist, &event->area);
    }

  return FALSE;
}

static gint
gtk_clist_button_press (GtkWidget * widget,
			GdkEventButton * event)
{
  gint i;
  GtkCList *clist;
  gint x, y, row, column;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  clist = GTK_CLIST (widget);

  /* selections on the list */
  if (event->window == clist->clist_window)
    {
      x = event->x;
      y = event->y;

      if (get_selection_info (clist, x, y, &row, &column))
	gtk_signal_emit (GTK_OBJECT (clist), clist_signals[SELECT_ROW],
			 row, column, event);

      return FALSE;
    }

  /* press on resize windows */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].window && event->window == clist->column[i].window)
      {
	GTK_CLIST_SET_FLAGS (clist, CLIST_IN_DRAG);
	gtk_widget_get_pointer (widget, &clist->x_drag, NULL);

	gdk_pointer_grab (clist->column[i].window, FALSE,
			  GDK_POINTER_MOTION_HINT_MASK |
			  GDK_BUTTON1_MOTION_MASK |
			  GDK_BUTTON_RELEASE_MASK,
			  NULL, NULL, event->time);

	draw_xor_line (clist);
	return FALSE;
      }

  return FALSE;
}

static gint
gtk_clist_button_release (GtkWidget * widget,
			  GdkEventButton * event)
{
  gint i, x, width, visible;
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  clist = GTK_CLIST (widget);

  /* release on resize windows */
  if (GTK_CLIST_IN_DRAG (clist))
    for (i = 0; i < clist->columns; i++)
      if (clist->column[i].window && event->window == clist->column[i].window)
	{
	  GTK_CLIST_UNSET_FLAGS (clist, CLIST_IN_DRAG);
	  gtk_widget_get_pointer (widget, &x, NULL);
	  width = new_column_width (clist, i, &x, &visible);
	  gdk_pointer_ungrab (event->time);
	  
	  if (visible)
	    draw_xor_line (clist);

	  resize_column (clist, i, width);
	  return FALSE;
	}

  return FALSE;
}

static gint
gtk_clist_motion (GtkWidget * widget,
		  GdkEventMotion * event)
{
  gint i, x, visible;
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);

  clist = GTK_CLIST (widget);

  if (GTK_CLIST_IN_DRAG (clist))
    for (i = 0; i < clist->columns; i++)
      if (clist->column[i].window && event->window == clist->column[i].window)
	{
	  if (event->is_hint || event->window != widget->window)
	    gtk_widget_get_pointer (widget, &x, NULL);
	  else
	    x = event->x;

	  new_column_width (clist, i, &x, &visible);
	  /* Welcome to my hack!  I'm going to use a value of x_drage = -99999 to
	   * indicate the the xor line is already no visible */
	  if (!visible && clist->x_drag != -99999)
	    {
	      draw_xor_line (clist);
	      clist->x_drag = -99999;
	    }

	  if (x != clist->x_drag && visible)
	    {
	      if (clist->x_drag != -99999)
		draw_xor_line (clist);

	      clist->x_drag = x;
	      draw_xor_line (clist);
	    }
	}

  return TRUE;
}

static void
gtk_clist_size_request (GtkWidget * widget,
			GtkRequisition * requisition)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));
  g_return_if_fail (requisition != NULL);

  clist = GTK_CLIST (widget);

  requisition->width = 0;
  requisition->height = 0;

  /* compute the size of the column title (title) area */
  clist->column_title_area.height = 0;
  if (GTK_CLIST_SHOW_TITLES (clist))
    for (i = 0; i < clist->columns; i++)
      if (clist->column[i].button)
	{
	  gtk_widget_size_request (clist->column[i].button, &clist->column[i].button->requisition);
	  clist->column_title_area.height = MAX (clist->column_title_area.height,
						 clist->column[i].button->requisition.height);
	}
  requisition->height += clist->column_title_area.height;

  /* add the vscrollbar space */
  if ((clist->vscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (clist->vscrollbar))
    {
      gtk_widget_size_request (clist->vscrollbar, &clist->vscrollbar->requisition);

      requisition->width += clist->vscrollbar->requisition.width + SCROLLBAR_SPACING (clist);
      requisition->height = MAX (requisition->height,
				 clist->vscrollbar->requisition.height);
    }

  /* add the hscrollbar space */
  if ((clist->hscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (clist->hscrollbar))
    {
      gtk_widget_size_request (clist->hscrollbar, &clist->hscrollbar->requisition);

      requisition->height += clist->hscrollbar->requisition.height + SCROLLBAR_SPACING (clist);
      requisition->width = MAX (clist->hscrollbar->requisition.width, 
				requisition->width - 
				clist->vscrollbar->requisition.width);

    }

  requisition->width += widget->style->klass->xthickness * 2 +
    GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += widget->style->klass->ythickness * 2 +
    GTK_CONTAINER (widget)->border_width * 2;
}

static void
gtk_clist_size_allocate (GtkWidget * widget,
			 GtkAllocation * allocation)
{
  GtkCList *clist;
  GtkAllocation clist_allocation;
  GtkAllocation child_allocation;
  gint i, vscrollbar_vis, hscrollbar_vis;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));
  g_return_if_fail (allocation != NULL);

  clist = GTK_CLIST (widget);
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + GTK_CONTAINER (widget)->border_width,
			      allocation->y + GTK_CONTAINER (widget)->border_width,
			      allocation->width - GTK_CONTAINER (widget)->border_width * 2,
			      allocation->height - GTK_CONTAINER (widget)->border_width * 2);

      /* use internal allocation structure for all the math
       * because it's easier than always subtracting the container
       * border width */
      clist->internal_allocation.x = 0;
      clist->internal_allocation.y = 0;
      clist->internal_allocation.width = allocation->width -
	GTK_CONTAINER (widget)->border_width * 2;
      clist->internal_allocation.height = allocation->height -
	GTK_CONTAINER (widget)->border_width * 2;
	
      /* allocate clist window assuming no scrollbars */
      clist_allocation.x = clist->internal_allocation.x + widget->style->klass->xthickness;
      clist_allocation.y = clist->internal_allocation.y + widget->style->klass->ythickness +
	clist->column_title_area.height;
      clist_allocation.width = clist->internal_allocation.width - 
	(2 * widget->style->klass->xthickness);
      clist_allocation.height = clist->internal_allocation.height -
	(2 * widget->style->klass->xthickness) -
	clist->column_title_area.height;

      /* 
       * here's where we decide to show/not show the scrollbars
       */
      vscrollbar_vis = 0;
      hscrollbar_vis = 0;

      for (i = 0; i <= 1; i++)
	{
	  if (LIST_HEIGHT (clist) <= clist_allocation.height &&
	      clist->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
	    {
	      vscrollbar_vis = 0;
	    }
	  else
	    {
	      if (!vscrollbar_vis)
		{
		  vscrollbar_vis = 1;
		  clist_allocation.width -= clist->vscrollbar->requisition.width +
		    SCROLLBAR_SPACING (clist);
		}  
	    }
	  
	  if (LIST_WIDTH (clist) <= clist_allocation.width &&
	      clist->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
	    {
	      hscrollbar_vis = 0;
	    }
	  else
	    {
	      if (!hscrollbar_vis)
		{
		  hscrollbar_vis = 1;
		  clist_allocation.height -= clist->hscrollbar->requisition.height + 
		    SCROLLBAR_SPACING (clist);	
		}  
	    }
	}

      clist->clist_window_width = clist_allocation.width;
      clist->clist_window_height = clist_allocation.height;

      gdk_window_move_resize (clist->clist_window,
			      clist_allocation.x,
			      clist_allocation.y,
			      clist_allocation.width,
			      clist_allocation.height);

      /* position the window which holds the column title buttons */
      clist->column_title_area.x = widget->style->klass->xthickness;
      clist->column_title_area.y = widget->style->klass->ythickness;
      clist->column_title_area.width = clist_allocation.width;

      gdk_window_move_resize (clist->title_window,
			      clist->column_title_area.x,
			      clist->column_title_area.y,
			      clist->column_title_area.width,
			      clist->column_title_area.height);

      /* column button allocation */
      size_allocate_columns (clist);
      size_allocate_title_buttons (clist);
      adjust_scrollbars (clist);

      /* allocate the vscrollbar */
      if (vscrollbar_vis)
	{
	  if (!GTK_WIDGET_VISIBLE (clist->vscrollbar))
	    gtk_widget_show (clist->vscrollbar);
	      
	  child_allocation.x = clist->internal_allocation.x + 
	    clist->internal_allocation.width -
	    clist->vscrollbar->requisition.width;
	  child_allocation.y = clist->internal_allocation.y;
	  child_allocation.width = clist->vscrollbar->requisition.width;
	  child_allocation.height = clist->internal_allocation.height -
	    (hscrollbar_vis ? (clist->hscrollbar->requisition.height + SCROLLBAR_SPACING (clist)) : 0);

	  gtk_widget_size_allocate (clist->vscrollbar, &child_allocation);
	}
      else
	{
	   if (GTK_WIDGET_VISIBLE (clist->vscrollbar))
		gtk_widget_hide (clist->vscrollbar);
	}

      if (hscrollbar_vis)
	{
	  if (!GTK_WIDGET_VISIBLE (clist->hscrollbar))
	    gtk_widget_show (clist->hscrollbar);
      
	  child_allocation.x = clist->internal_allocation.x;
	  child_allocation.y = clist->internal_allocation.y +
	    clist->internal_allocation.height -
	    clist->hscrollbar->requisition.height;
	  child_allocation.width = clist->internal_allocation.width -
	    (vscrollbar_vis ? (clist->vscrollbar->requisition.width + SCROLLBAR_SPACING (clist)) : 0);
	  child_allocation.height = clist->hscrollbar->requisition.height;

	  gtk_widget_size_allocate (clist->hscrollbar, &child_allocation);
	}
      else
	{
	  if (GTK_WIDGET_VISIBLE (clist->hscrollbar))
		gtk_widget_hide (clist->hscrollbar);
	}
    }

  /* set the vscrollbar adjustments */
  adjust_scrollbars (clist);
}

/* 
 * GTKCONTAINER
 *   gtk_clist_foreach
 */
static void
gtk_clist_foreach (GtkContainer * container,
		   GtkCallback callback,
		   gpointer callback_data)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CLIST (container));
  g_return_if_fail (callback != NULL);

  clist = GTK_CLIST (container);

  /* callback for the column buttons */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      (*callback) (clist->column[i].button, callback_data);

  /* callbacks for the scrollbars */
  if (clist->vscrollbar)
    (*callback) (clist->vscrollbar, callback_data);
  if (clist->hscrollbar)
    (*callback) (clist->hscrollbar, callback_data);
}

/*
 * DRAWING
 *   draw_row
 *   draw_rows
 */
static void
draw_row (GtkCList * clist,
	  GdkRectangle * area,
	  gint row,
	  GtkCListRow * clist_row)
{
  GtkWidget *widget;
  GdkGC *fg_gc, *bg_gc;
  GdkRectangle row_rectangle, cell_rectangle, clip_rectangle, intersect_rectangle,
   *rect;
  gint i, offset = 0, width, height, pixmap_width = 0;
  gint xsrc, ysrc, xdest, ydest;

  g_return_if_fail (clist != NULL);

  /* bail now if we arn't drawable yet */
  if (!GTK_WIDGET_DRAWABLE (clist))
    return;

  if (row < 0 || row >= clist->rows)
    return;

  widget = GTK_WIDGET (clist);

  /* if the function is passed the pointer to the row instead of null,
   * it avoids this expensive lookup */
  if (!clist_row)
    clist_row = (g_list_nth (clist->row_list, row))->data;

  /* rectangle of the entire row */
  row_rectangle.x = 0;
  row_rectangle.y = ROW_TOP_YPIXEL (clist, row);
  row_rectangle.width = clist->clist_window_width;
  row_rectangle.height = clist->row_height;

  /* rectangle of the cell spacing above the row */
  cell_rectangle.x = 0;
  cell_rectangle.y = row_rectangle.y - CELL_SPACING;
  cell_rectangle.width = row_rectangle.width;
  cell_rectangle.height = CELL_SPACING;

  /* rectangle used to clip drawing operations, it's y and height
   * positions only need to be set once, so we set them once here. 
   * the x and width are set withing the drawing loop below once per
   * column */
  clip_rectangle.y = row_rectangle.y;
  clip_rectangle.height = row_rectangle.height;

  /* select GC for background rectangle */
  if (clist_row->state == GTK_STATE_SELECTED)
    {
      fg_gc = widget->style->fg_gc[GTK_STATE_SELECTED];
      bg_gc = widget->style->bg_gc[GTK_STATE_SELECTED];
    }
  else
    {
      if (clist_row->fg_set)
	{
	  gdk_gc_set_foreground (clist->fg_gc, &clist_row->foreground);
	  fg_gc = clist->fg_gc;
	}
      else
	fg_gc = widget->style->fg_gc[GTK_STATE_NORMAL];
	
      if (clist_row->bg_set)
	{
	  gdk_gc_set_foreground (clist->bg_gc, &clist_row->background);
	  bg_gc = clist->bg_gc;
	}
      else
	bg_gc = widget->style->bg_gc[GTK_STATE_PRELIGHT];
    }

  /* draw the cell borders and background */
  if (area)
    {
      if (gdk_rectangle_intersect (area, &cell_rectangle, &intersect_rectangle))
	gdk_draw_rectangle (clist->clist_window,
			    widget->style->white_gc,
			    TRUE,
			    intersect_rectangle.x,
			    intersect_rectangle.y,
			    intersect_rectangle.width,
			    intersect_rectangle.height);

      /* the last row has to clear it's bottom cell spacing too */
      if (clist_row == clist->row_list_end->data)
	{
	  cell_rectangle.y += clist->row_height + CELL_SPACING;

	  if (gdk_rectangle_intersect (area, &cell_rectangle, &intersect_rectangle))
	    gdk_draw_rectangle (clist->clist_window,
				widget->style->white_gc,
				TRUE,
				intersect_rectangle.x,
				intersect_rectangle.y,
				intersect_rectangle.width,
				intersect_rectangle.height);
	}	  

      if (!gdk_rectangle_intersect (area, &row_rectangle, &intersect_rectangle))
	return;

      if (clist_row->state == GTK_STATE_SELECTED || clist_row->fg_set)
	gdk_draw_rectangle (clist->clist_window,
			    bg_gc,
			    TRUE,
			    intersect_rectangle.x,
			    intersect_rectangle.y,
			    intersect_rectangle.width,
			    intersect_rectangle.height);
      else
	gdk_window_clear_area (clist->clist_window,
			       intersect_rectangle.x,
			       intersect_rectangle.y,
			       intersect_rectangle.width,
			       intersect_rectangle.height);
    }
  else
    {
      gdk_draw_rectangle (clist->clist_window,
			  widget->style->white_gc,
			  TRUE,
			  cell_rectangle.x,
			  cell_rectangle.y,
			  cell_rectangle.width,
			  cell_rectangle.height);

      /* the last row has to clear it's bottom cell spacing too */
      if (clist_row == clist->row_list_end->data)
	{
	  cell_rectangle.y += clist->row_height + CELL_SPACING;

	  gdk_draw_rectangle (clist->clist_window,
			      widget->style->white_gc,
			      TRUE,
			      cell_rectangle.x,
			      cell_rectangle.y,
			      cell_rectangle.width,
			      cell_rectangle.height);     
	}	  

      if (clist_row->state == GTK_STATE_SELECTED || clist_row->fg_set)
	gdk_draw_rectangle (clist->clist_window,
			    bg_gc,
			    TRUE,
			    row_rectangle.x,
			    row_rectangle.y,
			    row_rectangle.width,
			    row_rectangle.height);
      else
	gdk_window_clear_area (clist->clist_window,
			       row_rectangle.x,
			       row_rectangle.y,
			       row_rectangle.width,
			       row_rectangle.height);
    }

  /* iterate and draw all the columns (row cells) and draw their contents */
  for (i = 0; i < clist->columns; i++)
    {
      clip_rectangle.x = clist->column[i].area.x + clist->hoffset;
      clip_rectangle.width = clist->column[i].area.width;

      /* calculate clipping region clipping region */
      if (!area)
	{
	  rect = &clip_rectangle;
	}
      else
	{
	  if (!gdk_rectangle_intersect (area, &clip_rectangle, &intersect_rectangle))
	    continue;
	  rect = &intersect_rectangle;
	}

      /* calculate real width for column justification */
      switch (clist_row->cell[i].type)
	{
	case GTK_CELL_EMPTY:
	  continue;
	  break;

	case GTK_CELL_TEXT:
	  width = gdk_string_width (GTK_WIDGET (clist)->style->font,
				    GTK_CELL_TEXT (clist_row->cell[i])->text);
	  break;

	case GTK_CELL_PIXMAP:
	  gdk_window_get_size (GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap, &width, &height);
	  pixmap_width = width;
	  break;

	case GTK_CELL_PIXTEXT:
	  gdk_window_get_size (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap, &width, &height);
	  pixmap_width = width;
	  width += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	  width = gdk_string_width (GTK_WIDGET (clist)->style->font,
				    GTK_CELL_PIXTEXT (clist_row->cell[i])->text);
	  break;

	case GTK_CELL_WIDGET:
	  /* unimplimented */
	  continue;
	  break;

	default:
	  continue;
	  break;
	}

      switch (clist->column[i].justification)
	{
	case GTK_JUSTIFY_LEFT:
	  offset = clip_rectangle.x;
	  break;

	case GTK_JUSTIFY_RIGHT:
	  offset = (clip_rectangle.x + clip_rectangle.width) - width;
	  break;

	case GTK_JUSTIFY_CENTER:
	  offset = (clip_rectangle.x + (clip_rectangle.width / 2)) - (width / 2);
	  break;

	case GTK_JUSTIFY_FILL:
	  offset = (clip_rectangle.x + (clip_rectangle.width / 2)) - (width / 2);
	  break;

	default:
	  offset = 0;
	  break;
	};

      /* Draw Text or Pixmap */
      switch (clist_row->cell[i].type)
	{
	case GTK_CELL_EMPTY:
	  continue;
	  break;

	case GTK_CELL_TEXT:
	  gdk_gc_set_clip_rectangle (fg_gc, rect);

	  gdk_draw_string (clist->clist_window, 
			   widget->style->font,
			   fg_gc,
			   offset + clist_row->cell[i].horizontal,
			   row_rectangle.y + clist->row_center_offset + 
			   clist_row->cell[i].vertical,
			   GTK_CELL_TEXT (clist_row->cell[i])->text);

	  gdk_gc_set_clip_rectangle (fg_gc, NULL);
	  break;

	case GTK_CELL_PIXMAP:
	  xsrc = 0;
	  ysrc = 0;
	  xdest = offset + clist_row->cell[i].horizontal;
	  ydest = (clip_rectangle.y + (clip_rectangle.height / 2)) - height / 2 +
	    clist_row->cell[i].vertical;

	  gdk_gc_set_clip_mask (fg_gc, GTK_CELL_PIXMAP (clist_row->cell[i])->mask);
	  gdk_gc_set_clip_origin (fg_gc, xdest, ydest);

	  gdk_draw_pixmap (clist->clist_window,
			   fg_gc,
			   GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
			   xsrc, ysrc,
			   xdest,
			   ydest,
			   pixmap_width, height);

	  gdk_gc_set_clip_origin (fg_gc, 0, 0);
	  gdk_gc_set_clip_mask (fg_gc, NULL);
	  break;

	case GTK_CELL_PIXTEXT:
	  /* draw the pixmap */
	  xsrc = 0;
	  ysrc = 0;
	  xdest = offset + clist_row->cell[i].horizontal;
	  ydest = (clip_rectangle.y + (clip_rectangle.height / 2)) - height / 2 +
	    clist_row->cell[i].vertical;

	  gdk_gc_set_clip_mask (fg_gc, GTK_CELL_PIXTEXT (clist_row->cell[i])->mask);
	  gdk_gc_set_clip_origin (fg_gc, xdest, ydest);

	  gdk_draw_pixmap (clist->clist_window,
			   fg_gc,
			   GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			   xsrc, ysrc,
			   xdest,
			   ydest,
			   pixmap_width, height);

	  gdk_gc_set_clip_origin (fg_gc, 0, 0);

	  offset += pixmap_width + GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	  
	  /* draw the string */
	  gdk_gc_set_clip_rectangle (fg_gc, rect);

	  gdk_draw_string (clist->clist_window, 
			   widget->style->font,
			   fg_gc,
			   offset + clist_row->cell[i].horizontal,
			   row_rectangle.y + clist->row_center_offset + 
			   clist_row->cell[i].vertical,
			   GTK_CELL_PIXTEXT (clist_row->cell[i])->text);

	  gdk_gc_set_clip_rectangle (fg_gc, NULL);

	  break;

	case GTK_CELL_WIDGET:
	  /* unimplimented */
	  continue;
	  break;

	default:
	  continue;
	  break;
	}
    }
}

static void
draw_rows (GtkCList * clist,
	   GdkRectangle * area)
{
  GList *list;
  GtkCListRow *clist_row;
  int i, first_row, last_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (clist->row_height == 0 ||
      !GTK_WIDGET_DRAWABLE (clist))
    return;

  if (area)
    {
      first_row = ROW_FROM_YPIXEL (clist, area->y);
      last_row = ROW_FROM_YPIXEL (clist, area->y + area->height);
    }
  else
    {
      first_row = ROW_FROM_YPIXEL (clist, 0);
      last_row = ROW_FROM_YPIXEL (clist, clist->clist_window_height);
    }

  /* this is a small special case which exposes the bottom cell line
   * on the last row -- it might go away if I change the wall the cell spacings
   * are drawn */
  if (clist->rows == first_row)
    first_row--;

  list = g_list_nth (clist->row_list, first_row);
  i = first_row;
  while (list)
    {
      clist_row = list->data;
      list = list->next;

      if (i > last_row)
	return;

      draw_row (clist, area, i, clist_row);
      i++;
    }

  if (!area)
    gdk_window_clear_area (clist->clist_window, 0, ROW_TOP_YPIXEL (clist, i), -1, -1);
}

/*
 * SIZE ALLOCATION
 *   size_allocate_title_buttons
 *   size_allocate_columns
 */
static void
size_allocate_title_buttons (GtkCList * clist)
{
  gint i, last_button = 0;
  GtkAllocation button_allocation;

  if (!GTK_WIDGET_REALIZED (clist))
    return;

  button_allocation.x = clist->hoffset;
  button_allocation.y = 0;
  button_allocation.width = 0;
  button_allocation.height = clist->column_title_area.height;

  for (i = 0; i < clist->columns; i++)
    {
      button_allocation.width += clist->column[i].area.width;

      if (i == clist->columns - 1)
	button_allocation.width += 2 * (CELL_SPACING + COLUMN_INSET);
      else
	button_allocation.width += CELL_SPACING + (2 * COLUMN_INSET);

      if (i == (clist->columns - 1) || clist->column[i + 1].button)
	{
	  gtk_widget_size_allocate (clist->column[last_button].button, &button_allocation);
	  button_allocation.x += button_allocation.width;
	  button_allocation.width = 0;

	  gdk_window_show (clist->column[last_button].window);
	  gdk_window_move_resize (clist->column[last_button].window,
				  button_allocation.x - (DRAG_WIDTH / 2), 
				  0, DRAG_WIDTH, clist->column_title_area.height);
	  
	  last_button = i + 1;
	}
      else
	{
	  gdk_window_hide (clist->column[i].window);
	}
    }
}

static void
size_allocate_columns (GtkCList * clist)
{
  gint i, xoffset = 0;

  if (!GTK_WIDGET_REALIZED (clist))
    return;

  for (i = 0; i < clist->columns; i++)
    {
      clist->column[i].area.x = xoffset + CELL_SPACING + COLUMN_INSET;

      if (i == clist->columns - 1)
	{
	  gint width;

	  if (clist->column[i].width_set)
	    width = clist->column[i].width;
	  else
	    width = gdk_string_width (GTK_WIDGET (clist)->style->font, clist->column[i].title);
	  clist->column[i].area.width = MAX (width,
					     clist->clist_window_width -
					     xoffset - (2 * (CELL_SPACING + COLUMN_INSET)));
					    
	}
      else
	{
	  clist->column[i].area.width = clist->column[i].width;
	}

      xoffset += clist->column[i].area.width + CELL_SPACING + (2 * COLUMN_INSET);
    }
}

/*
 * SELECTION
 *   real_select_row
 *   real_unselect_row
 *   get_selection_info
 */
static void
real_select_row (GtkCList * clist,
		 gint row,
		 gint column,
		 GdkEventButton * event)
{
  gint i;
  GList *list;
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  if (row < 0 || row >= clist->rows)
    return;

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
      i = 0;
      list = clist->row_list;
      while (list)
	{
	  clist_row = list->data;
	  list = list->next;

	  if (row == i)
	    {
	      if (clist_row->state == GTK_STATE_SELECTED)
		{
		  clist_row->state = GTK_STATE_NORMAL;
		  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW], 
				   i, column, event);
		}
	      else
		{
		  clist_row->state = GTK_STATE_SELECTED;
		  clist->selection = g_list_append (clist->selection, clist_row);
		}

	      if (!GTK_CLIST_FROZEN (clist) && gtk_clist_row_is_visible (clist, row))
		draw_row (clist, NULL, row, clist_row);
	    }
	  else if (clist_row->state == GTK_STATE_SELECTED)
	    {
	      gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW], 
			       i, column, event);
	    }

	  i++;
	}
      break;

    case GTK_SELECTION_BROWSE:
      i = 0;
      list = clist->row_list;
      while (list)
	{
	  clist_row = list->data;
	  list = list->next;

	  if (row == i)
	    {
	      if (clist_row->state != GTK_STATE_SELECTED)
		{
	      	  clist_row->state = GTK_STATE_SELECTED;
		  clist->selection = g_list_append (clist->selection, clist_row);
		
		  if (!GTK_CLIST_FROZEN (clist) && gtk_clist_row_is_visible (clist, row))
		    draw_row (clist, NULL, row, clist_row);
		}
	    }
	  else if (clist_row->state == GTK_STATE_SELECTED)
	    {
	      gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW], 
			       i, column, event);
	    }

	  i++;
	}
      break;

    case GTK_SELECTION_MULTIPLE:
      i = 0;
      list = clist->row_list;
      while (list)
	{
	  clist_row = list->data;
	  list = list->next;

	  if (row == i)
	    {
	      if (clist_row->state == GTK_STATE_SELECTED)
		{
		  clist_row->state = GTK_STATE_NORMAL;
		  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW], 
				   i, column, event);
		}
	      else
		{
		  clist->selection = g_list_append (clist->selection, clist_row);
		  clist_row->state = GTK_STATE_SELECTED;
		}

	      if (!GTK_CLIST_FROZEN (clist) && gtk_clist_row_is_visible (clist, row))
		draw_row (clist, NULL, row, clist_row);
	    }

	  i++;
	}
      break;

    case GTK_SELECTION_EXTENDED:
      break;

    default:
      break;
    }
}

static void
real_unselect_row (GtkCList * clist,
		   gint row,
		   gint column,
		   GdkEventButton * event)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);

  if (row < 0 || row > (clist->rows - 1))
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  clist_row->state = GTK_STATE_NORMAL;
  clist->selection = g_list_remove (clist->selection, clist_row);

  if (!GTK_CLIST_FROZEN (clist) && gtk_clist_row_is_visible (clist, row))
    draw_row (clist, NULL, row, clist_row);
}

static gint
get_selection_info (GtkCList * clist,
		    gint x,
		    gint y,
		    gint * row,
		    gint * column)
{
  gint trow, tcol;

  g_return_val_if_fail (clist != NULL, 0);

  /* bounds checking, return false if the user clicked 
   * on a blank area */
  trow = ROW_FROM_YPIXEL (clist, y);
  if (trow >= clist->rows)
    return 0;

  if (row)
    *row = trow;

  tcol = COLUMN_FROM_XPIXEL (clist, x);
  if (tcol >= clist->columns)
    return 0;

  if (column)
    *column = tcol;

  return 1;
}

/* 
 * RESIZE COLUMNS
 *   draw_xor_line
 *   new_column_width
 *   resize_column
 */
static void                          
draw_xor_line (GtkCList * clist)
{
  GtkWidget *widget;
  
  g_return_if_fail (clist != NULL);
  
  widget = GTK_WIDGET (clist);

  gdk_draw_line (widget->window, clist->xor_gc,  
                 clist->x_drag,                                       
                 widget->style->klass->ythickness,                               
                 clist->x_drag,                                             
                 clist->column_title_area.height + clist->clist_window_height + 1);
}

/* this function returns the new width of the column being resized given
 * the column and x position of the cursor; the x cursor position is passed
 * in as a pointer and automagicly corrected if it's beyond min/max limits */
static gint
new_column_width (GtkCList * clist,
		  gint column,
		  gint * x,
		  gint * visible)
{
  gint cx, rx, width;

  cx = *x;

  /* first translate the x position from widget->window
   * to clist->clist_window */
  cx -= GTK_WIDGET (clist)->style->klass->xthickness;

  /* rx is x from the list beginning */
  rx = cx - clist->hoffset;

  /* you can't shrink a column to less than its minimum width */
  if (cx < (COLUMN_LEFT_XPIXEL (clist, column) + CELL_SPACING + COLUMN_INSET + COLUMN_MIN_WIDTH))
    {
      *x = cx = COLUMN_LEFT_XPIXEL (clist, column) + CELL_SPACING + COLUMN_INSET + COLUMN_MIN_WIDTH +
	GTK_WIDGET (clist)->style->klass->xthickness;
      cx -= GTK_WIDGET (clist)->style->klass->xthickness;
      rx = cx - clist->hoffset;
    }

  if (cx > clist->clist_window_width)
    *visible = 0;
  else
    *visible = 1;

  /* calculate new column width making sure it doesn't end up
   * less than the minimum width */
  width = (rx - COLUMN_LEFT (clist, column)) - COLUMN_INSET -
    ((clist->columns == (column - 1)) ? CELL_SPACING : 0);
  if (width < COLUMN_MIN_WIDTH)
    width = COLUMN_MIN_WIDTH;

  return width;
}

/* this will do more later */
static void
resize_column (GtkCList * clist,
	       gint column,
	       gint width)
{
  gtk_clist_set_column_width (clist, column, width);
}

/* BUTTONS */
static void
column_button_create (GtkCList * clist,
		      gint column)
{
  GtkWidget *button;

  button = clist->column[column].button = gtk_button_new ();
  gtk_widget_set_parent (button, GTK_WIDGET (clist));
  if (GTK_WIDGET_REALIZED (clist) && clist->title_window)
    gtk_widget_set_parent_window (clist->column[column].button, clist->title_window);
  
  gtk_signal_connect (GTK_OBJECT (button),
		      "clicked",
		      (GtkSignalFunc) column_button_clicked,
		      (gpointer) clist);

  gtk_widget_show (button);
}

static void
column_button_clicked (GtkWidget * widget,
		       gpointer data)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (data));

  clist = GTK_CLIST (data);

  /* find the column who's button was pressed */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button == widget)
      break;

  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[CLICK_COLUMN], i);
}

/* 
 * SCROLLBARS
 *
 * functions:
 *   create_scrollbars
 *   adjust_scrollbars
 *   vadjustment_changed
 *   hadjustment_changed
 *   vadjustment_value_changed
 *   hadjustment_value_changed 
 */
static void
create_scrollbars (GtkCList * clist)
{
  GtkAdjustment *adjustment;

  clist->vscrollbar = gtk_vscrollbar_new (NULL);
  adjustment = gtk_range_get_adjustment (GTK_RANGE (clist->vscrollbar));

  gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
		      (GtkSignalFunc) vadjustment_changed,
		      (gpointer) clist);

  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      (GtkSignalFunc) vadjustment_value_changed,
		      (gpointer) clist);

  gtk_widget_set_parent (clist->vscrollbar, GTK_WIDGET (clist));
  gtk_widget_show (clist->vscrollbar);

  clist->hscrollbar = gtk_hscrollbar_new (NULL);
  adjustment = gtk_range_get_adjustment (GTK_RANGE (clist->hscrollbar));

  gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
		      (GtkSignalFunc) hadjustment_changed,
		      (gpointer) clist);

  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      (GtkSignalFunc) hadjustment_value_changed,
		      (gpointer) clist);

  gtk_widget_set_parent (clist->hscrollbar, GTK_WIDGET (clist));
  gtk_widget_show (clist->hscrollbar);
}

static void
adjust_scrollbars (GtkCList * clist)
{
  GTK_RANGE (clist->vscrollbar)->adjustment->page_size = clist->clist_window_height;
  GTK_RANGE (clist->vscrollbar)->adjustment->page_increment = clist->clist_window_height / 2;
  GTK_RANGE (clist->vscrollbar)->adjustment->step_increment = 10;
  GTK_RANGE (clist->vscrollbar)->adjustment->lower = 0;
  GTK_RANGE (clist->vscrollbar)->adjustment->upper = LIST_HEIGHT (clist);

  if (clist->clist_window_height - clist->voffset > LIST_HEIGHT (clist))
    {
      GTK_RANGE (clist->vscrollbar)->adjustment->value = LIST_HEIGHT (clist) - 
	clist->clist_window_height;
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->vscrollbar)->adjustment), 
			       "value_changed");
    }

  GTK_RANGE (clist->hscrollbar)->adjustment->page_size = clist->clist_window_width;
  GTK_RANGE (clist->hscrollbar)->adjustment->page_increment = clist->clist_window_width / 2;
  GTK_RANGE (clist->hscrollbar)->adjustment->step_increment = 10;
  GTK_RANGE (clist->hscrollbar)->adjustment->lower = 0;
  GTK_RANGE (clist->hscrollbar)->adjustment->upper = LIST_WIDTH (clist);

  if (clist->clist_window_width - clist->hoffset > LIST_WIDTH (clist))
    {
      GTK_RANGE (clist->hscrollbar)->adjustment->value = LIST_WIDTH (clist) - 
	clist->clist_window_width;
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->hscrollbar)->adjustment), 
			       "value_changed");
    }

  if (LIST_HEIGHT (clist) <= clist->clist_window_height &&
      clist->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
    {
      if (GTK_WIDGET_VISIBLE (clist->vscrollbar))
	{
	  gtk_widget_hide (clist->vscrollbar);
	  gtk_widget_queue_resize (GTK_WIDGET (clist));
	}
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (clist->vscrollbar))
	{
	  gtk_widget_show (clist->vscrollbar);
	  gtk_widget_queue_resize (GTK_WIDGET (clist));
	}
    }

  if (LIST_WIDTH (clist) <= clist->clist_window_width &&
      clist->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
    {
      if (GTK_WIDGET_VISIBLE (clist->hscrollbar))
	{
	  gtk_widget_hide (clist->hscrollbar);
	  gtk_widget_queue_resize (GTK_WIDGET (clist));
	}
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (clist->hscrollbar))
	{
	  gtk_widget_show (clist->hscrollbar);
	  gtk_widget_queue_resize (GTK_WIDGET (clist));
	}
    }

  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->vscrollbar)->adjustment), "changed");
  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->hscrollbar)->adjustment), "changed");
}

static void
vadjustment_changed (GtkAdjustment * adjustment,
			       gpointer data)
{
  GtkCList *clist;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  clist = GTK_CLIST (data);
}

static void
hadjustment_changed (GtkAdjustment * adjustment,
			       gpointer data)
{
  GtkCList *clist;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  clist = GTK_CLIST (data);
}

static void
check_exposures (GtkCList *clist)
{
  GdkEvent *event;

  if (!GTK_WIDGET_REALIZED (clist))
    return;

  /* Make sure graphics expose events are processed before scrolling
   * again */
  while ((event = gdk_event_get_graphics_expose (clist->clist_window)) != NULL)
    {
      gtk_widget_event (GTK_WIDGET (clist), event);
      if (event->expose.count == 0)
	{
	  gdk_event_free (event);
	  break;
	}
      gdk_event_free (event);
    }
}

static void
vadjustment_value_changed (GtkAdjustment * adjustment,
				     gpointer data)
{
  GtkCList *clist;
  GdkRectangle area;
  gint diff, value;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_CLIST (data));

  clist = GTK_CLIST (data);

  if (!GTK_WIDGET_DRAWABLE (clist))
    return;

  value = adjustment->value;

  if (adjustment == gtk_range_get_adjustment (GTK_RANGE (clist->vscrollbar)))
    {
      if (value > -clist->voffset)
	{
	  /* scroll down */
	  diff = value + clist->voffset;

	  /* we have to re-draw the whole screen here... */
	  if (diff >= clist->clist_window_height)
	    {
	      clist->voffset = -value;
	      draw_rows (clist, NULL);
	      return;
	    }

	  if ((diff != 0) && (diff != clist->clist_window_height))
	    gdk_window_copy_area (clist->clist_window,
				  clist->fg_gc,
				  0, 0,
				  clist->clist_window,
				  0,
				  diff,
				  clist->clist_window_width,
				  clist->clist_window_height - diff);

	  area.x = 0;
	  area.y = clist->clist_window_height - diff;
	  area.width = clist->clist_window_width;
	  area.height = diff;
	}
      else
	{
	  /* scroll up */
	  diff = -clist->voffset - value;

	  /* we have to re-draw the whole screen here... */
	  if (diff >= clist->clist_window_height)
	    {
	      clist->voffset = -value;
	      draw_rows (clist, NULL);
	      return;
	    }

	  if ((diff != 0) && (diff != clist->clist_window_height))
	    gdk_window_copy_area (clist->clist_window,
				  clist->fg_gc,
				  0, diff,
				  clist->clist_window,
				  0,
				  0,
				  clist->clist_window_width,
				  clist->clist_window_height - diff);

	  area.x = 0;
	  area.y = 0;
	  area.width = clist->clist_window_width;
	  area.height = diff;

	}

      clist->voffset = -value;
      if ((diff != 0) && (diff != clist->clist_window_height))
	check_exposures (clist);
    }

  draw_rows (clist, &area);
}

static void
hadjustment_value_changed (GtkAdjustment * adjustment,
				     gpointer data)
{
  GtkCList *clist;
  GdkRectangle area;
  gint i, diff, value;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_CLIST (data));

  clist = GTK_CLIST (data);

  if (!GTK_WIDGET_DRAWABLE (clist))
    return;

  value = adjustment->value;

  if (adjustment == gtk_range_get_adjustment (GTK_RANGE (clist->hscrollbar)))
    {
      /* move the column buttons and resize windows */
      for (i = 0; i < clist->columns; i++)
	{
	  if (clist->column[i].button)
	    {
	      clist->column[i].button->allocation.x -= value + clist->hoffset;

	      if (clist->column[i].button->window)
		{
		  gdk_window_move (clist->column[i].button->window,
				   clist->column[i].button->allocation.x,
				   clist->column[i].button->allocation.y);

		  if (clist->column[i].window)
		    gdk_window_move (clist->column[i].window,
				     clist->column[i].button->allocation.x +
				     clist->column[i].button->allocation.width - 
				     (DRAG_WIDTH / 2), 0); 
		}
	    }
	}

      if (value > -clist->hoffset)
	{
	  /* scroll right */
	  diff = value + clist->hoffset;

	  /* we have to re-draw the whole screen here... */
	  if (diff >= clist->clist_window_width)
	    {
	      clist->hoffset = -value;
	      draw_rows (clist, NULL);
	      return;
	    }

	  if ((diff != 0) && (diff != clist->clist_window_width))
	    gdk_window_copy_area (clist->clist_window,
				  clist->fg_gc,
				  0, 0,
				  clist->clist_window,
				  diff,
				  0,
				  clist->clist_window_width - diff,
				  clist->clist_window_height);

	  area.x = clist->clist_window_width - diff;
	  area.y = 0;
	  area.width = diff;
	  area.height = clist->clist_window_height;
	}
      else
	{
	  /* scroll left */
	  diff = -clist->hoffset - value;

	  /* we have to re-draw the whole screen here... */
	  if (diff >= clist->clist_window_width)
	    {
	      clist->hoffset = -value;
	      draw_rows (clist, NULL);
	      return;
	    }

	  if ((diff != 0) && (diff != clist->clist_window_width))
	    gdk_window_copy_area (clist->clist_window,
				  clist->fg_gc,
				  diff, 0,
				  clist->clist_window,
				  0,
				  0,
				  clist->clist_window_width - diff,
				  clist->clist_window_height);

	  area.x = 0;
	  area.y = 0;
	  area.width = diff;
	  area.height = clist->clist_window_height;
	}

      clist->hoffset = -value;
      if ((diff != 0) && (diff != clist->clist_window_width))
	check_exposures (clist);
    }

  draw_rows (clist, &area);
}

/* 
 * Memory Allocation/Distruction Routines for GtkCList stuctures
 *
 * functions:
 *   columns_new
 *   column_title_new
 *   columns_delete
 *   row_new
 *   row_delete
 *   cell_empty
 *   cell_set_text
 *   cell_set_pixmap 
 */
static GtkCListColumn *
columns_new (GtkCList * clist)
{
  gint i;
  GtkCListColumn *column;

  column = g_new (GtkCListColumn, clist->columns);

  for (i = 0; i < clist->columns; i++)
    {
      column[i].area.x = 0;
      column[i].area.y = 0;
      column[i].area.width = 0;
      column[i].area.height = 0;
      column[i].title = NULL;
      column[i].button = NULL;
      column[i].window = NULL;
      column[i].width = 0;
      column[i].width_set = FALSE;
      column[i].justification = GTK_JUSTIFY_LEFT;
    }

  return column;
}

static void
column_title_new (GtkCList * clist,
		  gint column,
		  gchar * title)
{
  if (clist->column[column].title)
    g_free (clist->column[column].title);

  clist->column[column].title = g_strdup (title);
}

static void
columns_delete (GtkCList * clist)
{
  gint i;

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].title)
      g_free (clist->column[i].title);
      
  g_free (clist->column);
}

static GtkCListRow *
row_new (GtkCList * clist)
{
  int i;
  GtkCListRow *clist_row;

  clist_row = g_chunk_new (GtkCListRow, clist->row_mem_chunk);
  clist_row->cell = g_chunk_new (GtkCell, clist->cell_mem_chunk);

  for (i = 0; i < clist->columns; i++)
    {
      clist_row->cell[i].type = GTK_CELL_EMPTY;
      clist_row->cell[i].vertical = 0;
      clist_row->cell[i].horizontal = 0;
    }

  clist_row->fg_set = FALSE;
  clist_row->bg_set = FALSE;
  clist_row->state = GTK_STATE_NORMAL;
  clist_row->data = NULL;

  return clist_row;
}

static void
row_delete (GtkCList * clist,
	    GtkCListRow * clist_row)
{
  gint i;

  for (i = 0; i < clist->columns; i++)
    cell_empty (clist, clist_row, i);

  g_mem_chunk_free (clist->cell_mem_chunk, clist_row->cell);
  g_mem_chunk_free (clist->row_mem_chunk, clist_row);
}

static void
cell_empty (GtkCList * clist,
	    GtkCListRow * clist_row,
	    gint column)
{
  switch (clist_row->cell[column].type)
    {
    case GTK_CELL_EMPTY:
      break;
      
    case GTK_CELL_TEXT:
      g_free (GTK_CELL_TEXT (clist_row->cell[column])->text);
      break;
      
    case GTK_CELL_PIXMAP:
      gdk_pixmap_unref (GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap);
      gdk_bitmap_unref (GTK_CELL_PIXMAP (clist_row->cell[column])->mask);
      break;
      
    case GTK_CELL_PIXTEXT:
      g_free (GTK_CELL_PIXTEXT (clist_row->cell[column])->text);
      gdk_pixmap_unref (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap);
      gdk_bitmap_unref (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask);
      break;

    case GTK_CELL_WIDGET:
      /* unimplimented */
      break;
      
    default:
      break;
    }

  clist_row->cell[column].type = GTK_CELL_EMPTY;
}

static void
cell_set_text (GtkCList * clist,
	       GtkCListRow * clist_row,
	       gint column,
	       gchar * text)
{
  cell_empty (clist, clist_row, column);

  if (text)
    {
      clist_row->cell[column].type = GTK_CELL_TEXT;
      GTK_CELL_TEXT (clist_row->cell[column])->text = g_strdup (text);
    }
}

static void
cell_set_pixmap (GtkCList * clist,
		 GtkCListRow * clist_row,
		 gint column,
		 GdkPixmap * pixmap,
		 GdkBitmap * mask)
{
  cell_empty (clist, clist_row, column);

  if (pixmap && mask)
    {
      clist_row->cell[column].type = GTK_CELL_PIXMAP;
      GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap = pixmap;
      GTK_CELL_PIXMAP (clist_row->cell[column])->mask = mask;
    }
}

static void
cell_set_pixtext (GtkCList * clist,
		  GtkCListRow * clist_row,
		  gint column,
		  gchar * text,
		  guint8 spacing,
		  GdkPixmap * pixmap,
		  GdkBitmap * mask)
{
  cell_empty (clist, clist_row, column);

  if (text && pixmap && mask)
    {
      clist_row->cell[column].type = GTK_CELL_PIXTEXT;
      GTK_CELL_PIXTEXT (clist_row->cell[column])->text = g_strdup (text);
      GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing = spacing;
      GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = pixmap;
      GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = mask;
    }
}

/* Fill in data after widget is realized and has style */

static void 
add_style_data (GtkCList * clist)
{
  GtkWidget *widget;

  widget = GTK_WIDGET(clist);

  /* text properties */
  if (!GTK_CLIST_ROW_HEIGHT_SET (clist))
    {
      clist->row_height = widget->style->font->ascent + widget->style->font->descent + 1;
      clist->row_center_offset = widget->style->font->ascent + 1.5;
    }
  else
    {
      gint text_height;
      text_height = clist->row_height - (GTK_WIDGET (clist)->style->font->ascent +
		          GTK_WIDGET (clist) ->style->font->descent + 1);
      clist->row_center_offset = (text_height / 2) + GTK_WIDGET (clist)->style->font->ascent + 1.5;
    }

  /* Column widths */
}
