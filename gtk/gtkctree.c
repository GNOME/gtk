/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald, 
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>  
 *
 * GtkCTree widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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

#include <stdlib.h>
#include "gtkctree.h"
#include "gtkbindings.h"
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#define PM_SIZE                    8
#define TAB_SIZE                   (PM_SIZE + 6)
#define CELL_SPACING               1
#define CLIST_OPTIMUM_SIZE         512
#define COLUMN_INSET               3
#define DRAG_WIDTH                 6

#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)
#define ROW_FROM_YPIXEL(clist, y)  (((y) - (clist)->voffset) / \
                                    ((clist)->row_height + CELL_SPACING))
#define COLUMN_LEFT_XPIXEL(clist, col)  ((clist)->column[(col)].area.x \
                                    + (clist)->hoffset)
#define COLUMN_LEFT(clist, column) ((clist)->column[(column)].area.x)

#define GTK_CLIST_CLASS_FW(_widget_) GTK_CLIST_CLASS (GTK_OBJECT (_widget_)->klass)


static void gtk_ctree_class_init        (GtkCTreeClass  *klass);
static void gtk_ctree_init              (GtkCTree       *ctree);
static void gtk_ctree_destroy           (GtkObject      *object);

static void gtk_ctree_realize           (GtkWidget      *widget);
static void gtk_ctree_unrealize         (GtkWidget      *widget);
static gint gtk_ctree_button_press      (GtkWidget      *widget,
					 GdkEventButton *event);
static gint gtk_ctree_button_release    (GtkWidget      *widget,
					 GdkEventButton *event);
static gint gtk_ctree_button_motion     (GtkWidget      *widget, 
					 GdkEventMotion *event);
static void draw_row                    (GtkCList       *clist,
					 GdkRectangle   *area,
					 gint            row,
					 GtkCListRow   *clist_row);
static void create_xor_gc               (GtkCTree       *ctree);
static void draw_xor_line               (GtkCTree       *ctree);
static void draw_xor_rect               (GtkCTree       *ctree);
static void create_drag_icon            (GtkCTree       *ctree,
					 GtkCTreeRow    *row);
static void tree_draw_row               (GtkCTree      *ctree,
					 GList         *row);
static void cell_empty                  (GtkCList      *clist,
					 GtkCListRow   *clist_row,
					 gint           column);
static void cell_set_text               (GtkCList      *clist,
					 GtkCListRow   *clist_row,
					 gint           column,
					 gchar         *text);
static void cell_set_pixmap             (GtkCList      *clist,
				   	 GtkCListRow   *clist_row,
					 gint           column,
					 GdkPixmap     *pixmap,
					 GdkBitmap     *mask);
static void cell_set_pixtext            (GtkCList      *clist,
				 	 GtkCListRow   *clist_row,
					 gint           column,
					 gchar         *text,
					 guint8         spacing,
					 GdkPixmap     *pixmap,
					 GdkBitmap     *mask);
static void set_node_info               (GtkCTree      *ctree,
					 GList         *node,
					 gchar         *text,
					 guint8         spacing,
					 GdkPixmap     *pixmap_closed,
					 GdkBitmap     *mask_closed,
					 GdkPixmap     *pixmap_opened,
					 GdkBitmap     *mask_opened,
					 gboolean       is_leaf,
					 gboolean       expanded);
static GtkCTreeRow *row_new             (GtkCTree      *ctree);
static void row_delete                  (GtkCTree      *ctree,
				 	 GtkCTreeRow   *ctree_row);
static void tree_delete                 (GtkCTree      *ctree, 
					 GList         *node, 
					 gpointer       data);
static void tree_delete_row             (GtkCTree      *ctree, 
					 GList         *node, 
					 gpointer       data);
static void real_clear                  (GtkCList      *clist);
static void tree_update_level           (GtkCTree      *ctree, 
					 GList         *node, 
					 gpointer       data);
static void tree_select                 (GtkCTree      *ctree, 
					 GList         *node, 
					 gpointer       data);
static void tree_unselect               (GtkCTree      *ctree, 
					 GList         *node, 
				         gpointer       data);
static void real_select_all             (GtkCList      *clist);
static void real_unselect_all           (GtkCList      *clist);
static void tree_expand                 (GtkCTree      *ctree, 
					 GList         *node,
					 gpointer       data);
static void tree_collapse               (GtkCTree      *ctree, 
					 GList         *node,
					 gpointer       data);
static void tree_toggle_expansion       (GtkCTree      *ctree,
					 GList         *node,
					 gpointer       data);
static void change_focus_row_expansion  (GtkCTree      *ctree,
				         GtkCTreeExpansion expansion);
static void real_select_row             (GtkCList      *clist,
					 gint           row,
					 gint           column,
					 GdkEvent      *event);
static void real_unselect_row           (GtkCList      *clist,
					 gint           row,
					 gint           column,
					 GdkEvent      *event);
static void real_tree_select            (GtkCTree      *ctree,
					 GList         *node,
					 gint           column);
static void real_tree_unselect          (GtkCTree      *ctree,
					 GList         *node,
					 gint           column);
static void tree_toggle_selection       (GtkCTree      *ctree, 
					 GList         *node, 
				         gint           column);
static void real_tree_expand            (GtkCTree      *ctree,
					 GList         *node);
static void real_tree_collapse          (GtkCTree      *ctree,
					 GList         *node);
static void real_tree_move              (GtkCTree      *ctree,
					 GList         *node,
					 GList         *new_parent, 
					 GList         *new_sibling);
static void gtk_ctree_link              (GtkCTree      *ctree,
					 GList         *node,
					 GList         *parent,
					 GList         *sibling,
					 gboolean       update_focus_row);
static void gtk_ctree_unlink            (GtkCTree      *ctree, 
					 GList         *node,
					 gboolean       update_focus_row);
static GList * gtk_ctree_last_visible    (GtkCTree     *ctree,
					  GList        *node);
static void gtk_ctree_marshal_signal_1  (GtkObject     *object,
					 GtkSignalFunc  func,
					 gpointer       func_data,
					 GtkArg        *args);
static void gtk_ctree_marshal_signal_2  (GtkObject     *object,
					 GtkSignalFunc  func,
					 gpointer       func_data,
					 GtkArg        *args);
static void gtk_ctree_marshal_signal_3  (GtkObject     *object,
					 GtkSignalFunc  func,
					 gpointer       func_data,
					 GtkArg        *args);
static void gtk_ctree_marshal_signal_4  (GtkObject     *object,
					 GtkSignalFunc  func,
					 gpointer       func_data,
					 GtkArg        *args);
static gboolean ctree_is_hot_spot       (GtkCTree      *ctree, 
					 GList         *node,
					 gint           row, 
					 gint           x, 
					 gint           y);
static void tree_sort                   (GtkCTree      *ctree,
					 GList         *node,
					 gpointer       data);
static gint default_compare             (GtkCTree      *ctree,
					 const GList   *node1,
					 const GList   *node2);



static void fake_unselect_all           (GtkCList      *clist,
					 gint           row);
static GList*  selection_find           (GtkCList      *clist,
					 gint           row_number,
					 GList         *row_list_element);
static void resync_selection            (GtkCList      *clist,
					 GdkEvent      *event);
static void real_undo_selection         (GtkCList      *clist);
static void select_row_recursive        (GtkCTree      *ctree, 
					 GList         *node, 
					 gpointer       data);




enum
{
  TREE_SELECT_ROW,
  TREE_UNSELECT_ROW,
  TREE_EXPAND,
  TREE_COLLAPSE,
  TREE_MOVE,
  CHANGE_FOCUS_ROW_EXPANSION,
  LAST_SIGNAL
};

typedef void (*GtkCTreeSignal1) (GtkObject *object,
				 GList     *arg1,
				 gint       arg2,
				 gpointer   data);

typedef void (*GtkCTreeSignal2) (GtkObject *object,
				 GList     *arg1,
				 GList     *arg2,
				 GList     *arg3,
				 gpointer   data);

typedef void (*GtkCTreeSignal3) (GtkObject *object,
				 GList     *arg1,
				 gpointer   data);

typedef void (*GtkCTreeSignal4) (GtkObject         *object,
				 GtkCTreeExpansion  arg1,
				 gpointer           data);


static GtkCListClass *parent_class = NULL;
static GtkContainerClass *container_class = NULL;
static guint ctree_signals[LAST_SIGNAL] = {0};


GtkType
gtk_ctree_get_type (void)
{
  static GtkType ctree_type = 0;

  if (!ctree_type)
    {
      GtkTypeInfo ctree_info =
      {
	"GtkCTree",
	sizeof (GtkCTree),
	sizeof (GtkCTreeClass),
	(GtkClassInitFunc) gtk_ctree_class_init,
	(GtkObjectInitFunc) gtk_ctree_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      ctree_type = gtk_type_unique (GTK_TYPE_CLIST, &ctree_info);
    }

  return ctree_type;
}

static void
gtk_ctree_marshal_signal_1 (GtkObject     *object,
			    GtkSignalFunc  func,
			    gpointer       func_data,
			    GtkArg        *args)
{
  GtkCTreeSignal1 rfunc;

  rfunc = (GtkCTreeSignal1) func;

  (*rfunc) (object, GTK_VALUE_POINTER (args[0]), GTK_VALUE_INT (args[1]), 
	    func_data);
}

static void
gtk_ctree_marshal_signal_2 (GtkObject     *object,
			    GtkSignalFunc  func,
			    gpointer       func_data,
			    GtkArg        *args)
{
  GtkCTreeSignal2 rfunc;

  rfunc = (GtkCTreeSignal2) func;

  (*rfunc) (object, GTK_VALUE_POINTER (args[0]), GTK_VALUE_POINTER (args[1]),
	    GTK_VALUE_POINTER (args[2]), func_data);
}

static void
gtk_ctree_marshal_signal_3 (GtkObject     *object,
			    GtkSignalFunc  func,
			    gpointer       func_data,
			    GtkArg        *args)
{
  GtkCTreeSignal3 rfunc;

  rfunc = (GtkCTreeSignal3) func;

  (*rfunc) (object, GTK_VALUE_POINTER (args[0]), func_data);
}

static void
gtk_ctree_marshal_signal_4 (GtkObject     *object,
			    GtkSignalFunc  func,
			    gpointer       func_data,
			    GtkArg        *args)
{
  GtkCTreeSignal4 rfunc;

  rfunc = (GtkCTreeSignal4) func;

  (*rfunc) (object, GTK_VALUE_ENUM (args[0]), func_data);
}

static void
gtk_ctree_class_init (GtkCTreeClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkCListClass *clist_class;

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;
  clist_class = (GtkCListClass *) klass;

  parent_class = gtk_type_class (GTK_TYPE_CLIST);
  container_class = gtk_type_class (GTK_TYPE_CONTAINER);

  ctree_signals[TREE_SELECT_ROW] =
    gtk_signal_new ("tree_select_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_select_row),
		    gtk_ctree_marshal_signal_1,
		    GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_INT);
  ctree_signals[TREE_UNSELECT_ROW] =
    gtk_signal_new ("tree_unselect_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_unselect_row),
		    gtk_ctree_marshal_signal_1,
		    GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_INT);
  ctree_signals[TREE_EXPAND] =
    gtk_signal_new ("tree_expand",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_expand),
		    gtk_ctree_marshal_signal_3,
		    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  ctree_signals[TREE_COLLAPSE] =
    gtk_signal_new ("tree_collapse",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_collapse),
		    gtk_ctree_marshal_signal_3,
		    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  ctree_signals[TREE_MOVE] =
    gtk_signal_new ("tree_move",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_move),
		    gtk_ctree_marshal_signal_2,
		    GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_POINTER, 
		    GTK_TYPE_POINTER);
  ctree_signals[CHANGE_FOCUS_ROW_EXPANSION] =
    gtk_signal_new ("change_focus_row_expansion",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass,
				       change_focus_row_expansion),
		    gtk_ctree_marshal_signal_4,
		    GTK_TYPE_NONE, 1, GTK_TYPE_ENUM);

  gtk_object_class_add_signals (object_class, ctree_signals, LAST_SIGNAL);

  object_class->destroy = gtk_ctree_destroy;

  widget_class->realize = gtk_ctree_realize;
  widget_class->unrealize = gtk_ctree_unrealize;
  widget_class->button_press_event = gtk_ctree_button_press;
  widget_class->button_release_event = gtk_ctree_button_release;
  widget_class->motion_notify_event = gtk_ctree_button_motion;

  clist_class->select_row = real_select_row;
  clist_class->unselect_row = real_unselect_row;
  clist_class->undo_selection = real_undo_selection;
  clist_class->resync_selection = resync_selection;
  clist_class->selection_find = selection_find;
  clist_class->click_column = NULL;
  clist_class->draw_row = draw_row;
  clist_class->clear = real_clear;
  clist_class->select_all = real_select_all;
  clist_class->unselect_all = real_unselect_all;
  clist_class->fake_unselect_all = fake_unselect_all;

  klass->tree_select_row = real_tree_select;
  klass->tree_unselect_row = real_tree_unselect;
  klass->tree_expand = real_tree_expand;
  klass->tree_collapse = real_tree_collapse;
  klass->tree_move = real_tree_move;
  klass->change_focus_row_expansion = change_focus_row_expansion;

  {
    GtkBindingSet *binding_set;

    binding_set = gtk_binding_set_by_class (klass);
    gtk_binding_entry_add_signal (binding_set,
				  '+', GDK_SHIFT_MASK,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_EXPAND);
    gtk_binding_entry_add_signal (binding_set,
				  GDK_KP_Add, 0,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_EXPAND);
    gtk_binding_entry_add_signal (binding_set,
				  GDK_KP_Add, GDK_CONTROL_MASK,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM,
				  GTK_CTREE_EXPANSION_EXPAND_RECURSIVE);
    gtk_binding_entry_add_signal (binding_set,
				  '-', 0,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_COLLAPSE);
    gtk_binding_entry_add_signal (binding_set,
				  GDK_KP_Subtract, 0,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_COLLAPSE);
    gtk_binding_entry_add_signal (binding_set,
				  GDK_KP_Subtract, GDK_CONTROL_MASK,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM,
				  GTK_CTREE_EXPANSION_COLLAPSE_RECURSIVE);
    gtk_binding_entry_add_signal (binding_set,
				  '=', 0,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_TOGGLE);
    gtk_binding_entry_add_signal (binding_set,
				  GDK_KP_Multiply, 0,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_TOGGLE);
    gtk_binding_entry_add_signal (binding_set,
				  GDK_KP_Multiply, GDK_CONTROL_MASK,
				  "change_focus_row_expansion", 1,
				  GTK_TYPE_ENUM,
				  GTK_CTREE_EXPANSION_TOGGLE_RECURSIVE);
  }

}

static void
gtk_ctree_init (GtkCTree *ctree)
{
  ctree->xor_gc         = NULL;
  ctree->drag_icon      = NULL;
  ctree->tree_indent    = 20;
  ctree->tree_column    = 0;
  ctree->drag_row       = -1;
  ctree->drag_source    = NULL;
  ctree->drag_target    = NULL;
  ctree->insert_pos     = GTK_CTREE_POS_AS_CHILD;
  ctree->node_compare   = default_compare;
  ctree->auto_sort      = FALSE;
  ctree->reorderable    = FALSE;
  ctree->use_icons      = TRUE;
  ctree->in_drag        = FALSE;
  ctree->drag_rect      = FALSE;
  ctree->line_style     = GTK_CTREE_LINES_SOLID;
}

static void
gtk_ctree_destroy (GtkObject *object)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CTREE (object));

  clist = GTK_CLIST (object);

  GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);

  gtk_clist_clear (GTK_CLIST (object));

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

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      {
        gtk_widget_unparent (clist->column[i].button);
        clist->column[i].button = NULL;
      }

  if (GTK_OBJECT_CLASS (container_class)->destroy)
    (*GTK_OBJECT_CLASS (container_class)->destroy) (object);
}

