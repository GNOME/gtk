/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_CLIST_H__
#define __GTK_CLIST_H__

#include <gdk/gdk.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

/* clist flags */
enum                    
{
  CLIST_FROZEN          = 1 << 0,                                     
  CLIST_IN_DRAG         = 1 << 1,                                        
  CLIST_ROW_HEIGHT_SET  = 1 << 2,
  CLIST_SHOW_TITLES     = 1 << 3
}; 

/* cell types */
typedef enum
{
  GTK_CELL_EMPTY,
  GTK_CELL_TEXT,
  GTK_CELL_PIXMAP,
  GTK_CELL_PIXTEXT,
  GTK_CELL_WIDGET
} GtkCellType;

#define GTK_CLIST(obj)          GTK_CHECK_CAST (obj, gtk_clist_get_type (), GtkCList)
#define GTK_CLIST_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_clist_get_type (), GtkCListClass)
#define GTK_IS_CLIST(obj)       GTK_CHECK_TYPE (obj, gtk_clist_get_type ())

#define GTK_CLIST_FLAGS(clist)             (GTK_CLIST (clist)->flags)
#define GTK_CLIST_SET_FLAGS(clist,flag)    (GTK_CLIST_FLAGS (clist) |= (flag))
#define GTK_CLIST_UNSET_FLAGS(clist,flag)  (GTK_CLIST_FLAGS (clist) &= ~(flag))

#define GTK_CLIST_FROZEN(clist)            (GTK_CLIST_FLAGS (clist) & CLIST_FROZEN)
#define GTK_CLIST_IN_DRAG(clist)           (GTK_CLIST_FLAGS (clist) & CLIST_IN_DRAG)
#define GTK_CLIST_ROW_HEIGHT_SET(clist)    (GTK_CLIST_FLAGS (clist) & CLIST_ROW_HEIGHT_SET)
#define GTK_CLIST_SHOW_TITLES(clist)       (GTK_CLIST_FLAGS (clist) & CLIST_SHOW_TITLES)

/* pointer casting for cells */
#define GTK_CELL_TEXT(cell)     (((GtkCellText *) &(cell)))
#define GTK_CELL_PIXMAP(cell)   (((GtkCellPixmap *) &(cell)))
#define GTK_CELL_PIXTEXT(cell)  (((GtkCellPixText *) &(cell)))
#define GTK_CELL_WIDGET(cell)   (((GtkCellWidget *) &(cell)))

typedef struct _GtkCList GtkCList;
typedef struct _GtkCListClass GtkCListClass;
typedef struct _GtkCListColumn GtkCListColumn;
typedef struct _GtkCListRow GtkCListRow;

typedef struct _GtkCell GtkCell;
typedef struct _GtkCellText GtkCellText;
typedef struct _GtkCellPixmap GtkCellPixmap;
typedef struct _GtkCellPixText GtkCellPixText;
typedef struct _GtkCellWidget GtkCellWidget;

struct _GtkCList
{
  GtkContainer container;
  
  guint8 flags;

  /* mem chunks */
  GMemChunk *row_mem_chunk;
  GMemChunk *cell_mem_chunk;

  /* allocation rectangle after the conatiner_border_width
   * and the width of the shadow border */
  GdkRectangle internal_allocation;

  /* rows */
  gint rows;
  gint row_center_offset;
  gint row_height;
  GList *row_list;
  GList *row_list_end;
  
  /* columns */
  gint columns;
  GdkRectangle column_title_area;
  GdkWindow *title_window;
  
  /* dynamicly allocated array of column structures */
  GtkCListColumn *column;
  
  /*the scrolling window and it's height and width to
   * make things a little speedier */
  GdkWindow *clist_window;
  gint clist_window_width;
  gint clist_window_height;
  
  /* offsets for scrolling */
  gint hoffset;
  gint voffset;
  
  /* border shadow style */
  GtkShadowType shadow_type;
  
  /* the list's selection mode (gtkenums.h) */
  GtkSelectionMode selection_mode;

  /* list of selected rows */
  GList *selection;

  /* scrollbars */
  GtkWidget *vscrollbar;
  GtkWidget *hscrollbar;
  guint8 vscrollbar_policy;
  guint8 hscrollbar_policy;

  /* xor GC for the vertical drag line */
  GdkGC *xor_gc;

  /* gc for drawing unselected cells */
  GdkGC *fg_gc;
  GdkGC *bg_gc;

  /* cursor used to indicate dragging */
  GdkCursor *cursor_drag;

