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
#include "gtkmain.h"
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
static void draw_xor_line               (GtkCTree       *ctree);
static void draw_xor_rect               (GtkCTree       *ctree);
static void create_drag_icon            (GtkCTree       *ctree,
					 GtkCTreeRow    *row);
static void tree_draw_node              (GtkCTree      *ctree,
					 GtkCTreeNode  *node);
static void set_cell_contents           (GtkCList      *clist,
					 GtkCListRow   *clist_row,
					 gint           column,
					 GtkCellType    type,
					 const gchar   *text,
					 guint8         spacing,
					 GdkPixmap     *pixmap,
					 GdkBitmap     *mask);
static void set_node_info               (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 const gchar   *text,
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
					 GtkCTreeNode  *node, 
					 gpointer       data);
static void tree_delete_row             (GtkCTree      *ctree, 
					 GtkCTreeNode  *node, 
					 gpointer       data);
static void real_clear                  (GtkCList      *clist);
static void tree_update_level           (GtkCTree      *ctree, 
					 GtkCTreeNode  *node, 
					 gpointer       data);
static void tree_select                 (GtkCTree      *ctree, 
					 GtkCTreeNode  *node, 
					 gpointer       data);
static void tree_unselect               (GtkCTree      *ctree, 
					 GtkCTreeNode  *node, 
				         gpointer       data);
static void real_select_all             (GtkCList      *clist);
static void real_unselect_all           (GtkCList      *clist);
static void tree_expand                 (GtkCTree      *ctree, 
					 GtkCTreeNode  *node,
					 gpointer       data);
static void tree_collapse               (GtkCTree      *ctree, 
					 GtkCTreeNode  *node,
					 gpointer       data);
static void tree_collapse_to_depth      (GtkCTree      *ctree, 
					 GtkCTreeNode  *node, 
					 gint           depth);
static void tree_toggle_expansion       (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 gpointer       data);
static void change_focus_row_expansion  (GtkCTree      *ctree,
				         GtkCTreeExpansionType expansion);
static void real_select_row             (GtkCList      *clist,
					 gint           row,
					 gint           column,
					 GdkEvent      *event);
static void real_unselect_row           (GtkCList      *clist,
					 gint           row,
					 gint           column,
					 GdkEvent      *event);
static void real_tree_select            (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 gint           column);
static void real_tree_unselect          (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 gint           column);
static void tree_toggle_selection       (GtkCTree      *ctree, 
					 GtkCTreeNode  *node, 
				         gint           column);
static void real_tree_expand            (GtkCTree      *ctree,
					 GtkCTreeNode  *node);
static void real_tree_collapse          (GtkCTree      *ctree,
					 GtkCTreeNode  *node);
static void real_tree_move              (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 GtkCTreeNode  *new_parent, 
					 GtkCTreeNode  *new_sibling);
static void gtk_ctree_link              (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 GtkCTreeNode  *parent,
					 GtkCTreeNode  *sibling,
					 gboolean       update_focus_row);
static void gtk_ctree_unlink            (GtkCTree      *ctree, 
					 GtkCTreeNode  *node,
					 gboolean       update_focus_row);
static GtkCTreeNode * gtk_ctree_last_visible (GtkCTree     *ctree,
					      GtkCTreeNode *node);
static gboolean ctree_is_hot_spot       (GtkCTree      *ctree, 
					 GtkCTreeNode  *node,
					 gint           row, 
					 gint           x, 
					 gint           y);
static void tree_sort                   (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 gpointer       data);
static void fake_unselect_all           (GtkCList      *clist,
					 gint           row);
static GList * selection_find           (GtkCList      *clist,
					 gint           row_number,
					 GList         *row_list_element);
static void resync_selection            (GtkCList      *clist,
					 GdkEvent      *event);
static void real_undo_selection         (GtkCList      *clist);
static void select_row_recursive        (GtkCTree      *ctree, 
					 GtkCTreeNode  *node, 
					 gpointer       data);
static gint real_insert_row             (GtkCList      *clist,
					 gint           row,
					 gchar         *text[]);
static void real_remove_row             (GtkCList      *clist,
					 gint           row);
static void real_sort_list              (GtkCList      *clist);
static void set_mouse_cursor		(GtkCTree	*ctree,
					 gboolean	enable);
static void check_cursor		(GtkCTree	*ctree);


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

typedef void (*GtkCTreeSignal1) (GtkObject    *object,
				 GtkCTreeNode *arg1,
				 gint          arg2,
				 gpointer      data);

typedef void (*GtkCTreeSignal2) (GtkObject    *object,
				 GtkCTreeNode *arg1,
				 GtkCTreeNode *arg2,
				 GtkCTreeNode *arg3,
				 gpointer      data);

typedef void (*GtkCTreeSignal3) (GtkObject    *object,
				 GtkCTreeNode *arg1,
				 gpointer      data);

typedef void (*GtkCTreeSignal4) (GtkObject         *object,
				 GtkCTreeExpansionType arg1,
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
		    gtk_marshal_NONE__POINTER_INT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_INT);
  ctree_signals[TREE_UNSELECT_ROW] =
    gtk_signal_new ("tree_unselect_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_unselect_row),
		    gtk_marshal_NONE__POINTER_INT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_INT);
  ctree_signals[TREE_EXPAND] =
    gtk_signal_new ("tree_expand",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_expand),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  ctree_signals[TREE_COLLAPSE] =
    gtk_signal_new ("tree_collapse",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_collapse),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  ctree_signals[TREE_MOVE] =
    gtk_signal_new ("tree_move",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_move),
		    gtk_marshal_NONE__POINTER_POINTER_POINTER,
		    GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_POINTER, 
		    GTK_TYPE_POINTER);
  ctree_signals[CHANGE_FOCUS_ROW_EXPANSION] =
    gtk_signal_new ("change_focus_row_expansion",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCTreeClass,
				       change_focus_row_expansion),
		    gtk_marshal_NONE__ENUM,
		    GTK_TYPE_NONE, 1, GTK_TYPE_C_TREE_EXPANSION_TYPE);

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
  clist_class->insert_row = real_insert_row;
  clist_class->remove_row = real_remove_row;
  clist_class->sort_list = real_sort_list;
  clist_class->set_cell_contents = set_cell_contents;

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
  ctree->drag_icon      = NULL;
  ctree->tree_indent    = 20;
  ctree->tree_column    = 0;
  ctree->drag_row       = -1;
  ctree->drag_source    = NULL;
  ctree->drag_target    = NULL;
  ctree->insert_pos     = GTK_CTREE_POS_AS_CHILD;
  ctree->reorderable    = FALSE;
  ctree->use_icons      = TRUE;
  ctree->in_drag        = FALSE;
  ctree->drag_rect      = FALSE;
  ctree->line_style     = GTK_CTREE_LINES_SOLID;
  ctree->drag_compare   = NULL;
  ctree->show_stub      = TRUE;
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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CTREE (widget));

  (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  ctree = GTK_CTREE (widget);

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
}