static void
gtk_ctree_realize (GtkWidget *widget)
{
  GtkCTree *ctree;
  GdkGCValues values;

  ctree = GTK_CTREE (widget);

  (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  values.foreground = widget->style->fg[GTK_STATE_NORMAL];
  values.background = widget->style->bg[GTK_STATE_NORMAL];
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  values.line_style = GDK_LINE_SOLID;
  ctree->lines_gc = gdk_gc_new_with_values (GTK_CLIST(widget)->clist_window, 
					    &values,
					    GDK_GC_FOREGROUND |
					    GDK_GC_BACKGROUND |
					    GDK_GC_SUBWINDOW |
					    GDK_GC_LINE_STYLE);

  if (ctree->line_style == GTK_CTREE_LINES_DOTTED)
    {
      gdk_gc_set_line_attributes (ctree->lines_gc, 1, 
				  GDK_LINE_ON_OFF_DASH, None, None);
      gdk_gc_set_dashes (ctree->lines_gc, 0, "\1\1", 2);
    }

  if (ctree->reorderable)
    create_xor_gc (ctree);
}

static void
gtk_ctree_unrealize (GtkWidget *widget)
{
  GtkCTree *ctree;

  ctree = GTK_CTREE (widget);

  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

  gdk_gc_destroy (ctree->lines_gc);

  if (ctree->reorderable)
    gdk_gc_destroy (ctree->xor_gc);
}

static gint
gtk_ctree_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkCTree *ctree;
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ctree = GTK_CTREE (widget);
  clist = GTK_CLIST (widget);

  if (event->window == clist->clist_window)
    {
      gboolean collapse_expand = FALSE;
      GList *work;
      gint x;
      gint y;
      gint row;
      gint column;

      x = event->x;
      y = event->y;

      if (!gtk_clist_get_selection_info (clist, x, y, &row, &column))
	return FALSE;

      if (event->button == 2)
	ctree->drag_row = - 1 - ROW_FROM_YPIXEL (clist, y);

      work = g_list_nth (clist->row_list, row);
	  
      if (ctree->reorderable && event->button == 2 && !ctree->in_drag &&
	  clist->anchor == -1)
	{
	  gdk_pointer_grab (event->window, FALSE,
			    GDK_POINTER_MOTION_HINT_MASK |
			    GDK_BUTTON2_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			    NULL, NULL, event->time);
	  ctree->in_drag = TRUE;
	  ctree->drag_source = work;
	  ctree->drag_target = NULL;
	  return FALSE;
	}
      else if (event->button == 1 &&
	       (GTK_CTREE_ROW (work)->children &&
		(event->type == GDK_2BUTTON_PRESS ||
		 ctree_is_hot_spot (ctree, work, row, x, y))))
	{
	  if (GTK_CTREE_ROW (work)->expanded)
	    gtk_ctree_collapse (ctree, work);
	  else
	    gtk_ctree_expand (ctree, work);

	  collapse_expand = TRUE;
	}
      if (event->button == 1)
	{
	  gint old_row = clist->focus_row;
	  gboolean no_focus_row = FALSE;

	  switch (clist->selection_mode)
	    {
	    case GTK_SELECTION_MULTIPLE:
	    case GTK_SELECTION_SINGLE:
	      if (!collapse_expand)
		break;

	      if (clist->focus_row == -1)
		{
		  old_row = row;
		  no_focus_row = TRUE;
		}
		  
	      GTK_CLIST_SET_FLAG (clist, CLIST_DRAG_SELECTION);
	      gdk_pointer_grab (clist->clist_window, FALSE,
				GDK_POINTER_MOTION_HINT_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_BUTTON_RELEASE_MASK,
				NULL, NULL, event->time);

	      if (GTK_CLIST_ADD_MODE (clist))
		{
		  GTK_CLIST_UNSET_FLAG (clist, CLIST_ADD_MODE);
		  if (GTK_WIDGET_HAS_FOCUS (widget))
		    {
		      gtk_widget_draw_focus (widget);
		      gdk_gc_set_line_attributes (clist->xor_gc, 1,
						  GDK_LINE_SOLID, 0, 0);
		      clist->focus_row = row;
		      gtk_widget_draw_focus (widget);
		    }
		  else
		    {
		      gdk_gc_set_line_attributes (clist->xor_gc, 1,
						  GDK_LINE_SOLID, 0, 0);
		      clist->focus_row = row;
		    }
		}
	      else if (row != clist->focus_row)
		{
		  if (GTK_WIDGET_HAS_FOCUS (widget))
		    {
		      gtk_widget_draw_focus (widget);
		      clist->focus_row = row;
		      gtk_widget_draw_focus (widget);
		    }
		  else
		    clist->focus_row = row;
		}

	      if (!GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	      return FALSE;

	    default:
	      break;
	    }
	}
    }
  return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
}

static gint
gtk_ctree_button_motion (GtkWidget      *widget, 
			 GdkEventMotion *event)
{
  GtkCTree *ctree;
  GtkCList *clist;
  gint x;
  gint y;
  gint row;
  gint insert_pos = GTK_CTREE_POS_AS_CHILD;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ctree = GTK_CTREE (widget);
  clist = GTK_CLIST (widget);

  if (GTK_CLIST_IN_DRAG (clist))
    return  GTK_WIDGET_CLASS (parent_class)->motion_notify_event
      (widget, event);

  if (event->window == clist->clist_window && 
      ctree->in_drag && ctree->reorderable)
    {
      GdkModifierType modmask;
      gint root_x;
      gint root_y;

      x = event->x;
      y = event->y;
      if (event->is_hint)
	gdk_window_get_pointer (event->window, &x, &y, NULL);

      /* delayed drag start */
      if (!ctree->drag_target &&
	  y >= ROW_TOP_YPIXEL (clist, -ctree->drag_row-1) &&
	  y <= ROW_TOP_YPIXEL (clist, -ctree->drag_row-1) + clist->row_height)
	return 
	  GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);

      if (ctree->use_icons)
	{
	  if (!ctree->drag_icon)
	    create_drag_icon (ctree, GTK_CTREE_ROW (ctree->drag_source));
	  else
	    {
	      gdk_window_get_pointer (NULL, &root_x, &root_y, &modmask);
	      gdk_window_move (ctree->drag_icon, root_x - ctree->icon_width /2,
			       root_y - ctree->icon_height);
	    }
	}

      /* out of bounds check */
      if (x < 0 || y < -3 || x > clist->clist_window_width ||
	  y > clist->clist_window_height + 3 ||
	  y > ROW_TOP_YPIXEL (clist, clist->rows-1) + clist->row_height + 3)
	{
	  if (ctree->drag_row >= 0)
	    {
	      if (ctree->drag_rect)
		{
		  draw_xor_rect (ctree);
		  ctree->drag_rect = FALSE;
		}
	      else
		draw_xor_line (ctree);
	      ctree->drag_row = -1;
	    }
	  return 
	    (* GTK_WIDGET_CLASS (parent_class)->motion_notify_event) 
	    (widget, event);
	}

      row = ROW_FROM_YPIXEL (clist, y);

      /* re-calculate target (mouse left the window) */
      if (ctree->drag_target && ctree->drag_row == -1)
	ctree->drag_target = g_list_nth (clist->row_list, row);
      
      if (y < 0 || y > clist->clist_window_height || 
	  ROW_TOP_YPIXEL (clist, row + 1) > clist->clist_window_height
	  || row >= clist->rows)
	return GTK_WIDGET_CLASS (parent_class)->motion_notify_event
	  (widget, event);

      if (y - ROW_TOP_YPIXEL (clist, row) < clist->row_height / 4)
	insert_pos = GTK_CTREE_POS_BEFORE;
      else if (ROW_TOP_YPIXEL (clist, row) + clist->row_height - y 
	       < clist->row_height / 4)
	insert_pos = GTK_CTREE_POS_AFTER;

      if (row != ctree->drag_row || 
	  (row == ctree->drag_row && ctree->insert_pos != insert_pos))
	{
	  if (insert_pos != GTK_CTREE_POS_AS_CHILD)
	    {
	      if (ctree->drag_row >= 0)
		{
		  if (ctree->drag_rect)
		    {
		      draw_xor_rect (ctree);
		      ctree->drag_rect = FALSE;
		    }
		  else
		    draw_xor_line (ctree);
		}
	      ctree->insert_pos = insert_pos;
	      ctree->drag_target = g_list_nth (clist->row_list, row);
	      ctree->drag_row = row;
	      draw_xor_line (ctree);
	    }
	  else if (ctree->drag_target &&
		   !GTK_CTREE_ROW (ctree->drag_target)->is_leaf)
	    {
	      if (ctree->drag_row >= 0)
		{
		  if (ctree->drag_rect)
		    draw_xor_rect (ctree);
		  else
		    draw_xor_line (ctree);
		}
	      ctree->drag_rect = TRUE;
	      ctree->insert_pos = insert_pos;
	      ctree->drag_target = g_list_nth (clist->row_list, row);
	      ctree->drag_row = row;
	      draw_xor_rect (ctree);
	    }
	}
    }
  return GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);
}

static gint
gtk_ctree_button_release (GtkWidget      *widget, 
			  GdkEventButton *event)
{
  GtkCTree *ctree;
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ctree = GTK_CTREE (widget);
  clist = GTK_CLIST (widget);

  if (event->button == 2 && clist->anchor == -1)
    {
      gdk_pointer_ungrab (event->time);
      ctree->in_drag = FALSE;

      if (ctree->use_icons && ctree->drag_icon)
	{
	  gdk_window_destroy (ctree->drag_icon);
	  ctree->drag_icon = NULL;
	}

      if (ctree->drag_row >= 0)
	{
	  if (ctree->drag_rect)
	    {
	      draw_xor_rect (ctree);
	      ctree->drag_rect = FALSE;
	    }
	  else
	    draw_xor_line (ctree);
	  ctree->drag_row = -1;
	}

      /* nop if out of bounds / source == target */
      if (event->x < 0 || event->y < -3 ||
	  event->x > clist->clist_window_width ||
	  event->y > clist->clist_window_height + 3 ||
	  ctree->drag_target == ctree->drag_source)
	return GTK_WIDGET_CLASS (parent_class)->button_release_event
	  (widget, event);

      if (!GTK_CTREE_ROW (ctree->drag_source)->children ||
	  !gtk_ctree_is_ancestor (ctree, ctree->drag_source,
				  ctree->drag_target))
	{
	  if (ctree->insert_pos == GTK_CTREE_POS_AFTER)
	    {
	      if (GTK_CTREE_ROW (ctree->drag_target)->sibling != 
		  ctree->drag_source)
		gtk_signal_emit (GTK_OBJECT (ctree), 
				 ctree_signals[TREE_MOVE],
				 ctree->drag_source,
				 GTK_CTREE_ROW (ctree->drag_target)->parent,
				 GTK_CTREE_ROW (ctree->drag_target)->sibling);
	    }
	  else if (ctree->insert_pos == GTK_CTREE_POS_BEFORE)
	    {
	      if (GTK_CTREE_ROW (ctree->drag_source)->sibling != 
		  ctree->drag_target)
		gtk_signal_emit (GTK_OBJECT (ctree), 
				 ctree_signals[TREE_MOVE],
				 ctree->drag_source,
				 GTK_CTREE_ROW (ctree->drag_target)->parent,
				 ctree->drag_target);
	    }
	  else if (!GTK_CTREE_ROW (ctree->drag_target)->is_leaf)
	    {
	      if (GTK_CTREE_ROW (ctree->drag_target)->children !=
		  ctree->drag_source)
		gtk_signal_emit (GTK_OBJECT (ctree), 
				 ctree_signals[TREE_MOVE],
				 ctree->drag_source,
				 ctree->drag_target,
				 GTK_CTREE_ROW (ctree->drag_target)->children);
	    }
	}
      ctree->drag_source = NULL;
      ctree->drag_target = NULL;
    }
  else if (event->button == 1 && GTK_CLIST_DRAG_SELECTION (clist) &&
	   (clist->selection_mode == GTK_SELECTION_SINGLE ||
	    clist->selection_mode == GTK_SELECTION_MULTIPLE))
    {
      gint row;
      gint column;
      GList *work;

      if (gtk_clist_get_selection_info
	  (clist, event->x, event->y, &row, &column))
	{
	  if (clist->anchor == clist->focus_row &&
	      (work = g_list_nth (clist->row_list, row)))
	    tree_toggle_selection (ctree, work, column);
	}
      clist->anchor = -1;
    }
  return GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event);
}

static void
create_drag_icon (GtkCTree    *ctree,
		  GtkCTreeRow *row)
{
  GtkCList *clist;
  GtkWidget *widget;
  GdkWindow *window = NULL;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GdkModifierType modmask;
  gint root_x;
  gint root_y;

  clist  = GTK_CLIST (ctree);
  widget = GTK_WIDGET (ctree);

  if (!(pixmap = GTK_CELL_PIXTEXT (row->row.cell[ctree->tree_column])->pixmap))
    return;
  mask = GTK_CELL_PIXTEXT (row->row.cell[ctree->tree_column])->mask;

  gdk_window_get_pointer (NULL, &root_x, &root_y, &modmask);
  gdk_window_get_size (pixmap, &ctree->icon_width, &ctree->icon_height);

  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.x = root_x - ctree->icon_width / 2;
  attributes.y = root_y - ctree->icon_height;
  attributes.width = ctree->icon_width;
  attributes.height = ctree->icon_height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_back_pixmap (window, pixmap, FALSE);
  if (mask)
    gdk_window_shape_combine_mask (window, mask, 0, 0);
  gdk_window_show (window);

  ctree->drag_icon = window;
}

static void
create_xor_gc (GtkCTree *ctree)
{
  GtkCList *clist;
  GdkGCValues values;

  clist = GTK_CLIST (ctree);

  values.foreground = GTK_WIDGET (clist)->style->bg[GTK_STATE_NORMAL];
  values.function = GDK_XOR;
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  ctree->xor_gc = gdk_gc_new_with_values (clist->clist_window, &values,
					  GDK_GC_FOREGROUND |
					  GDK_GC_FUNCTION |
					  GDK_GC_SUBWINDOW);
  gdk_gc_set_line_attributes (ctree->xor_gc, 1, GDK_LINE_ON_OFF_DASH, 
			      None, None);
  gdk_gc_set_dashes (ctree->xor_gc, 0, "\2\2", 2);
}

static void
draw_xor_line (GtkCTree *ctree)
{
  GtkCList *clist;
  gint level;
  gint y = 0;

  clist = GTK_CLIST (ctree);

  level = GTK_CTREE_ROW (ctree->drag_target)->level;

  if (ctree->insert_pos == GTK_CTREE_POS_AFTER)
    y = ROW_TOP_YPIXEL (clist, ctree->drag_row) + clist->row_height;
  else 
    y = ROW_TOP_YPIXEL (clist, ctree->drag_row) - 1;

  switch (clist->column[ctree->tree_column].justification)
    {
    case GTK_JUSTIFY_CENTER:
    case GTK_JUSTIFY_FILL:
    case GTK_JUSTIFY_LEFT:
      if (ctree->tree_column > 0)
	gdk_draw_line (clist->clist_window, ctree->xor_gc, 
		       COLUMN_LEFT_XPIXEL(clist, 0), y,
		       COLUMN_LEFT_XPIXEL(clist, ctree->tree_column - 1) +
		       clist->column[ctree->tree_column - 1].area.width, y);
      
      gdk_draw_line (clist->clist_window, ctree->xor_gc, 
		     COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) + 
		     ctree->tree_indent * level -
		     (ctree->tree_indent - PM_SIZE) / 2, y,
		     GTK_WIDGET (ctree)->allocation.width, y);
      break;
    case GTK_JUSTIFY_RIGHT:
      if (ctree->tree_column < clist->columns - 1)
	gdk_draw_line (clist->clist_window, ctree->xor_gc, 
		       COLUMN_LEFT_XPIXEL(clist, ctree->tree_column + 1), y,
		       COLUMN_LEFT_XPIXEL(clist, clist->columns - 1) +
		       clist->column[clist->columns - 1].area.width, y);
      
      gdk_draw_line (clist->clist_window, ctree->xor_gc, 
		     0, y, COLUMN_LEFT_XPIXEL(clist, ctree->tree_column)
		     + clist->column[ctree->tree_column].area.width
		     - ctree->tree_indent * level +
		     (ctree->tree_indent - PM_SIZE) / 2, y);
      break;
    }
}

static void
draw_xor_rect (GtkCTree *ctree)
{
  GtkCList *clist;
  GdkPoint points[4];
  guint level;
  gint i;
  gint y;

  clist = GTK_CLIST (ctree);

  level = GTK_CTREE_ROW (ctree->drag_target)->level;

  y = ROW_TOP_YPIXEL (clist, ctree->drag_row) + clist->row_height;

  switch (clist->column[ctree->tree_column].justification)
    {
    case GTK_JUSTIFY_CENTER:
    case GTK_JUSTIFY_FILL:
    case GTK_JUSTIFY_LEFT:
      points[0].x =  COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) + 
	ctree->tree_indent * level - (ctree->tree_indent - PM_SIZE) / 2;
      points[0].y = y;
      points[3].x = points[0].x;
      points[3].y = y - clist->row_height - 1;
      points[1].x = clist->clist_window_width - 1;
      points[1].y = points[0].y;
      points[2].x = points[1].x;
      points[2].y = points[3].y;

      for (i = 0; i < 3; i++)
	gdk_draw_line (clist->clist_window, ctree->xor_gc,
		       points[i].x, points[i].y, points[i+1].x, points[i+1].y);

      if (ctree->tree_column > 0)
	{
	  points[0].x = COLUMN_LEFT_XPIXEL(clist, ctree->tree_column - 1) +
	    clist->column[ctree->tree_column - 1].area.width ;
	  points[0].y = y;
	  points[3].x = points[0].x;
	  points[3].y = y - clist->row_height - 1;
	  points[1].x = 0;
	  points[1].y = points[0].y;
	  points[2].x = 0;
	  points[2].y = points[3].y;

	  for (i = 0; i < 3; i++)
	    gdk_draw_line (clist->clist_window, ctree->xor_gc,
			   points[i].x, points[i].y, points[i+1].x, 
			   points[i+1].y);
	}
      break;
    case GTK_JUSTIFY_RIGHT:
      points[0].x =  COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) - 
	ctree->tree_indent * level + (ctree->tree_indent - PM_SIZE) / 2  +
	clist->column[ctree->tree_column].area.width;
      points[0].y = y;
      points[3].x = points[0].x;
      points[3].y = y - clist->row_height - 1;
      points[1].x = 0;
      points[1].y = points[0].y;
      points[2].x = 0;
      points[2].y = points[3].y;

      for (i = 0; i < 3; i++)
	gdk_draw_line (clist->clist_window, ctree->xor_gc,
		       points[i].x, points[i].y, points[i+1].x, points[i+1].y);

      if (ctree->tree_column < clist->columns - 1)
	{
	  points[0].x = COLUMN_LEFT_XPIXEL(clist, ctree->tree_column + 1);
	  points[0].y = y;
	  points[3].x = points[0].x;
	  points[3].y = y - clist->row_height - 1;
	  points[1].x = clist->clist_window_width - 1;
	  points[1].y = points[0].y;
	  points[2].x = points[1].x;
	  points[2].y = points[3].y;

	  for (i = 0; i < 3; i++)
	    gdk_draw_line (clist->clist_window, ctree->xor_gc,
			   points[i].x, points[i].y, points[i+1].x, 
			   points[i+1].y);
	}
      break;
    }      
}