  /* the current x-pixel location of the xor-drag line */
  gint x_drag;
};

struct _GtkCListClass
{
  GtkContainerClass parent_class;
  
  void (*select_row) (GtkCList * clist,
		      gint row,
		      gint column,
		      GdkEventButton * event);
  void (*unselect_row) (GtkCList * clist,
			gint row,
			gint column,
			GdkEventButton * event);
  void (*click_column) (GtkCList * clist,
			gint column);

  gint scrollbar_spacing;
};

struct _GtkCListColumn
{
  gchar *title;
  GdkRectangle area;
  
  GtkWidget *button;
  GdkWindow *window;

  gint width;
  GtkJustification justification;

  gint width_set : 1;
};

struct _GtkCListRow
{
  GtkCell *cell;
  GtkStateType state;

  GdkColor foreground;
  GdkColor background;

  gpointer data;
  GtkDestroyNotify destroy;

  gint fg_set : 1;
  gint bg_set : 1;
};

/* Cell Structures */
struct _GtkCellText
{
  GtkCellType type;
  
  gint vertical;
  gint horizontal;
  
  gchar *text;
};

struct _GtkCellPixmap
{
  GtkCellType type;
  
  gint vertical;
  gint horizontal;
  
  GdkPixmap *pixmap;
  GdkBitmap *mask;
};

struct _GtkCellPixText
{
  GtkCellType type;
  
  gint vertical;
  gint horizontal;
  
  gchar *text;
  guint8 spacing;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
};

struct _GtkCellWidget
{
  GtkCellType type;
  
  gint vertical;
  gint horizontal;
  
  GtkWidget *widget;
};

struct _GtkCell
{
  GtkCellType type;

  gint vertical;
  gint horizontal;

  union {
    gchar *text;

    struct {
      GdkPixmap *pixmap;
      GdkBitmap *mask;
    } pm;

    struct {
      gchar *text;
      guint8 spacing;
      GdkPixmap *pixmap;
      GdkBitmap *mask;
    } pt;

    GtkWidget *widget;
  } u;
};

guint gtk_clist_get_type (void);

/* constructers useful for gtk-- wrappers */
void gtk_clist_construct (GtkCList * clist,
			  gint columns,
			  gchar * titles[]);

/* create a new GtkCList */
GtkWidget *gtk_clist_new (gint columns);
GtkWidget *gtk_clist_new_with_titles (gint columns,
				      gchar * titles[]);

/* set the border style of the clist */
void gtk_clist_set_border (GtkCList * clist,
			   GtkShadowType border);

/* set the clist's selection mode */
void gtk_clist_set_selection_mode (GtkCList * clist,
				   GtkSelectionMode mode);

/* set policy on the scrollbar, to either show them all the time
 * or show them only when they are needed, ie., when there is more than one page
 * of information */
void gtk_clist_set_policy (GtkCList * clist,
			   GtkPolicyType vscrollbar_policy,
			   GtkPolicyType hscrollbar_policy);

/* freeze all visual updates of the list, and then thaw the list after you have made
 * a number of changes and the updates wil occure in a more efficent mannor than if
 * you made them on a unfrozen list */
void gtk_clist_freeze (GtkCList * clist);
void gtk_clist_thaw (GtkCList * clist);

/* show and hide the column title buttons */
void gtk_clist_column_titles_show (GtkCList * clist);
void gtk_clist_column_titles_hide (GtkCList * clist);

/* set the column title to be a active title (responds to button presses, 
 * prelights, and grabs keyboard focus), or passive where it acts as just
 * a title */
void gtk_clist_column_title_active (GtkCList * clist,
				     gint column);
void gtk_clist_column_title_passive (GtkCList * clist,
				     gint column);
void gtk_clist_column_titles_active (GtkCList * clist);
void gtk_clist_column_titles_passive (GtkCList * clist);

/* set the title in the column title button */
void gtk_clist_set_column_title (GtkCList * clist,
				 gint column,
				 gchar * title);

/* set a widget instead of a title for the column title button */
void gtk_clist_set_column_widget (GtkCList * clist,
				  gint column,
				  GtkWidget * widget);

/* set the justification on a column */
void gtk_clist_set_column_justification (GtkCList * clist,
					 gint column,
					 GtkJustification justification);

/* set the pixel width of a column; this is a necessary step in
 * creating a CList because otherwise the column width is chozen from
 * the width of the column title, which will never be right */
void gtk_clist_set_column_width (GtkCList * clist,
				 gint column,
				 gint width);