static void
gtk_ctree_unrealize (GtkWidget *widget)
{
  GtkCTree *ctree;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CTREE (widget));

  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

  ctree = GTK_CTREE (widget);

  gdk_gc_destroy (ctree->lines_gc);
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
      GtkCTreeNode *work;
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

      work = GTK_CTREE_NODE (g_list_nth (clist->row_list, row));
	  
      if (ctree->reorderable && event->button == 2 && !ctree->in_drag &&
	  clist->anchor == -1)
	{
	  gdk_pointer_grab (event->window, FALSE,
			    GDK_POINTER_MOTION_HINT_MASK |
			    GDK_BUTTON2_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			    NULL, NULL, event->time);
	  gtk_grab_add (widget);
	  ctree->in_drag = TRUE;
	  ctree->drag_source = work;
	  ctree->drag_target = NULL;
	  gdk_gc_set_line_attributes (clist->xor_gc, 1, GDK_LINE_ON_OFF_DASH, 
				      None, None);
	  gdk_gc_set_dashes (clist->xor_gc, 0, "\2\2", 2);
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
	      gtk_grab_add (widget);

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
	ctree->drag_target = GTK_CTREE_NODE (g_list_nth (clist->row_list,row));
      
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
	      ctree->drag_target =
		GTK_CTREE_NODE (g_list_nth (clist->row_list, row));
	      ctree->drag_row = row;
	      draw_xor_line (ctree);
	      check_cursor(ctree);
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
	      ctree->drag_target =
		GTK_CTREE_NODE (g_list_nth (clist->row_list, row));
	      ctree->drag_row = row;
	      draw_xor_rect (ctree);
	      check_cursor(ctree);
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
      gtk_grab_remove (widget);
      gdk_pointer_ungrab (event->time);

      ctree->in_drag = FALSE;

      set_mouse_cursor(ctree, TRUE);

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

      if (GTK_CLIST_ADD_MODE (clist))
	gdk_gc_set_dashes (clist->xor_gc, 0, "\4\4", 2);
      else
	gdk_gc_set_line_attributes (clist->xor_gc, 1,
				    GDK_LINE_SOLID, 0, 0);

      /* nop if out of bounds / source == target */
      if (event->x < 0 || event->y < -3 ||
	  event->x > clist->clist_window_width ||
	  event->y > clist->clist_window_height + 3 ||
	  ctree->drag_target == ctree->drag_source ||
	  !ctree->drag_target)
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
		if (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 ctree->drag_source,
					 GTK_CTREE_ROW (ctree->drag_target)->parent,
					 GTK_CTREE_ROW (ctree->drag_target)->sibling))
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
		if (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 ctree->drag_source,
					 GTK_CTREE_ROW (ctree->drag_target)->parent,
					 ctree->drag_target))
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
		if (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 ctree->drag_source,
					 ctree->drag_target,
					 GTK_CTREE_ROW (ctree->drag_target)->children))
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
      GtkCTreeNode *work;

      if (gtk_clist_get_selection_info
	  (clist, event->x, event->y, &row, &column))
	{
	  if (clist->anchor == clist->focus_row &&
	      (work = GTK_CTREE_NODE (g_list_nth (clist->row_list, row))))
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

  if (clist->column[ctree->tree_column].visible)
    switch (clist->column[ctree->tree_column].justification)
      {
      case GTK_JUSTIFY_CENTER:
      case GTK_JUSTIFY_FILL:
      case GTK_JUSTIFY_LEFT:
	if (ctree->tree_column > 0)
	  gdk_draw_line (clist->clist_window, clist->xor_gc, 
			 COLUMN_LEFT_XPIXEL(clist, 0), y,
			 COLUMN_LEFT_XPIXEL(clist, ctree->tree_column - 1) +
			 clist->column[ctree->tree_column - 1].area.width, y);
      
	gdk_draw_line (clist->clist_window, clist->xor_gc, 
		       COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) + 
		       ctree->tree_indent * level -
		       (ctree->tree_indent - PM_SIZE) / 2, y,
		       GTK_WIDGET (ctree)->allocation.width, y);
	break;
      case GTK_JUSTIFY_RIGHT:
	if (ctree->tree_column < clist->columns - 1)
	  gdk_draw_line (clist->clist_window, clist->xor_gc, 
			 COLUMN_LEFT_XPIXEL(clist, ctree->tree_column + 1), y,
			 COLUMN_LEFT_XPIXEL(clist, clist->columns - 1) +
			 clist->column[clist->columns - 1].area.width, y);
      
	gdk_draw_line (clist->clist_window, clist->xor_gc, 
		       0, y, COLUMN_LEFT_XPIXEL(clist, ctree->tree_column)
		       + clist->column[ctree->tree_column].area.width -
		       ctree->tree_indent * level +
		       (ctree->tree_indent - PM_SIZE) / 2, y);
	break;
      }
  else
    gdk_draw_line (clist->clist_window, clist->xor_gc, 
		   0, y, clist->clist_window_width, y);
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

  if (clist->column[ctree->tree_column].visible)
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
	  gdk_draw_line (clist->clist_window, clist->xor_gc,
			 points[i].x, points[i].y,
			 points[i+1].x, points[i+1].y);

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
	      gdk_draw_line (clist->clist_window, clist->xor_gc,
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
	  gdk_draw_line (clist->clist_window, clist->xor_gc,
			 points[i].x, points[i].y,
			 points[i+1].x, points[i+1].y);

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
	      gdk_draw_line (clist->clist_window, clist->xor_gc,
			     points[i].x, points[i].y,
			     points[i+1].x, points[i+1].y);
	  }
	break;
      }      
  else
    gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			0, y - clist->row_height,
			clist->clist_window_width - 1, clist->row_height);
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
      if (!clist->column[i].visible)
	continue;
	
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
	  GtkCTreeNode *work;
	  GtkCTreeNode *work2;
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
		      work2 = gtk_ctree_find_node_ptr
			(ctree, (GtkCTreeRow *) clist_row);

		      if (GTK_CTREE_NODE_NEXT (work2))
			next_level =
			  GTK_CTREE_ROW (GTK_CTREE_NODE_NEXT (work2))->level;
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
			      work2 = GTK_CTREE_ROW (work)->parent;

			      if (work2 && GTK_CTREE_ROW (work2)->row.bg_set)
				{
				  gdk_gc_set_foreground
				    (clist->bg_gc,
				     &(GTK_CTREE_ROW (work2)->row.background));
				  
				  gdk_draw_rectangle 
				    (clist->clist_window, clist->bg_gc, TRUE,
				     xcenter,
				     ycenter,
				     in / 2 + in % 2,
				     row_rectangle.height / 2 + 1);

				  if (GTK_CTREE_ROW (work)->row.bg_set)
				    gdk_gc_set_foreground
				      (clist->bg_gc,
				       &(GTK_CTREE_ROW (work)->row.background));
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
				 xcenter, 
				 (ctree->show_stub ||
				  clist->row_list->data != clist_row) ?
				 clip_rectangle.y + offset_y : ycenter,
				 xcenter,
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
		      work2 = gtk_ctree_find_node_ptr
			(ctree, (GtkCTreeRow *) clist_row);

		      if (GTK_CTREE_NODE_NEXT (work2))
			next_level =
			  GTK_CTREE_ROW (GTK_CTREE_NODE_NEXT (work2))->level;
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
			     &(GTK_CTREE_ROW (work)->row.background));
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
			      work2 = GTK_CTREE_ROW (work)->parent;

			      if (work2 && GTK_CTREE_ROW (work2)->row.bg_set)
				{
				  gdk_gc_set_foreground
				    (clist->bg_gc,
				     &(GTK_CTREE_ROW (work2)->row.background));
				  
				  gdk_draw_rectangle 
				    (clist->clist_window, clist->bg_gc, TRUE,
				     xcenter + 1 - in / 2 - in % 2,
				     ycenter,
				     in / 2 + in % 2,
				     row_rectangle.height / 2 + 1);

				  if (GTK_CTREE_ROW (work)->row.bg_set)
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
		      if (GTK_CTREE_ROW (work)->sibling)
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
tree_draw_node (GtkCTree     *ctree, 
	        GtkCTreeNode *node)
{
  GtkCList *clist;
  
  clist = GTK_CLIST (ctree);

  if (!GTK_CLIST_FROZEN (clist) && gtk_ctree_is_viewable (ctree, node))
    {
      GtkCTreeNode *work;
      gint num = 0;
      
      work = GTK_CTREE_NODE (clist->row_list);
      while (work && work != node)
	{
	  work = GTK_CTREE_NODE_NEXT (work);
	  num++;
	}
      if (work && gtk_clist_row_is_visible (clist, num) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row
	  (clist, NULL, num, GTK_CLIST_ROW ((GList *) node));
    }
}

static GtkCTreeNode *
gtk_ctree_last_visible (GtkCTree     *ctree,
			GtkCTreeNode *node)
{
  GtkCTreeNode *work;
  
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
gtk_ctree_link (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *parent,
		GtkCTreeNode *sibling,
		gboolean      update_focus_row)
{
  GtkCList *clist;
  GList *list_end;
  GList *list;
  GList *work;
  gboolean visible = FALSE;
  gint rows = 0;
  
  if (sibling)
    g_return_if_fail (GTK_CTREE_ROW (sibling)->parent == parent);
  g_return_if_fail (node != NULL);
  g_return_if_fail (node != sibling);
  g_return_if_fail (node != parent);

  clist = GTK_CLIST (ctree);

  if (update_focus_row && clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  for (rows = 1, list_end = (GList *)node; list_end->next;
       list_end = list_end->next)
    rows++;

  GTK_CTREE_ROW (node)->parent = parent;
  GTK_CTREE_ROW (node)->sibling = sibling;

  if (!parent || (parent && (gtk_ctree_is_viewable (ctree, parent) &&
			     GTK_CTREE_ROW (parent)->expanded)))
    {
      visible = TRUE;
      clist->rows += rows;
    }

  if (parent)
    work = (GList *)(GTK_CTREE_ROW (parent)->children);
  else
    work = clist->row_list;

  if (sibling)
    {
      if (work != (GList *)sibling)
	{
	  while (GTK_CTREE_ROW (work)->sibling != sibling)
	    work = (GList *)(GTK_CTREE_ROW (work)->sibling);
	  GTK_CTREE_ROW (work)->sibling = node;
	}

      if (sibling == GTK_CTREE_NODE (clist->row_list))
	clist->row_list = (GList *) node;
      if (GTK_CTREE_NODE_PREV (sibling) &&
	  GTK_CTREE_NODE_NEXT (GTK_CTREE_NODE_PREV (sibling)) == sibling)
	{
	  list = (GList *)GTK_CTREE_NODE_PREV (sibling);
	  list->next = (GList *)node;
	}
      
      list = (GList *)node;
      list->prev = (GList *)GTK_CTREE_NODE_PREV (sibling);
      list_end->next = (GList *)sibling;
      list = (GList *)sibling;
      list->prev = list_end;
      if (parent && GTK_CTREE_ROW (parent)->children == sibling)
	GTK_CTREE_ROW (parent)->children = node;
    }
  else
    {
      if (work)
	{
	  /* find sibling */
	  while (GTK_CTREE_ROW (work)->sibling)
	    work = (GList *)(GTK_CTREE_ROW (work)->sibling);
	  GTK_CTREE_ROW (work)->sibling = node;
	  
	  /* find last visible child of sibling */
	  work = (GList *) gtk_ctree_last_visible (ctree,
						   GTK_CTREE_NODE (work));
	  
	  list_end->next = work->next;
	  if (work->next)
	    list = work->next->prev = list_end;
	  work->next = (GList *)node;
	  list = (GList *)node;
	  list->prev = work;
	}
      else
	{
	  if (parent)
	    {
	      GTK_CTREE_ROW (parent)->children = node;
	      list = (GList *)node;
	      list->prev = (GList *)parent;
	      if (GTK_CTREE_ROW (parent)->expanded)
		{
		  list_end->next = (GList *)GTK_CTREE_NODE_NEXT (parent);
		  if (GTK_CTREE_NODE_NEXT(parent))
		    {
		      list = (GList *)GTK_CTREE_NODE_NEXT (parent);
		      list->prev = list_end;
		    }
		  list = (GList *)parent;
		  list->next = (GList *)node;
		}
	      else
		list_end->next = NULL;
	    }
	  else
	    {
	      clist->row_list = (GList *)node;
	      list = (GList *)node;
	      list->prev = NULL;
	      list_end->next = NULL;
	    }
	}
    }

  gtk_ctree_pre_recursive (ctree, node, tree_update_level, NULL); 

  if (clist->row_list_end == NULL ||
      clist->row_list_end->next == (GList *)node)
    clist->row_list_end = list_end;

  if (visible && update_focus_row)
    {
      gint pos;
	  
      pos = g_list_position (clist->row_list, (GList *)node);
  
      if (pos <= clist->focus_row)
	{
	  clist->focus_row += rows;
	  clist->undo_anchor = clist->focus_row;
	}
    }
}

static void
gtk_ctree_unlink (GtkCTree     *ctree, 
		  GtkCTreeNode *node,
                  gboolean      update_focus_row)
{
  GtkCList *clist;
  gint rows;
  gint level;
  gint visible;
  GtkCTreeNode *work;
  GtkCTreeNode *parent;
  GList *list;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);
  
  if (update_focus_row && clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  visible = gtk_ctree_is_viewable (ctree, node);

  /* clist->row_list_end unlinked ? */
  if (visible &&
      (GTK_CTREE_NODE_NEXT (node) == NULL ||
       (GTK_CTREE_ROW (node)->children &&
	gtk_ctree_is_ancestor (ctree, node,
			       GTK_CTREE_NODE (clist->row_list_end)))))
    clist->row_list_end = (GList *) (GTK_CTREE_NODE_PREV (node));

  /* update list */
  rows = 0;
  level = GTK_CTREE_ROW (node)->level;
  work = GTK_CTREE_NODE_NEXT (node);
  while (work && GTK_CTREE_ROW (work)->level > level)
    {
      work = GTK_CTREE_NODE_NEXT (work);
      rows++;
    }

  if (visible)
    {
      clist->rows -= (rows + 1);

      if (update_focus_row)
	{
	  gint pos;
	  
	  pos = g_list_position (clist->row_list, (GList *)node);
	  if (pos + rows + 1 < clist->focus_row)
	    clist->focus_row -= (rows + 1);
	  else if (pos <= clist->focus_row)
	    clist->focus_row = pos - 1;
	  clist->undo_anchor = clist->focus_row;
	}
    }

  if (work)
    {
      list = (GList *)GTK_CTREE_NODE_PREV (work);
      list->next = NULL;
      list = (GList *)work;
      list->prev = (GList *)GTK_CTREE_NODE_PREV (node);
    }

  if (GTK_CTREE_NODE_PREV (node) &&
      GTK_CTREE_NODE_NEXT (GTK_CTREE_NODE_PREV (node)) == node)
    {
      list = (GList *)GTK_CTREE_NODE_PREV (node);
      list->next = (GList *)work;
    }

  /* update tree */
  parent = GTK_CTREE_ROW (node)->parent;
  if (parent)
    {
      if (GTK_CTREE_ROW (parent)->children == node)
	{
	  GTK_CTREE_ROW (parent)->children = GTK_CTREE_ROW (node)->sibling;
	  if (!GTK_CTREE_ROW (parent)->children)
	    gtk_ctree_collapse (ctree, parent);
	}
      else
	{
	  GtkCTreeNode *sibling;

	  sibling = GTK_CTREE_ROW (parent)->children;
	  while (GTK_CTREE_ROW (sibling)->sibling != node)
	    sibling = GTK_CTREE_ROW (sibling)->sibling;
	  GTK_CTREE_ROW (sibling)->sibling = GTK_CTREE_ROW (node)->sibling;
	}
    }
  else
    {
      if (clist->row_list == (GList *)node)
	clist->row_list = (GList *) (GTK_CTREE_ROW (node)->sibling);
      else
	{
	  GtkCTreeNode *sibling;

	  sibling = GTK_CTREE_NODE (clist->row_list);
	  while (GTK_CTREE_ROW (sibling)->sibling != node)
	    sibling = GTK_CTREE_ROW (sibling)->sibling;
	  GTK_CTREE_ROW (sibling)->sibling = GTK_CTREE_ROW (node)->sibling;
	}
    }
}

static void
real_tree_move (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *new_parent, 
		GtkCTreeNode *new_sibling)
{
  GtkCList *clist;
  GtkCTreeNode *work;
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

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (new_parent == GTK_CTREE_ROW (node)->parent)
	return;
      
      if (new_parent)
	new_sibling = GTK_CTREE_ROW (new_parent)->children;
      else
	new_sibling = GTK_CTREE_NODE (clist->row_list);

      while (new_sibling && clist->compare
	     (clist, GTK_CTREE_ROW (node), GTK_CTREE_ROW (new_sibling)) > 0)
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
  if (gtk_ctree_is_viewable (ctree, node) ||
      gtk_ctree_is_viewable (ctree, new_sibling))
    work = GTK_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row));
      
  gtk_ctree_unlink (ctree, node, FALSE);
  gtk_ctree_link (ctree, node, new_parent, new_sibling, FALSE);
  
  if (work)
    {
      while (work &&  !gtk_ctree_is_viewable (ctree, work))
	work = GTK_CTREE_ROW (work)->parent;
      clist->focus_row = g_list_position (clist->row_list, (GList *)work);
      clist->undo_anchor = clist->focus_row;
    }

  if (thaw)
    gtk_clist_thaw (clist);
}

static void
change_focus_row_expansion (GtkCTree          *ctree,
			    GtkCTreeExpansionType action)
{
  GtkCList *clist;
  GtkCTreeNode *node;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (ctree))
    return;
  
  if (!(node =
	GTK_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row))) ||
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
real_tree_expand (GtkCTree     *ctree,
		  GtkCTreeNode *node)
{
  GtkCList *clist;
  GtkCTreeNode *work;
  gint level;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || GTK_CTREE_ROW (node)->expanded || GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);
  
  if (clist->selection_mode == GTK_SELECTION_EXTENDED && clist->anchor >= 0)
    GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);

  GTK_CTREE_ROW (node)->expanded = TRUE;
  level = GTK_CTREE_ROW (node)->level;

  if (GTK_CELL_PIXTEXT 
      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
    {
      gdk_pixmap_unref
	(GTK_CELL_PIXTEXT
	 (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap);
      
      GTK_CELL_PIXTEXT
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = NULL;
      
      if (GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask)
	{
	  gdk_pixmap_unref
	    (GTK_CELL_PIXTEXT 
	     (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask);
	  GTK_CELL_PIXTEXT 
	    (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = NULL;
	}
    }

  if (GTK_CTREE_ROW (node)->pixmap_opened)
    {
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	gdk_pixmap_ref (GTK_CTREE_ROW (node)->pixmap_opened);

      if (GTK_CTREE_ROW (node)->mask_opened) 
	GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	  gdk_pixmap_ref (GTK_CTREE_ROW (node)->mask_opened);
    }

  work = GTK_CTREE_ROW (node)->children;
  if (work)
    {
      gint tmp = 0;
      gint row;
      GList *list;
      
      while (GTK_CTREE_NODE_NEXT (work))
	{
	  work = GTK_CTREE_NODE_NEXT (work);
	  tmp++;
	}

      list = (GList *)work;
      list->next = (GList *)GTK_CTREE_NODE_NEXT (node);

      if (GTK_CTREE_NODE_NEXT (node))
	{
	  list = (GList *)GTK_CTREE_NODE_NEXT (node);
	  list->prev = (GList *)work;
	}
      else
	clist->row_list_end = (GList *)work;

      list = (GList *)node;
      list->next = (GList *)(GTK_CTREE_ROW (node)->children);
      
      if (gtk_ctree_is_viewable (ctree, node))
	{
	  row = g_list_position (clist->row_list, (GList *)node);
	  if (row < clist->focus_row)
	    clist->focus_row += tmp + 1;
	  clist->rows += tmp + 1;
	  if (!GTK_CLIST_FROZEN (ctree))
	    gtk_clist_thaw (clist);
	}
    }
}

static void 
real_tree_collapse (GtkCTree     *ctree,
		    GtkCTreeNode *node)
{
  GtkCList *clist;
  GtkCTreeNode *work;
  gint level;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || !GTK_CTREE_ROW (node)->expanded ||GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (clist->selection_mode == GTK_SELECTION_EXTENDED && clist->anchor >= 0)
    GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
  
  GTK_CTREE_ROW (node)->expanded = FALSE;
  level = GTK_CTREE_ROW (node)->level;

  if (GTK_CELL_PIXTEXT 
      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
    {
      gdk_pixmap_unref
	(GTK_CELL_PIXTEXT
	 (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap);
      
      GTK_CELL_PIXTEXT
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = NULL;
      
      if (GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask)
	{
	  gdk_pixmap_unref
	    (GTK_CELL_PIXTEXT 
	     (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask);
	  GTK_CELL_PIXTEXT 
	    (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = NULL;
	}
    }

  if (GTK_CTREE_ROW (node)->pixmap_closed)
    {
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	gdk_pixmap_ref (GTK_CTREE_ROW (node)->pixmap_closed);

      if (GTK_CTREE_ROW (node)->mask_closed) 
	GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	  gdk_pixmap_ref (GTK_CTREE_ROW (node)->mask_closed);
    }

  work = GTK_CTREE_ROW (node)->children;
  if (work)
    {
      gint tmp = 0;
      gint row;
      GList *list;

      while (work && GTK_CTREE_ROW (work)->level > level)
	{
	  work = GTK_CTREE_NODE_NEXT (work);
	  tmp++;
	}

      if (work)
	{
	  list = (GList *)node;
	  list->next = (GList *)work;
	  list = (GList *)GTK_CTREE_NODE_PREV (work);
	  list->next = NULL;
	  list = (GList *)work;
	  list->prev = (GList *)node;
	}
      else
	{
	  list = (GList *)node;
	  list->next = NULL;
	  clist->row_list_end = (GList *)node;
	}

      if (gtk_ctree_is_viewable (ctree, node))
	{
	  row = g_list_position (clist->row_list, (GList *)node);
	  if (row < clist->focus_row)
	    clist->focus_row -= tmp;
	  clist->rows -= tmp;
	  if (!GTK_CLIST_FROZEN (ctree))
	    gtk_clist_thaw (clist);
	}
    }
}

static void
set_cell_contents (GtkCList    *clist,
		   GtkCListRow *clist_row,
		   gint         column,
		   GtkCellType  type,
		   const gchar *text,
		   guint8       spacing,
		   GdkPixmap   *pixmap,
		   GdkBitmap   *mask)
{
  GtkCTree *ctree;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));
  g_return_if_fail (clist_row != NULL);

  ctree = GTK_CTREE (clist);

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
      if (GTK_CELL_PIXTEXT (clist_row->cell[column])->text)
	g_free (GTK_CELL_PIXTEXT (clist_row->cell[column])->text);
      if (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap)
	{
	  gdk_pixmap_unref
	    (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap);
	  if (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask)
	    gdk_bitmap_unref
	      (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask);
	}
      break;

    case GTK_CELL_WIDGET:
      /* unimplimented */
      break;
      
    default:
      break;
    }

  clist_row->cell[column].type = GTK_CELL_EMPTY;
  if (column == ctree->tree_column && type != GTK_CELL_EMPTY)
    type = GTK_CELL_PIXTEXT;

  switch (type)
    {
    case GTK_CELL_TEXT:
      if (text)
	{
	  clist_row->cell[column].type = GTK_CELL_TEXT;
	  GTK_CELL_TEXT (clist_row->cell[column])->text = g_strdup (text);
	}
      break;

    case GTK_CELL_PIXMAP:
      if (pixmap)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXMAP;
	  GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap = pixmap;
	  /* We set the mask even if it is NULL */
	  GTK_CELL_PIXMAP (clist_row->cell[column])->mask = mask;
	}
      break;

    case GTK_CELL_PIXTEXT:
      if (column == ctree->tree_column)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXTEXT;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing = spacing;
	  if (text)
	    GTK_CELL_PIXTEXT (clist_row->cell[column])->text = g_strdup (text);
	  else
	    GTK_CELL_PIXTEXT (clist_row->cell[column])->text = NULL;
	  if (pixmap)
	    {
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = pixmap;
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = mask;
	    }
	  else
	    {
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = NULL;
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = NULL;
	    }
	}
      else if (text && pixmap)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXTEXT;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->text = g_strdup (text);
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing = spacing;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = pixmap;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = mask;
	}
      break;

    default:
      break;
    }
}

static void 
set_node_info (GtkCTree     *ctree,
	       GtkCTreeNode *node,
	       const gchar  *text,
	       guint8        spacing,
	       GdkPixmap    *pixmap_closed,
	       GdkBitmap    *mask_closed,
	       GdkPixmap    *pixmap_opened,
	       GdkBitmap    *mask_opened,
	       gboolean      is_leaf,
	       gboolean      expanded)
{
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

  if (GTK_CTREE_ROW (node)->expanded)
    gtk_ctree_node_set_pixtext (ctree, node, ctree->tree_column,
				text, spacing, pixmap_opened, mask_opened);
  else 
    gtk_ctree_node_set_pixtext (ctree, node, ctree->tree_column,
				text, spacing, pixmap_closed, mask_closed);
}

static void
tree_delete (GtkCTree     *ctree, 
	     GtkCTreeNode *node, 
	     gpointer      data)
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
  g_list_free_1 ((GList *)node);
}