static void
draw_row (GtkCList     *clist,
	  GdkRectangle *area,
	  gint          row,
	  GtkCListRow  *clist_row)
{
  GtkWidget *widget;
  GtkCTree  *ctree;
  GdkGC *fg_gc; 
  GdkGC *bg_gc;
  GdkRectangle row_rectangle;
  GdkRectangle cell_rectangle; 
  GdkRectangle clip_rectangle;
  GdkRectangle intersect_rectangle;
  GdkRectangle *rect;

  gint i, offset = 0, width, height, pixmap_width = 0, string_width = 0;
  gint xsrc, ysrc, xdest = 0, ydest;
  gboolean need_redraw = TRUE;
  gboolean draw_pixmap = FALSE;

  g_return_if_fail (clist != NULL);

  /* bail now if we arn't drawable yet */
  if (!GTK_WIDGET_DRAWABLE (clist))
    return;

  if (row < 0 || row >= clist->rows)
    return;

  widget = GTK_WIDGET (clist);
  ctree  = GTK_CTREE  (clist);

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
      if (gdk_rectangle_intersect (area, &cell_rectangle, 
				   &intersect_rectangle))
	gdk_draw_rectangle (clist->clist_window,
			    widget->style->base_gc[GTK_STATE_NORMAL],
			    TRUE,
			    intersect_rectangle.x,
			    intersect_rectangle.y,
			    intersect_rectangle.width,
			    intersect_rectangle.height);

      /* the last row has to clear it's bottom cell spacing too */
      if (clist_row == clist->row_list_end->data)
	{
	  cell_rectangle.y += clist->row_height + CELL_SPACING;

	  if (gdk_rectangle_intersect (area, &cell_rectangle, 
				       &intersect_rectangle))
	    gdk_draw_rectangle (clist->clist_window,
				widget->style->base_gc[GTK_STATE_NORMAL],
				TRUE,
				intersect_rectangle.x,
				intersect_rectangle.y,
				intersect_rectangle.width,
				intersect_rectangle.height);
	}

      if (gdk_rectangle_intersect 
	  (area, &row_rectangle, &intersect_rectangle))
	{
	  if (clist_row->state == GTK_STATE_SELECTED || clist_row->bg_set)
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
	need_redraw = FALSE;
    }
  else
    {
      gdk_draw_rectangle (clist->clist_window,
			  widget->style->base_gc[GTK_STATE_NORMAL],
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
			      widget->style->base_gc[GTK_STATE_NORMAL],
			      TRUE,
			      cell_rectangle.x,
			      cell_rectangle.y,
			      cell_rectangle.width,
			      cell_rectangle.height);     
	}	  

      if (clist_row->state == GTK_STATE_SELECTED || clist_row->bg_set)
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
	
      if (!need_redraw && ctree->tree_column != i)
	continue;

      clip_rectangle.x = clist->column[i].area.x + clist->hoffset;
      clip_rectangle.width = clist->column[i].area.width;

      /* calculate clipping region */
      if (i == ctree->tree_column)
	{
	  clip_rectangle.y -= CELL_SPACING;
	  clip_rectangle.height += CELL_SPACING;
	}

      if (i == ctree->tree_column)
	{

	  if (clist_row->state == GTK_STATE_SELECTED)
	    {
	      gdk_gc_set_foreground (ctree->lines_gc, 
				     &GTK_WIDGET (ctree)->style->
				     fg[GTK_STATE_SELECTED]);
	      gdk_gc_set_background (ctree->lines_gc, 
				     &GTK_WIDGET (ctree)->style->
				     bg[GTK_STATE_SELECTED]);
	    }
	  else
	    {
	      gdk_gc_set_foreground (ctree->lines_gc, 
				     &GTK_WIDGET (ctree)->style->
				     fg[GTK_STATE_NORMAL]);
	      if (clist_row->bg_set)
		gdk_gc_set_background (ctree->lines_gc, 
				       &clist_row->background);
	    }

	  if (ctree->line_style == GTK_CTREE_LINES_TABBED)
	    {
	      if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
		{
		  xdest = clip_rectangle.x + clip_rectangle.width - 1 -
		    (((GtkCTreeRow *) clist_row)->level - 1) *
		    ctree->tree_indent;

		  gdk_draw_line (clist->clist_window,
				 ctree->lines_gc, 
				 -1,
				 row_rectangle.y - 1,
				 MAX (xdest - TAB_SIZE, clip_rectangle.x - 1),
				 row_rectangle.y - 1);

		  if (clist_row == clist->row_list_end->data)
		    gdk_draw_line
		      (clist->clist_window,
		       ctree->lines_gc, 
		       -1,
		       row_rectangle.y + clist->row_height,
		       MAX (clip_rectangle.x + clip_rectangle.width -
			    TAB_SIZE - 1 -
			    (((GtkCTreeRow *) clist_row)->level > 1) *
			    MIN (ctree->tree_indent / 2, TAB_SIZE),
			    clip_rectangle.x - 1),
		       row_rectangle.y + clist->row_height);

		  if (clist_row->state != GTK_STATE_SELECTED)
		    gdk_draw_rectangle
		      (clist->clist_window,
		       widget->style->bg_gc[GTK_STATE_PRELIGHT],
		       TRUE,
		       clip_rectangle.x + clip_rectangle.width,
		       row_rectangle.y,
		       CELL_SPACING + COLUMN_INSET,
		       row_rectangle.height);
		}
	      else
		{
		  xdest = clip_rectangle.x +
		    (((GtkCTreeRow *) clist_row)->level - 1) *
		    ctree->tree_indent;
		  
		  gdk_draw_line (clist->clist_window,
				 ctree->lines_gc, 
				 MIN (xdest + TAB_SIZE,
				      clip_rectangle.x + clip_rectangle.width),
				 row_rectangle.y - 1,
				 clist->clist_window_width,
				 row_rectangle.y - 1);

		  if (clist_row == clist->row_list_end->data)
		    gdk_draw_line
		      (clist->clist_window, ctree->lines_gc, 
		       MIN (clip_rectangle.x + TAB_SIZE +
			    (((GtkCTreeRow *) clist_row)->level > 1) *
			    MIN (ctree->tree_indent / 2, TAB_SIZE),
			    clip_rectangle.x + clip_rectangle.width),
		       row_rectangle.y + clist->row_height,
		       clist->clist_window_width,
		       row_rectangle.y + clist->row_height);

		  if (clist_row->state != GTK_STATE_SELECTED)
		    gdk_draw_rectangle
		      (clist->clist_window,
		       widget->style->bg_gc[GTK_STATE_PRELIGHT],
		       TRUE,
		       clip_rectangle.x - CELL_SPACING - COLUMN_INSET,
		       row_rectangle.y,
		       CELL_SPACING + COLUMN_INSET,
		       row_rectangle.height);
		}
	    }
	}

      if (!area)
	{
	  rect = &clip_rectangle;
	}
      else
	{

	  if (!gdk_rectangle_intersect (area, &clip_rectangle, 
					&intersect_rectangle))
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
	  gdk_window_get_size (GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap, 
			       &width, &height);
	  pixmap_width = width;
	  break;

	case GTK_CELL_PIXTEXT:
	  if (i == ctree->tree_column)
	    {
	      string_width = 0;
	      width = 0;

	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		gdk_window_get_size (GTK_CELL_PIXTEXT 
				     (clist_row->cell[i])->pixmap, 
				     &width, &height);

	      pixmap_width = width;
	      width += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;

	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->text)
		string_width += gdk_string_width 
		  (GTK_WIDGET (clist)->style->font,
		   GTK_CELL_PIXTEXT(clist_row->cell[i])->text);
	      
	      width += string_width + 
		((GtkCTreeRow *)clist_row)->level * ctree->tree_indent;
	    }
	  else
	    {
	      gdk_window_get_size (GTK_CELL_PIXTEXT 
				   (clist_row->cell[i])->pixmap, 
				   &width, &height);
	      pixmap_width = width;
	      width += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	      width += gdk_string_width (GTK_WIDGET (clist)->style->font,
					 GTK_CELL_PIXTEXT 
					 (clist_row->cell[i])->text);
	    }
	  break;

	case GTK_CELL_WIDGET:
	  /* unimplemented */
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
	  offset = (clip_rectangle.x + (clip_rectangle.width / 2)) 
	    - (width / 2);
	  break;

	case GTK_JUSTIFY_FILL:
	  offset = (clip_rectangle.x + (clip_rectangle.width / 2)) 
	    - (width / 2);
	  break;

	default:
	  offset = 0;
	  break;
	};

      if (i == ctree->tree_column)
	{
	  GdkGC *cgc;
	  GdkGC *tgc;
	  GdkGC *mbg_gc;
	  GList *work;
	  GList *work2;
	  gint xoffset;
	  gint yoffset;
	  gint xcenter;
	  gint ycenter;
	  gint offset_x;
	  gint offset_y = 0; 
	  gint next_level;
	  gint in;
	  GdkPoint points[6];

	  xsrc = 0;
	  ysrc = 0;

	  yoffset = (clip_rectangle.height - PM_SIZE) / 2;
	  xoffset = (ctree->tree_indent - PM_SIZE) / 2;
	  ycenter = clip_rectangle.y + (clip_rectangle.height / 2);
	  ydest = ycenter - height / 2 + clist_row->cell[i].vertical;

	  gdk_gc_set_clip_origin (fg_gc, 0, 0);
	  gdk_gc_set_clip_rectangle (fg_gc, rect);
	  if (ctree->line_style != GTK_CTREE_LINES_NONE)
	    {
	      gdk_gc_set_clip_origin (ctree->lines_gc, 0, 0);
	      gdk_gc_set_clip_rectangle (ctree->lines_gc, rect);
	    }

	  switch (clist->column[i].justification)
	    {
	    case GTK_JUSTIFY_CENTER:
	    case GTK_JUSTIFY_FILL:
	      offset = clip_rectangle.x;
	    case GTK_JUSTIFY_LEFT:
	      offset_x = 1;
	      xdest = clip_rectangle.x - xoffset +
		(((GtkCTreeRow *) clist_row)->level - 1) * ctree->tree_indent;
	      xcenter = xdest + (ctree->tree_indent / 2);
	  
	      switch (ctree->line_style)
		{
		case GTK_CTREE_LINES_NONE:
		  break;
		case GTK_CTREE_LINES_TABBED:
		  xdest = clip_rectangle.x +
		    (((GtkCTreeRow *) clist_row)->level - 1) *
		    ctree->tree_indent;
		  xcenter = xdest + TAB_SIZE;

		  gdk_gc_set_clip_origin (clist->bg_gc, 0, 0);
		  gdk_gc_set_clip_rectangle (clist->bg_gc, rect);

		  gdk_gc_set_clip_origin 
		    (widget->style->bg_gc[GTK_STATE_PRELIGHT], 0, 0);
		  gdk_gc_set_clip_rectangle 
		    (widget->style->bg_gc[GTK_STATE_PRELIGHT], rect);

		  work = ((GtkCTreeRow *)clist_row)->parent;
		  next_level = ((GtkCTreeRow *)clist_row)->level;

		  if (!(((GtkCTreeRow *)clist_row)->sibling ||
			(((GtkCTreeRow *)clist_row)->children &&
			 ((GtkCTreeRow *)clist_row)->expanded)))
		    {
		      work2 = gtk_ctree_find_glist_ptr
			(ctree, (GtkCTreeRow *) clist_row);

		      if (work2->next)
			next_level = GTK_CTREE_ROW (work2->next)->level;
		      else
			next_level = 0;
		    }

		  while (work)
		    {
		      xcenter -= ctree->tree_indent;

		      if (GTK_CTREE_ROW(work)->row.bg_set)
			{
			  gdk_gc_set_foreground
			    (clist->bg_gc,
			     &(GTK_CTREE_ROW(work)->row.background));
			  mbg_gc = clist->bg_gc;
			}
		      else
			mbg_gc = widget->style->bg_gc[GTK_STATE_PRELIGHT];

		      if (clist_row->state != GTK_STATE_SELECTED)
			gdk_draw_rectangle (clist->clist_window, mbg_gc, TRUE,
					    xcenter, rect->y,
					    ctree->tree_indent, rect->height);

		      if (next_level > GTK_CTREE_ROW (work)->level)
			gdk_draw_line 
			  (clist->clist_window, ctree->lines_gc, 
			   xcenter, rect->y,
			   xcenter, rect->y + rect->height);
		      else
			{
			  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
					 xcenter, clip_rectangle.y,
					 xcenter, ycenter);

			  in = MIN (ctree->tree_indent, 2 * TAB_SIZE);

			  if (clist_row->state != GTK_STATE_SELECTED)
			    {
			      if ((work2 = GTK_CTREE_ROW (work)->parent) &&
				  GTK_CTREE_ROW(work2)->row.bg_set)
				{
				  gdk_gc_set_foreground
				    (clist->bg_gc,
				     &(GTK_CTREE_ROW(work2)->row.background));
				  
				  gdk_draw_rectangle 
				    (clist->clist_window, clist->bg_gc, TRUE,
				     xcenter,
				     ycenter,
				     in / 2 + in % 2,
				     row_rectangle.height / 2 + 1);

				  if (GTK_CTREE_ROW(work)->row.bg_set)
				    gdk_gc_set_foreground
				      (clist->bg_gc,
				       &(GTK_CTREE_ROW(work)->row.background));
				}
			      else 
				gdk_draw_rectangle 
				  (clist->clist_window,
				   widget->style->bg_gc[GTK_STATE_PRELIGHT],
				   TRUE,
				   xcenter,
				   ycenter,
				   in / 2 + in % 2,
				   row_rectangle.height / 2 + 1);

			      gdk_draw_arc (clist->clist_window, mbg_gc,
					    TRUE,
					    xcenter, clip_rectangle.y,
					    in, clist->row_height,
					    180 * 64, 90 * 64);
			    }

			  gdk_draw_arc (clist->clist_window, ctree->lines_gc,
					FALSE,
					xcenter, clip_rectangle.y,
					in, clist->row_height,
					180 * 64, 90 * 64);
			}
		      work = GTK_CTREE_ROW (work)->parent;
		    }

		  if (clist_row->state != GTK_STATE_SELECTED)
		    gdk_draw_rectangle
		      (clist->clist_window,
		       widget->style->bg_gc[GTK_STATE_PRELIGHT], TRUE,
		       clip_rectangle.x, row_rectangle.y,
		       TAB_SIZE, row_rectangle.height);

		  xcenter = xdest + (ctree->tree_indent / 2);
		  
		  if (clist_row->bg_set)
		    gdk_gc_set_foreground 
		      (clist->bg_gc, &clist_row->background);
		  
		  if (((GtkCTreeRow *)clist_row)->is_leaf)
		    {
		      points[0].x = xdest + TAB_SIZE;
		      points[0].y = row_rectangle.y - 1;
		      
		      points[1].x = points[0].x - 4;
		      points[1].y = points[0].y;
		      
		      points[2].x = points[1].x - 2;
		      points[2].y = points[1].y + 3;

		      points[3].x = points[2].x;
		      points[3].y = points[2].y + clist->row_height - 5;

		      points[4].x = points[3].x + 2;
		      points[4].y = points[3].y + 3;

		      points[5].x = points[4].x + 4;
		      points[5].y = points[4].y;

		      if (clist_row->state != GTK_STATE_SELECTED)
			gdk_draw_polygon (clist->clist_window, bg_gc, TRUE,
					  points, 6);

		      gdk_draw_lines (clist->clist_window, ctree->lines_gc,
				      points, 6);
		    }
		  else 
		    {
		      if (clist_row->state != GTK_STATE_SELECTED)
			gdk_draw_arc (clist->clist_window, bg_gc, TRUE,
				      xdest, row_rectangle.y - 1,
				      2 * TAB_SIZE, clist->row_height,
				      90 * 64, 180 * 64);

		      gdk_draw_arc (clist->clist_window, ctree->lines_gc,
				    FALSE,
				    xdest, row_rectangle.y - 1,
				    2 * TAB_SIZE, clist->row_height,
				    90 * 64, 180 * 64);

		    }

		  gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);
		  gdk_gc_set_clip_rectangle (clist->bg_gc, NULL);
		  gdk_gc_set_clip_rectangle 
		    (widget->style->bg_gc[GTK_STATE_PRELIGHT], NULL);
		  
		  break;
		default:
		  xcenter = xdest + (ctree->tree_indent / 2);
		  if (ctree->line_style == GTK_CTREE_LINES_DOTTED)
		    {
		      offset_x += abs ((clip_rectangle.x + clist->hoffset) %
				       2); 
		      offset_y = abs ((clip_rectangle.y + clist->voffset) % 2);
		    }
		  
		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 xcenter, clip_rectangle.y + offset_y, xcenter,
				 (((GtkCTreeRow *)clist_row)->sibling) ?
				 rect->y + rect->height : ycenter);	
		  
		  gdk_draw_line (clist->clist_window, ctree->lines_gc,
				 xcenter + offset_x, ycenter,
				 xcenter + PM_SIZE / 2 + 2, ycenter);
		  
		  work = ((GtkCTreeRow *)clist_row)->parent;
		  while (work)
		    {
		      xcenter -= ctree->tree_indent;
		      if (GTK_CTREE_ROW (work)->sibling)
			gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				       xcenter, clip_rectangle.y + offset_y,
				       xcenter, rect->y + rect->height);
		      work = GTK_CTREE_ROW (work)->parent;
		    }
		  gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);
		  break;
		}
	  
	      if (((GtkCTreeRow *)clist_row)->children)
		{
		  if (clist_row->state == GTK_STATE_SELECTED)
		    {
		      if (clist_row->fg_set)
			tgc = clist->fg_gc;
		      else
			tgc = widget->style->fg_gc[GTK_STATE_NORMAL];
		      cgc = tgc;
		    }
		  else 
		    {
		      cgc = GTK_WIDGET 
			(clist)->style->fg_gc[GTK_STATE_SELECTED];
		      tgc = fg_gc;
		    }
	      
		  gdk_gc_set_clip_rectangle (cgc, rect);

		  switch (ctree->line_style)
		    {
		    case GTK_CTREE_LINES_NONE:
		      if (!((GtkCTreeRow *)clist_row)->expanded)
			{
			  points[0].x = xdest + xoffset + (PM_SIZE+2) / 6 + 2;
			  points[0].y = clip_rectangle.y + yoffset - 1;
			  points[1].x = points[0].x;
			  points[1].y = points[0].y + (PM_SIZE+2);
			  points[2].x = points[0].x + 2 * (PM_SIZE+2) / 3 - 1;
			  points[2].y = points[0].y + (PM_SIZE+2) / 2;
			}
		      else
			{
			  points[0].x = xdest + xoffset;
			  points[0].y = clip_rectangle.y + yoffset 
			    + (PM_SIZE+2) / 6;
			  points[1].x = points[0].x + (PM_SIZE+2);
			  points[1].y = points[0].y;
			  points[2].x = points[0].x + (PM_SIZE+2) / 2;
			  points[2].y = clip_rectangle.y + yoffset +
			    2 * (PM_SIZE+2) / 3;
			}

		      gdk_draw_polygon (clist->clist_window,
					GTK_WIDGET (clist)->style->
					fg_gc[GTK_STATE_SELECTED],
					TRUE, points, 3);
		      gdk_draw_polygon (clist->clist_window, tgc, FALSE, 
					points, 3);
		      break;
		    case GTK_CTREE_LINES_TABBED:
		      xcenter = xdest + PM_SIZE + 2;
		      gdk_draw_arc (clist->clist_window,
				    GTK_WIDGET (clist)->style->
				    fg_gc[GTK_STATE_SELECTED],
				    TRUE,
				    xcenter - PM_SIZE/2,
				    ycenter - PM_SIZE/2,
				    PM_SIZE, PM_SIZE, 0, 360 * 64);

		      gdk_draw_line (clist->clist_window, tgc, 
				     xcenter - 2,
				     ycenter,
				     xcenter + 2,
				     ycenter);
		
		      if (!((GtkCTreeRow *)clist_row)->expanded)
			gdk_draw_line (clist->clist_window, tgc,
				       xcenter, clip_rectangle.y + yoffset + 2,
				       xcenter,
				       clip_rectangle.y + yoffset + PM_SIZE-2);
		      break;
		    default:
		      gdk_draw_rectangle 
			(clist->clist_window,
			 GTK_WIDGET (clist)->style->fg_gc[GTK_STATE_SELECTED],
			 TRUE,
			 xdest + xoffset, 
			 clip_rectangle.y + yoffset,
			 PM_SIZE, PM_SIZE);
		
		      gdk_draw_rectangle (clist->clist_window, tgc, FALSE,
					  xdest + xoffset, 
					  clip_rectangle.y + yoffset,
					  PM_SIZE, PM_SIZE);
		
		      gdk_draw_line (clist->clist_window, tgc, 
				     xdest + xoffset + 2, ycenter, 
				     xdest + xoffset + PM_SIZE - 2, ycenter);
		
		      if (!((GtkCTreeRow *)clist_row)->expanded)
			{
			  xcenter = xdest + (ctree->tree_indent / 2);
			  gdk_draw_line (clist->clist_window, tgc,
					 xcenter,
					 clip_rectangle.y + yoffset + 2,
					 xcenter,
					 clip_rectangle.y + yoffset +
					 PM_SIZE - 2);
			}
		      break;
		    }
		  
		  gdk_gc_set_clip_rectangle (cgc, NULL);
		}
	  
	      xdest += offset - clip_rectangle.x + ctree->tree_indent +
		clist_row->cell[i].horizontal;

	      if (pixmap_width && xdest + pixmap_width >= rect->x && 
		  xdest <= rect->x + rect->width)
		draw_pixmap = TRUE;

	      break;

	    case  GTK_JUSTIFY_RIGHT:
	      offset_x = 0;
	  
	      xdest = clip_rectangle.x + clip_rectangle.width + xoffset - 1 -
		(((GtkCTreeRow *) clist_row)->level - 1) * ctree->tree_indent;
	    
	      switch (ctree->line_style)
		{
		case  GTK_CTREE_LINES_NONE:
		  break;
		case GTK_CTREE_LINES_TABBED:
		  xdest = clip_rectangle.x + clip_rectangle.width - 1
		    - (((GtkCTreeRow *) clist_row)->level - 1)
		    * ctree->tree_indent;
		  xcenter = xdest - TAB_SIZE;

		  gdk_gc_set_clip_origin (clist->bg_gc, 0, 0);
		  gdk_gc_set_clip_rectangle (clist->bg_gc, rect);

		  gdk_gc_set_clip_origin 
		    (widget->style->bg_gc[GTK_STATE_PRELIGHT], 0, 0);
		  gdk_gc_set_clip_rectangle 
		    (widget->style->bg_gc[GTK_STATE_PRELIGHT], rect);

		  work = ((GtkCTreeRow *)clist_row)->parent;
		  next_level = ((GtkCTreeRow *)clist_row)->level;

		  if (!(((GtkCTreeRow *)clist_row)->sibling ||
			(((GtkCTreeRow *)clist_row)->children &&
			 ((GtkCTreeRow *)clist_row)->expanded)))
		    {
		      work2 = gtk_ctree_find_glist_ptr 
			(ctree, (GtkCTreeRow *) clist_row);

		      if (work2->next)
			next_level = GTK_CTREE_ROW (work2->next)->level;
		      else
			next_level = 0;
		    }

		  while (work)
		    {
		      xcenter += ctree->tree_indent;

		      if (GTK_CTREE_ROW(work)->row.bg_set)
			{
			  gdk_gc_set_foreground
			    (clist->bg_gc,
			     &(GTK_CTREE_ROW(work)->row.background));
			  mbg_gc = clist->bg_gc;
			}
		      else
			mbg_gc = widget->style->bg_gc[GTK_STATE_PRELIGHT];

		      if (clist_row->state != GTK_STATE_SELECTED)
			gdk_draw_rectangle (clist->clist_window,
					    mbg_gc, TRUE,
					    xcenter - ctree->tree_indent + 1,
					    rect->y,
					    ctree->tree_indent,
					    rect->height);

		      if (next_level > GTK_CTREE_ROW (work)->level)
			gdk_draw_line 
			  (clist->clist_window, ctree->lines_gc, 
			   xcenter, rect->y,
			   xcenter, rect->y + rect->height);
		      else
			{
			  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
					 xcenter, clip_rectangle.y,
					 xcenter, ycenter);

			  in = MIN (ctree->tree_indent, 2 * TAB_SIZE);

			  if (clist_row->state != GTK_STATE_SELECTED)
			    {
			      if ((work2 = GTK_CTREE_ROW (work)->parent) &&
				  GTK_CTREE_ROW(work2)->row.bg_set)
				{
				  gdk_gc_set_foreground
				    (clist->bg_gc,
				     &(GTK_CTREE_ROW(work2)->row.background));
				  
				  gdk_draw_rectangle 
				    (clist->clist_window, clist->bg_gc, TRUE,
				     xcenter + 1 - in / 2 - in % 2,
				     ycenter,
				     in / 2 + in % 2,
				     row_rectangle.height / 2 + 1);

				  if (GTK_CTREE_ROW(work)->row.bg_set)
				    gdk_gc_set_foreground
				      (clist->bg_gc,
				       &(GTK_CTREE_ROW(work)->row.background));
				}
			      else 
				gdk_draw_rectangle 
				  (clist->clist_window,
				   widget->style->bg_gc[GTK_STATE_PRELIGHT],
				   TRUE,
				   xcenter + 1 - in / 2 - in % 2,
				   ycenter,
				   in / 2 + in % 2,
				   row_rectangle.height / 2 + 1);

			      gdk_draw_arc (clist->clist_window, mbg_gc, TRUE,
					    xcenter - in, clip_rectangle.y,
					    in, clist->row_height,
					    270 * 64, 90 * 64);
			    }

			  gdk_draw_arc (clist->clist_window, ctree->lines_gc,
					FALSE,
					xcenter - in, clip_rectangle.y,
					in, clist->row_height,
					270 * 64, 90 * 64);
			}

		      work = GTK_CTREE_ROW (work)->parent;
		    }

		  if (clist_row->state != GTK_STATE_SELECTED)
		    gdk_draw_rectangle
		      (clist->clist_window,
		       widget->style->bg_gc[GTK_STATE_PRELIGHT], TRUE,
		       xcenter + 1, row_rectangle.y,
		       TAB_SIZE, row_rectangle.height);

		  xcenter = xdest - (ctree->tree_indent / 2);
		  
		  if (clist_row->bg_set)
		    gdk_gc_set_foreground 
		      (clist->bg_gc, &clist_row->background);
		  
		  if (((GtkCTreeRow *)clist_row)->is_leaf)
		    {
		      points[0].x = xdest - TAB_SIZE;
		      points[0].y = row_rectangle.y - 1;
		      
		      points[1].x = points[0].x + 4;
		      points[1].y = points[0].y;
		      
		      points[2].x = points[1].x + 2;
		      points[2].y = points[1].y + 3;

		      points[3].x = points[2].x;
		      points[3].y = points[2].y + clist->row_height - 5;

		      points[4].x = points[3].x - 2;
		      points[4].y = points[3].y + 3;

		      points[5].x = points[4].x - 4;
		      points[5].y = points[4].y;

		      if (clist_row->state != GTK_STATE_SELECTED)
			gdk_draw_polygon (clist->clist_window, bg_gc, TRUE,
					  points, 6);

		      gdk_draw_lines (clist->clist_window, ctree->lines_gc,
				      points, 6);
		    }
		  else 
		    {
		      if (clist_row->state != GTK_STATE_SELECTED)
			gdk_draw_arc (clist->clist_window, bg_gc, TRUE,
				      xdest - 2 * TAB_SIZE,
				      row_rectangle.y - 1,
				      2 * TAB_SIZE, clist->row_height,
				      270 * 64, 180 * 64);

		      gdk_draw_arc (clist->clist_window, ctree->lines_gc,
				    FALSE,
				    xdest - 2 * TAB_SIZE,
				    row_rectangle.y - 1,
				    2 * TAB_SIZE, clist->row_height,
				    270 * 64, 180 * 64);
		    }

		  gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);
		  gdk_gc_set_clip_rectangle (clist->bg_gc, NULL);
		  gdk_gc_set_clip_rectangle 
		    (widget->style->bg_gc[GTK_STATE_PRELIGHT], NULL);
		  
		  break;
		default:
		  xcenter = xdest - (ctree->tree_indent / 2);
		  if (ctree->line_style == GTK_CTREE_LINES_DOTTED)
		    {
		      offset_x += abs ((clip_rectangle.x + clist->hoffset) % 
				       2); 
		      offset_y = abs ((clip_rectangle.y + clist->voffset) % 2);
		    }
		  
		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 xcenter,  clip_rectangle.y + offset_y,
				 xcenter,
				 (((GtkCTreeRow *)clist_row)->sibling) ?
				 rect->y + rect->height : ycenter);
		  
		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 xcenter - offset_x, ycenter, 
				 xcenter - PM_SIZE / 2 - 2, ycenter);
		  
		  work = ((GtkCTreeRow *)clist_row)->parent;
		  while (work)
		    {
		      xcenter += ctree->tree_indent;
		      if (GTK_CTREE_ROW(work)->sibling)
			gdk_draw_line (clist->clist_window, ctree->lines_gc,
				       xcenter, clip_rectangle.y - offset_y,
				       xcenter, rect->y + rect->height);
		      work = GTK_CTREE_ROW (work)->parent;
		    }
		  gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);
		}

	  
	      if (((GtkCTreeRow *)clist_row)->children)
		{
		  if (clist_row->state == GTK_STATE_SELECTED)
		    {
		      if (clist_row->fg_set)
			tgc = clist->fg_gc;
		      else
			tgc = widget->style->fg_gc[GTK_STATE_NORMAL];
		      cgc = tgc;
		    }
		  else 
		    {
		      cgc = 
			GTK_WIDGET(clist)->style->fg_gc[GTK_STATE_SELECTED];
		      tgc = fg_gc;
		    }
	      
		  gdk_gc_set_clip_rectangle (cgc, rect);
	      
		  switch (ctree->line_style)
		    {
		    case GTK_CTREE_LINES_NONE:
		      if (!((GtkCTreeRow *)clist_row)->expanded)
			{
			  points[0].x = xdest - xoffset - (PM_SIZE+2) / 6 - 2;
			  points[0].y = clip_rectangle.y + yoffset - 1;
			  points[1].x = points[0].x;
			  points[1].y = points[0].y + (PM_SIZE+2);
			  points[2].x = points[0].x - 2 * (PM_SIZE+2) / 3 + 1;
			  points[2].y = points[0].y + (PM_SIZE+2) / 2;
			}
		      else
			{
			  points[0].x = xdest - xoffset;
			  points[0].y = clip_rectangle.y + yoffset + 
			    (PM_SIZE+2) / 6;
			  points[1].x = points[0].x - (PM_SIZE+2);
			  points[1].y = points[0].y;
			  points[2].x = points[0].x - (PM_SIZE+2) / 2;
			  points[2].y = clip_rectangle.y + yoffset +
			    2 * (PM_SIZE+2) / 3;
			}

		      gdk_draw_polygon (clist->clist_window,
					GTK_WIDGET (clist)->style->
					fg_gc[GTK_STATE_SELECTED],
					TRUE, points, 3);
		      gdk_draw_polygon (clist->clist_window, tgc, FALSE, 
					points, 3);
		      break;
		    case GTK_CTREE_LINES_TABBED:
		      xcenter = xdest - PM_SIZE - 2;

		      gdk_draw_arc (clist->clist_window,
				    GTK_WIDGET (clist)->style->
				    fg_gc[GTK_STATE_SELECTED],
				    TRUE,
				    xcenter - PM_SIZE/2,
				    ycenter - PM_SIZE/2,
				    PM_SIZE, PM_SIZE, 0, 360 * 64);

		      gdk_draw_line (clist->clist_window, tgc, 
				     xcenter - 2,
				     ycenter,
				     xcenter + 2,
				     ycenter);
		
		      if (!((GtkCTreeRow *)clist_row)->expanded)
			{
			  gdk_draw_line (clist->clist_window, tgc, xcenter,
					 clip_rectangle.y + yoffset + 2,
					 xcenter, clip_rectangle.y + yoffset
					 + PM_SIZE - 2);
			}
		      break;
		    default:
		      gdk_draw_rectangle (clist->clist_window, 
					  GTK_WIDGET(clist)->style->
					  fg_gc[GTK_STATE_SELECTED], TRUE, 
					  xdest - xoffset - PM_SIZE, 
					  clip_rectangle.y + yoffset,
					  PM_SIZE, PM_SIZE);
		      
		      gdk_draw_rectangle (clist->clist_window, tgc, FALSE,
					  xdest - xoffset - PM_SIZE, 
					  clip_rectangle.y + yoffset,
					  PM_SIZE, PM_SIZE);
		  
		      gdk_draw_line (clist->clist_window, tgc, 
				     xdest - xoffset - 2, ycenter,
				     xdest - xoffset - PM_SIZE + 2, ycenter);
		
		      if (!((GtkCTreeRow *)clist_row)->expanded)
			{
			  xcenter = xdest - (ctree->tree_indent / 2);
			  gdk_draw_line (clist->clist_window, tgc, xcenter, 
					 clip_rectangle.y + yoffset + 2,
					 xcenter, clip_rectangle.y + yoffset
					 + PM_SIZE - 2);
			}
		    }
		  gdk_gc_set_clip_rectangle (cgc, NULL);
		}
	      
	      xdest -=  (ctree->tree_indent + pixmap_width 
			 + clist_row->cell[i].horizontal);

	      if (pixmap_width && xdest + pixmap_width >= rect->x && 
		  xdest <= rect->x + rect->width)
		draw_pixmap = TRUE;
	      break;
	    default :
	      break;
	    }

	  if (draw_pixmap)
	    {
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->mask)
		{
		  gdk_gc_set_clip_mask 
		    (fg_gc, GTK_CELL_PIXTEXT (clist_row->cell[i])->mask);
		  gdk_gc_set_clip_origin (fg_gc, xdest, ydest);
		}
	      
	      if (xdest < clip_rectangle.x)
		{
		  xsrc = clip_rectangle.x - xdest;
		  pixmap_width -= xsrc;
		  xdest = clip_rectangle.x;
		}
	      
	      if (xdest + pixmap_width >
		  clip_rectangle.x + clip_rectangle.width)
		pixmap_width =
		  (clip_rectangle.x + clip_rectangle.width) - xdest;
	      
	      if (ydest < clip_rectangle.y)
		{
		  ysrc = clip_rectangle.y - ydest;
		  height -= ysrc;
		  ydest = clip_rectangle.y;
		}
		  
	      if (ydest + height > clip_rectangle.y + clip_rectangle.height)
		height = (clip_rectangle.y + clip_rectangle.height) - ydest;

	      gdk_draw_pixmap (clist->clist_window, fg_gc,
			       GTK_CELL_PIXTEXT 
			       (clist_row->cell[i])->pixmap,
			       xsrc, ysrc, xdest, ydest, 
			       pixmap_width, height);
	    }

	  if (string_width)
	    { 
	      gint delta;
		  
	      if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
		xdest -= (GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing +
			  string_width);
	      else
		xdest += (GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing +
			  pixmap_width);

	      delta = CELL_SPACING - (rect->y - clip_rectangle.y);
	      if (delta > 0)
		{
		  rect->y += delta;
		  rect->height -= delta;
		}
		  
	      gdk_gc_set_clip_rectangle (fg_gc, rect);
	      
	      gdk_draw_string 
		(clist->clist_window, widget->style->font, fg_gc, xdest,
		 row_rectangle.y + clist->row_center_offset +
		 clist_row->cell[i].vertical,
		 GTK_CELL_PIXTEXT (clist_row->cell[i])->text);
	    }
	  gdk_gc_set_clip_rectangle (fg_gc, NULL);
	}
      else
	{
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
	      ydest = (clip_rectangle.y + (clip_rectangle.height / 2)) 
		- height / 2 + clist_row->cell[i].vertical;

	      if (GTK_CELL_PIXMAP (clist_row->cell[i])->mask)
		{
		  gdk_gc_set_clip_mask (fg_gc, GTK_CELL_PIXMAP 
					(clist_row->cell[i])->mask);
		  gdk_gc_set_clip_origin (fg_gc, xdest, ydest);
		}

	      if (xdest < clip_rectangle.x)
		{
		  xsrc = clip_rectangle.x - xdest;
		  pixmap_width -= xsrc;
		  xdest = clip_rectangle.x;
		}
	      
	      if (xdest + pixmap_width > 
		  clip_rectangle.x + clip_rectangle.width)
		pixmap_width = (clip_rectangle.x + clip_rectangle.width) -
		  xdest;
	      
	      if (ydest < clip_rectangle.y)
		{
		  ysrc = clip_rectangle.y - ydest;
		  height -= ysrc;
		  ydest = clip_rectangle.y;
		}

	      if (ydest + height > clip_rectangle.y + clip_rectangle.height)
		height = (clip_rectangle.y + clip_rectangle.height) - ydest;

	      gdk_draw_pixmap (clist->clist_window, fg_gc,
			       GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
			       xsrc, ysrc, xdest, ydest, pixmap_width, height);

	      if (GTK_CELL_PIXMAP (clist_row->cell[i])->mask)
		{
		  gdk_gc_set_clip_origin (fg_gc, 0, 0);
		  gdk_gc_set_clip_mask (fg_gc, NULL);
		}
	      break;

	    case GTK_CELL_PIXTEXT:
	      /* draw the pixmap */
	      xsrc = 0;
	      ysrc = 0;
	      xdest = offset + clist_row->cell[i].horizontal;
	      ydest = (clip_rectangle.y + (clip_rectangle.height / 2)) 
		- height / 2 + clist_row->cell[i].vertical;
	      
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->mask)
		{
		  gdk_gc_set_clip_mask (fg_gc, GTK_CELL_PIXTEXT
					(clist_row->cell[i])->mask);
		  gdk_gc_set_clip_origin (fg_gc, xdest, ydest);
		}

	      if (xdest < clip_rectangle.x)
		{
		  xsrc = clip_rectangle.x - xdest;
		  pixmap_width -= xsrc;
		  xdest = clip_rectangle.x;
		}

	      if (xdest + pixmap_width >
		  clip_rectangle.x + clip_rectangle.width)
		pixmap_width = (clip_rectangle.x + clip_rectangle.width)
		  - xdest;

	      if (ydest < clip_rectangle.y)
		{
		  ysrc = clip_rectangle.y - ydest;
		  height -= ysrc;
		  ydest = clip_rectangle.y;
		}

	      if (ydest + height > clip_rectangle.y + clip_rectangle.height)
		height = (clip_rectangle.y + clip_rectangle.height) - ydest;

	      gdk_draw_pixmap (clist->clist_window, fg_gc,
			       GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			       xsrc, ysrc, xdest, ydest, pixmap_width, height);
	      
	      gdk_gc_set_clip_origin (fg_gc, 0, 0);
	      
	      xdest += pixmap_width + GTK_CELL_PIXTEXT
		(clist_row->cell[i])->spacing;
	  
	      /* draw the string */
	      gdk_gc_set_clip_rectangle (fg_gc, rect);

	      gdk_draw_string (clist->clist_window, widget->style->font, fg_gc,
			       xdest, 
			       row_rectangle.y + clist->row_center_offset + 
			       clist_row->cell[i].vertical,
			       GTK_CELL_PIXTEXT (clist_row->cell[i])->text);
	      
	      gdk_gc_set_clip_rectangle (fg_gc, NULL);
	      
	      break;

	    case GTK_CELL_WIDGET:
	      /* unimplemented */
	      continue;
	      break;

	    default:
	      continue;
	      break;
	    }
	}
    }
  if (clist->focus_row == row && GTK_WIDGET_HAS_FOCUS (widget))
    {
      if (area)
	{
	  if (gdk_rectangle_intersect (area, &row_rectangle,
				       &intersect_rectangle))
	    {
	      gdk_gc_set_clip_rectangle (clist->xor_gc, &intersect_rectangle);
	      gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
				  row_rectangle.x, row_rectangle.y,
				  row_rectangle.width - 1,
				  row_rectangle.height - 1);
	      gdk_gc_set_clip_rectangle (clist->xor_gc, NULL);
	    }
	}
      else
	gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			    row_rectangle.x, row_rectangle.y,
			    row_rectangle.width - 1, row_rectangle.height - 1);
    }
}