/* change the height of the rows, the default is the hight of the current
 * font */
void gtk_clist_set_row_height (GtkCList * clist,
			       gint height);

/* scroll the viewing area of the list to the given column
 * and row; row_align and col_align are between 0-1 representing the
 * location the row should appear on the screnn, 0.0 being top or left,
 * 1.0 being bottom or right; if row or column is -1 then then there
 * is no change */
void gtk_clist_moveto (GtkCList * clist,
		       gint row,
		       gint column,
		       gfloat row_align,
		       gfloat col_align);

/* returns whether the row is visible */
GtkVisibility gtk_clist_row_is_visible (GtkCList * clist,
					gint row);

/* returns the cell type */
GtkCellType gtk_clist_get_cell_type (GtkCList * clist,
				     gint row,
				     gint column);

/* sets a given cell's text, replacing it's current contents */
void gtk_clist_set_text (GtkCList * clist,
			 gint row,
			 gint column,
			 gchar * text);

/* for the "get" functions, any of the return pointer can be
 * NULL if you are not interested */
gint gtk_clist_get_text (GtkCList * clist,
			 gint row,
			 gint column,
			 gchar ** text);

/* sets a given cell's pixmap, replacing it's current contents */
void gtk_clist_set_pixmap (GtkCList * clist,
			   gint row,
			   gint column,
			   GdkPixmap * pixmap,
			   GdkBitmap * mask);

gint gtk_clist_get_pixmap (GtkCList * clist,
			   gint row,
			   gint column,
			   GdkPixmap ** pixmap,
			   GdkBitmap ** mask);

/* sets a given cell's pixmap and text, replacing it's current contents */
void gtk_clist_set_pixtext (GtkCList * clist,
			    gint row,
			    gint column,
			    gchar * text,
			    guint8 spacing,
			    GdkPixmap * pixmap,
			    GdkBitmap * mask);

gint gtk_clist_get_pixtext (GtkCList * clist,
			    gint row,
			    gint column,
			    gchar ** text,
			    guint8 * spacing,
			    GdkPixmap ** pixmap,
			    GdkBitmap ** mask);

/* sets the foreground color of a row, the colar must already
 * be allocated */
void gtk_clist_set_foreground (GtkCList * clist,
			       gint row,
			       GdkColor * color);

/* sets the background color of a row, the colar must already
 * be allocated */
void gtk_clist_set_background (GtkCList * clist,
			       gint row,
			       GdkColor * color);

/* this sets a horizontal and vertical shift for drawing
 * the contents of a cell; it can be positive or negitive; this is
 * partuculary useful for indenting items in a column */
void gtk_clist_set_shift (GtkCList * clist,
			  gint row,
			  gint column,
			  gint vertical,
			  gint horizontal);

/* append returns the index of the row you just added, making
 * it easier to append and modify a row */
gint gtk_clist_append (GtkCList * clist,
		       gchar * text[]);

/* inserts a row at index row */
void gtk_clist_insert (GtkCList * clist,
		       gint row,
		       gchar * text[]);

/* removes row at index row */
void gtk_clist_remove (GtkCList * clist,
		       gint row);

/* sets a arbitrary data pointer for a given row */
void gtk_clist_set_row_data (GtkCList * clist,
			     gint row,
			     gpointer data);

/* sets a data pointer for a given row with destroy notification */
void gtk_clist_set_row_data_full (GtkCList * clist,
			          gint row,
			          gpointer data,
				  GtkDestroyNotify destroy);

/* returns the data set for a row */
gpointer gtk_clist_get_row_data (GtkCList * clist,
				 gint row);

/* givin a data pointer, find the first (and hopefully only!)
 * row that points to that data, or -1 if none do */
gint gtk_clist_find_row_from_data (GtkCList * clist,
				   gpointer data);

/* force selection of a row */
void gtk_clist_select_row (GtkCList * clist,
			   gint row,
			   gint column);

/* force unselection of a row */
void gtk_clist_unselect_row (GtkCList * clist,
			     gint row,
			     gint column);

/* clear the entire list -- this is much faster than removing each item 
 * with gtk_clist_remove */
void gtk_clist_clear (GtkCList * clist);

/* return the row column corresponding to the x and y coordinates */
gint gtk_clist_get_selection_info (GtkCList * clist,
			     	   gint x,
			     	   gint y,
			     	   gint * row,
			     	   gint * column);

#ifdef __cplusplus
}
#endif				/* __cplusplus */


#endif				/* __GTK_CLIST_H__ */