static void
tree_delete_row (GtkCTree     *ctree, 
		 GtkCTreeNode *node, 
		 gpointer      data)
{
  row_delete (ctree, GTK_CTREE_ROW (node));
  g_list_free_1 ((GList *)node);
}

static void
tree_update_level (GtkCTree     *ctree, 
		   GtkCTreeNode *node, 
		   gpointer      data)
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
tree_select (GtkCTree     *ctree, 
	     GtkCTreeNode *node, 
	     gpointer      data)
{
  if (node && GTK_CTREE_ROW (node)->row.state != GTK_STATE_SELECTED &&
      GTK_CTREE_ROW (node)->row.selectable)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW],
		     node, -1);
}

static void
tree_unselect (GtkCTree     *ctree, 
	       GtkCTreeNode *node, 
	       gpointer      data)
{
  if (node && GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_UNSELECT_ROW], 
		     node, -1);
}

static void
tree_expand (GtkCTree     *ctree, 
	     GtkCTreeNode *node, 
	     gpointer      data)
{
  if (node && !GTK_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node);
}

static void
tree_collapse (GtkCTree     *ctree, 
	       GtkCTreeNode *node, 
	       gpointer      data)
{
  if (node && GTK_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node);
}

static void
tree_collapse_to_depth (GtkCTree     *ctree, 
			GtkCTreeNode *node, 
			gint          depth)
{
  if (node && GTK_CTREE_ROW (node)->level == depth)
    gtk_ctree_collapse_recursive (ctree, node);
}