static void
tree_draw_row (GtkCTree *ctree, 
	       GList    *row)
{
  GtkCList *clist;
  
  clist = GTK_CLIST (ctree);

  if (!GTK_CLIST_FROZEN (clist) && gtk_ctree_is_visible (ctree, row))
    {
      GList *work;
      gint num = 0;
      
      work = clist->row_list;
      while (work != row)
	{
	  work = work->next;
	  num++;
	}
      if (gtk_clist_row_is_visible (clist, num) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, num,
					      GTK_CLIST_ROW (row));
    }
}

static GList *
gtk_ctree_last_visible (GtkCTree *ctree,
			GList    *node)
{
  GList *work;
  
  if (!node)
    return NULL;

  work = GTK_CTREE_ROW (node)->children;

  if (!work || !GTK_CTREE_ROW (node)->expanded)
    return node;

  while (GTK_CTREE_ROW (work)->sibling)
    work = GTK_CTREE_ROW (work)->sibling;

  return gtk_ctree_last_visible (ctree, work);
}

static void
gtk_ctree_link (GtkCTree *ctree,
		GList    *node,
		GList    *parent,
		GList    *sibling,
		gboolean  update_focus_row)
{
  GtkCList *clist;
  GList *list_end;
  gboolean visible = FALSE;
  gint rows = 0;
  
  g_return_if_fail (!sibling || GTK_CTREE_ROW (sibling)->parent == parent);
  g_return_if_fail (node != NULL);
  g_return_if_fail (node != sibling);
  g_return_if_fail (node != parent);

  clist = GTK_CLIST (ctree);

  if (update_focus_row && clist->selection_mode == GTK_SELECTION_BROWSE)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  for (rows = 1, list_end = node; list_end->next; list_end = list_end->next)
    rows++;

  GTK_CTREE_ROW (node)->parent = parent;
  GTK_CTREE_ROW (node)->sibling = sibling;

  if (!parent || (parent && (gtk_ctree_is_visible (ctree, parent) &&
			     GTK_CTREE_ROW (parent)->expanded)))
    {
      visible = TRUE;
      clist->rows += rows;
    }
      

  if (sibling)
    {
      GList *work;

      if (parent)
	work = GTK_CTREE_ROW (parent)->children;
      else
	work = clist->row_list;
      if (work != sibling)
	{
	  while (GTK_CTREE_ROW (work)->sibling != sibling)
	    work = GTK_CTREE_ROW (work)->sibling;
	  GTK_CTREE_ROW (work)->sibling = node;
	}

      if (sibling == clist->row_list)
	clist->row_list = node;
      if (sibling->prev && sibling->prev->next == sibling)
	sibling->prev->next = node;
      
      node->prev = sibling->prev;
      list_end->next = sibling;
      sibling->prev = list_end;
      if (parent && GTK_CTREE_ROW (parent)->children == sibling)
	GTK_CTREE_ROW (parent)->children = node;
    }
  else
    {
      GList *work;

      if (parent)
	work = GTK_CTREE_ROW (parent)->children;
      else
	work = clist->row_list;

      if (work)
	{
	  /* find sibling */
	  while (GTK_CTREE_ROW (work)->sibling)
	    work = GTK_CTREE_ROW (work)->sibling;
	  GTK_CTREE_ROW (work)->sibling = node;
	  
	  /* find last visible child of sibling */
	  work = gtk_ctree_last_visible (ctree, work);
	  
	  list_end->next = work->next;
	  if (work->next)
	    work->next->prev = list_end;
	  work->next = node;
	  node->prev = work;
	}
      else
	{
	  if (parent)
	    {
	      GTK_CTREE_ROW (parent)->children = node;
	      node->prev = parent;
	      if (GTK_CTREE_ROW (parent)->expanded)
		{
		  list_end->next = parent->next;
		  if (parent->next)
		    parent->next->prev = list_end;
		  parent->next = node;
		}
	      else
		list_end->next = NULL;
	    }
	  else
	    {
	      clist->row_list = node;
	      node->prev = NULL;
	      list_end->next = NULL;
	    }
	}
    }

  gtk_ctree_pre_recursive (ctree, node, tree_update_level, NULL); 

  if (clist->row_list_end == NULL || clist->row_list_end->next == node)
    clist->row_list_end = list_end;

  if (visible && update_focus_row)
    {
      gint pos;
	  
      pos = g_list_position (clist->row_list, node);
  
      if (pos <= clist->focus_row)
	{
	  clist->focus_row += rows;
	  clist->undo_anchor = clist->focus_row;
	}
    }
}