static void
tree_toggle_expansion (GtkCTree     *ctree,
		       GtkCTreeNode *node,
		       gpointer      data)
{
  if (!node)
    return;

  if (GTK_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node);
  else
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node);
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

  ctree_row->row.fg_set     = FALSE;
  ctree_row->row.bg_set     = FALSE;
  ctree_row->row.selectable = TRUE;
  ctree_row->row.state      = GTK_STATE_NORMAL;
  ctree_row->row.data       = NULL;
  ctree_row->row.destroy    = NULL;

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
    GTK_CLIST_CLASS_FW (clist)->set_cell_contents
      (clist, &(ctree_row->row), i, GTK_CELL_EMPTY, NULL, 0, NULL, NULL);

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
real_select_row (GtkCList *clist,
		 gint      row,
		 gint      column,
		 GdkEvent *event)
{
  GList *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));
  
  if ((node = g_list_nth (clist->row_list, row)) &&
      GTK_CTREE_ROW (node)->row.selectable)
    gtk_signal_emit (GTK_OBJECT (clist), ctree_signals[TREE_SELECT_ROW],
		     node, column);
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
		     node, column);
}

static void
real_tree_select (GtkCTree     *ctree,
		  GtkCTreeNode *node,
		  gint          column)
{
  GtkCList *clist;
  GList *list;
  GtkCTreeNode *sel_row;
  gboolean node_selected;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED ||
      !GTK_CTREE_ROW (node)->row.selectable)
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

  tree_draw_node (ctree, node);
}