static void
gtk_ctree_unlink (GtkCTree *ctree, 
		  GList    *node,
                  gboolean  update_focus_row)
{
  GtkCList *clist;
  gint rows;
  gint level;
  gint visible;
  GList *work;
  GList *parent;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);
  
  if (update_focus_row && clist->selection_mode == GTK_SELECTION_BROWSE)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  visible = gtk_ctree_is_visible (ctree, node);

  /* clist->row_list_end unlinked ? */
  if (visible && (node->next == NULL ||
		  (GTK_CTREE_ROW (node)->children &&
		   gtk_ctree_is_ancestor (ctree, node, clist->row_list_end))))
    clist->row_list_end = node->prev;

  /* update list */
  rows = 0;
  level = GTK_CTREE_ROW (node)->level;
  work = node->next;
  while (work && GTK_CTREE_ROW (work)->level > level)
    {
      work = work->next;
      rows++;
    }

  if (visible)
    {
      clist->rows -= (rows + 1);

      if (update_focus_row)
	{
	  gint pos;
	  
	  pos = g_list_position (clist->row_list, node);
	  if (pos + rows + 1 < clist->focus_row)
	    clist->focus_row -= (rows + 1);
	  else if (pos <= clist->focus_row)
	    clist->focus_row = pos - 1;
	  clist->undo_anchor = clist->focus_row;
	}
    }


  if (work)
    {
      work->prev->next = NULL;
      work->prev = node->prev;
    }
      

  if (node->prev && node->prev->next == node)
    node->prev->next = work;

  /* update tree */
  parent = GTK_CTREE_ROW (node)->parent;
  if (parent)
    {
      if (GTK_CTREE_ROW (parent)->children == node)
	{
	  GTK_CTREE_ROW (parent)->children = GTK_CTREE_ROW (node)->sibling;
	  if (!GTK_CTREE_ROW (parent)->children && 
	      GTK_CTREE_ROW (parent)->pixmap_closed)
	    {
	      GTK_CTREE_ROW (parent)->expanded = FALSE;
	      GTK_CELL_PIXTEXT 
		(GTK_CTREE_ROW(parent)->row.cell[ctree->tree_column])->pixmap =
		GTK_CTREE_ROW (parent)->pixmap_closed;
	      GTK_CELL_PIXTEXT 
		(GTK_CTREE_ROW (parent)->row.cell[ctree->tree_column])->mask = 
		GTK_CTREE_ROW (parent)->mask_closed;
	    }
	}
      else
	{
	  GList *sibling;

	  sibling = GTK_CTREE_ROW (parent)->children;
	  while (GTK_CTREE_ROW (sibling)->sibling != node)
	    sibling = GTK_CTREE_ROW (sibling)->sibling;
	  GTK_CTREE_ROW (sibling)->sibling = GTK_CTREE_ROW (node)->sibling;
	}
    }
  else
    {
      if (clist->row_list == node)
	clist->row_list = GTK_CTREE_ROW (node)->sibling;
      else
	{
	  GList *sibling;
	  
	  sibling = clist->row_list;
	  while (GTK_CTREE_ROW (sibling)->sibling != node)
	    sibling = GTK_CTREE_ROW (sibling)->sibling;
	  GTK_CTREE_ROW (sibling)->sibling = GTK_CTREE_ROW (node)->sibling;
	}
    }
}