static void
real_tree_unselect (GtkCTree     *ctree,
		    GtkCTreeNode *node,
		    gint          column)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || GTK_CTREE_ROW (node)->row.state != GTK_STATE_SELECTED)
    return;

  clist = GTK_CLIST (ctree);

  if (clist->selection_end && clist->selection_end->data == node)
    clist->selection_end = clist->selection_end->prev;

  clist->selection = g_list_remove (clist->selection, node);
  
  GTK_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;

  tree_draw_node (ctree, node);
}

static void
tree_toggle_selection (GtkCTree     *ctree,
		       GtkCTreeNode *node,
		       gint          column)
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
      else if (node && GTK_CTREE_ROW (node)->row.selectable)
	gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW], 
			 node, column);
      break;

    case GTK_SELECTION_BROWSE:
      if (node && GTK_CTREE_ROW (node)->row.state == GTK_STATE_NORMAL &&
	  GTK_CTREE_ROW (node)->row.selectable)
	gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW], 
			 node, column);
      break;

    case GTK_SELECTION_EXTENDED:
      break;
    }
}

static void
select_row_recursive (GtkCTree     *ctree, 
		      GtkCTreeNode *node, 
		      gpointer      data)
{
  if (!node || GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED ||
      !GTK_CTREE_ROW (node)->row.selectable)
    return;

  GTK_CLIST (ctree)->undo_unselection = 
    g_list_prepend (GTK_CLIST (ctree)->undo_unselection, node);
  gtk_ctree_select (ctree, node);
}

static void
real_select_all (GtkCList *clist)
{
  GtkCTree *ctree;
  GtkCTreeNode *node;
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

      for (node = GTK_CTREE_NODE (clist->row_list); node;
	   node = GTK_CTREE_NODE_NEXT (node))
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
  GtkCTreeNode *node;
  GList *list;
 
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));
  
  ctree = GTK_CTREE (clist);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_BROWSE:
      if (clist->focus_row >= 0)
	{
	  gtk_ctree_select
	    (ctree,
	     GTK_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row)));
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
ctree_is_hot_spot (GtkCTree     *ctree, 
		   GtkCTreeNode *node,
		   gint          row, 
		   gint          x, 
		   gint          y)
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

  if (!clist->column[ctree->tree_column].visible)
    return FALSE;

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

static gint
real_insert_row (GtkCList *clist,
		 gint      row,
		 gchar    *text[])
{
  GtkCTreeNode *parent = NULL;
  GtkCTreeNode *sibling;
  GtkCTreeNode *node;

  g_return_val_if_fail (clist != NULL, -1);
  g_return_val_if_fail (GTK_IS_CTREE (clist), -1);

  sibling = GTK_CTREE_NODE (g_list_nth (clist->row_list, row));
  if (sibling)
    parent = GTK_CTREE_ROW (sibling)->parent;

  node = gtk_ctree_insert_node (GTK_CTREE (clist), parent, sibling, text, 5,
				NULL, NULL, NULL, NULL, TRUE, FALSE);

  if (GTK_CLIST_AUTO_SORT (clist) || !sibling)
    return g_list_position (clist->row_list, (GList *) node);
  
  return row;
}

GtkCTreeNode * 
gtk_ctree_insert_node (GtkCTree     *ctree,
		       GtkCTreeNode *parent, 
		       GtkCTreeNode *sibling,
		       gchar        *text[],
		       guint8        spacing,
		       GdkPixmap    *pixmap_closed,
		       GdkBitmap    *mask_closed,
		       GdkPixmap    *pixmap_opened,
		       GdkBitmap    *mask_opened,
		       gboolean      is_leaf,
		       gboolean      expanded)
{
  GtkCList *clist;
  GtkCTreeRow *new_row;
  GtkCTreeNode *node;
  GList *list;
  gint i;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  if (sibling)
    g_return_val_if_fail (GTK_CTREE_ROW (sibling)->parent == parent, NULL);

  if (parent && GTK_CTREE_ROW (parent)->is_leaf)
    return NULL;

  clist = GTK_CLIST (ctree);

  /* create the row */
  new_row = row_new (ctree);
  list = g_list_alloc ();
  list->data = new_row;
  node = GTK_CTREE_NODE (list);

  if (text)
    for (i = 0; i < clist->columns; i++)
      if (text[i] && i != ctree->tree_column)
	GTK_CLIST_CLASS_FW (clist)->set_cell_contents
	  (clist, &(new_row->row), i, GTK_CELL_TEXT, text[i], 0, NULL, NULL);

  set_node_info (ctree, node, text ?
		 text[ctree->tree_column] : NULL, spacing, pixmap_closed,
		 mask_closed, pixmap_opened, mask_opened, is_leaf, expanded);

  /* sorted insertion */
  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (parent)
	sibling = GTK_CTREE_ROW (parent)->children;
      else
	sibling = GTK_CTREE_NODE (clist->row_list);

      while (sibling && clist->compare
	     (clist, GTK_CTREE_ROW (node), GTK_CTREE_ROW (sibling)) > 0)
	sibling = GTK_CTREE_ROW (sibling)->sibling;
    }

  gtk_ctree_link (ctree, node, parent, sibling, TRUE);

  if (!GTK_CLIST_FROZEN (clist))
    gtk_clist_thaw (clist);

  return node;
}

GtkCTreeNode *
gtk_ctree_insert_gnode (GtkCTree          *ctree,
			GtkCTreeNode      *parent,
			GtkCTreeNode      *sibling,
			GNode             *gnode,
			GtkCTreeGNodeFunc  func,
			gpointer           data)
{
  GtkCList *clist;
  GtkCTreeNode *cnode = NULL;
  GtkCTreeNode *child = NULL;
  GtkCTreeNode *new_child;
  GList *list;
  gboolean thaw;
  GNode *work;
  guint depth = 1;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (gnode != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  if (sibling)
    g_return_val_if_fail (GTK_CTREE_ROW (sibling)->parent == parent, NULL);
  
  clist = GTK_CLIST (ctree);

  if (parent)
    depth = GTK_CTREE_ROW (parent)->level + 1;

  list = g_list_alloc ();
  list->data = row_new (ctree);
  cnode = GTK_CTREE_NODE (list);

  thaw = !GTK_CLIST_FROZEN (clist);
  if (thaw)
    gtk_clist_freeze (clist);

  set_node_info (ctree, cnode, "", 0, NULL, NULL, NULL, NULL, TRUE, FALSE);

  if (!func (ctree, depth, gnode, cnode, data))
    {
      tree_delete_row (ctree, cnode, NULL);
      return NULL;
    }

  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (parent)
	sibling = GTK_CTREE_ROW (parent)->children;
      else
	sibling = GTK_CTREE_NODE (clist->row_list);

      while (sibling && clist->compare
	     (clist, GTK_CTREE_ROW (cnode), GTK_CTREE_ROW (sibling)) > 0)
	sibling = GTK_CTREE_ROW (sibling)->sibling;
    }

  gtk_ctree_link (ctree, cnode, parent, sibling, TRUE);

  for (work = g_node_last_child (gnode); work; work = work->prev)
    {
      new_child = gtk_ctree_insert_gnode (ctree, cnode, child,
					  work, func, data);
      if (new_child)
	child = new_child;
    }	
  
  if (thaw) 
    gtk_clist_thaw (clist);

  return cnode;
}

GNode *
gtk_ctree_export_to_gnode (GtkCTree          *ctree,
			   GNode             *parent,
			   GNode             *sibling,
			   GtkCTreeNode      *node,
			   GtkCTreeGNodeFunc  func,
			   gpointer           data)
{
  GtkCTreeNode *work;
  GNode *gnode;
  GNode *new_sibling;
  gint depth;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  if (sibling)
    {
      g_return_val_if_fail (parent != NULL, NULL);
      g_return_val_if_fail (sibling->parent == parent, NULL);
    }

  gnode = g_node_new (NULL);
  depth = g_node_depth (parent) + 1;
  
  if (!func (ctree, depth, gnode, node, data))
    {
      g_node_destroy (gnode);
      return NULL;
    }

  if (parent)
    g_node_insert_before (parent, sibling, gnode);

  for (work = GTK_CTREE_ROW (node)->children, new_sibling = NULL; work;
       work = GTK_CTREE_NODE_NEXT (work))
    {
      sibling = gtk_ctree_export_to_gnode (ctree, gnode, new_sibling,
					   work, func, data);
      if (sibling)
	new_sibling = sibling;
    }
  g_node_reverse_children (gnode);

  return gnode;
}
  
static void
real_remove_row (GtkCList *clist,
		 gint      row)
{
  GtkCTreeNode *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));

  node = GTK_CTREE_NODE (g_list_nth (clist->row_list, row));

  if (node)
    gtk_ctree_remove_node (GTK_CTREE (clist), node);
}

void
gtk_ctree_remove_node (GtkCTree     *ctree, 
		       GtkCTreeNode *node)
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
  GtkCTreeNode *work;
  GtkCTreeNode *ptr;

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
  work = GTK_CTREE_NODE (clist->row_list);
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
			  GtkCTreeNode *node,
			  GtkCTreeFunc  func,
			  gpointer      data)
{
  GtkCTreeNode *work;
  GtkCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (node)
    work = GTK_CTREE_ROW (node)->children;
  else
    work = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

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
gtk_ctree_post_recursive_to_depth (GtkCTree     *ctree, 
				   GtkCTreeNode *node,
				   gint          depth,
				   GtkCTreeFunc  func,
				   gpointer      data)
{
  GtkCTreeNode *work;
  GtkCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (depth < 0)
    {
      gtk_ctree_post_recursive (ctree, node, func, data);
      return;
    }

  if (node)
    work = GTK_CTREE_ROW (node)->children;
  else
    work = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  if (work && GTK_CTREE_ROW (work)->level <= depth)
    {
      while (work)
	{
	  tmp = GTK_CTREE_ROW (work)->sibling;
	  gtk_ctree_post_recursive_to_depth (ctree, work, depth, func, data);
	  work = tmp;
	}
    }

  if (node && GTK_CTREE_ROW (node)->level <= depth)
    func (ctree, node, data);
}

void
gtk_ctree_pre_recursive (GtkCTree     *ctree, 
			 GtkCTreeNode *node,
			 GtkCTreeFunc  func,
			 gpointer      data)
{
  GtkCTreeNode *work;
  GtkCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (node)
    {
      work = GTK_CTREE_ROW (node)->children;
      func (ctree, node, data);
    }
  else
    work = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (work)
    {
      tmp = GTK_CTREE_ROW (work)->sibling;
      gtk_ctree_pre_recursive (ctree, work, func, data);
      work = tmp;
    }
}

void
gtk_ctree_pre_recursive_to_depth (GtkCTree     *ctree, 
				  GtkCTreeNode *node,
				  gint          depth, 
				  GtkCTreeFunc  func,
				  gpointer      data)
{
  GtkCTreeNode *work;
  GtkCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (depth < 0)
    {
      gtk_ctree_pre_recursive (ctree, node, func, data);
      return;
    }

  if (node)
    {
      work = GTK_CTREE_ROW (node)->children;
      if (GTK_CTREE_ROW (node)->level <= depth)
	func (ctree, node, data);
    }
  else
    work = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  if (work && GTK_CTREE_ROW (work)->level <= depth)
    {
      while (work)
	{
	  tmp = GTK_CTREE_ROW (work)->sibling;
	  gtk_ctree_pre_recursive_to_depth (ctree, work, depth, func, data);
	  work = tmp;
	}
    }
}

gboolean
gtk_ctree_is_viewable (GtkCTree     *ctree, 
		       GtkCTreeNode *node)
{ 
  GtkCTreeRow *work;

  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  work = GTK_CTREE_ROW (node);

  while (work->parent && GTK_CTREE_ROW (work->parent)->expanded)
    work = GTK_CTREE_ROW (work->parent);

  if (!work->parent)
    return TRUE;

  return FALSE;
}

GtkCTreeNode * 
gtk_ctree_last (GtkCTree     *ctree,
		GtkCTreeNode *node)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);

  if (!node) 
    return NULL;

  while (GTK_CTREE_ROW (node)->sibling)
    node = GTK_CTREE_ROW (node)->sibling;
  
  if (GTK_CTREE_ROW (node)->children)
    return gtk_ctree_last (ctree, GTK_CTREE_ROW (node)->children);
  
  return node;
}

GtkCTreeNode *
gtk_ctree_find_node_ptr (GtkCTree    *ctree,
			 GtkCTreeRow *ctree_row)
{
  GtkCTreeNode *node;
  
  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (ctree_row != NULL, FALSE);
  
  if (ctree_row->parent)
    node = GTK_CTREE_ROW(ctree_row->parent)->children;
  else
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (GTK_CTREE_ROW (node) != ctree_row)
    node = GTK_CTREE_ROW (node)->sibling;
  
  return node;
}

gboolean
gtk_ctree_find (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *child)
{
  if (!child)
    return FALSE;

  if (!node)
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

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
gtk_ctree_is_ancestor (GtkCTree     *ctree,
		       GtkCTreeNode *node,
		       GtkCTreeNode *child)
{
  g_return_val_if_fail (node != NULL, FALSE);

  return gtk_ctree_find (ctree, GTK_CTREE_ROW (node)->children, child);
}

GtkCTreeNode *
gtk_ctree_find_by_row_data (GtkCTree     *ctree,
			    GtkCTreeNode *node,
			    gpointer      data)
{
  GtkCTreeNode *work;
  
  if (!node)
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);
  
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

GList *
gtk_ctree_find_all_by_row_data (GtkCTree     *ctree,
				GtkCTreeNode *node,
				gpointer      data)
{
  GList *list = NULL;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);

  /* if node == NULL then look in the whole tree */
  if (!node)
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (node)
    {
      if (GTK_CTREE_ROW (node)->row.data == data)
        list = g_list_append (list, node);

      if (GTK_CTREE_ROW (node)->children)
        {
	  GList *sub_list;

          sub_list = gtk_ctree_find_all_by_row_data (ctree,
						     GTK_CTREE_ROW
						     (node)->children,
						     data);
          list = g_list_concat (list, sub_list);
        }
      node = GTK_CTREE_ROW (node)->sibling;
    }
  return list;
}

GtkCTreeNode *
gtk_ctree_find_by_row_data_custom (GtkCTree     *ctree,
				   GtkCTreeNode *node,
				   gpointer      data,
				   GCompareFunc  func)
{
  GtkCTreeNode *work;

  g_return_val_if_fail (func != NULL, NULL);

  if (!node)
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (node)
    {
      if (!func (GTK_CTREE_ROW (node)->row.data, data))
	return node;
      if (GTK_CTREE_ROW (node)->children &&
	  (work = gtk_ctree_find_by_row_data_custom
	   (ctree, GTK_CTREE_ROW (node)->children, data, func)))
	return work;
      node = GTK_CTREE_ROW (node)->sibling;
    }
  return NULL;
}

GList *
gtk_ctree_find_all_by_row_data_custom (GtkCTree     *ctree,
				       GtkCTreeNode *node,
				       gpointer      data,
				       GCompareFunc  func)
{
  GList *list = NULL;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (func != NULL, NULL);

  /* if node == NULL then look in the whole tree */
  if (!node)
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (node)
    {
      if (!func (GTK_CTREE_ROW (node)->row.data, data))
        list = g_list_append (list, node);

      if (GTK_CTREE_ROW (node)->children)
        {
	  GList *sub_list;

          sub_list = gtk_ctree_find_all_by_row_data_custom (ctree,
							    GTK_CTREE_ROW
							    (node)->children,
							    data,
							    func);
          list = g_list_concat (list, sub_list);
        }
      node = GTK_CTREE_ROW (node)->sibling;
    }
  return list;
}

gboolean
gtk_ctree_is_hot_spot (GtkCTree *ctree, 
		       gint      x, 
		       gint      y)
{
  GtkCTreeNode *node;
  gint column;
  gint row;
  
  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);

  if (gtk_clist_get_selection_info (GTK_CLIST (ctree), x, y, &row, &column))
    if ((node = GTK_CTREE_NODE(g_list_nth (GTK_CLIST (ctree)->row_list, row))))
      return ctree_is_hot_spot (ctree, node, row, x, y);

  return FALSE;
}


/***********************************************************
 *   Tree signals : move, expand, collapse, (un)select     *
 ***********************************************************/


void
gtk_ctree_move (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *new_parent, 
		GtkCTreeNode *new_sibling)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_MOVE], node,
		   new_parent, new_sibling);
}

void
gtk_ctree_expand (GtkCTree     *ctree,
		  GtkCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  if (GTK_CTREE_ROW (node)->is_leaf)
    return;

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node);
}