static void
real_tree_move (GtkCTree *ctree,
		GList    *node,
		GList    *new_parent, 
		GList    *new_sibling)
{
  GtkCList *clist;
  GList *work;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (!new_sibling || 
		    GTK_CTREE_ROW (new_sibling)->parent == new_parent);

  if (new_parent && GTK_CTREE_ROW (new_parent)->is_leaf)
    return;

  /* new_parent != child of child */
  for (work = new_parent; work; work = GTK_CTREE_ROW (work)->parent)
    if (work == node)
      return;

  clist = GTK_CLIST (ctree);

  if (clist->selection_mode == GTK_SELECTION_BROWSE)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (ctree->auto_sort)
    {
      if (new_parent == GTK_CTREE_ROW (node)->parent)
	return;
      
      if (new_parent)
	new_sibling = GTK_CTREE_ROW (new_parent)->children;
      else
	new_sibling = clist->row_list;

      while (new_sibling && ctree->node_compare (ctree, node, new_sibling) > 0)
	new_sibling = GTK_CTREE_ROW (new_sibling)->sibling;
    }

  if (new_parent == GTK_CTREE_ROW (node)->parent && 
      new_sibling == GTK_CTREE_ROW (node)->sibling)
    return;

  if (!GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  work = NULL;
  if (gtk_ctree_is_visible (ctree, node) ||
      gtk_ctree_is_visible (ctree, new_sibling))
    work = g_list_nth (clist->row_list, clist->focus_row);
      
  gtk_ctree_unlink (ctree, node, FALSE);
  gtk_ctree_link (ctree, node, new_parent, new_sibling, FALSE);
  
  if (work)
    {
      while (work &&  !gtk_ctree_is_visible (ctree, work))
	work = GTK_CTREE_ROW (work)->parent;
      clist->focus_row = g_list_position (clist->row_list, work);
      clist->undo_anchor = clist->focus_row;
    }

  if (thaw)
    gtk_clist_thaw (clist);
}

static void
change_focus_row_expansion (GtkCTree          *ctree,
			    GtkCTreeExpansion  action)
{
  GtkCList *clist;
  GList *node;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (gdk_pointer_is_grabbed ())
    return;
  
  if (!(node = g_list_nth (clist->row_list, clist->focus_row)) ||
      GTK_CTREE_ROW (node)->is_leaf || !(GTK_CTREE_ROW (node)->children))
    return;

  switch (action)
    {
    case GTK_CTREE_EXPANSION_EXPAND:
      gtk_ctree_expand (ctree, node);
      break;
    case GTK_CTREE_EXPANSION_EXPAND_RECURSIVE:
      gtk_ctree_expand_recursive (ctree, node);
      break;
    case GTK_CTREE_EXPANSION_COLLAPSE:
      gtk_ctree_collapse (ctree, node);
      break;
    case GTK_CTREE_EXPANSION_COLLAPSE_RECURSIVE:
      gtk_ctree_collapse_recursive (ctree, node);
      break;
    case GTK_CTREE_EXPANSION_TOGGLE:
      gtk_ctree_toggle_expansion (ctree, node);
      break;
    case GTK_CTREE_EXPANSION_TOGGLE_RECURSIVE:
      gtk_ctree_toggle_expansion_recursive (ctree, node);
      break;
    }
}

static void 
real_tree_expand (GtkCTree *ctree,
		  GList    *node)
{
  GtkCList *clist;
  GList *work;
  gint level;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || GTK_CTREE_ROW (node)->expanded)
    return;

  clist = GTK_CLIST (ctree);
  
  if (clist->selection_mode == GTK_SELECTION_EXTENDED && clist->anchor >= 0)
    GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);

  GTK_CTREE_ROW (node)->expanded = TRUE;
  level = GTK_CTREE_ROW (node)->level;

  if (GTK_CTREE_ROW (node)->pixmap_opened)
    {
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	GTK_CTREE_ROW (node)->pixmap_opened;
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	GTK_CTREE_ROW (node)->mask_opened;
    }

  work = GTK_CTREE_ROW (node)->children;
  if (work)
    {
      GtkCList *clist;
      gint tmp = 0;
      gint row;

      clist = GTK_CLIST (ctree);

      while (work->next)
	{
	  work = work->next;
	  tmp++;
	}

      work->next = node->next;

      if (node->next)
	node->next->prev = work;
      else
	clist->row_list_end = work;

      node->next = GTK_CTREE_ROW (node)->children;
      
      if (gtk_ctree_is_visible (ctree, node))
	{
	  row = g_list_position (clist->row_list, node);
	  if (row < clist->focus_row)
	    clist->focus_row += tmp + 1;
	  clist->rows += tmp + 1;
	  if (!GTK_CLIST_FROZEN (ctree))
	    gtk_clist_thaw (clist);
	}
    }
}

static void 
real_tree_collapse (GtkCTree *ctree,
		    GList    *node)
{
  GtkCList *clist;
  GList *work;
  gint level;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || !GTK_CTREE_ROW (node)->expanded)
    return;

  clist = GTK_CLIST (ctree);

  if (clist->selection_mode == GTK_SELECTION_EXTENDED && clist->anchor >= 0)
    GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
  
  GTK_CTREE_ROW (node)->expanded = FALSE;
  level = GTK_CTREE_ROW (node)->level;

  if (GTK_CTREE_ROW (node)->pixmap_closed)
    {
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	GTK_CTREE_ROW (node)->pixmap_closed;
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	GTK_CTREE_ROW (node)->mask_closed;
    }

  work = GTK_CTREE_ROW (node)->children;
  if (work)
    {
      GtkCList *clist;
      gint tmp = 0;
      gint row;

      clist = GTK_CLIST (ctree);

      while (work && GTK_CTREE_ROW (work)->level > level)
	{
	  work = work->next;
	  tmp++;
	}

      if (work)
	{
	  node->next = work;
	  work->prev->next = NULL;
	  work->prev = node;
	}
      else
	{
	  node->next = NULL;
	  clist->row_list_end = node;
	}

      if (gtk_ctree_is_visible (ctree, node))
	{
	  row = g_list_position (clist->row_list, node);
	  if (row < clist->focus_row)
	    clist->focus_row -= tmp;
	  clist->rows -= tmp;
	  if (!GTK_CLIST_FROZEN (ctree))
	    gtk_clist_thaw (clist);
	}
    }
}

static void
cell_set_text (GtkCList    *clist,
	       GtkCListRow *clist_row,
	       gint         column,
	       gchar       *text)
{
  cell_empty (clist, clist_row, column);

  if (text)
    {
      clist_row->cell[column].type = GTK_CELL_TEXT;
      GTK_CELL_TEXT (clist_row->cell[column])->text = g_strdup (text);
    }
}

static void
cell_set_pixmap (GtkCList    *clist,
		 GtkCListRow *clist_row,
		 gint         column,
		 GdkPixmap   *pixmap,
		 GdkBitmap   *mask)
{
  cell_empty (clist, clist_row, column);

  if (pixmap)
    {
      clist_row->cell[column].type = GTK_CELL_PIXMAP;
      GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap = pixmap;
      GTK_CELL_PIXMAP (clist_row->cell[column])->mask = mask;
    }
}

static void
cell_set_pixtext (GtkCList    *clist,
		  GtkCListRow *clist_row,
		  gint         column,
		  gchar       *text,
		  guint8       spacing,
		  GdkPixmap   *pixmap,
		  GdkBitmap   *mask)
{
  cell_empty (clist, clist_row, column);

  if (text && pixmap)
    {
      clist_row->cell[column].type = GTK_CELL_PIXTEXT;
      GTK_CELL_PIXTEXT (clist_row->cell[column])->text = g_strdup (text);
      GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing = spacing;
      GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = pixmap;
      GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = mask;
    }
}

static void 
set_node_info (GtkCTree  *ctree,
	       GList     *node,
	       gchar     *text,
	       guint8     spacing,
	       GdkPixmap *pixmap_closed,
	       GdkBitmap *mask_closed,
	       GdkPixmap *pixmap_opened,
	       GdkBitmap *mask_opened,
	       gboolean   is_leaf,
	       gboolean   expanded)
{
  GtkCellPixText *tree_cell;

  if (GTK_CTREE_ROW (node)->pixmap_opened)
    {
      gdk_pixmap_unref (GTK_CTREE_ROW (node)->pixmap_opened);
      if (GTK_CTREE_ROW (node)->mask_opened) 
	gdk_bitmap_unref (GTK_CTREE_ROW (node)->mask_opened);
    }
  if (GTK_CTREE_ROW (node)->pixmap_closed)
    {
      gdk_pixmap_unref (GTK_CTREE_ROW (node)->pixmap_closed);
      if (GTK_CTREE_ROW (node)->mask_closed) 
	gdk_bitmap_unref (GTK_CTREE_ROW (node)->mask_closed);
    }

  GTK_CTREE_ROW (node)->pixmap_opened = NULL;
  GTK_CTREE_ROW (node)->mask_opened   = NULL;
  GTK_CTREE_ROW (node)->pixmap_closed = NULL;
  GTK_CTREE_ROW (node)->mask_closed   = NULL;

  if (pixmap_closed)
    {
      GTK_CTREE_ROW (node)->pixmap_closed = gdk_pixmap_ref (pixmap_closed);
      if (mask_closed) 
	GTK_CTREE_ROW (node)->mask_closed = gdk_bitmap_ref (mask_closed);
    }
  if (pixmap_opened)
    {
      GTK_CTREE_ROW (node)->pixmap_opened = gdk_pixmap_ref (pixmap_opened);
      if (mask_opened) 
	GTK_CTREE_ROW (node)->mask_opened = gdk_bitmap_ref (mask_opened);
    }

  GTK_CTREE_ROW (node)->is_leaf  = is_leaf;
  GTK_CTREE_ROW (node)->expanded = (is_leaf) ? FALSE : expanded;

  GTK_CTREE_ROW (node)->row.cell[ctree->tree_column].type = GTK_CELL_PIXTEXT;

  tree_cell = GTK_CELL_PIXTEXT (GTK_CTREE_ROW 
				(node)->row.cell[ctree->tree_column]);

  if (tree_cell->text)
    g_free (tree_cell->text);

  tree_cell->text = g_strdup (text);
  tree_cell->spacing = spacing;

  if (expanded)
    {
      tree_cell->pixmap = pixmap_opened;
      tree_cell->mask   = mask_opened;
    }
  else 
    {
      tree_cell->pixmap = pixmap_closed;
      tree_cell->mask   = mask_closed;
    }
}

static void
tree_delete (GtkCTree *ctree, 
	     GList    *node, 
	     gpointer  data)
{
  GtkCList *clist;
  
  clist = GTK_CLIST (ctree);
  
  if (GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
    {
      GList *work;

      work = g_list_find (clist->selection, node);
      if (work)
	{
	  if (clist->selection_end && clist->selection_end == work)
	    clist->selection_end = clist->selection_end->prev;
	  clist->selection = g_list_remove (clist->selection, node);
	}
    }

  row_delete (ctree, GTK_CTREE_ROW (node));
  g_list_free_1 (node);
}

static void
tree_delete_row (GtkCTree *ctree, 
		 GList    *node, 
		 gpointer  data)
{
  row_delete (ctree, GTK_CTREE_ROW (node));
  g_list_free_1 (node);
}

static void
tree_update_level (GtkCTree *ctree, 
		   GList    *node, 
		   gpointer  data)
{
  if (!node)
    return;

  if (GTK_CTREE_ROW (node)->parent)
      GTK_CTREE_ROW (node)->level = 
	GTK_CTREE_ROW (GTK_CTREE_ROW (node)->parent)->level + 1;
  else
      GTK_CTREE_ROW (node)->level = 1;
}

static void
tree_select (GtkCTree *ctree, 
	     GList    *node, 
	     gpointer  data)
{
  if (node && GTK_CTREE_ROW (node)->row.state != GTK_STATE_SELECTED)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW], 
		     node, data);
}

static void
tree_unselect (GtkCTree *ctree, 
	       GList    *node, 
	       gpointer  data)
{
  if (node && GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_UNSELECT_ROW], 
		     node, data);
}

static void
tree_expand (GtkCTree *ctree, 
	     GList    *node, 
	     gpointer  data)
{
  if (node && !GTK_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node,
		     data);
}

static void
tree_collapse (GtkCTree *ctree, 
	       GList    *node, 
	       gpointer  data)
{
  if (node && GTK_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node,
		     data);
}

static void
tree_toggle_expansion (GtkCTree *ctree,
		       GList    *node,
		       gpointer  data)
{
  if (!node)
    return;

  if (GTK_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node,
		     data);
  else
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node,
		     data);
}

static GtkCTreeRow *
row_new (GtkCTree *ctree)
{
  GtkCList *clist;
  GtkCTreeRow *ctree_row;
  int i;

  clist = GTK_CLIST (ctree);
  ctree_row = g_chunk_new (GtkCTreeRow, clist->row_mem_chunk);
  ctree_row->row.cell = g_chunk_new (GtkCell, clist->cell_mem_chunk);

  for (i = 0; i < clist->columns; i++)
    {
      ctree_row->row.cell[i].type = GTK_CELL_EMPTY;
      ctree_row->row.cell[i].vertical = 0;
      ctree_row->row.cell[i].horizontal = 0;
    }

  GTK_CELL_PIXTEXT (ctree_row->row.cell[ctree->tree_column])->text = NULL;

  ctree_row->row.fg_set  = FALSE;
  ctree_row->row.bg_set  = FALSE;
  ctree_row->row.state   = GTK_STATE_NORMAL;
  ctree_row->row.data    = NULL;
  ctree_row->row.destroy = NULL;

  ctree_row->level         = 0;
  ctree_row->expanded      = FALSE;
  ctree_row->parent        = NULL;
  ctree_row->sibling       = NULL;
  ctree_row->children      = NULL;
  ctree_row->pixmap_closed = NULL;
  ctree_row->mask_closed   = NULL;
  ctree_row->pixmap_opened = NULL;
  ctree_row->mask_opened   = NULL;
  
  return ctree_row;
}

static void
row_delete (GtkCTree    *ctree,
	    GtkCTreeRow *ctree_row)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (ctree);

  for (i = 0; i < clist->columns; i++)
    cell_empty (clist, &(ctree_row->row), i);

  if (ctree_row->pixmap_closed)
    {
      gdk_pixmap_unref (ctree_row->pixmap_closed);
      if (ctree_row->mask_closed)
	gdk_bitmap_unref (ctree_row->mask_closed);
    }

  if (ctree_row->pixmap_opened)
    {
      gdk_pixmap_unref (ctree_row->pixmap_opened);
      if (ctree_row->mask_opened)
	gdk_bitmap_unref (ctree_row->mask_opened);
    }

  if (ctree_row->row.destroy)
    ctree_row->row.destroy (ctree_row->row.data);

  g_mem_chunk_free (clist->cell_mem_chunk, ctree_row->row.cell);
  g_mem_chunk_free (clist->row_mem_chunk, ctree_row);
}

static void
cell_empty (GtkCList    *clist,
	    GtkCListRow *clist_row,
	    gint         column)
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
      if (GTK_CELL_PIXMAP (clist_row->cell[column])->mask)
          gdk_bitmap_unref (GTK_CELL_PIXMAP (clist_row->cell[column])->mask);
      break;
      
    case GTK_CELL_PIXTEXT:
      if (GTK_CTREE (clist)->tree_column == column)
	{
	  if (GTK_CELL_PIXTEXT (clist_row->cell[column])->text)
	    g_free (GTK_CELL_PIXTEXT (clist_row->cell[column])->text);
	  break;
	}
      g_free (GTK_CELL_PIXTEXT (clist_row->cell[column])->text);
      gdk_pixmap_unref (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap);
      if (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask)
	gdk_bitmap_unref (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask);
      break;

    case GTK_CELL_WIDGET:
      /* unimplemented */
      break;
      
    default:
      break;
    }

  clist_row->cell[column].type = GTK_CELL_EMPTY;
}

static void
real_select_row (GtkCList *clist,
		 gint      row,
		 gint      column,
		 GdkEvent *event)
{
  GList *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));
  
  if ((node = g_list_nth (clist->row_list, row)))
    gtk_signal_emit (GTK_OBJECT (clist), ctree_signals[TREE_SELECT_ROW],
		     node, event);
}

static void
real_unselect_row (GtkCList *clist,
		   gint      row,
		   gint      column,
		   GdkEvent *event)
{
  GList *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));

  if ((node = g_list_nth (clist->row_list, row)))
    gtk_signal_emit (GTK_OBJECT (clist), ctree_signals[TREE_UNSELECT_ROW],
		     node, event);
}

static void
real_tree_select (GtkCTree *ctree,
		  GList    *node,
		  gint      column)
{
  GtkCList *clist;
  GList *list;
  GList *sel_row;
  gboolean node_selected;

  g_return_if_fail (ctree != NULL);

  if (!node)
    return;

  clist = GTK_CLIST (ctree);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:

      node_selected = FALSE;
      list = clist->selection;

      while (list)
	{
	  sel_row = list->data;
	  list = list->next;
	  
	  if (node == sel_row)
	    node_selected = TRUE;
	  else
	    gtk_signal_emit (GTK_OBJECT (ctree),
			     ctree_signals[TREE_UNSELECT_ROW], sel_row, column);
	}

      if (node_selected)
	return;

    default:
      break;
    }

  GTK_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;

  if (!clist->selection)
    {
      clist->selection = g_list_append (clist->selection, node);
      clist->selection_end = clist->selection;
    }
  else
    clist->selection_end = g_list_append (clist->selection_end, node)->next;

  tree_draw_row (ctree, node);
}

static void
real_tree_unselect (GtkCTree *ctree,
		    GList    *node,
		    gint      column)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);

  if (!node)
    return;

  clist = GTK_CLIST (ctree);

  if (GTK_CTREE_ROW (node)->row.state != GTK_STATE_SELECTED)
    return;

  if (clist->selection_end && clist->selection_end->data == node)
    clist->selection_end = clist->selection_end->prev;

  clist->selection = g_list_remove (clist->selection, node);
  
  GTK_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;

  tree_draw_row (ctree, node);
}

static void
tree_toggle_selection (GtkCTree *ctree,
		       GList    *node,
		       gint      column)
{
  GtkCList *clist;
  
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_MULTIPLE:
      if (node && GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
	gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_UNSELECT_ROW], 
			 node, column);
      else
	gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW], 
			 node, column);
      break;

    case GTK_SELECTION_BROWSE:
      if (node && GTK_CTREE_ROW (node)->row.state == GTK_STATE_NORMAL)
	gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW], 
			 node, column);
      break;

    case GTK_SELECTION_EXTENDED:
      break;
    }
}