void 
gtk_ctree_expand_recursive (GtkCTree     *ctree,
			    GtkCTreeNode *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  if (((node && gtk_ctree_is_viewable (ctree, node)) || !node) && 
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
gtk_ctree_expand_to_depth (GtkCTree     *ctree,
			   GtkCTreeNode *node,
			   gint          depth)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  if (((node && gtk_ctree_is_viewable (ctree, node)) || !node) && 
      !GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  gtk_ctree_post_recursive_to_depth (ctree, node, depth,
				     GTK_CTREE_FUNC (tree_expand), NULL);

  if (thaw)
    gtk_clist_thaw (clist);
}

void
gtk_ctree_collapse (GtkCTree     *ctree,
		    GtkCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  if (GTK_CTREE_ROW (node)->is_leaf)
    return;

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node);
}

void 
gtk_ctree_collapse_recursive (GtkCTree     *ctree,
			      GtkCTreeNode *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (((node && gtk_ctree_is_viewable (ctree, node)) || !node) && 
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
gtk_ctree_collapse_to_depth (GtkCTree     *ctree,
			     GtkCTreeNode *node,
			     gint          depth)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (((node && gtk_ctree_is_viewable (ctree, node)) || !node) && 
      !GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  gtk_ctree_post_recursive_to_depth (ctree, node, depth,
				     GTK_CTREE_FUNC (tree_collapse_to_depth),
				     GINT_TO_POINTER (depth));

  if (thaw)
    gtk_clist_thaw (clist);
}

void
gtk_ctree_toggle_expansion (GtkCTree     *ctree,
			    GtkCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  if (GTK_CTREE_ROW (node)->is_leaf)
    return;

  tree_toggle_expansion (ctree, node, NULL);
}

void 
gtk_ctree_toggle_expansion_recursive (GtkCTree     *ctree,
				      GtkCTreeNode *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  
  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (((node && gtk_ctree_is_viewable (ctree, node)) || !node) && 
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
gtk_ctree_select (GtkCTree     *ctree, 
		  GtkCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (GTK_CTREE_ROW (node)->row.selectable)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW],
		     node, -1);
}

void
gtk_ctree_unselect (GtkCTree     *ctree, 
		    GtkCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_UNSELECT_ROW],
		   node, -1);
}

void
gtk_ctree_select_recursive (GtkCTree     *ctree, 
			    GtkCTreeNode *node)
{
  gtk_ctree_real_select_recursive (ctree, node, TRUE);
}

void
gtk_ctree_unselect_recursive (GtkCTree     *ctree, 
			      GtkCTreeNode *node)
{
  gtk_ctree_real_select_recursive (ctree, node, FALSE);
}

void
gtk_ctree_real_select_recursive (GtkCTree     *ctree, 
				 GtkCTreeNode *node, 
				 gint          state)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if ((state && 
       (clist->selection_mode ==  GTK_SELECTION_BROWSE ||
	clist->selection_mode == GTK_SELECTION_SINGLE)) ||
      (!state && clist->selection_mode ==  GTK_SELECTION_BROWSE))
    return;

  if (((node && gtk_ctree_is_viewable (ctree, node)) || !node) && 
      !GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
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
gtk_ctree_node_set_text (GtkCTree     *ctree,
			 GtkCTreeNode *node,
			 gint          column,
			 const gchar  *text)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;
  
  clist = GTK_CLIST (ctree);

  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, &(GTK_CTREE_ROW(node)->row), column, GTK_CELL_TEXT,
     text, 0, NULL, NULL);

  tree_draw_node (ctree, node);
}

void 
gtk_ctree_node_set_pixmap (GtkCTree     *ctree,
			   GtkCTreeNode *node,
			   gint          column,
			   GdkPixmap    *pixmap,
			   GdkBitmap    *mask)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  g_return_if_fail (pixmap != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  gdk_pixmap_ref (pixmap);
  if (mask) 
    gdk_pixmap_ref (mask);

  clist = GTK_CLIST (ctree);

  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, &(GTK_CTREE_ROW (node)->row), column, GTK_CELL_PIXMAP,
     NULL, 0, pixmap, mask);

  tree_draw_node (ctree, node);
}

void 
gtk_ctree_node_set_pixtext (GtkCTree     *ctree,
			    GtkCTreeNode *node,
			    gint          column,
			    const gchar  *text,
			    guint8        spacing,
			    GdkPixmap    *pixmap,
			    GdkBitmap    *mask)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  if (column != ctree->tree_column)
    g_return_if_fail (pixmap != NULL);
  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  clist = GTK_CLIST (ctree);

  if (pixmap)
    {
      gdk_pixmap_ref (pixmap);
      if (mask) 
	gdk_pixmap_ref (mask);
    }

  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, &(GTK_CTREE_ROW (node)->row), column, GTK_CELL_PIXTEXT,
     text, spacing, pixmap, mask);

  tree_draw_node (ctree, node);
}

void 
gtk_ctree_set_node_info (GtkCTree     *ctree,
			 GtkCTreeNode *node,
			 const gchar  *text,
			 guint8        spacing,
			 GdkPixmap    *pixmap_closed,
			 GdkBitmap    *mask_closed,
			 GdkPixmap    *pixmap_opened,
			 GdkBitmap    *mask_opened,
			 gboolean      is_leaf,
			 gboolean      expanded)
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
      GtkCTreeNode *work;
      GtkCTreeNode *ptr;
      
      work = GTK_CTREE_ROW (node)->children;
      while (work)
	{
	  ptr = work;
	  work = GTK_CTREE_ROW(work)->sibling;
	  gtk_ctree_remove_node (ctree, ptr);
	}
    }

  set_node_info (ctree, node, text, spacing, pixmap_closed, mask_closed,
		 pixmap_opened, mask_opened, is_leaf, expanded);

  if (!is_leaf && !old_leaf)
    {
      GTK_CTREE_ROW (node)->expanded = old_expanded;
      if (expanded && !old_expanded)
	gtk_ctree_expand (ctree, node);
      else if (!expanded && old_expanded)
	gtk_ctree_collapse (ctree, node);
    }

  GTK_CTREE_ROW (node)->expanded = (is_leaf) ? FALSE : expanded;
  
  tree_draw_node (ctree, node);
}

void
gtk_ctree_node_set_shift (GtkCTree     *ctree,
			  GtkCTreeNode *node,
			  gint          column,
			  gint          vertical,
			  gint          horizontal)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  GTK_CTREE_ROW (node)->row.cell[column].vertical   = vertical;
  GTK_CTREE_ROW (node)->row.cell[column].horizontal = horizontal;

  tree_draw_node (ctree, node);
}

void
gtk_ctree_node_set_selectable (GtkCTree     *ctree,
			       GtkCTreeNode *node,
			       gboolean     selectable)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (selectable == GTK_CTREE_ROW (node)->row.selectable)
    return;

  GTK_CTREE_ROW (node)->row.selectable = selectable;

  if (!selectable && GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
    {
      GtkCList *clist;

      clist = GTK_CLIST (ctree);

      if (clist->anchor >= 0 &&
	  clist->selection_mode == GTK_SELECTION_EXTENDED)
	{
	  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_FOCUS (clist)))
	    {
	      GTK_CLIST_UNSET_FLAG (clist, CLIST_DRAG_SELECTION);
	      gtk_grab_remove (GTK_WIDGET (clist));
	      gdk_pointer_ungrab (GDK_CURRENT_TIME);
	      if (clist->htimer)
		{
		  gtk_timeout_remove (clist->htimer);
		  clist->htimer = 0;
		}
	      if (clist->vtimer)
		{
		  gtk_timeout_remove (clist->vtimer);
		  clist->vtimer = 0;
		}
	    }
	  GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
	}
      gtk_ctree_unselect (ctree, node);
    }      
}

gboolean
gtk_ctree_node_get_selectable (GtkCTree     *ctree,
			       GtkCTreeNode *node)
{
  g_return_val_if_fail (node != NULL, FALSE);

  return GTK_CTREE_ROW (node)->row.selectable;
}

GtkCellType 
gtk_ctree_node_get_cell_type (GtkCTree     *ctree,
			      GtkCTreeNode *node,
			      gint          column)
{
  g_return_val_if_fail (ctree != NULL, -1);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), -1);
  g_return_val_if_fail (node != NULL, -1);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return -1;

  return GTK_CTREE_ROW (node)->row.cell[column].type;
}

gint
gtk_ctree_node_get_text (GtkCTree      *ctree,
			 GtkCTreeNode  *node,
			 gint           column,
			 gchar        **text)
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
gtk_ctree_node_get_pixmap (GtkCTree     *ctree,
			   GtkCTreeNode *node,
			   gint          column,
			   GdkPixmap   **pixmap,
			   GdkBitmap   **mask)
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
gtk_ctree_node_get_pixtext (GtkCTree      *ctree,
			    GtkCTreeNode  *node,
			    gint           column,
			    gchar        **text,
			    guint8        *spacing,
			    GdkPixmap    **pixmap,
			    GdkBitmap    **mask)
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
gtk_ctree_get_node_info (GtkCTree      *ctree,
			 GtkCTreeNode  *node,
			 gchar        **text,
			 guint8        *spacing,
			 GdkPixmap    **pixmap_closed,
			 GdkBitmap    **mask_closed,
			 GdkPixmap    **pixmap_opened,
			 GdkBitmap    **mask_opened,
			 gboolean      *is_leaf,
			 gboolean      *expanded)
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
gtk_ctree_node_set_foreground (GtkCTree     *ctree,
			       GtkCTreeNode *node,
			       GdkColor     *color)
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

  tree_draw_node (ctree, node);
}

void
gtk_ctree_node_set_background (GtkCTree     *ctree,
			       GtkCTreeNode *node,
			       GdkColor     *color)
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

  tree_draw_node (ctree, node);
}

void
gtk_ctree_node_set_row_data (GtkCTree     *ctree,
			     GtkCTreeNode *node,
			     gpointer      data)
{
  gtk_ctree_node_set_row_data_full (ctree, node, data, NULL);
}

void
gtk_ctree_node_set_row_data_full (GtkCTree         *ctree,
				  GtkCTreeNode     *node,
				  gpointer          data,
				  GtkDestroyNotify  destroy)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  GTK_CTREE_ROW (node)->row.data = data;
  GTK_CTREE_ROW (node)->row.destroy = destroy;
}

gpointer
gtk_ctree_node_get_row_data (GtkCTree     *ctree,
			     GtkCTreeNode *node)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);

  return node ? GTK_CTREE_ROW (node)->row.data : NULL;
}

void
gtk_ctree_node_moveto (GtkCTree     *ctree,
		       GtkCTreeNode *node,
		       gint          column,
		       gfloat        row_align,
		       gfloat        col_align)
{
  gint row = -1;
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  while (node && !gtk_ctree_is_viewable (ctree, node))
    node = GTK_CTREE_ROW (node)->parent;

  if (node)
    row = g_list_position (clist->row_list, (GList *)node);
  
  gtk_clist_moveto (clist, row, column, row_align, col_align);
}

GtkVisibility gtk_ctree_node_is_visible (GtkCTree     *ctree,
                                         GtkCTreeNode *node)
{
  gint row;
  
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (node != NULL, 0);
  
  row = g_list_position (GTK_CLIST (ctree)->row_list, (GList*) node);
  return gtk_clist_row_is_visible (GTK_CLIST (ctree), row);
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
gtk_ctree_show_stub (GtkCTree *ctree, 
		     gboolean  show_stub)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  show_stub = show_stub != FALSE;

  if (show_stub != ctree->show_stub)
    {
      GtkCList *clist;

      clist = GTK_CLIST (ctree);
      ctree->show_stub = show_stub;

      if (!GTK_CLIST_FROZEN (clist) && clist->rows &&
	  gtk_clist_row_is_visible (clist, 0) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row
	  (clist, NULL, 0, GTK_CLIST_ROW (clist->row_list));
    }
}

void
gtk_ctree_set_reorderable (GtkCTree *ctree, 
			   gboolean  reorderable)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  ctree->reorderable = reorderable;
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


static void
tree_sort (GtkCTree     *ctree,
	   GtkCTreeNode *node,
	   gpointer      data)
{
  GtkCTreeNode *list_start;
  GtkCTreeNode *cmp;
  GtkCTreeNode *work;
  GtkCList *clist;

  clist = GTK_CLIST (ctree);

  if (node)
    list_start = GTK_CTREE_ROW (node)->children;
  else
    list_start = GTK_CTREE_NODE (clist->row_list);

  while (list_start)
    {
      cmp = list_start;
      work = GTK_CTREE_ROW (cmp)->sibling;
      while (work)
	{
	  if (clist->sort_type == GTK_SORT_ASCENDING)
	    {
	      if (clist->compare 
		  (clist, GTK_CTREE_ROW (work), GTK_CTREE_ROW (cmp)) < 0)
		cmp = work;
	    }
	  else
	    {
	      if (clist->compare 
		  (clist, GTK_CTREE_ROW (work), GTK_CTREE_ROW (cmp)) > 0)
		cmp = work;
	    }
	  work = GTK_CTREE_ROW (work)->sibling;
	}
      if (cmp == list_start)
	list_start = GTK_CTREE_ROW (cmp)->sibling;
      else
	{
	  gtk_ctree_unlink (ctree, cmp, FALSE);
	  gtk_ctree_link (ctree, cmp, node, list_start, FALSE);
	}
    }
}

void
gtk_ctree_sort_recursive (GtkCTree     *ctree, 
			  GtkCTreeNode *node)
{
  GtkCList *clist;
  GtkCTreeNode *focus_node = NULL;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (!GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (!node || (node && gtk_ctree_is_viewable (ctree, node)))
    focus_node =
      GTK_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row));
      
  gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (tree_sort), NULL);

  if (!node)
    tree_sort (ctree, NULL, NULL);

  if (focus_node)
    {
      clist->focus_row = g_list_position (clist->row_list,(GList *)focus_node);
      clist->undo_anchor = clist->focus_row;
    }

  if (thaw)
    gtk_clist_thaw (clist);
}

static void
real_sort_list (GtkCList *clist)
{
  gtk_ctree_sort_recursive (GTK_CTREE (clist), NULL);
}

void
gtk_ctree_sort_node (GtkCTree     *ctree, 
		     GtkCTreeNode *node)
{
  GtkCList *clist;
  GtkCTreeNode *focus_node = NULL;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (!GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      if (clist->anchor != -1)
	GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (!node || (node && gtk_ctree_is_viewable (ctree, node)))
    focus_node = GTK_CTREE_NODE
      (g_list_nth (clist->row_list, clist->focus_row));

  tree_sort (ctree, node, NULL);

  if (focus_node)
    {
      clist->focus_row = g_list_position (clist->row_list,(GList *)focus_node);
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
      if (GTK_CTREE_ROW (focus_node)->row.state == GTK_STATE_NORMAL &&
	  GTK_CTREE_ROW (focus_node)->row.selectable)
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
      tree_draw_node (GTK_CTREE (clist), GTK_CTREE_NODE (list->data));
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
  GtkCTreeNode *node;
  gint i;
  gint e;
  gint row;
  gboolean thaw = FALSE;
  gboolean unselect;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CTREE (clist));

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

	  if (gtk_ctree_is_viewable (ctree, node))
	    {
	      row = g_list_position (clist->row_list, (GList *)node);
	      if (row >= i && row <= e)
		unselect = FALSE;
	    }
	  if (unselect && GTK_CTREE_ROW (node)->row.selectable)
	    {
	      GTK_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;
	      gtk_ctree_unselect (ctree, node);
	      clist->undo_selection = g_list_prepend (clist->undo_selection,
						      node);
	    }
	}
    }    


  for (node = GTK_CTREE_NODE (g_list_nth (clist->row_list, i)); i <= e;
       i++, node = GTK_CTREE_NODE_NEXT (node))
    if (GTK_CTREE_ROW (node)->row.selectable)
      {
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
  g_return_if_fail (GTK_IS_CTREE (clist));

  if (clist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (!(clist->undo_selection || clist->undo_unselection))
    {
      gtk_clist_unselect_all (clist);
      return;
    }

  ctree = GTK_CTREE (clist);

  for (work = clist->undo_selection; work; work = work->next)
    if (GTK_CTREE_ROW (work->data)->row.selectable)
      gtk_ctree_select (ctree, GTK_CTREE_NODE (work->data));

  for (work = clist->undo_unselection; work; work = work->next)
    if (GTK_CTREE_ROW (work->data)->row.selectable)
      gtk_ctree_unselect (ctree, GTK_CTREE_NODE (work->data));

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

void
gtk_ctree_set_drag_compare_func (GtkCTree                *ctree,
				 GtkCTreeCompareDragFunc  cmp_func)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  ctree->drag_compare = cmp_func;
}

static void
set_mouse_cursor (GtkCTree *ctree,
		  gboolean  enable)
{
  GdkCursor *cursor;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (enable)
    cursor = gdk_cursor_new (GDK_LEFT_PTR);
  else
    cursor = gdk_cursor_new (GDK_CIRCLE);

  gdk_window_set_cursor (GTK_CLIST (ctree)->clist_window, cursor);
  gdk_cursor_destroy (cursor);
}

static void
check_cursor (GtkCTree *ctree)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!GTK_CTREE_ROW (ctree->drag_source)->children ||
      !gtk_ctree_is_ancestor (ctree, ctree->drag_source, ctree->drag_target))
    {
      if (ctree->insert_pos == GTK_CTREE_POS_AFTER)
	{
	  if (GTK_CTREE_ROW (ctree->drag_target)->sibling !=
	      ctree->drag_source)
	    set_mouse_cursor
	      (ctree,
	       (!ctree->drag_compare ||
		ctree->drag_compare
		(ctree,
		 ctree->drag_source,
		 GTK_CTREE_ROW (ctree->drag_target)->parent,
		 GTK_CTREE_ROW (ctree->drag_target)->sibling)));
	}
      else if (ctree->insert_pos == GTK_CTREE_POS_BEFORE)
	{
	  if (GTK_CTREE_ROW (ctree->drag_source)->sibling !=
	      ctree->drag_target)
	    set_mouse_cursor
	      (ctree,
	       (!ctree->drag_compare ||
		ctree->drag_compare
		(ctree,
		 ctree->drag_source,
		 GTK_CTREE_ROW (ctree->drag_target)->parent,
		 ctree->drag_target)));
	}
      else if (!GTK_CTREE_ROW (ctree->drag_target)->is_leaf)
	{
	  if (GTK_CTREE_ROW (ctree->drag_target)->children !=
	      ctree->drag_source)
	    set_mouse_cursor
	      (ctree,
	       (!ctree->drag_compare ||
		ctree->drag_compare
		(ctree,
		 ctree->drag_source,
		 ctree->drag_target,
		 GTK_CTREE_ROW (ctree->drag_target)->children)));
	}
    }
  else
    set_mouse_cursor(ctree, FALSE);
}