static void
select_row_recursive (GtkCTree *ctree, 
		      GList    *node, 
		      gpointer  data)
{
  if (!node || GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
    return;

  GTK_CLIST (ctree)->undo_unselection = 
    g_list_prepend (GTK_CLIST (ctree)->undo_unselection, node);
  gtk_ctree_select (ctree, node);
}

static void
real_select_all (GtkCList *clist)
{
  GtkCTree *ctree;
  GList *node;
  gboolean thaw = FALSE;
  
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));

  ctree = GTK_CTREE (clist);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:
      return;

    case GTK_SELECTION_EXTENDED:

      if (!GTK_CLIST_FROZEN (clist))
	{
	  gtk_clist_freeze (clist);
	  thaw = TRUE;
	}

      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
	  
      clist->anchor_state = GTK_STATE_SELECTED;
      clist->anchor = -1;
      clist->drag_pos = -1;
      clist->undo_anchor = clist->focus_row;

      for (node = clist->row_list; node; node = node->next)
	gtk_ctree_pre_recursive (ctree, node, select_row_recursive, NULL);

      if (thaw)
	gtk_clist_thaw (clist);
      break;

    case GTK_SELECTION_MULTIPLE:
      gtk_ctree_select_recursive (ctree, NULL);
      break;;
    }
}

static void
real_unselect_all (GtkCList *clist)
{
  GtkCTree *ctree;
  GList *list;
  GList *node;
 
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));
  
  ctree = GTK_CTREE (clist);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_BROWSE:
      if (clist->focus_row >= 0)
	{
	  gtk_ctree_select
	    (ctree, g_list_nth (clist->row_list, clist->focus_row));
	  return;
	}
      break;

    case GTK_SELECTION_EXTENDED:
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;

      clist->anchor = -1;
      clist->drag_pos = -1;
      clist->undo_anchor = clist->focus_row;
      break;

    default:
      break;
    }

  list = clist->selection;

  while (list)
    {
      node = list->data;
      list = list->next;
      gtk_ctree_unselect (ctree, node);
    }
}

static gboolean
ctree_is_hot_spot (GtkCTree *ctree, 
		   GList    *node,
		   gint      row, 
		   gint      x, 
		   gint      y)
{
  GtkCTreeRow *tree_row;
  GtkCList *clist;
  GtkCellPixText *cell;
  gint xl;
  gint yu;
  
  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  tree_row = GTK_CTREE_ROW (node);
  clist = GTK_CLIST (ctree);

  cell = GTK_CELL_PIXTEXT(tree_row->row.cell[ctree->tree_column]);

  yu = ROW_TOP_YPIXEL (clist, row) + (clist->row_height - PM_SIZE) / 2;

  if (clist->column[ctree->tree_column].justification == GTK_JUSTIFY_RIGHT)
    {
      xl = clist->column[ctree->tree_column].area.x 
	+ clist->column[ctree->tree_column].area.width + clist->hoffset 
	/*+ cell->horizontal +*/
	- (tree_row->level - 1) * ctree->tree_indent 
	- PM_SIZE - 1 -
	(ctree->line_style == GTK_CTREE_LINES_TABBED) * ((PM_SIZE / 2) + 1);
    }
  else
    {
      xl = clist->column[ctree->tree_column].area.x + clist->hoffset 
	+ cell->horizontal + (tree_row->level - 1) * ctree->tree_indent +
	(ctree->line_style == GTK_CTREE_LINES_TABBED) * ((PM_SIZE / 2) + 2);
    }

  return (x >= xl && x <= xl + PM_SIZE && y >= yu && y <= yu + PM_SIZE);
}

static gint
default_compare (GtkCTree    *ctree,
		 const GList *node1,
		 const GList *node2)
{
  char *text1;
  char *text2;

  text1 = GTK_CELL_PIXTEXT (GTK_CTREE_ROW
			    (node1)->row.cell[ctree->tree_column])->text;
  text2 = GTK_CELL_PIXTEXT (GTK_CTREE_ROW
			    (node2)->row.cell[ctree->tree_column])->text;
  return strcmp (text1, text2);
}


/***********************************************************
 ***********************************************************
 ***                  Public interface                   ***
 ***********************************************************
 ***********************************************************/


/***********************************************************
 *           Creation, insertion, deletion                 *
 ***********************************************************/

void
gtk_ctree_construct (GtkCTree *ctree,
		     gint      columns, 
		     gint      tree_column,
		     gchar    *titles[])
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (GTK_CLIST_CONSTRUCTED (ctree) == FALSE);

  clist = GTK_CLIST (ctree);

  clist->row_mem_chunk = g_mem_chunk_new ("ctree row mem chunk",
					  sizeof (GtkCTreeRow),
					  sizeof (GtkCTreeRow)
					  * CLIST_OPTIMUM_SIZE, 
					  G_ALLOC_AND_FREE);

  clist->cell_mem_chunk = g_mem_chunk_new ("ctree cell mem chunk",
					   sizeof (GtkCell) * columns,
					   sizeof (GtkCell) * columns
					   * CLIST_OPTIMUM_SIZE, 
					   G_ALLOC_AND_FREE);

  ctree->tree_column = tree_column;

  gtk_clist_construct (clist, columns, titles);
}

GtkWidget *
gtk_ctree_new_with_titles (gint   columns, 
			   gint   tree_column,
			   gchar *titles[])
{
  GtkWidget *widget;

  g_return_val_if_fail (columns > 0, NULL);
  g_return_val_if_fail (tree_column >= 0 && tree_column < columns, NULL);

  widget = gtk_type_new (GTK_TYPE_CTREE);
  gtk_ctree_construct (GTK_CTREE (widget), columns, tree_column, titles);
  return widget;
}

GtkWidget *
gtk_ctree_new (gint columns, 
	       gint tree_column)
{
  return gtk_ctree_new_with_titles (columns, tree_column, NULL);
}

GList * 
gtk_ctree_insert (GtkCTree  *ctree,
		  GList     *parent, 
		  GList     *sibling,
		  gchar     *text[],
		  guint8     spacing,
		  GdkPixmap *pixmap_closed,
		  GdkBitmap *mask_closed,
		  GdkPixmap *pixmap_opened,
		  GdkBitmap *mask_opened,
		  gboolean   is_leaf,
		  gboolean   expanded)
{
  GtkCList *clist;
  GtkCTreeRow *new_row;
  GList *node;
  gint i;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (!sibling || GTK_CTREE_ROW (sibling)->parent == parent,
			NULL);

  if (parent && GTK_CTREE_ROW (parent)->is_leaf)
    return NULL;

  clist = GTK_CLIST (ctree);

  /* create the row */
  new_row = row_new (ctree);
  node = g_list_alloc ();
  node->data = new_row;

  if (text)
    for (i = 0; i < clist->columns; i++)
      if (text[i] && i != ctree->tree_column)
	cell_set_text (clist, &(new_row->row), i, text[i]);

  set_node_info (ctree, node, text[ctree->tree_column], spacing, pixmap_closed,
		 mask_closed, pixmap_opened, mask_opened, is_leaf, expanded);

  /* sorted insertion */
  if (ctree->auto_sort)
    {
      if (parent)
	sibling = GTK_CTREE_ROW (parent)->children;
      else
	sibling = clist->row_list;

      while (sibling && ctree->node_compare (ctree, node, sibling) > 0)
	sibling = GTK_CTREE_ROW (sibling)->sibling;
    }

  gtk_ctree_link (ctree, node, parent, sibling, TRUE);

  if (!GTK_CLIST_FROZEN (clist))
    gtk_clist_thaw (clist);

  return node;
}

void
gtk_ctree_remove (GtkCTree *ctree, 
		  GList    *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (!GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (node)
    {
      gtk_ctree_unlink (ctree, node, TRUE);
      gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (tree_delete),
				NULL);
    }
  else
    gtk_clist_clear (clist);

  if (thaw)
    gtk_clist_thaw (clist);
}

static void
real_clear (GtkCList *clist)
{
  GtkCTree *ctree;
  GList *work;
  GList *ptr;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));

  ctree = GTK_CTREE (clist);

  ctree->drag_row       = -1;
  ctree->drag_rect      = FALSE;
  ctree->in_drag        = FALSE;
  ctree->drag_source    = NULL;
  ctree->drag_target    = NULL;
  ctree->drag_icon      = NULL;

  /* remove all the rows */
  work = clist->row_list;
  clist->row_list = NULL;
  clist->row_list_end = NULL;

  while (work)
    {
      ptr = work;
      work = GTK_CTREE_ROW (work)->sibling;
      gtk_ctree_post_recursive (ctree, ptr, GTK_CTREE_FUNC (tree_delete_row), 
				NULL);
    }

  (parent_class->clear) (clist);
}


/***********************************************************
 *  Generic recursive functions, querying / finding tree   *
 *  information                                            *
 ***********************************************************/


void
gtk_ctree_post_recursive (GtkCTree     *ctree, 
			  GList        *node,
			  GtkCTreeFunc  func,
			  gpointer      data)
{
  GList *work;
  GList *tmp;

  if (node)
    work = GTK_CTREE_ROW (node)->children;
  else
    work = GTK_CLIST (ctree)->row_list;

  while (work)
    {
      tmp = GTK_CTREE_ROW (work)->sibling;
      gtk_ctree_post_recursive (ctree, work, func, data);
      work = tmp;
    }

  if (node)
    func (ctree, node, data);
}

void
gtk_ctree_pre_recursive (GtkCTree     *ctree, 
			 GList        *node,
			 GtkCTreeFunc  func,
			 gpointer      data)
{
  GList *work;
  GList *tmp;


  if (node)
    {
      work = GTK_CTREE_ROW (node)->children;
      func (ctree, node, data);
    }
  else
    work = GTK_CLIST (ctree)->row_list;

  while (work)
    {
      tmp = GTK_CTREE_ROW (work)->sibling;
      gtk_ctree_pre_recursive (ctree, work, func, data);
      work = tmp;
    }
}

gboolean
gtk_ctree_is_visible (GtkCTree *ctree, 
		      GList    *node)
{ 
  GtkCTreeRow *work;

  work = GTK_CTREE_ROW (node);

  while (work->parent && GTK_CTREE_ROW (work->parent)->expanded)
    work = GTK_CTREE_ROW (work->parent);

  if (!work->parent)
    return TRUE;

  return FALSE;
}

GList * 
gtk_ctree_last (GtkCTree *ctree,
		GList    *node)
{
  if (!node) 
    return NULL;

  while (GTK_CTREE_ROW (node)->sibling)
    node = GTK_CTREE_ROW (node)->sibling;
  
  if (GTK_CTREE_ROW (node)->children)
    return gtk_ctree_last (ctree, GTK_CTREE_ROW (node)->children);
  
  return node;
}

GList *
gtk_ctree_find_glist_ptr (GtkCTree    *ctree,
			  GtkCTreeRow *ctree_row)
{
  GList *node;
  
  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (ctree_row != NULL, FALSE);
  
  if (ctree_row->parent)
    node = GTK_CTREE_ROW(ctree_row->parent)->children;
  else
    node = GTK_CLIST (ctree)->row_list;

  while (node->data != ctree_row)
    node = GTK_CTREE_ROW (node)->sibling;
  
  return node;
}

gint
gtk_ctree_find (GtkCTree *ctree,
		GList    *node,
		GList    *child)
{
  while (node)
    {
      if (node == child) 
	return TRUE;
      if (GTK_CTREE_ROW (node)->children)
	{
	  if (gtk_ctree_find (ctree, GTK_CTREE_ROW (node)->children, child))
	    return TRUE;
	}
      node = GTK_CTREE_ROW (node)->sibling;
    }
  return FALSE;
}

gboolean
gtk_ctree_is_ancestor (GtkCTree *ctree,
		       GList    *node,
		       GList    *child)
{
  return gtk_ctree_find (ctree, GTK_CTREE_ROW (node)->children, child);
}

GList *
gtk_ctree_find_by_row_data (GtkCTree    *ctree,
			    GList       *node,
			    gpointer     data)
{
  GList *work;
  
  while (node)
    {
      if (GTK_CTREE_ROW (node)->row.data == data) 
	return node;
      if (GTK_CTREE_ROW (node)->children &&
	  (work = gtk_ctree_find_by_row_data 
	   (ctree, GTK_CTREE_ROW (node)->children, data)))
	return work;
      node = GTK_CTREE_ROW (node)->sibling;
    }
  return NULL;
}

gboolean
gtk_ctree_is_hot_spot (GtkCTree *ctree, 
		       gint      x, 
		       gint      y)
{
  GList *node;
  gint row;
  gint column;
  
  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);

  if (gtk_clist_get_selection_info (GTK_CLIST (ctree), x, y, &row, &column))
    if ((node = g_list_nth (GTK_CLIST (ctree)->row_list, row)))
      return ctree_is_hot_spot (ctree, node, row, x, y);

  return FALSE;
}


/***********************************************************
 *   Tree signals : move, expand, collapse, (un)select     *
 ***********************************************************/


void
gtk_ctree_move (GtkCTree *ctree,
		GList    *node,
		GList    *new_parent, 
		GList    *new_sibling)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);
  
  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_MOVE], node,
		   new_parent, new_sibling);
}

void
gtk_ctree_expand (GtkCTree *ctree,
		  GList    *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);
  
  if (GTK_CTREE_ROW (node)->is_leaf)
    return;

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node);
}

void 
gtk_ctree_expand_recursive (GtkCTree *ctree,
			    GList    *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);

  clist = GTK_CLIST (ctree);

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  if (((node && gtk_ctree_is_visible (ctree, node)) || !node) && 
      !GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (tree_expand), NULL);

  if (thaw)
    gtk_clist_thaw (clist);
}

void
gtk_ctree_collapse (GtkCTree *ctree,
		    GList    *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);
  
  if (GTK_CTREE_ROW (node)->is_leaf)
    return;

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node);
}

void 
gtk_ctree_collapse_recursive (GtkCTree *ctree,
			      GList    *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (((node && gtk_ctree_is_visible (ctree, node)) || !node) && 
      !GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (tree_collapse), NULL);

  if (thaw)
    gtk_clist_thaw (clist);
}

void
gtk_ctree_toggle_expansion (GtkCTree *ctree,
			    GList    *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);
  
  if (GTK_CTREE_ROW (node)->is_leaf)
    return;

  tree_toggle_expansion (ctree, node, NULL);
}

void 
gtk_ctree_toggle_expansion_recursive (GtkCTree *ctree,
				      GList    *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  
  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (((node && gtk_ctree_is_visible (ctree, node)) || !node) && 
      !GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }
  
  gtk_ctree_post_recursive (ctree, node,
			    GTK_CTREE_FUNC (tree_toggle_expansion), NULL);

  if (thaw)
    gtk_clist_thaw (clist);
}

void
gtk_ctree_select (GtkCTree *ctree, 
		  GList    *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW], node);
}

void
gtk_ctree_unselect (GtkCTree *ctree, 
		    GList    *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_UNSELECT_ROW], node);
}

void
gtk_ctree_select_recursive (GtkCTree *ctree, 
			    GList    *node)
{
  gtk_ctree_real_select_recursive (ctree, node, TRUE);
}

void
gtk_ctree_unselect_recursive (GtkCTree *ctree, 
			      GList    *node)
{
  gtk_ctree_real_select_recursive (ctree, node, FALSE);
}

void
gtk_ctree_real_select_recursive (GtkCTree *ctree, 
				 GList    *node, 
				 gint      state)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);

  clist = GTK_CLIST (ctree);

  if ((state && 
       (clist->selection_mode !=  GTK_SELECTION_MULTIPLE ||
	clist->selection_mode == GTK_SELECTION_EXTENDED)) ||
      (!state && clist->selection_mode ==  GTK_SELECTION_BROWSE))
    return;

  if (((node && gtk_ctree_is_visible (ctree, node)) || !node) && 
      !GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (state)
    gtk_ctree_post_recursive (ctree, node,
			      GTK_CTREE_FUNC (tree_select), NULL);
  else 
    gtk_ctree_post_recursive (ctree, node,
			      GTK_CTREE_FUNC (tree_unselect), NULL);
  
  if (thaw)
    gtk_clist_thaw (clist);
}


/***********************************************************
 *           Analogons of GtkCList functions               *
 ***********************************************************/


void 
gtk_ctree_set_text (GtkCTree *ctree,
		    GList    *node,
		    gint      column,
		    gchar    *text)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  g_return_if_fail (ctree->tree_column != column);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  cell_set_text (GTK_CLIST (ctree), &(GTK_CTREE_ROW(node)->row), column, text);
  tree_draw_row (ctree, node);
}

void 
gtk_ctree_set_pixmap (GtkCTree  *ctree,
		      GList     *node,
		      gint       column,
		      GdkPixmap *pixmap,
		      GdkBitmap *mask)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  g_return_if_fail (pixmap != NULL);
  g_return_if_fail (ctree->tree_column != column);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  gdk_pixmap_ref (pixmap);
  if (mask) 
    gdk_pixmap_ref (mask);

  cell_set_pixmap (GTK_CLIST (ctree), &(GTK_CTREE_ROW (node)->row), column, 
		   pixmap, mask);
  tree_draw_row (ctree, node);
}

void 
gtk_ctree_set_pixtext (GtkCTree  *ctree,
		       GList     *node,
		       gint       column,
		       gchar     *text,
		       guint8     spacing,
		       GdkPixmap *pixmap,
		       GdkBitmap *mask)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  g_return_if_fail (pixmap != NULL);
  g_return_if_fail (ctree->tree_column != column);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  gdk_pixmap_ref (pixmap);
  if (mask) 
    gdk_pixmap_ref (mask);

  cell_set_pixtext (GTK_CLIST (ctree), &(GTK_CTREE_ROW (node)->row), column,
		    text, spacing, pixmap, mask);
  tree_draw_row (ctree, node);
}

void 
gtk_ctree_set_node_info (GtkCTree  *ctree,
			 GList     *node,
			 gchar     *text,
			 guint8     spacing,
			 GdkPixmap *pixmap_closed,
			 GdkBitmap *mask_closed,
			 GdkPixmap *pixmap_opened,
			 GdkBitmap *mask_opened,
			 gboolean   is_leaf,
			 gboolean   expanded)
{
  gboolean old_leaf;
  gboolean old_expanded;
 
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  old_leaf = GTK_CTREE_ROW (node)->is_leaf;
  old_expanded = GTK_CTREE_ROW (node)->expanded;

  if (is_leaf && GTK_CTREE_ROW (node)->children)
    {
      GList *work;
      GList *ptr;
      
      work = GTK_CTREE_ROW (node)->children;
      while (work)
	{
	  ptr = work;
	  work = GTK_CTREE_ROW(work)->sibling;
	  gtk_ctree_remove (ctree, ptr);
	}
    }

  set_node_info (ctree, node, text, spacing, pixmap_closed, mask_closed,
		 pixmap_opened, mask_opened, is_leaf, old_expanded);

  if (!is_leaf && !old_leaf)
    {
      if (expanded && !old_expanded)
	gtk_ctree_expand (ctree, node);
      else if (!expanded && old_expanded)
	gtk_ctree_collapse (ctree, node);
    }

  GTK_CTREE_ROW (node)->expanded = expanded;
  
  tree_draw_row (ctree, node);
}

void
gtk_ctree_set_shift (GtkCTree *ctree,
		     GList    *node,
                     gint      column,
                     gint      vertical,
                     gint      horizontal)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  GTK_CTREE_ROW (node)->row.cell[column].vertical   = vertical;
  GTK_CTREE_ROW (node)->row.cell[column].horizontal = horizontal;

  tree_draw_row (ctree, node);
}

GtkCellType 
gtk_ctree_get_cell_type (GtkCTree *ctree,
			 GList    *node,
			 gint      column)
{
  g_return_val_if_fail (ctree != NULL, -1);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), -1);
  g_return_val_if_fail (node != NULL, -1);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return -1;

  return GTK_CTREE_ROW (node)->row.cell[column].type;
}

gint
gtk_ctree_get_text (GtkCTree  *ctree,
                    GList     *node,
		    gint       column,
                    gchar    **text)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return 0;

  if (GTK_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_TEXT)
    return 0;

  if (text)
    *text = GTK_CELL_TEXT (GTK_CTREE_ROW (node)->row.cell[column])->text;

  return 1;
}

gint
gtk_ctree_get_pixmap (GtkCTree   *ctree,
		      GList      *node,
                      gint        column,
                      GdkPixmap **pixmap,
                      GdkBitmap **mask)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return 0;

  if (GTK_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_PIXMAP)
    return 0;

  if (pixmap)
    *pixmap = GTK_CELL_PIXMAP (GTK_CTREE_ROW(node)->row.cell[column])->pixmap;
  if (mask)
    *mask = GTK_CELL_PIXMAP (GTK_CTREE_ROW (node)->row.cell[column])->mask;

  return 1;
}

gint
gtk_ctree_get_pixtext (GtkCTree   *ctree,
		       GList      *node,
                       gint        column,
                       gchar     **text,
                       guint8     *spacing,
                       GdkPixmap **pixmap,
                       GdkBitmap **mask)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);
  
  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return 0;
  
  if (GTK_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_PIXTEXT)
    return 0;
  
  if (text)
    *text = GTK_CELL_PIXTEXT (GTK_CTREE_ROW (node)->row.cell[column])->text;
  if (spacing)
    *spacing = GTK_CELL_PIXTEXT (GTK_CTREE_ROW 
				 (node)->row.cell[column])->spacing;
  if (pixmap)
    *pixmap = GTK_CELL_PIXTEXT (GTK_CTREE_ROW 
				(node)->row.cell[column])->pixmap;
  if (mask)
    *mask = GTK_CELL_PIXTEXT (GTK_CTREE_ROW (node)->row.cell[column])->mask;
  
  return 1;
}

gint
gtk_ctree_get_node_info (GtkCTree     *ctree,
			 GList        *node,
			 gchar       **text,
			 guint8       *spacing,
			 GdkPixmap   **pixmap_closed,
			 GdkBitmap   **mask_closed,
			 GdkPixmap   **pixmap_opened,
			 GdkBitmap   **mask_opened,
			 gboolean     *is_leaf,
			 gboolean     *expanded)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);
  
  if (text)
    *text = GTK_CELL_PIXTEXT 
      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->text;
  if (spacing)
    *spacing = GTK_CELL_PIXTEXT 
      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->spacing;
  if (pixmap_closed)
    *pixmap_closed = GTK_CTREE_ROW (node)->pixmap_closed;
  if (mask_closed)
    *mask_closed = GTK_CTREE_ROW (node)->mask_closed;
  if (pixmap_opened)
    *pixmap_opened = GTK_CTREE_ROW (node)->pixmap_opened;
  if (mask_opened)
    *mask_opened = GTK_CTREE_ROW (node)->mask_opened;
  if (is_leaf)
    *is_leaf = GTK_CTREE_ROW (node)->is_leaf;
  if (expanded)
    *expanded = GTK_CTREE_ROW (node)->expanded;
  
  return 1;
}

void
gtk_ctree_set_foreground (GtkCTree   *ctree,
			  GList      *node,
                          GdkColor   *color)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (color)
    {
      GTK_CTREE_ROW (node)->row.foreground = *color;
      GTK_CTREE_ROW (node)->row.fg_set = TRUE;
    }
  else
    GTK_CTREE_ROW (node)->row.fg_set = FALSE;

  tree_draw_row (ctree, node);
}

void
gtk_ctree_set_background (GtkCTree   *ctree,
			  GList      *node,
                          GdkColor   *color)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (color)
    {
      GTK_CTREE_ROW (node)->row.background = *color;
      GTK_CTREE_ROW (node)->row.bg_set = TRUE;
    }
  else
    GTK_CTREE_ROW (node)->row.bg_set = FALSE;

  tree_draw_row (ctree, node);
}

void
gtk_ctree_set_row_data (GtkCTree *ctree,
                        GList    *node,
                        gpointer  data)
{
  gtk_ctree_set_row_data_full (ctree, node, data, NULL);
}

void
gtk_ctree_set_row_data_full (GtkCTree        *ctree,
                             GList           *node,
                             gpointer         data,
                             GtkDestroyNotify destroy)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  GTK_CTREE_ROW (node)->row.data = data;
  GTK_CTREE_ROW (node)->row.destroy = destroy;
}

gpointer
gtk_ctree_get_row_data (GtkCTree *ctree,
                        GList    *node)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);

  return node ? GTK_CTREE_ROW (node)->row.data : NULL;
}

void
gtk_ctree_moveto (GtkCTree *ctree,
		  GList    *node,
		  gint      column,
		  gfloat    row_align,
		  gfloat    col_align)
{
  gint row = -1;
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  while (node && !gtk_ctree_is_visible (ctree, node))
    node = GTK_CTREE_ROW (node)->parent;

  if (node)
    row = g_list_position (clist->row_list, node);
  
  gtk_clist_moveto (clist, row, column, row_align, col_align);
}


/***********************************************************
 *             GtkCTree specific functions                 *
 ***********************************************************/


void
gtk_ctree_set_indent (GtkCTree *ctree, 
                      gint      indent)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (indent >= 0);

  if (indent != ctree->tree_indent)
    {
      ctree->tree_indent = indent;
      if (!GTK_CLIST_FROZEN (ctree))
	gtk_clist_thaw (GTK_CLIST (ctree));
    }
}

void
gtk_ctree_set_reorderable (GtkCTree *ctree, 
			   gboolean  reorderable)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (ctree->reorderable == (reorderable != 0))
    return;

  ctree->reorderable = (reorderable != 0);
  
  if (GTK_WIDGET_REALIZED (ctree))
    {
      if (ctree->reorderable)
	create_xor_gc (ctree);
      else
	gdk_gc_destroy (ctree->xor_gc);
    }
}

void
gtk_ctree_set_use_drag_icons (GtkCTree *ctree,
			      gboolean  use_icons)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (ctree->use_icons == (use_icons != 0))
    return;

  ctree->use_icons = (use_icons != 0);
}

void 
gtk_ctree_set_line_style (GtkCTree          *ctree, 
			  GtkCTreeLineStyle  line_style)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (line_style != ctree->line_style)
    {
      ctree->line_style = line_style;

      if (!GTK_WIDGET_REALIZED (ctree))
	return;

      switch (line_style)
	{
	case GTK_CTREE_LINES_SOLID:
	  if (GTK_WIDGET_REALIZED (ctree))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, GDK_LINE_SOLID, 
					None, None);
	  break;
	case GTK_CTREE_LINES_DOTTED:
	  if (GTK_WIDGET_REALIZED (ctree))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, 
					GDK_LINE_ON_OFF_DASH, None, None);
	  gdk_gc_set_dashes (ctree->lines_gc, 0, "\1\1", 2);
	  break;
	case GTK_CTREE_LINES_TABBED:
	  if (GTK_WIDGET_REALIZED (ctree))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, GDK_LINE_SOLID, 
					None, None);
	  break;
	case GTK_CTREE_LINES_NONE:
	  break;
	default:
	  return;
	}
      if (!GTK_CLIST_FROZEN (ctree))
	gtk_clist_thaw (GTK_CLIST (ctree));
    }
}


/***********************************************************
 *             Tree sorting functions                      *
 ***********************************************************/


void       
gtk_ctree_set_auto_sort (GtkCTree *ctree,
			 gboolean  auto_sort)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  
  if (ctree->auto_sort == (auto_sort != 0))
    return;

  ctree->auto_sort = (auto_sort != 0);

  if (auto_sort)
    gtk_ctree_sort_recursive (ctree, NULL);
}

void
gtk_ctree_set_compare_func (GtkCTree            *ctree,
			    GtkCTreeCompareFunc  cmp_func)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (cmp_func == NULL)
    ctree->node_compare = default_compare;
  else
    ctree->node_compare = cmp_func;
}

static void
tree_sort (GtkCTree *ctree,
	   GList    *node,
	   gpointer  data)
{
  GList *list_start;
  GList *max;
  GList *work;

  if (node)
    list_start = GTK_CTREE_ROW (node)->children;
  else
    list_start = GTK_CLIST (ctree)->row_list;

  while (list_start)
    {
      max = list_start;
      work = GTK_CTREE_ROW (max)->sibling;
      while (work)
	{
	  if (ctree->node_compare (ctree, work, max) < 0)
	    max = work;
	  work = GTK_CTREE_ROW (work)->sibling;
	}
      if (max == list_start)
	list_start = GTK_CTREE_ROW (max)->sibling;
      else
	{
	  gtk_ctree_unlink (ctree, max, FALSE);
	  gtk_ctree_link (ctree, max, node, list_start, FALSE);
	}
    }
}

void
gtk_ctree_sort_recursive (GtkCTree *ctree, 
			  GList    *node)
{
  GtkCList *clist;
  GList *focus_node = NULL;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (!GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (clist->selection_mode == GTK_SELECTION_BROWSE)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (gtk_ctree_is_visible (ctree, node))
    focus_node = g_list_nth (clist->row_list, clist->focus_row);
      
  gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (tree_sort), NULL);

  if (focus_node)
    {
      clist->focus_row = g_list_position (clist->row_list, focus_node);
      clist->undo_anchor = clist->focus_row;
    }

  if (thaw)
    gtk_clist_thaw (clist);
}

void
gtk_ctree_sort (GtkCTree *ctree, 
		GList    *node)
{
  GtkCList *clist;
  GList *focus_node = NULL;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (!GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (clist->selection_mode == GTK_SELECTION_BROWSE)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (gtk_ctree_is_visible (ctree, node))
    focus_node = g_list_nth (clist->row_list, clist->focus_row);

  tree_sort (ctree, node, NULL);

  if (focus_node)
    {
      clist->focus_row = g_list_position (clist->row_list, focus_node);
      clist->undo_anchor = clist->focus_row;
    }

  if (thaw)
    gtk_clist_thaw (clist);
}

/************************************************************************/

static void
fake_unselect_all (GtkCList *clist,
		   gint      row)
{
  GList *list;
  GList *focus_node = NULL;

  if (row >= 0 && (focus_node = g_list_nth (clist->row_list, row)))
    {
      if (GTK_CTREE_ROW (focus_node)->row.state == GTK_STATE_NORMAL)
	{
	  GTK_CTREE_ROW (focus_node)->row.state = GTK_STATE_SELECTED;
	  
	  if (!GTK_CLIST_FROZEN (clist) &&
	      gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row,
						  GTK_CLIST_ROW (focus_node));
	}  
    }

  clist->undo_selection = clist->selection;
  clist->selection = NULL;
  clist->selection_end = NULL;
  
  for (list = clist->undo_selection; list; list = list->next)
    {
      if (list->data == focus_node)
	continue;

      GTK_CTREE_ROW ((GList *)(list->data))->row.state = GTK_STATE_NORMAL;
      tree_draw_row (GTK_CTREE (clist), (GList *)(list->data));
    }
}

static GList *
selection_find (GtkCList *clist,
		gint      row_number,
		GList    *row_list_element)
{
  return g_list_find (clist->selection, row_list_element);
}

static void
resync_selection (GtkCList *clist, GdkEvent *event)
{
  GtkCTree *ctree;
  GList *list;
  GList *node;
  gint i;
  gint e;
  gint row;
  gboolean thaw = FALSE;
  gboolean unselect;

  if (clist->anchor < 0)
    return;

  ctree = GTK_CTREE (clist);
  
  if (!GTK_CLIST_FROZEN (clist))
    {
      GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);
      thaw = TRUE;
    }

  i = MIN (clist->anchor, clist->drag_pos);
  e = MAX (clist->anchor, clist->drag_pos);

  if (clist->undo_selection)
    {
      list = clist->selection;
      clist->selection = clist->undo_selection;
      clist->selection_end = g_list_last (clist->selection);
      clist->undo_selection = list;
      list = clist->selection;

      while (list)
	{
	  node = list->data;
	  list = list->next;
	  
	  unselect = TRUE;

	  if (gtk_ctree_is_visible (ctree, node))
	    {
	      row = g_list_position (clist->row_list, node);
	      if (row >= i && row <= e)
		unselect = FALSE;
	    }
	  if (unselect)
	    {
	      GTK_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;
	      gtk_ctree_unselect (ctree, node);
	      clist->undo_selection = g_list_prepend (clist->undo_selection,
						      node);
	    }
	}
    }    


  for (node = g_list_nth (clist->row_list, i); i <= e; i++, node = node->next)
    if (g_list_find (clist->selection, node))
      {
	if (GTK_CTREE_ROW (node)->row.state == GTK_STATE_NORMAL)
	  {
	    GTK_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;
	    gtk_ctree_unselect (ctree, node);
	    clist->undo_selection = g_list_prepend (clist->undo_selection,
						    node);
	  }
      }
    else if (GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
      {
	GTK_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;
	clist->undo_unselection = g_list_prepend (clist->undo_unselection,
						  node);
      }

  for (list = clist->undo_unselection; list; list = list->next)
    gtk_ctree_select (ctree, list->data);

  clist->anchor = -1;
  clist->drag_pos = -1;

  if (thaw)
    GTK_CLIST_UNSET_FLAG (clist, CLIST_FROZEN);
}

static void
real_undo_selection (GtkCList *clist)
{
  GtkCTree *ctree;
  GList *work;


  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (clist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (!(clist->undo_selection || clist->undo_unselection))
    {
      gtk_clist_unselect_all (clist);
      return;
    }

  ctree = GTK_CTREE (clist);

  for (work = clist->undo_selection; work; work = work->next)
    gtk_ctree_select (ctree, (GList *) work->data);

  for (work = clist->undo_unselection; work; work = work->next)
    gtk_ctree_unselect (ctree, (GList *) work->data);

  if (GTK_WIDGET_HAS_FOCUS (clist) && clist->focus_row != clist->undo_anchor)
    {
      gtk_widget_draw_focus (GTK_WIDGET (clist));
      clist->focus_row = clist->undo_anchor;
      gtk_widget_draw_focus (GTK_WIDGET (clist));
    }
  else
    clist->focus_row = clist->undo_anchor;
  
  clist->undo_anchor = -1;
 
  g_list_free (clist->undo_selection);
  g_list_free (clist->undo_unselection);
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;

  if (ROW_TOP_YPIXEL (clist, clist->focus_row) + clist->row_height >
      clist->clist_window_height)
    gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
  else if (ROW_TOP_YPIXEL (clist, clist->focus_row) < 0)
    gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);

}
