/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald, 
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>  
 *
 * GtkCTree widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdlib.h>

#undef GDK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#define __GTK_CTREE_C__

#include "gtkctree.h"
#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkdnd.h"
#include "gtkintl.h"
#include <gdk/gdkkeysyms.h>

#include "gtkalias.h"

#define PM_SIZE                    8
#define TAB_SIZE                   (PM_SIZE + 6)
#define CELL_SPACING               1
#define CLIST_OPTIMUM_SIZE         64
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

#define CLIST_UNFROZEN(clist)     (((GtkCList*) (clist))->freeze_count == 0)
#define CLIST_REFRESH(clist)    G_STMT_START { \
  if (CLIST_UNFROZEN (clist)) \
    GTK_CLIST_GET_CLASS (clist)->refresh ((GtkCList*) (clist)); \
} G_STMT_END


enum {
  ARG_0,
  ARG_N_COLUMNS,
  ARG_TREE_COLUMN,
  ARG_INDENT,
  ARG_SPACING,
  ARG_SHOW_STUB,
  ARG_LINE_STYLE,
  ARG_EXPANDER_STYLE
};


static void     gtk_ctree_class_init    (GtkCTreeClass         *klass);
static void     gtk_ctree_init          (GtkCTree              *ctree);
static GObject* gtk_ctree_constructor   (GType                  type,
				         guint                  n_construct_properties,
				         GObjectConstructParam *construct_params);
static void gtk_ctree_set_arg		(GtkObject      *object,
					 GtkArg         *arg,
					 guint           arg_id);
static void gtk_ctree_get_arg      	(GtkObject      *object,
					 GtkArg         *arg,
					 guint           arg_id);
static void gtk_ctree_realize           (GtkWidget      *widget);
static void gtk_ctree_unrealize         (GtkWidget      *widget);
static gint gtk_ctree_button_press      (GtkWidget      *widget,
					 GdkEventButton *event);
static void ctree_attach_styles         (GtkCTree       *ctree,
					 GtkCTreeNode   *node,
					 gpointer        data);
static void ctree_detach_styles         (GtkCTree       *ctree,
					 GtkCTreeNode   *node, 
					 gpointer        data);
static gint draw_cell_pixmap            (GdkWindow      *window,
					 GdkRectangle   *clip_rectangle,
					 GdkGC          *fg_gc,
					 GdkPixmap      *pixmap,
					 GdkBitmap      *mask,
					 gint            x,
					 gint            y,
					 gint            width,
					 gint            height);
static void get_cell_style              (GtkCList       *clist,
					 GtkCListRow    *clist_row,
					 gint            state,
					 gint            column,
					 GtkStyle      **style,
					 GdkGC         **fg_gc,
					 GdkGC         **bg_gc);
static gint gtk_ctree_draw_expander     (GtkCTree       *ctree,
					 GtkCTreeRow    *ctree_row,
					 GtkStyle       *style,
					 GdkRectangle   *clip_rectangle,
					 gint            x);
static gint gtk_ctree_draw_lines        (GtkCTree       *ctree,
					 GtkCTreeRow    *ctree_row,
					 gint            row,
					 gint            column,
					 gint            state,
					 GdkRectangle   *clip_rectangle,
					 GdkRectangle   *cell_rectangle,
					 GdkRectangle   *crect,
					 GdkRectangle   *area,
					 GtkStyle       *style);
static void draw_row                    (GtkCList       *clist,
					 GdkRectangle   *area,
					 gint            row,
					 GtkCListRow    *clist_row);
static void draw_drag_highlight         (GtkCList        *clist,
					 GtkCListRow     *dest_row,
					 gint             dest_row_number,
					 GtkCListDragPos  drag_pos);
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
static void real_tree_expand            (GtkCTree      *ctree,
					 GtkCTreeNode  *node);
static void real_tree_collapse          (GtkCTree      *ctree,
					 GtkCTreeNode  *node);
static void real_tree_move              (GtkCTree      *ctree,
					 GtkCTreeNode  *node,
					 GtkCTreeNode  *new_parent, 
					 GtkCTreeNode  *new_sibling);
static void real_row_move               (GtkCList      *clist,
					 gint           source_row,
					 gint           dest_row);
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
static void cell_size_request           (GtkCList       *clist,
					 GtkCListRow    *clist_row,
					 gint            column,
					 GtkRequisition *requisition);
static void column_auto_resize          (GtkCList       *clist,
					 GtkCListRow    *clist_row,
					 gint            column,
					 gint            old_width);
static void auto_resize_columns         (GtkCList       *clist);


static gboolean check_drag               (GtkCTree         *ctree,
			                  GtkCTreeNode     *drag_source,
					  GtkCTreeNode     *drag_target,
					  GtkCListDragPos   insert_pos);
static void gtk_ctree_drag_begin         (GtkWidget        *widget,
					  GdkDragContext   *context);
static gint gtk_ctree_drag_motion        (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  guint             time);
static void gtk_ctree_drag_data_received (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  GtkSelectionData *selection_data,
					  guint             info,
					  guint32           time);
static void remove_grab                  (GtkCList         *clist);
static void drag_dest_cell               (GtkCList         *clist,
					  gint              x,
					  gint              y,
					  GtkCListDestInfo *dest_info);


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

static GtkCListClass *parent_class = NULL;
static GtkContainerClass *container_class = NULL;
static guint ctree_signals[LAST_SIGNAL] = {0};


GtkType
gtk_ctree_get_type (void)
{
  static GtkType ctree_type = 0;

  if (!ctree_type)
    {
      static const GtkTypeInfo ctree_info =
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

      I_("GtkCTree");
      ctree_type = gtk_type_unique (GTK_TYPE_CLIST, &ctree_info);
    }

  return ctree_type;
}

static void
gtk_ctree_class_init (GtkCTreeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkCListClass *clist_class;
  GtkBindingSet *binding_set;

  gobject_class->constructor = gtk_ctree_constructor;

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;
  clist_class = (GtkCListClass *) klass;

  parent_class = gtk_type_class (GTK_TYPE_CLIST);
  container_class = gtk_type_class (GTK_TYPE_CONTAINER);

  object_class->set_arg = gtk_ctree_set_arg;
  object_class->get_arg = gtk_ctree_get_arg;

  widget_class->realize = gtk_ctree_realize;
  widget_class->unrealize = gtk_ctree_unrealize;
  widget_class->button_press_event = gtk_ctree_button_press;

  widget_class->drag_begin = gtk_ctree_drag_begin;
  widget_class->drag_motion = gtk_ctree_drag_motion;
  widget_class->drag_data_received = gtk_ctree_drag_data_received;

  clist_class->select_row = real_select_row;
  clist_class->unselect_row = real_unselect_row;
  clist_class->row_move = real_row_move;
  clist_class->undo_selection = real_undo_selection;
  clist_class->resync_selection = resync_selection;
  clist_class->selection_find = selection_find;
  clist_class->click_column = NULL;
  clist_class->draw_row = draw_row;
  clist_class->draw_drag_highlight = draw_drag_highlight;
  clist_class->clear = real_clear;
  clist_class->select_all = real_select_all;
  clist_class->unselect_all = real_unselect_all;
  clist_class->fake_unselect_all = fake_unselect_all;
  clist_class->insert_row = real_insert_row;
  clist_class->remove_row = real_remove_row;
  clist_class->sort_list = real_sort_list;
  clist_class->set_cell_contents = set_cell_contents;
  clist_class->cell_size_request = cell_size_request;

  klass->tree_select_row = real_tree_select;
  klass->tree_unselect_row = real_tree_unselect;
  klass->tree_expand = real_tree_expand;
  klass->tree_collapse = real_tree_collapse;
  klass->tree_move = real_tree_move;
  klass->change_focus_row_expansion = change_focus_row_expansion;

  gtk_object_add_arg_type ("GtkCTree::n-columns", /* overrides GtkCList::n_columns!! */
			   GTK_TYPE_UINT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME,
			   ARG_N_COLUMNS);
  gtk_object_add_arg_type ("GtkCTree::tree-column",
			   GTK_TYPE_UINT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME,
			   ARG_TREE_COLUMN);
  gtk_object_add_arg_type ("GtkCTree::indent",
			   GTK_TYPE_UINT,
			   GTK_ARG_READWRITE | G_PARAM_STATIC_NAME,
			   ARG_INDENT);
  gtk_object_add_arg_type ("GtkCTree::spacing",
			   GTK_TYPE_UINT,
			   GTK_ARG_READWRITE | G_PARAM_STATIC_NAME,
			   ARG_SPACING);
  gtk_object_add_arg_type ("GtkCTree::show-stub",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE | G_PARAM_STATIC_NAME,
			   ARG_SHOW_STUB);
  gtk_object_add_arg_type ("GtkCTree::line-style",
			   GTK_TYPE_CTREE_LINE_STYLE,
			   GTK_ARG_READWRITE | G_PARAM_STATIC_NAME,
			   ARG_LINE_STYLE);
  gtk_object_add_arg_type ("GtkCTree::expander-style",
			   GTK_TYPE_CTREE_EXPANDER_STYLE,
			   GTK_ARG_READWRITE | G_PARAM_STATIC_NAME,
			   ARG_EXPANDER_STYLE);

  ctree_signals[TREE_SELECT_ROW] =
    gtk_signal_new (I_("tree-select-row"),
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_select_row),
		    _gtk_marshal_VOID__POINTER_INT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_CTREE_NODE,
		    GTK_TYPE_INT);
  ctree_signals[TREE_UNSELECT_ROW] =
    gtk_signal_new (I_("tree-unselect-row"),
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_unselect_row),
		    _gtk_marshal_VOID__POINTER_INT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_CTREE_NODE,
		    GTK_TYPE_INT);
  ctree_signals[TREE_EXPAND] =
    gtk_signal_new (I_("tree-expand"),
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_expand),
		    _gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_CTREE_NODE);
  ctree_signals[TREE_COLLAPSE] =
    gtk_signal_new (I_("tree-collapse"),
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_collapse),
		    _gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_CTREE_NODE);
  ctree_signals[TREE_MOVE] =
    gtk_signal_new (I_("tree-move"),
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkCTreeClass, tree_move),
		    _gtk_marshal_VOID__POINTER_POINTER_POINTER,
		    GTK_TYPE_NONE, 3,
		    GTK_TYPE_CTREE_NODE,
		    GTK_TYPE_CTREE_NODE,
		    GTK_TYPE_CTREE_NODE);
  ctree_signals[CHANGE_FOCUS_ROW_EXPANSION] =
    gtk_signal_new (I_("change-focus-row-expansion"),
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkCTreeClass,
				       change_focus_row_expansion),
		    _gtk_marshal_VOID__ENUM,
		    GTK_TYPE_NONE, 1, GTK_TYPE_CTREE_EXPANSION_TYPE);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
				GDK_plus, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_EXPAND);
  gtk_binding_entry_add_signal (binding_set,
				GDK_plus, GDK_CONTROL_MASK,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_EXPAND_RECURSIVE);

  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Add, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_EXPAND);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Add, GDK_CONTROL_MASK,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_EXPAND_RECURSIVE);
  
  gtk_binding_entry_add_signal (binding_set,
				GDK_minus, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_COLLAPSE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_minus, GDK_CONTROL_MASK,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM,
				GTK_CTREE_EXPANSION_COLLAPSE_RECURSIVE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Subtract, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_COLLAPSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Subtract, GDK_CONTROL_MASK,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM,
				GTK_CTREE_EXPANSION_COLLAPSE_RECURSIVE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_equal, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_TOGGLE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Equal, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_TOGGLE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Multiply, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_TOGGLE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_asterisk, 0,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM, GTK_CTREE_EXPANSION_TOGGLE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Multiply, GDK_CONTROL_MASK,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM,
				GTK_CTREE_EXPANSION_TOGGLE_RECURSIVE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_asterisk, GDK_CONTROL_MASK,
				"change-focus-row-expansion", 1,
				GTK_TYPE_ENUM,
				GTK_CTREE_EXPANSION_TOGGLE_RECURSIVE);  
}

static void
gtk_ctree_set_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkCTree *ctree;
  GtkCList *clist;

  ctree = GTK_CTREE (object);
  clist = GTK_CLIST (ctree);

  switch (arg_id)
    {
    case ARG_N_COLUMNS: /* construct-only arg, only set at construction time */
      clist->columns = MAX (1, GTK_VALUE_UINT (*arg));
      ctree->tree_column = CLAMP (ctree->tree_column, 0, clist->columns);
      break;
    case ARG_TREE_COLUMN: /* construct-only arg, only set at construction time */
      ctree->tree_column = GTK_VALUE_UINT (*arg);
      ctree->tree_column = CLAMP (ctree->tree_column, 0, clist->columns);
      break;
    case ARG_INDENT:
      gtk_ctree_set_indent (ctree, GTK_VALUE_UINT (*arg));
      break;
    case ARG_SPACING:
      gtk_ctree_set_spacing (ctree, GTK_VALUE_UINT (*arg));
      break;
    case ARG_SHOW_STUB:
      gtk_ctree_set_show_stub (ctree, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_LINE_STYLE:
      gtk_ctree_set_line_style (ctree, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_EXPANDER_STYLE:
      gtk_ctree_set_expander_style (ctree, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_ctree_get_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkCTree *ctree;

  ctree = GTK_CTREE (object);

  switch (arg_id)
    {
    case ARG_N_COLUMNS:
      GTK_VALUE_UINT (*arg) = GTK_CLIST (ctree)->columns;
      break;
    case ARG_TREE_COLUMN:
      GTK_VALUE_UINT (*arg) = ctree->tree_column;
      break;
    case ARG_INDENT:
      GTK_VALUE_UINT (*arg) = ctree->tree_indent;
      break;
    case ARG_SPACING:
      GTK_VALUE_UINT (*arg) = ctree->tree_spacing;
      break;
    case ARG_SHOW_STUB:
      GTK_VALUE_BOOL (*arg) = ctree->show_stub;
      break;
    case ARG_LINE_STYLE:
      GTK_VALUE_ENUM (*arg) = ctree->line_style;
      break;
    case ARG_EXPANDER_STYLE:
      GTK_VALUE_ENUM (*arg) = ctree->expander_style;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_ctree_init (GtkCTree *ctree)
{
  GtkCList *clist;

  GTK_CLIST_SET_FLAG (ctree, CLIST_DRAW_DRAG_RECT);
  GTK_CLIST_SET_FLAG (ctree, CLIST_DRAW_DRAG_LINE);

  clist = GTK_CLIST (ctree);

  ctree->tree_indent    = 20;
  ctree->tree_spacing   = 5;
  ctree->tree_column    = 0;
  ctree->line_style     = GTK_CTREE_LINES_SOLID;
  ctree->expander_style = GTK_CTREE_EXPANDER_SQUARE;
  ctree->drag_compare   = NULL;
  ctree->show_stub      = TRUE;

  clist->button_actions[0] |= GTK_BUTTON_EXPANDS;
}

static void
ctree_attach_styles (GtkCTree     *ctree,
		     GtkCTreeNode *node,
		     gpointer      data)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (ctree);

  if (GTK_CTREE_ROW (node)->row.style)
    GTK_CTREE_ROW (node)->row.style =
      gtk_style_attach (GTK_CTREE_ROW (node)->row.style, clist->clist_window);

  if (GTK_CTREE_ROW (node)->row.fg_set || GTK_CTREE_ROW (node)->row.bg_set)
    {
      GdkColormap *colormap;

      colormap = gtk_widget_get_colormap (GTK_WIDGET (ctree));
      if (GTK_CTREE_ROW (node)->row.fg_set)
	gdk_colormap_alloc_color (colormap,
                                  &(GTK_CTREE_ROW (node)->row.foreground),
                                  FALSE, TRUE);
      if (GTK_CTREE_ROW (node)->row.bg_set)
	gdk_colormap_alloc_color (colormap,
                                  &(GTK_CTREE_ROW (node)->row.background),
                                  FALSE, TRUE);
    }

  for (i = 0; i < clist->columns; i++)
    if  (GTK_CTREE_ROW (node)->row.cell[i].style)
      GTK_CTREE_ROW (node)->row.cell[i].style =
	gtk_style_attach (GTK_CTREE_ROW (node)->row.cell[i].style,
			  clist->clist_window);
}

static void
ctree_detach_styles (GtkCTree     *ctree,
		     GtkCTreeNode *node,
		     gpointer      data)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (ctree);

  if (GTK_CTREE_ROW (node)->row.style)
    gtk_style_detach (GTK_CTREE_ROW (node)->row.style);
  for (i = 0; i < clist->columns; i++)
    if  (GTK_CTREE_ROW (node)->row.cell[i].style)
      gtk_style_detach (GTK_CTREE_ROW (node)->row.cell[i].style);
}

static void
gtk_ctree_realize (GtkWidget *widget)
{
  GtkCTree *ctree;
  GtkCList *clist;
  GdkGCValues values;
  GtkCTreeNode *node;
  GtkCTreeNode *child;
  gint i;

  g_return_if_fail (GTK_IS_CTREE (widget));

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  ctree = GTK_CTREE (widget);
  clist = GTK_CLIST (widget);

  node = GTK_CTREE_NODE (clist->row_list);
  for (i = 0; i < clist->rows; i++)
    {
      if (GTK_CTREE_ROW (node)->children && !GTK_CTREE_ROW (node)->expanded)
	for (child = GTK_CTREE_ROW (node)->children; child;
	     child = GTK_CTREE_ROW (child)->sibling)
	  gtk_ctree_pre_recursive (ctree, child, ctree_attach_styles, NULL);
      node = GTK_CTREE_NODE_NEXT (node);
    }

  values.foreground = widget->style->fg[GTK_STATE_NORMAL];
  values.background = widget->style->base[GTK_STATE_NORMAL];
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
      gint8 dashes[] = { 1, 1 };

      gdk_gc_set_line_attributes (ctree->lines_gc, 1, 
				  GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
      gdk_gc_set_dashes (ctree->lines_gc, 0, dashes, G_N_ELEMENTS (dashes));
    }
}

static void
gtk_ctree_unrealize (GtkWidget *widget)
{
  GtkCTree *ctree;
  GtkCList *clist;

  g_return_if_fail (GTK_IS_CTREE (widget));

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

  ctree = GTK_CTREE (widget);
  clist = GTK_CLIST (widget);

  if (gtk_widget_get_realized (widget))
    {
      GtkCTreeNode *node;
      GtkCTreeNode *child;
      gint i;

      node = GTK_CTREE_NODE (clist->row_list);
      for (i = 0; i < clist->rows; i++)
	{
	  if (GTK_CTREE_ROW (node)->children &&
	      !GTK_CTREE_ROW (node)->expanded)
	    for (child = GTK_CTREE_ROW (node)->children; child;
		 child = GTK_CTREE_ROW (child)->sibling)
	      gtk_ctree_pre_recursive(ctree, child, ctree_detach_styles, NULL);
	  node = GTK_CTREE_NODE_NEXT (node);
	}
    }

  g_object_unref (ctree->lines_gc);
}

static gint
gtk_ctree_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkCTree *ctree;
  GtkCList *clist;
  gint button_actions;

  g_return_val_if_fail (GTK_IS_CTREE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ctree = GTK_CTREE (widget);
  clist = GTK_CLIST (widget);

  button_actions = clist->button_actions[event->button - 1];

  if (button_actions == GTK_BUTTON_IGNORED)
    return FALSE;

  if (event->window == clist->clist_window)
    {
      GtkCTreeNode *work;
      gint x;
      gint y;
      gint row;
      gint column;

      x = event->x;
      y = event->y;

      if (!gtk_clist_get_selection_info (clist, x, y, &row, &column))
	return FALSE;

      work = GTK_CTREE_NODE (g_list_nth (clist->row_list, row));
	  
      if (button_actions & GTK_BUTTON_EXPANDS &&
	  (GTK_CTREE_ROW (work)->children && !GTK_CTREE_ROW (work)->is_leaf  &&
	   (event->type == GDK_2BUTTON_PRESS ||
	    ctree_is_hot_spot (ctree, work, row, x, y))))
	{
	  if (GTK_CTREE_ROW (work)->expanded)
	    gtk_ctree_collapse (ctree, work);
	  else
	    gtk_ctree_expand (ctree, work);

	  return TRUE;
	}
    }
  
  return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
}

static void
draw_drag_highlight (GtkCList        *clist,
		     GtkCListRow     *dest_row,
		     gint             dest_row_number,
		     GtkCListDragPos  drag_pos)
{
  GtkCTree *ctree;
  GdkPoint points[4];
  gint level;
  gint i;
  gint y = 0;

  g_return_if_fail (GTK_IS_CTREE (clist));

  ctree = GTK_CTREE (clist);

  level = ((GtkCTreeRow *)(dest_row))->level;

  y = ROW_TOP_YPIXEL (clist, dest_row_number) - 1;

  switch (drag_pos)
    {
    case GTK_CLIST_DRAG_NONE:
      break;
    case GTK_CLIST_DRAG_AFTER:
      y += clist->row_height + 1;
    case GTK_CLIST_DRAG_BEFORE:
      
      if (clist->column[ctree->tree_column].visible)
	switch (clist->column[ctree->tree_column].justification)
	  {
	  case GTK_JUSTIFY_CENTER:
	  case GTK_JUSTIFY_FILL:
	  case GTK_JUSTIFY_LEFT:
	    if (ctree->tree_column > 0)
	      gdk_draw_line (clist->clist_window, clist->xor_gc, 
			     COLUMN_LEFT_XPIXEL(clist, 0), y,
			     COLUMN_LEFT_XPIXEL(clist, ctree->tree_column - 1)+
			     clist->column[ctree->tree_column - 1].area.width,
			     y);

	    gdk_draw_line (clist->clist_window, clist->xor_gc, 
			   COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) + 
			   ctree->tree_indent * level -
			   (ctree->tree_indent - PM_SIZE) / 2, y,
			   GTK_WIDGET (ctree)->allocation.width, y);
	    break;
	  case GTK_JUSTIFY_RIGHT:
	    if (ctree->tree_column < clist->columns - 1)
	      gdk_draw_line (clist->clist_window, clist->xor_gc, 
			     COLUMN_LEFT_XPIXEL(clist, ctree->tree_column + 1),
			     y,
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
      break;
    case GTK_CLIST_DRAG_INTO:
      y = ROW_TOP_YPIXEL (clist, dest_row_number) + clist->row_height;

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
		points[0].x = COLUMN_LEFT_XPIXEL(clist,
						 ctree->tree_column - 1) +
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
	      ctree->tree_indent * level + (ctree->tree_indent - PM_SIZE) / 2 +
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
		points[0].x = COLUMN_LEFT_XPIXEL(clist, ctree->tree_column +1);
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
      break;
    }
}

static gint
draw_cell_pixmap (GdkWindow    *window,
		  GdkRectangle *clip_rectangle,
		  GdkGC        *fg_gc,
		  GdkPixmap    *pixmap,
		  GdkBitmap    *mask,
		  gint          x,
		  gint          y,
		  gint          width,
		  gint          height)
{
  gint xsrc = 0;
  gint ysrc = 0;

  if (mask)
    {
      gdk_gc_set_clip_mask (fg_gc, mask);
      gdk_gc_set_clip_origin (fg_gc, x, y);
    }
  if (x < clip_rectangle->x)
    {
      xsrc = clip_rectangle->x - x;
      width -= xsrc;
      x = clip_rectangle->x;
    }
  if (x + width > clip_rectangle->x + clip_rectangle->width)
    width = clip_rectangle->x + clip_rectangle->width - x;

  if (y < clip_rectangle->y)
    {
      ysrc = clip_rectangle->y - y;
      height -= ysrc;
      y = clip_rectangle->y;
    }
  if (y + height > clip_rectangle->y + clip_rectangle->height)
    height = clip_rectangle->y + clip_rectangle->height - y;

  if (width > 0 && height > 0)
    gdk_draw_drawable (window, fg_gc, pixmap, xsrc, ysrc, x, y, width, height);

  if (mask)
    {
      gdk_gc_set_clip_rectangle (fg_gc, NULL);
      gdk_gc_set_clip_origin (fg_gc, 0, 0);
    }

  return x + MAX (width, 0);
}

static void
get_cell_style (GtkCList     *clist,
		GtkCListRow  *clist_row,
		gint          state,
		gint          column,
		GtkStyle    **style,
		GdkGC       **fg_gc,
		GdkGC       **bg_gc)
{
  gint fg_state;

  if ((state == GTK_STATE_NORMAL) &&
      (GTK_WIDGET (clist)->state == GTK_STATE_INSENSITIVE))
    fg_state = GTK_STATE_INSENSITIVE;
  else
    fg_state = state;

  if (clist_row->cell[column].style)
    {
      if (style)
	*style = clist_row->cell[column].style;
      if (fg_gc)
	*fg_gc = clist_row->cell[column].style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = clist_row->cell[column].style->bg_gc[state];
	else
	  *bg_gc = clist_row->cell[column].style->base_gc[state];
      }
    }
  else if (clist_row->style)
    {
      if (style)
	*style = clist_row->style;
      if (fg_gc)
	*fg_gc = clist_row->style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = clist_row->style->bg_gc[state];
	else
	  *bg_gc = clist_row->style->base_gc[state];
      }
    }
  else
    {
      if (style)
	*style = GTK_WIDGET (clist)->style;
      if (fg_gc)
	*fg_gc = GTK_WIDGET (clist)->style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = GTK_WIDGET (clist)->style->bg_gc[state];
	else
	  *bg_gc = GTK_WIDGET (clist)->style->base_gc[state];
      }

      if (state != GTK_STATE_SELECTED)
	{
	  if (fg_gc && clist_row->fg_set)
	    *fg_gc = clist->fg_gc;
	  if (bg_gc && clist_row->bg_set)
	    *bg_gc = clist->bg_gc;
	}
    }
}

static gint
gtk_ctree_draw_expander (GtkCTree     *ctree,
			 GtkCTreeRow  *ctree_row,
			 GtkStyle     *style,
			 GdkRectangle *clip_rectangle,
			 gint          x)
{
  GtkCList *clist;
  GdkPoint points[3];
  gint justification_factor;
  gint y;

 if (ctree->expander_style == GTK_CTREE_EXPANDER_NONE)
   return x;

  clist = GTK_CLIST (ctree);
  if (clist->column[ctree->tree_column].justification == GTK_JUSTIFY_RIGHT)
    justification_factor = -1;
  else
    justification_factor = 1;
  y = (clip_rectangle->y + (clip_rectangle->height - PM_SIZE) / 2 -
       (clip_rectangle->height + 1) % 2);

  if (!ctree_row->children)
    {
      switch (ctree->expander_style)
	{
	case GTK_CTREE_EXPANDER_NONE:
	  return x;
	case GTK_CTREE_EXPANDER_TRIANGLE:
	  return x + justification_factor * (PM_SIZE + 3);
	case GTK_CTREE_EXPANDER_SQUARE:
	case GTK_CTREE_EXPANDER_CIRCULAR:
	  return x + justification_factor * (PM_SIZE + 1);
	}
    }

  gdk_gc_set_clip_rectangle (style->fg_gc[GTK_STATE_NORMAL], clip_rectangle);
  gdk_gc_set_clip_rectangle (style->base_gc[GTK_STATE_NORMAL], clip_rectangle);

  switch (ctree->expander_style)
    {
    case GTK_CTREE_EXPANDER_NONE:
      break;
    case GTK_CTREE_EXPANDER_TRIANGLE:
      if (ctree_row->expanded)
	{
	  points[0].x = x;
	  points[0].y = y + (PM_SIZE + 2) / 6;
	  points[1].x = points[0].x + justification_factor * (PM_SIZE + 2);
	  points[1].y = points[0].y;
	  points[2].x = (points[0].x +
			 justification_factor * (PM_SIZE + 2) / 2);
	  points[2].y = y + 2 * (PM_SIZE + 2) / 3;
	}
      else
	{
	  points[0].x = x + justification_factor * ((PM_SIZE + 2) / 6 + 2);
	  points[0].y = y - 1;
	  points[1].x = points[0].x;
	  points[1].y = points[0].y + (PM_SIZE + 2);
	  points[2].x = (points[0].x +
			 justification_factor * (2 * (PM_SIZE + 2) / 3 - 1));
	  points[2].y = points[0].y + (PM_SIZE + 2) / 2;
	}

      gdk_draw_polygon (clist->clist_window, style->base_gc[GTK_STATE_NORMAL],
			TRUE, points, 3);
      gdk_draw_polygon (clist->clist_window, style->fg_gc[GTK_STATE_NORMAL],
			FALSE, points, 3);

      x += justification_factor * (PM_SIZE + 3);
      break;
    case GTK_CTREE_EXPANDER_SQUARE:
    case GTK_CTREE_EXPANDER_CIRCULAR:
      if (justification_factor == -1)
	x += justification_factor * (PM_SIZE + 1);

      if (ctree->expander_style == GTK_CTREE_EXPANDER_CIRCULAR)
	{
	  gdk_draw_arc (clist->clist_window, style->base_gc[GTK_STATE_NORMAL],
			TRUE, x, y, PM_SIZE, PM_SIZE, 0, 360 * 64);
	  gdk_draw_arc (clist->clist_window, style->fg_gc[GTK_STATE_NORMAL],
			FALSE, x, y, PM_SIZE, PM_SIZE, 0, 360 * 64);
	}
      else
	{
	  gdk_draw_rectangle (clist->clist_window,
			      style->base_gc[GTK_STATE_NORMAL], TRUE,
			      x, y, PM_SIZE, PM_SIZE);
	  gdk_draw_rectangle (clist->clist_window,
			      style->fg_gc[GTK_STATE_NORMAL], FALSE,
			      x, y, PM_SIZE, PM_SIZE);
	}

      gdk_draw_line (clist->clist_window, style->fg_gc[GTK_STATE_NORMAL], 
		     x + 2, y + PM_SIZE / 2, x + PM_SIZE - 2, y + PM_SIZE / 2);

      if (!ctree_row->expanded)
	gdk_draw_line (clist->clist_window, style->fg_gc[GTK_STATE_NORMAL],
		       x + PM_SIZE / 2, y + 2,
		       x + PM_SIZE / 2, y + PM_SIZE - 2);

      if (justification_factor == 1)
	x += justification_factor * (PM_SIZE + 1);
      break;
    }

  gdk_gc_set_clip_rectangle (style->fg_gc[GTK_STATE_NORMAL], NULL);
  gdk_gc_set_clip_rectangle (style->base_gc[GTK_STATE_NORMAL], NULL);

  return x;
}


static gint
gtk_ctree_draw_lines (GtkCTree     *ctree,
		      GtkCTreeRow  *ctree_row,
		      gint          row,
		      gint          column,
		      gint          state,
		      GdkRectangle *clip_rectangle,
		      GdkRectangle *cell_rectangle,
		      GdkRectangle *crect,
		      GdkRectangle *area,
		      GtkStyle     *style)
{
  GtkCList *clist;
  GtkCTreeNode *node;
  GtkCTreeNode *parent;
  GdkRectangle tree_rectangle;
  GdkRectangle tc_rectangle;
  GdkGC *bg_gc;
  gint offset;
  gint offset_x;
  gint offset_y;
  gint xcenter;
  gint ycenter;
  gint next_level;
  gint column_right;
  gint column_left;
  gint justify_right;
  gint justification_factor;
  
  clist = GTK_CLIST (ctree);
  ycenter = clip_rectangle->y + (clip_rectangle->height / 2);
  justify_right = (clist->column[column].justification == GTK_JUSTIFY_RIGHT);

  if (justify_right)
    {
      offset = (clip_rectangle->x + clip_rectangle->width - 1 -
		ctree->tree_indent * (ctree_row->level - 1));
      justification_factor = -1;
    }
  else
    {
      offset = clip_rectangle->x + ctree->tree_indent * (ctree_row->level - 1);
      justification_factor = 1;
    }

  switch (ctree->line_style)
    {
    case GTK_CTREE_LINES_NONE:
      break;
    case GTK_CTREE_LINES_TABBED:
      xcenter = offset + justification_factor * TAB_SIZE;

      column_right = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) +
		      clist->column[ctree->tree_column].area.width +
		      COLUMN_INSET);
      column_left = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) -
		     COLUMN_INSET - CELL_SPACING);

      if (area)
	{
	  tree_rectangle.y = crect->y;
	  tree_rectangle.height = crect->height;

	  if (justify_right)
	    {
	      tree_rectangle.x = xcenter;
	      tree_rectangle.width = column_right - xcenter;
	    }
	  else
	    {
	      tree_rectangle.x = column_left;
	      tree_rectangle.width = xcenter - column_left;
	    }

	  if (!gdk_rectangle_intersect (area, &tree_rectangle, &tc_rectangle))
	    {
	      offset += justification_factor * 3;
	      break;
	    }
	}

      gdk_gc_set_clip_rectangle (ctree->lines_gc, crect);

      next_level = ctree_row->level;

      if (!ctree_row->sibling || (ctree_row->children && ctree_row->expanded))
	{
	  node = gtk_ctree_find_node_ptr (ctree, ctree_row);
	  if (GTK_CTREE_NODE_NEXT (node))
	    next_level = GTK_CTREE_ROW (GTK_CTREE_NODE_NEXT (node))->level;
	  else
	    next_level = 0;
	}

      if (ctree->tree_indent > 0)
	{
	  node = ctree_row->parent;
	  while (node)
	    {
	      xcenter -= (justification_factor * ctree->tree_indent);

	      if ((justify_right && xcenter < column_left) ||
		  (!justify_right && xcenter > column_right))
		{
		  node = GTK_CTREE_ROW (node)->parent;
		  continue;
		}

	      tree_rectangle.y = cell_rectangle->y;
	      tree_rectangle.height = cell_rectangle->height;
	      if (justify_right)
		{
		  tree_rectangle.x = MAX (xcenter - ctree->tree_indent + 1,
					  column_left);
		  tree_rectangle.width = MIN (xcenter - column_left,
					      ctree->tree_indent);
		}
	      else
		{
		  tree_rectangle.x = xcenter;
		  tree_rectangle.width = MIN (column_right - xcenter,
					      ctree->tree_indent);
		}

	      if (!area || gdk_rectangle_intersect (area, &tree_rectangle,
						    &tc_rectangle))
		{
		  get_cell_style (clist, &GTK_CTREE_ROW (node)->row,
				  state, column, NULL, NULL, &bg_gc);

		  if (bg_gc == clist->bg_gc)
		    gdk_gc_set_foreground
		      (clist->bg_gc, &GTK_CTREE_ROW (node)->row.background);

		  if (!area)
		    gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
					tree_rectangle.x,
					tree_rectangle.y,
					tree_rectangle.width,
					tree_rectangle.height);
		  else 
		    gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
					tc_rectangle.x,
					tc_rectangle.y,
					tc_rectangle.width,
					tc_rectangle.height);
		}
	      if (next_level > GTK_CTREE_ROW (node)->level)
		gdk_draw_line (clist->clist_window, ctree->lines_gc,
			       xcenter, crect->y,
			       xcenter, crect->y + crect->height);
	      else
		{
		  gint width;

		  offset_x = MIN (ctree->tree_indent, 2 * TAB_SIZE);
		  width = offset_x / 2 + offset_x % 2;

		  parent = GTK_CTREE_ROW (node)->parent;

		  tree_rectangle.y = ycenter;
		  tree_rectangle.height = (cell_rectangle->y - ycenter +
					   cell_rectangle->height);

		  if (justify_right)
		    {
		      tree_rectangle.x = MAX(xcenter + 1 - width, column_left);
		      tree_rectangle.width = MIN (xcenter + 1 - column_left,
						  width);
		    }
		  else
		    {
		      tree_rectangle.x = xcenter;
		      tree_rectangle.width = MIN (column_right - xcenter,
						  width);
		    }

		  if (!area ||
		      gdk_rectangle_intersect (area, &tree_rectangle,
					       &tc_rectangle))
		    {
		      if (parent)
			{
			  get_cell_style (clist, &GTK_CTREE_ROW (parent)->row,
					  state, column, NULL, NULL, &bg_gc);
			  if (bg_gc == clist->bg_gc)
			    gdk_gc_set_foreground
			      (clist->bg_gc,
			       &GTK_CTREE_ROW (parent)->row.background);
			}
		      else if (state == GTK_STATE_SELECTED)
			bg_gc = style->base_gc[state];
		      else
			bg_gc = GTK_WIDGET (clist)->style->base_gc[state];

		      if (!area)
			gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
					    tree_rectangle.x,
					    tree_rectangle.y,
					    tree_rectangle.width,
					    tree_rectangle.height);
		      else
			gdk_draw_rectangle (clist->clist_window,
					    bg_gc, TRUE,
					    tc_rectangle.x,
					    tc_rectangle.y,
					    tc_rectangle.width,
					    tc_rectangle.height);
		    }

		  get_cell_style (clist, &GTK_CTREE_ROW (node)->row,
				  state, column, NULL, NULL, &bg_gc);
		  if (bg_gc == clist->bg_gc)
		    gdk_gc_set_foreground
		      (clist->bg_gc, &GTK_CTREE_ROW (node)->row.background);

		  gdk_gc_set_clip_rectangle (bg_gc, crect);
		  gdk_draw_arc (clist->clist_window, bg_gc, TRUE,
				xcenter - (justify_right * offset_x),
				cell_rectangle->y,
				offset_x, clist->row_height,
				(180 + (justify_right * 90)) * 64, 90 * 64);
		  gdk_gc_set_clip_rectangle (bg_gc, NULL);

		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 xcenter, cell_rectangle->y, xcenter, ycenter);

		  if (justify_right)
		    gdk_draw_arc (clist->clist_window, ctree->lines_gc, FALSE,
				  xcenter - offset_x, cell_rectangle->y,
				  offset_x, clist->row_height,
				  270 * 64, 90 * 64);
		  else
		    gdk_draw_arc (clist->clist_window, ctree->lines_gc, FALSE,
				  xcenter, cell_rectangle->y,
				  offset_x, clist->row_height,
				  180 * 64, 90 * 64);
		}
	      node = GTK_CTREE_ROW (node)->parent;
	    }
	}

      if (state != GTK_STATE_SELECTED)
	{
	  tree_rectangle.y = clip_rectangle->y;
	  tree_rectangle.height = clip_rectangle->height;
	  tree_rectangle.width = COLUMN_INSET + CELL_SPACING +
	    MIN (clist->column[ctree->tree_column].area.width + COLUMN_INSET,
		 TAB_SIZE);

	  if (justify_right)
	    tree_rectangle.x = MAX (xcenter + 1, column_left);
	  else
	    tree_rectangle.x = column_left;

	  if (!area)
	    gdk_draw_rectangle (clist->clist_window,
				GTK_WIDGET
				(ctree)->style->base_gc[GTK_STATE_NORMAL],
				TRUE,
				tree_rectangle.x,
				tree_rectangle.y,
				tree_rectangle.width,
				tree_rectangle.height);
	  else if (gdk_rectangle_intersect (area, &tree_rectangle,
					    &tc_rectangle))
	    gdk_draw_rectangle (clist->clist_window,
				GTK_WIDGET
				(ctree)->style->base_gc[GTK_STATE_NORMAL],
				TRUE,
				tc_rectangle.x,
				tc_rectangle.y,
				tc_rectangle.width,
				tc_rectangle.height);
	}

      xcenter = offset + (justification_factor * ctree->tree_indent / 2);

      get_cell_style (clist, &ctree_row->row, state, column, NULL, NULL,
		      &bg_gc);
      if (bg_gc == clist->bg_gc)
	gdk_gc_set_foreground (clist->bg_gc, &ctree_row->row.background);

      gdk_gc_set_clip_rectangle (bg_gc, crect);
      if (ctree_row->is_leaf)
	{
	  GdkPoint points[6];

	  points[0].x = offset + justification_factor * TAB_SIZE;
	  points[0].y = cell_rectangle->y;

	  points[1].x = points[0].x - justification_factor * 4;
	  points[1].y = points[0].y;

	  points[2].x = points[1].x - justification_factor * 2;
	  points[2].y = points[1].y + 3;

	  points[3].x = points[2].x;
	  points[3].y = points[2].y + clist->row_height - 5;

	  points[4].x = points[3].x + justification_factor * 2;
	  points[4].y = points[3].y + 3;

	  points[5].x = points[4].x + justification_factor * 4;
	  points[5].y = points[4].y;

	  gdk_draw_polygon (clist->clist_window, bg_gc, TRUE, points, 6);
	  gdk_draw_lines (clist->clist_window, ctree->lines_gc, points, 6);
	}
      else 
	{
	  gdk_draw_arc (clist->clist_window, bg_gc, TRUE,
			offset - (justify_right * 2 * TAB_SIZE),
			cell_rectangle->y,
			2 * TAB_SIZE, clist->row_height,
			(90 + (180 * justify_right)) * 64, 180 * 64);
	  gdk_draw_arc (clist->clist_window, ctree->lines_gc, FALSE,
			offset - (justify_right * 2 * TAB_SIZE),
			cell_rectangle->y,
			2 * TAB_SIZE, clist->row_height,
			(90 + (180 * justify_right)) * 64, 180 * 64);
	}
      gdk_gc_set_clip_rectangle (bg_gc, NULL);
      gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);

      offset += justification_factor * 3;
      break;
    default:
      xcenter = offset + justification_factor * PM_SIZE / 2;

      if (area)
	{
	  tree_rectangle.y = crect->y;
	  tree_rectangle.height = crect->height;

	  if (justify_right)
	    {
	      tree_rectangle.x = xcenter - PM_SIZE / 2 - 2;
	      tree_rectangle.width = (clip_rectangle->x +
				      clip_rectangle->width -tree_rectangle.x);
	    }
	  else
	    {
	      tree_rectangle.x = clip_rectangle->x + PM_SIZE / 2;
	      tree_rectangle.width = (xcenter + PM_SIZE / 2 + 2 -
				      clip_rectangle->x);
	    }

	  if (!gdk_rectangle_intersect (area, &tree_rectangle, &tc_rectangle))
	    break;
	}

      offset_x = 1;
      offset_y = 0;
      if (ctree->line_style == GTK_CTREE_LINES_DOTTED)
	{
	  offset_x += abs((clip_rectangle->x + clist->hoffset) % 2);
	  offset_y  = abs((cell_rectangle->y + clist->voffset) % 2);
	}

      clip_rectangle->y--;
      clip_rectangle->height++;
      gdk_gc_set_clip_rectangle (ctree->lines_gc, clip_rectangle);
      gdk_draw_line (clist->clist_window, ctree->lines_gc,
		     xcenter,
		     (ctree->show_stub || clist->row_list->data != ctree_row) ?
		     cell_rectangle->y + offset_y : ycenter,
		     xcenter,
		     (ctree_row->sibling) ? crect->y +crect->height : ycenter);

      gdk_draw_line (clist->clist_window, ctree->lines_gc,
		     xcenter + (justification_factor * offset_x), ycenter,
		     xcenter + (justification_factor * (PM_SIZE / 2 + 2)),
		     ycenter);

      node = ctree_row->parent;
      while (node)
	{
	  xcenter -= (justification_factor * ctree->tree_indent);

	  if (GTK_CTREE_ROW (node)->sibling)
	    gdk_draw_line (clist->clist_window, ctree->lines_gc, 
			   xcenter, cell_rectangle->y + offset_y,
			   xcenter, crect->y + crect->height);
	  node = GTK_CTREE_ROW (node)->parent;
	}
      gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);
      clip_rectangle->y++;
      clip_rectangle->height--;
      break;
    }
  return offset;
}

static void
draw_row (GtkCList     *clist,
	  GdkRectangle *area,
	  gint          row,
	  GtkCListRow  *clist_row)
{
  GtkWidget *widget;
  GtkCTree  *ctree;
  GdkRectangle *crect;
  GdkRectangle row_rectangle;
  GdkRectangle cell_rectangle; 
  GdkRectangle clip_rectangle;
  GdkRectangle intersect_rectangle;
  gint last_column;
  gint column_left = 0;
  gint column_right = 0;
  gint offset = 0;
  gint state;
  gint i;

  g_return_if_fail (clist != NULL);

  /* bail now if we arn't drawable yet */
  if (!GTK_WIDGET_DRAWABLE (clist) || row < 0 || row >= clist->rows)
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

  /* rectangle used to clip drawing operations, its y and height
   * positions only need to be set once, so we set them once here. 
   * the x and width are set withing the drawing loop below once per
   * column */
  clip_rectangle.y = row_rectangle.y;
  clip_rectangle.height = row_rectangle.height;

  if (clist_row->state == GTK_STATE_NORMAL)
    {
      if (clist_row->fg_set)
	gdk_gc_set_foreground (clist->fg_gc, &clist_row->foreground);
      if (clist_row->bg_set)
	gdk_gc_set_foreground (clist->bg_gc, &clist_row->background);
    }
  
  state = clist_row->state;

  gdk_gc_set_foreground (ctree->lines_gc,
			 &widget->style->fg[clist_row->state]);

  /* draw the cell borders */
  if (area)
    {
      crect = &intersect_rectangle;

      if (gdk_rectangle_intersect (area, &cell_rectangle, crect))
	gdk_draw_rectangle (clist->clist_window,
			    widget->style->base_gc[GTK_STATE_NORMAL], TRUE,
			    crect->x, crect->y, crect->width, crect->height);
    }
  else
    {
      crect = &cell_rectangle;

      gdk_draw_rectangle (clist->clist_window,
			  widget->style->base_gc[GTK_STATE_NORMAL], TRUE,
			  crect->x, crect->y, crect->width, crect->height);
    }

  /* horizontal black lines */
  if (ctree->line_style == GTK_CTREE_LINES_TABBED)
    { 

      column_right = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) +
		      clist->column[ctree->tree_column].area.width +
		      COLUMN_INSET);
      column_left = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) -
		     COLUMN_INSET - (ctree->tree_column != 0) * CELL_SPACING);

      switch (clist->column[ctree->tree_column].justification)
	{
	case GTK_JUSTIFY_CENTER:
	case GTK_JUSTIFY_FILL:
	case GTK_JUSTIFY_LEFT:
	  offset = (column_left + ctree->tree_indent *
		    (((GtkCTreeRow *)clist_row)->level - 1));

	  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
			 MIN (offset + TAB_SIZE, column_right),
			 cell_rectangle.y,
			 clist->clist_window_width, cell_rectangle.y);
	  break;
	case GTK_JUSTIFY_RIGHT:
	  offset = (column_right - 1 - ctree->tree_indent *
		    (((GtkCTreeRow *)clist_row)->level - 1));

	  gdk_draw_line (clist->clist_window, ctree->lines_gc,
			 -1, cell_rectangle.y,
			 MAX (offset - TAB_SIZE, column_left),
			 cell_rectangle.y);
	  break;
	}
    }

  /* the last row has to clear its bottom cell spacing too */
  if (clist_row == clist->row_list_end->data)
    {
      cell_rectangle.y += clist->row_height + CELL_SPACING;

      if (!area || gdk_rectangle_intersect (area, &cell_rectangle, crect))
	{
	  gdk_draw_rectangle (clist->clist_window,
			      widget->style->base_gc[GTK_STATE_NORMAL], TRUE,
			      crect->x, crect->y, crect->width, crect->height);

	  /* horizontal black lines */
	  if (ctree->line_style == GTK_CTREE_LINES_TABBED)
	    { 
	      switch (clist->column[ctree->tree_column].justification)
		{
		case GTK_JUSTIFY_CENTER:
		case GTK_JUSTIFY_FILL:
		case GTK_JUSTIFY_LEFT:
		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 MIN (column_left + TAB_SIZE + COLUMN_INSET +
				      (((GtkCTreeRow *)clist_row)->level > 1) *
				      MIN (ctree->tree_indent / 2, TAB_SIZE),
				      column_right),
				 cell_rectangle.y,
				 clist->clist_window_width, cell_rectangle.y);
		  break;
		case GTK_JUSTIFY_RIGHT:
		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 -1, cell_rectangle.y,
				 MAX (column_right - TAB_SIZE - 1 -
				      COLUMN_INSET -
				      (((GtkCTreeRow *)clist_row)->level > 1) *
				      MIN (ctree->tree_indent / 2, TAB_SIZE),
				      column_left - 1), cell_rectangle.y);
		  break;
		}
	    }
	}
    }	  

  for (last_column = clist->columns - 1;
       last_column >= 0 && !clist->column[last_column].visible; last_column--)
    ;

  /* iterate and draw all the columns (row cells) and draw their contents */
  for (i = 0; i < clist->columns; i++)
    {
      GtkStyle *style;
      GdkGC *fg_gc; 
      GdkGC *bg_gc;
      PangoLayout *layout = NULL;
      PangoRectangle logical_rect;

      gint width;
      gint height;
      gint pixmap_width;
      gint string_width;
      gint old_offset;

      if (!clist->column[i].visible)
	continue;

      get_cell_style (clist, clist_row, state, i, &style, &fg_gc, &bg_gc);

      /* calculate clipping region */
      clip_rectangle.x = clist->column[i].area.x + clist->hoffset;
      clip_rectangle.width = clist->column[i].area.width;

      cell_rectangle.x = clip_rectangle.x - COLUMN_INSET - CELL_SPACING;
      cell_rectangle.width = (clip_rectangle.width + 2 * COLUMN_INSET +
			      (1 + (i == last_column)) * CELL_SPACING);
      cell_rectangle.y = clip_rectangle.y;
      cell_rectangle.height = clip_rectangle.height;

      string_width = 0;
      pixmap_width = 0;

      if (area && !gdk_rectangle_intersect (area, &cell_rectangle,
					    &intersect_rectangle))
	{
	  if (i != ctree->tree_column)
	    continue;
	}
      else
	{
	  gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
			      crect->x, crect->y, crect->width, crect->height);


	  layout = _gtk_clist_create_cell_layout (clist, clist_row, i);
	  if (layout)
	    {
	      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	      width = logical_rect.width;
	    }
	  else
	    width = 0;

	  switch (clist_row->cell[i].type)
	    {
	    case GTK_CELL_PIXMAP:
	      gdk_drawable_get_size
		(GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap, &pixmap_width,
		 &height);
	      width += pixmap_width;
	      break;
	    case GTK_CELL_PIXTEXT:
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		{
		  gdk_drawable_get_size
		    (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
		     &pixmap_width, &height);
		  width += pixmap_width;
		}

	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->text &&
		  GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		width +=  GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;

	      if (i == ctree->tree_column)
		width += (ctree->tree_indent *
			  ((GtkCTreeRow *)clist_row)->level);
	      break;
	    default:
	      break;
	    }

	  switch (clist->column[i].justification)
	    {
	    case GTK_JUSTIFY_LEFT:
	      offset = clip_rectangle.x + clist_row->cell[i].horizontal;
	      break;
	    case GTK_JUSTIFY_RIGHT:
	      offset = (clip_rectangle.x + clist_row->cell[i].horizontal +
			clip_rectangle.width - width);
	      break;
	    case GTK_JUSTIFY_CENTER:
	    case GTK_JUSTIFY_FILL:
	      offset = (clip_rectangle.x + clist_row->cell[i].horizontal +
			(clip_rectangle.width / 2) - (width / 2));
	      break;
	    };

	  if (i != ctree->tree_column)
	    {
	      offset += clist_row->cell[i].horizontal;
	      switch (clist_row->cell[i].type)
		{
		case GTK_CELL_PIXMAP:
		  draw_cell_pixmap
		    (clist->clist_window, &clip_rectangle, fg_gc,
		     GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
		     GTK_CELL_PIXMAP (clist_row->cell[i])->mask,
		     offset,
		     clip_rectangle.y + clist_row->cell[i].vertical +
		     (clip_rectangle.height - height) / 2,
		     pixmap_width, height);
		  break;
		case GTK_CELL_PIXTEXT:
		  offset = draw_cell_pixmap
		    (clist->clist_window, &clip_rectangle, fg_gc,
		     GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
		     GTK_CELL_PIXTEXT (clist_row->cell[i])->mask,
		     offset,
		     clip_rectangle.y + clist_row->cell[i].vertical +
		     (clip_rectangle.height - height) / 2,
		     pixmap_width, height);
		  offset += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;

		  /* Fall through */
		case GTK_CELL_TEXT:
		  if (layout)
		    {
		      gint row_center_offset = (clist->row_height - logical_rect.height) / 2;

		      gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);
		      gdk_draw_layout (clist->clist_window, fg_gc,
				       offset,
				       row_rectangle.y + row_center_offset + clist_row->cell[i].vertical,
				       layout);
		      gdk_gc_set_clip_rectangle (fg_gc, NULL);
		      g_object_unref (G_OBJECT (layout));
		    }
		  break;
		default:
		  break;
		}
	      continue;
	    }
	}

      if (bg_gc == clist->bg_gc)
	gdk_gc_set_background (ctree->lines_gc, &clist_row->background);

      /* draw ctree->tree_column */
      cell_rectangle.y -= CELL_SPACING;
      cell_rectangle.height += CELL_SPACING;

      if (area && !gdk_rectangle_intersect (area, &cell_rectangle,
					    &intersect_rectangle))
	{
	  if (layout)
            g_object_unref (G_OBJECT (layout));
	  continue;
	}

      /* draw lines */
      offset = gtk_ctree_draw_lines (ctree, (GtkCTreeRow *)clist_row, row, i,
				     state, &clip_rectangle, &cell_rectangle,
				     crect, area, style);

      /* draw expander */
      offset = gtk_ctree_draw_expander (ctree, (GtkCTreeRow *)clist_row,
					style, &clip_rectangle, offset);

      if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
	offset -= ctree->tree_spacing;
      else
	offset += ctree->tree_spacing;

      if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
	offset -= (pixmap_width + clist_row->cell[i].horizontal);
      else
	offset += clist_row->cell[i].horizontal;

      old_offset = offset;
      offset = draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
				 GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
				 GTK_CELL_PIXTEXT (clist_row->cell[i])->mask,
				 offset, 
				 clip_rectangle.y + clist_row->cell[i].vertical
				 + (clip_rectangle.height - height) / 2,
				 pixmap_width, height);

      if (layout)
	{
	  gint row_center_offset = (clist->row_height - logical_rect.height) / 2;
	  
	  if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
	    {
	      offset = (old_offset - string_width);
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		offset -= GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	    }
	  else
	    {
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		offset += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	    }
	  
	  gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);
	  gdk_draw_layout (clist->clist_window, fg_gc,
			   offset,
			   row_rectangle.y + row_center_offset + clist_row->cell[i].vertical,
			   layout);

          g_object_unref (G_OBJECT (layout));
	}
      gdk_gc_set_clip_rectangle (fg_gc, NULL);
    }

  /* draw focus rectangle */
  if (clist->focus_row == row &&
      GTK_WIDGET_CAN_FOCUS (widget) && gtk_widget_has_focus (widget))
    {
      if (!area)
	gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			    row_rectangle.x, row_rectangle.y,
			    row_rectangle.width - 1, row_rectangle.height - 1);
      else if (gdk_rectangle_intersect (area, &row_rectangle,
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
}

static void
tree_draw_node (GtkCTree     *ctree, 
	        GtkCTreeNode *node)
{
  GtkCList *clist;
  
  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && gtk_ctree_is_viewable (ctree, node))
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
	GTK_CLIST_GET_CLASS (clist)->draw_row
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

  if (update_focus_row && clist->selection_mode == GTK_SELECTION_MULTIPLE)
    {
      GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
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

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);
  
  if (update_focus_row && clist->selection_mode == GTK_SELECTION_MULTIPLE)
    {
      GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
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
	  if (pos + rows < clist->focus_row)
	    clist->focus_row -= (rows + 1);
	  else if (pos <= clist->focus_row)
	    {
	      if (!GTK_CTREE_ROW (node)->sibling)
		clist->focus_row = MAX (pos - 1, 0);
	      else
		clist->focus_row = pos;
	      
	      clist->focus_row = MIN (clist->focus_row, clist->rows - 1);
	    }
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
real_row_move (GtkCList *clist,
	       gint      source_row,
	       gint      dest_row)
{
  GtkCTree *ctree;
  GtkCTreeNode *node;

  g_return_if_fail (GTK_IS_CTREE (clist));

  if (GTK_CLIST_AUTO_SORT (clist))
    return;

  if (source_row < 0 || source_row >= clist->rows ||
      dest_row   < 0 || dest_row   >= clist->rows ||
      source_row == dest_row)
    return;

  ctree = GTK_CTREE (clist);
  node = GTK_CTREE_NODE (g_list_nth (clist->row_list, source_row));

  if (source_row < dest_row)
    {
      GtkCTreeNode *work; 

      dest_row++;
      work = GTK_CTREE_ROW (node)->children;

      while (work && GTK_CTREE_ROW (work)->level > GTK_CTREE_ROW (node)->level)
	{
	  work = GTK_CTREE_NODE_NEXT (work);
	  dest_row++;
	}

      if (dest_row > clist->rows)
	dest_row = clist->rows;
    }

  if (dest_row < clist->rows)
    {
      GtkCTreeNode *sibling;

      sibling = GTK_CTREE_NODE (g_list_nth (clist->row_list, dest_row));
      gtk_ctree_move (ctree, node, GTK_CTREE_ROW (sibling)->parent, sibling);
    }
  else
    gtk_ctree_move (ctree, node, NULL, NULL);
}

static void
real_tree_move (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *new_parent, 
		GtkCTreeNode *new_sibling)
{
  GtkCList *clist;
  GtkCTreeNode *work;
  gboolean visible = FALSE;

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

  visible = gtk_ctree_is_viewable (ctree, node);

  if (clist->selection_mode == GTK_SELECTION_MULTIPLE)
    {
      GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
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

  gtk_clist_freeze (clist);

  work = NULL;
  if (gtk_ctree_is_viewable (ctree, node))
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

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist) &&
      (visible || gtk_ctree_is_viewable (ctree, node)))
    gtk_clist_set_column_width
      (clist, ctree->tree_column,
       gtk_clist_optimal_column_width (clist, ctree->tree_column));

  gtk_clist_thaw (clist);
}

static void
change_focus_row_expansion (GtkCTree          *ctree,
			    GtkCTreeExpansionType action)
{
  GtkCList *clist;
  GtkCTreeNode *node;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (gdk_display_pointer_is_grabbed (gtk_widget_get_display (GTK_WIDGET (ctree))) && 
      GTK_WIDGET_HAS_GRAB (ctree))
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
  GtkRequisition requisition;
  gboolean visible;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || GTK_CTREE_ROW (node)->expanded || GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);
  
  GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);

  GTK_CTREE_ROW (node)->expanded = TRUE;

  visible = gtk_ctree_is_viewable (ctree, node);
  /* get cell width if tree_column is auto resized */
  if (visible && clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    GTK_CLIST_GET_CLASS (clist)->cell_size_request
      (clist, &GTK_CTREE_ROW (node)->row, ctree->tree_column, &requisition);

  /* unref/unset closed pixmap */
  if (GTK_CELL_PIXTEXT 
      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
    {
      g_object_unref
	(GTK_CELL_PIXTEXT
	 (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap);
      
      GTK_CELL_PIXTEXT
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = NULL;
      
      if (GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask)
	{
	  g_object_unref
	    (GTK_CELL_PIXTEXT 
	     (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask);
	  GTK_CELL_PIXTEXT 
	    (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = NULL;
	}
    }

  /* set/ref opened pixmap */
  if (GTK_CTREE_ROW (node)->pixmap_opened)
    {
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	g_object_ref (GTK_CTREE_ROW (node)->pixmap_opened);

      if (GTK_CTREE_ROW (node)->mask_opened) 
	GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	  g_object_ref (GTK_CTREE_ROW (node)->mask_opened);
    }


  work = GTK_CTREE_ROW (node)->children;
  if (work)
    {
      GList *list = (GList *)work;
      gint *cell_width = NULL;
      gint tmp = 0;
      gint row;
      gint i;
      
      if (visible && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
	{
	  cell_width = g_new0 (gint, clist->columns);
	  if (clist->column[ctree->tree_column].auto_resize)
	      cell_width[ctree->tree_column] = requisition.width;

	  while (work)
	    {
	      /* search maximum cell widths of auto_resize columns */
	      for (i = 0; i < clist->columns; i++)
		if (clist->column[i].auto_resize)
		  {
		    GTK_CLIST_GET_CLASS (clist)->cell_size_request
		      (clist, &GTK_CTREE_ROW (work)->row, i, &requisition);
		    cell_width[i] = MAX (requisition.width, cell_width[i]);
		  }

	      list = (GList *)work;
	      work = GTK_CTREE_NODE_NEXT (work);
	      tmp++;
	    }
	}
      else
	while (work)
	  {
	    list = (GList *)work;
	    work = GTK_CTREE_NODE_NEXT (work);
	    tmp++;
	  }

      list->next = (GList *)GTK_CTREE_NODE_NEXT (node);

      if (GTK_CTREE_NODE_NEXT (node))
	{
	  GList *tmp_list;

	  tmp_list = (GList *)GTK_CTREE_NODE_NEXT (node);
	  tmp_list->prev = list;
	}
      else
	clist->row_list_end = list;

      list = (GList *)node;
      list->next = (GList *)(GTK_CTREE_ROW (node)->children);

      if (visible)
	{
	  /* resize auto_resize columns if needed */
	  for (i = 0; i < clist->columns; i++)
	    if (clist->column[i].auto_resize &&
		cell_width[i] > clist->column[i].width)
	      gtk_clist_set_column_width (clist, i, cell_width[i]);
	  g_free (cell_width);

	  /* update focus_row position */
	  row = g_list_position (clist->row_list, (GList *)node);
	  if (row < clist->focus_row)
	    clist->focus_row += tmp;

	  clist->rows += tmp;
	  CLIST_REFRESH (clist);
	}
    }
  else if (visible && clist->column[ctree->tree_column].auto_resize)
    /* resize tree_column if needed */
    column_auto_resize (clist, &GTK_CTREE_ROW (node)->row, ctree->tree_column,
			requisition.width);
}

static void 
real_tree_collapse (GtkCTree     *ctree,
		    GtkCTreeNode *node)
{
  GtkCList *clist;
  GtkCTreeNode *work;
  GtkRequisition requisition;
  gboolean visible;
  gint level;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || !GTK_CTREE_ROW (node)->expanded ||
      GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
  
  GTK_CTREE_ROW (node)->expanded = FALSE;
  level = GTK_CTREE_ROW (node)->level;

  visible = gtk_ctree_is_viewable (ctree, node);
  /* get cell width if tree_column is auto resized */
  if (visible && clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    GTK_CLIST_GET_CLASS (clist)->cell_size_request
      (clist, &GTK_CTREE_ROW (node)->row, ctree->tree_column, &requisition);

  /* unref/unset opened pixmap */
  if (GTK_CELL_PIXTEXT 
      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
    {
      g_object_unref
	(GTK_CELL_PIXTEXT
	 (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap);
      
      GTK_CELL_PIXTEXT
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = NULL;
      
      if (GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask)
	{
	  g_object_unref
	    (GTK_CELL_PIXTEXT 
	     (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask);
	  GTK_CELL_PIXTEXT 
	    (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = NULL;
	}
    }

  /* set/ref closed pixmap */
  if (GTK_CTREE_ROW (node)->pixmap_closed)
    {
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	g_object_ref (GTK_CTREE_ROW (node)->pixmap_closed);

      if (GTK_CTREE_ROW (node)->mask_closed) 
	GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	  g_object_ref (GTK_CTREE_ROW (node)->mask_closed);
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

      if (visible)
	{
	  /* resize auto_resize columns if needed */
	  auto_resize_columns (clist);

	  row = g_list_position (clist->row_list, (GList *)node);
	  if (row < clist->focus_row)
	    clist->focus_row -= tmp;
	  clist->rows -= tmp;
	  CLIST_REFRESH (clist);
	}
    }
  else if (visible && clist->column[ctree->tree_column].auto_resize &&
	   !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    /* resize tree_column if needed */
    column_auto_resize (clist, &GTK_CTREE_ROW (node)->row, ctree->tree_column,
			requisition.width);
    
}

static void
column_auto_resize (GtkCList    *clist,
		    GtkCListRow *clist_row,
		    gint         column,
		    gint         old_width)
{
  /* resize column if needed for auto_resize */
  GtkRequisition requisition;

  if (!clist->column[column].auto_resize ||
      GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    return;

  if (clist_row)
    GTK_CLIST_GET_CLASS (clist)->cell_size_request (clist, clist_row,
						   column, &requisition);
  else
    requisition.width = 0;

  if (requisition.width > clist->column[column].width)
    gtk_clist_set_column_width (clist, column, requisition.width);
  else if (requisition.width < old_width &&
	   old_width == clist->column[column].width)
    {
      GList *list;
      gint new_width;

      /* run a "gtk_clist_optimal_column_width" but break, if
       * the column doesn't shrink */
      if (GTK_CLIST_SHOW_TITLES (clist) && clist->column[column].button)
	new_width = (clist->column[column].button->requisition.width -
		     (CELL_SPACING + (2 * COLUMN_INSET)));
      else
	new_width = 0;

      for (list = clist->row_list; list; list = list->next)
	{
	  GTK_CLIST_GET_CLASS (clist)->cell_size_request
	    (clist, GTK_CLIST_ROW (list), column, &requisition);
	  new_width = MAX (new_width, requisition.width);
	  if (new_width == clist->column[column].width)
	    break;
	}
      if (new_width < clist->column[column].width)
	gtk_clist_set_column_width (clist, column, new_width);
    }
}

static void
auto_resize_columns (GtkCList *clist)
{
  gint i;

  if (GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    return;

  for (i = 0; i < clist->columns; i++)
    column_auto_resize (clist, NULL, i, clist->column[i].width);
}

static void
cell_size_request (GtkCList       *clist,
		   GtkCListRow    *clist_row,
		   gint            column,
		   GtkRequisition *requisition)
{
  GtkCTree *ctree;
  gint width;
  gint height;
  PangoLayout *layout;
  PangoRectangle logical_rect;

  g_return_if_fail (GTK_IS_CTREE (clist));
  g_return_if_fail (requisition != NULL);

  ctree = GTK_CTREE (clist);

  layout = _gtk_clist_create_cell_layout (clist, clist_row, column);
  if (layout)
    {
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      requisition->width = logical_rect.width;
      requisition->height = logical_rect.height;
      
      g_object_unref (G_OBJECT (layout));
    }
  else
    {
      requisition->width  = 0;
      requisition->height = 0;
    }

  switch (clist_row->cell[column].type)
    {
    case GTK_CELL_PIXTEXT:
      if (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap)
	{
	  gdk_drawable_get_size (GTK_CELL_PIXTEXT
                                 (clist_row->cell[column])->pixmap,
                                 &width, &height);
	  width += GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing;
	}
      else
	width = height = 0;
	  
      requisition->width += width;
      requisition->height = MAX (requisition->height, height);
      
      if (column == ctree->tree_column)
	{
	  requisition->width += (ctree->tree_spacing + ctree->tree_indent *
				 (((GtkCTreeRow *) clist_row)->level - 1));
	  switch (ctree->expander_style)
	    {
	    case GTK_CTREE_EXPANDER_NONE:
	      break;
	    case GTK_CTREE_EXPANDER_TRIANGLE:
	      requisition->width += PM_SIZE + 3;
	      break;
	    case GTK_CTREE_EXPANDER_SQUARE:
	    case GTK_CTREE_EXPANDER_CIRCULAR:
	      requisition->width += PM_SIZE + 1;
	      break;
	    }
	  if (ctree->line_style == GTK_CTREE_LINES_TABBED)
	    requisition->width += 3;
	}
      break;
    case GTK_CELL_PIXMAP:
      gdk_drawable_get_size (GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap,
                             &width, &height);
      requisition->width += width;
      requisition->height = MAX (requisition->height, height);
      break;
    default:
      break;
    }

  requisition->width  += clist_row->cell[column].horizontal;
  requisition->height += clist_row->cell[column].vertical;
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
  gboolean visible = FALSE;
  GtkCTree *ctree;
  GtkRequisition requisition;
  gchar *old_text = NULL;
  GdkPixmap *old_pixmap = NULL;
  GdkBitmap *old_mask = NULL;

  g_return_if_fail (GTK_IS_CTREE (clist));
  g_return_if_fail (clist_row != NULL);

  ctree = GTK_CTREE (clist);

  if (clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      GtkCTreeNode *parent;

      parent = ((GtkCTreeRow *)clist_row)->parent;
      if (!parent || (parent && GTK_CTREE_ROW (parent)->expanded &&
		      gtk_ctree_is_viewable (ctree, parent)))
	{
	  visible = TRUE;
	  GTK_CLIST_GET_CLASS (clist)->cell_size_request (clist, clist_row,
							 column, &requisition);
	}
    }

  switch (clist_row->cell[column].type)
    {
    case GTK_CELL_EMPTY:
      break;
    case GTK_CELL_TEXT:
      old_text = GTK_CELL_TEXT (clist_row->cell[column])->text;
      break;
    case GTK_CELL_PIXMAP:
      old_pixmap = GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap;
      old_mask = GTK_CELL_PIXMAP (clist_row->cell[column])->mask;
      break;
    case GTK_CELL_PIXTEXT:
      old_text = GTK_CELL_PIXTEXT (clist_row->cell[column])->text;
      old_pixmap = GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap;
      old_mask = GTK_CELL_PIXTEXT (clist_row->cell[column])->mask;
      break;
    case GTK_CELL_WIDGET:
      /* unimplemented */
      break;
      
    default:
      break;
    }

  clist_row->cell[column].type = GTK_CELL_EMPTY;
  if (column == ctree->tree_column && type != GTK_CELL_EMPTY)
    type = GTK_CELL_PIXTEXT;

  /* Note that pixmap and mask were already ref'ed by the caller
   */
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
  
  if (visible && clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    column_auto_resize (clist, clist_row, column, requisition.width);

  g_free (old_text);
  if (old_pixmap)
    g_object_unref (old_pixmap);
  if (old_mask)
    g_object_unref (old_mask);
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
      g_object_unref (GTK_CTREE_ROW (node)->pixmap_opened);
      if (GTK_CTREE_ROW (node)->mask_opened) 
	g_object_unref (GTK_CTREE_ROW (node)->mask_opened);
    }
  if (GTK_CTREE_ROW (node)->pixmap_closed)
    {
      g_object_unref (GTK_CTREE_ROW (node)->pixmap_closed);
      if (GTK_CTREE_ROW (node)->mask_closed) 
	g_object_unref (GTK_CTREE_ROW (node)->mask_closed);
    }

  GTK_CTREE_ROW (node)->pixmap_opened = NULL;
  GTK_CTREE_ROW (node)->mask_opened   = NULL;
  GTK_CTREE_ROW (node)->pixmap_closed = NULL;
  GTK_CTREE_ROW (node)->mask_closed   = NULL;

  if (pixmap_closed)
    {
      GTK_CTREE_ROW (node)->pixmap_closed = g_object_ref (pixmap_closed);
      if (mask_closed) 
	GTK_CTREE_ROW (node)->mask_closed = g_object_ref (mask_closed);
    }
  if (pixmap_opened)
    {
      GTK_CTREE_ROW (node)->pixmap_opened = g_object_ref (pixmap_opened);
      if (mask_opened) 
	GTK_CTREE_ROW (node)->mask_opened = g_object_ref (mask_opened);
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
  tree_unselect (ctree,  node, NULL);
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
  ctree_row = g_slice_new (GtkCTreeRow);
  ctree_row->row.cell = g_slice_alloc (sizeof (GtkCell) * clist->columns);

  for (i = 0; i < clist->columns; i++)
    {
      ctree_row->row.cell[i].type = GTK_CELL_EMPTY;
      ctree_row->row.cell[i].vertical = 0;
      ctree_row->row.cell[i].horizontal = 0;
      ctree_row->row.cell[i].style = NULL;
    }

  GTK_CELL_PIXTEXT (ctree_row->row.cell[ctree->tree_column])->text = NULL;

  ctree_row->row.fg_set     = FALSE;
  ctree_row->row.bg_set     = FALSE;
  ctree_row->row.style      = NULL;
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
    {
      GTK_CLIST_GET_CLASS (clist)->set_cell_contents
	(clist, &(ctree_row->row), i, GTK_CELL_EMPTY, NULL, 0, NULL, NULL);
      if (ctree_row->row.cell[i].style)
	{
	  if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
	    gtk_style_detach (ctree_row->row.cell[i].style);
	  g_object_unref (ctree_row->row.cell[i].style);
	}
    }

  if (ctree_row->row.style)
    {
      if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
	gtk_style_detach (ctree_row->row.style);
      g_object_unref (ctree_row->row.style);
    }

  if (ctree_row->pixmap_closed)
    {
      g_object_unref (ctree_row->pixmap_closed);
      if (ctree_row->mask_closed)
	g_object_unref (ctree_row->mask_closed);
    }

  if (ctree_row->pixmap_opened)
    {
      g_object_unref (ctree_row->pixmap_opened);
      if (ctree_row->mask_opened)
	g_object_unref (ctree_row->mask_opened);
    }

  if (ctree_row->row.destroy)
    {
      GDestroyNotify dnotify = ctree_row->row.destroy;
      gpointer ddata = ctree_row->row.data;

      ctree_row->row.destroy = NULL;
      ctree_row->row.data = NULL;

      dnotify (ddata);
    }

  g_slice_free1 (sizeof (GtkCell) * clist->columns, ctree_row->row.cell);
  g_slice_free (GtkCTreeRow, ctree_row);
}

static void
real_select_row (GtkCList *clist,
		 gint      row,
		 gint      column,
		 GdkEvent *event)
{
  GList *node;

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
  
  g_return_if_fail (GTK_IS_CTREE (clist));

  ctree = GTK_CTREE (clist);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:
      return;

    case GTK_SELECTION_MULTIPLE:

      gtk_clist_freeze (clist);

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

      gtk_clist_thaw (clist);
      break;

    default:
      /* do nothing */
      break;
    }
}

static void
real_unselect_all (GtkCList *clist)
{
  GtkCTree *ctree;
  GtkCTreeNode *node;
  GList *list;
 
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

    case GTK_SELECTION_MULTIPLE:
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
  gint xl;
  gint yu;
  
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  clist = GTK_CLIST (ctree);

  if (!clist->column[ctree->tree_column].visible ||
      ctree->expander_style == GTK_CTREE_EXPANDER_NONE)
    return FALSE;

  tree_row = GTK_CTREE_ROW (node);

  yu = (ROW_TOP_YPIXEL (clist, row) + (clist->row_height - PM_SIZE) / 2 -
	(clist->row_height - 1) % 2);

  if (clist->column[ctree->tree_column].justification == GTK_JUSTIFY_RIGHT)
    xl = (clist->column[ctree->tree_column].area.x + 
	  clist->column[ctree->tree_column].area.width - 1 + clist->hoffset -
	  (tree_row->level - 1) * ctree->tree_indent - PM_SIZE -
	  (ctree->line_style == GTK_CTREE_LINES_TABBED) * 3);
  else
    xl = (clist->column[ctree->tree_column].area.x + clist->hoffset +
	  (tree_row->level - 1) * ctree->tree_indent +
	  (ctree->line_style == GTK_CTREE_LINES_TABBED) * 3);

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

static GObject*
gtk_ctree_constructor (GType                  type,
		       guint                  n_construct_properties,
		       GObjectConstructParam *construct_properties)
{
  GObject *object = G_OBJECT_CLASS (parent_class)->constructor (type,
								n_construct_properties,
								construct_properties);

  return object;
}

GtkWidget*
gtk_ctree_new_with_titles (gint         columns, 
			   gint         tree_column,
			   gchar       *titles[])
{
  GtkWidget *widget;

  g_return_val_if_fail (columns > 0, NULL);
  g_return_val_if_fail (tree_column >= 0 && tree_column < columns, NULL);

  widget = g_object_new (GTK_TYPE_CTREE,
			   "n_columns", columns,
			   "tree_column", tree_column,
			   NULL);
  if (titles)
    {
      GtkCList *clist = GTK_CLIST (widget);
      guint i;

      for (i = 0; i < columns; i++)
	gtk_clist_set_column_title (clist, i, titles[i]);
      gtk_clist_column_titles_show (clist);
    }

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


/**
 * gtk_ctree_insert_node:
 * @pixmap_closed: (allow-none):
 * @mask_closed: (allow-none):
 * @pixmap_opened: (allow-none):
 * @mask_opened: (allow-none):
 */
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
	GTK_CLIST_GET_CLASS (clist)->set_cell_contents
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

  if (text && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist) &&
      gtk_ctree_is_viewable (ctree, node))
    {
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].auto_resize)
	  column_auto_resize (clist, &(new_row->row), i, 0);
    }

  if (clist->rows == 1)
    {
      clist->focus_row = 0;
      if (clist->selection_mode == GTK_SELECTION_BROWSE)
	gtk_ctree_select (ctree, node);
    }


  CLIST_REFRESH (clist);

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
  GNode *work;
  guint depth = 1;

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

  gtk_clist_freeze (clist);

  set_node_info (ctree, cnode, "", 0, NULL, NULL, NULL, NULL, TRUE, FALSE);

  if (!func (ctree, depth, gnode, cnode, data))
    {
      tree_delete_row (ctree, cnode, NULL);
      gtk_clist_thaw (clist);
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
  gint depth;

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

  if (!GTK_CTREE_ROW (node)->is_leaf)
    {
      GNode *new_sibling = NULL;

      for (work = GTK_CTREE_ROW (node)->children; work;
	   work = GTK_CTREE_ROW (work)->sibling)
	new_sibling = gtk_ctree_export_to_gnode (ctree, gnode, new_sibling,
						 work, func, data);

      g_node_reverse_children (gnode);
    }

  return gnode;
}
  
static void
real_remove_row (GtkCList *clist,
		 gint      row)
{
  GtkCTreeNode *node;

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

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  if (node)
    {
      gtk_ctree_unlink (ctree, node, TRUE);
      gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (tree_delete),
				NULL);
      if (clist->selection_mode == GTK_SELECTION_BROWSE && !clist->selection &&
	  clist->focus_row >= 0)
	gtk_clist_select_row (clist, clist->focus_row, -1);

      auto_resize_columns (clist);
    }
  else
    gtk_clist_clear (clist);

  gtk_clist_thaw (clist);
}

static void
real_clear (GtkCList *clist)
{
  GtkCTree *ctree;
  GtkCTreeNode *work;
  GtkCTreeNode *ptr;

  g_return_if_fail (GTK_IS_CTREE (clist));

  ctree = GTK_CTREE (clist);

  /* remove all rows */
  work = GTK_CTREE_NODE (clist->row_list);
  clist->row_list = NULL;
  clist->row_list_end = NULL;

  GTK_CLIST_SET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  while (work)
    {
      ptr = work;
      work = GTK_CTREE_ROW (work)->sibling;
      gtk_ctree_post_recursive (ctree, ptr, GTK_CTREE_FUNC (tree_delete_row), 
				NULL);
    }
  GTK_CLIST_UNSET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);

  parent_class->clear (clist);
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
  
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (ctree_row != NULL, NULL);
  
  if (ctree_row->parent)
    node = GTK_CTREE_ROW (ctree_row->parent)->children;
  else
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (GTK_CTREE_ROW (node) != ctree_row)
    node = GTK_CTREE_ROW (node)->sibling;
  
  return node;
}

GtkCTreeNode *
gtk_ctree_node_nth (GtkCTree *ctree,
		    guint     row)
{
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);

  if ((row >= GTK_CLIST(ctree)->rows))
    return NULL;
 
  return GTK_CTREE_NODE (g_list_nth (GTK_CLIST (ctree)->row_list, row));
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
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  if (GTK_CTREE_ROW (node)->children)
    return gtk_ctree_find (ctree, GTK_CTREE_ROW (node)->children, child);

  return FALSE;
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
  
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);

  if (gtk_clist_get_selection_info (GTK_CLIST (ctree), x, y, &row, &column))
    if ((node = GTK_CTREE_NODE(g_list_nth (GTK_CLIST (ctree)->row_list, row))))
      return ctree_is_hot_spot (ctree, node, row, x, y);

  return FALSE;
}


/***********************************************************
 *   Tree signals : move, expand, collapse, (un)select     *
 ***********************************************************/


/**
 * gtk_ctree_move:
 * @new_parent: (allow-none):
 * @new_sibling: (allow-none):
 */
void
gtk_ctree_move (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *new_parent,
		GtkCTreeNode *new_sibling)
{
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_MOVE], node,
		   new_parent, new_sibling);
}

void
gtk_ctree_expand (GtkCTree     *ctree,
		  GtkCTreeNode *node)
{
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

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  if (CLIST_UNFROZEN (clist) && (!node || gtk_ctree_is_viewable (ctree, node)))
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

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  if (CLIST_UNFROZEN (clist) && (!node || gtk_ctree_is_viewable (ctree, node)))
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
  gint i;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && (!node || gtk_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  GTK_CLIST_SET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (tree_collapse), NULL);
  GTK_CLIST_UNSET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].auto_resize)
      gtk_clist_set_column_width (clist, i,
				  gtk_clist_optimal_column_width (clist, i));

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
  gint i;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && (!node || gtk_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  GTK_CLIST_SET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  gtk_ctree_post_recursive_to_depth (ctree, node, depth,
				     GTK_CTREE_FUNC (tree_collapse_to_depth),
				     GINT_TO_POINTER (depth));
  GTK_CLIST_UNSET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].auto_resize)
      gtk_clist_set_column_width (clist, i,
				  gtk_clist_optimal_column_width (clist, i));

  if (thaw)
    gtk_clist_thaw (clist);
}

void
gtk_ctree_toggle_expansion (GtkCTree     *ctree,
			    GtkCTreeNode *node)
{
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

  g_return_if_fail (GTK_IS_CTREE (ctree));
  
  if (node && GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && (!node || gtk_ctree_is_viewable (ctree, node)))
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

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if ((state && 
       (clist->selection_mode ==  GTK_SELECTION_BROWSE ||
	clist->selection_mode == GTK_SELECTION_SINGLE)) ||
      (!state && clist->selection_mode ==  GTK_SELECTION_BROWSE))
    return;

  if (CLIST_UNFROZEN (clist) && (!node || gtk_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (clist->selection_mode == GTK_SELECTION_MULTIPLE)
    {
      GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
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

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;
  
  clist = GTK_CLIST (ctree);

  GTK_CLIST_GET_CLASS (clist)->set_cell_contents
    (clist, &(GTK_CTREE_ROW (node)->row), column, GTK_CELL_TEXT,
     text, 0, NULL, NULL);

  tree_draw_node (ctree, node);
}


/**
 * gtk_ctree_node_set_pixmap:
 * @mask: (allow-none):
 */
void
gtk_ctree_node_set_pixmap (GtkCTree     *ctree,
			   GtkCTreeNode *node,
			   gint          column,
			   GdkPixmap    *pixmap,
			   GdkBitmap    *mask)
{
  GtkCList *clist;

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  g_return_if_fail (pixmap != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  g_object_ref (pixmap);
  if (mask) 
    g_object_ref (mask);

  clist = GTK_CLIST (ctree);

  GTK_CLIST_GET_CLASS (clist)->set_cell_contents
    (clist, &(GTK_CTREE_ROW (node)->row), column, GTK_CELL_PIXMAP,
     NULL, 0, pixmap, mask);

  tree_draw_node (ctree, node);
}


/**
 * gtk_ctree_node_set_pixtext:
 * @mask: (allow-none):
 */
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

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  if (column != ctree->tree_column)
    g_return_if_fail (pixmap != NULL);
  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  clist = GTK_CLIST (ctree);

  if (pixmap)
    {
      g_object_ref (pixmap);
      if (mask) 
	g_object_ref (mask);
    }

  GTK_CLIST_GET_CLASS (clist)->set_cell_contents
    (clist, &(GTK_CTREE_ROW (node)->row), column, GTK_CELL_PIXTEXT,
     text, spacing, pixmap, mask);

  tree_draw_node (ctree, node);
}


/**
 * gtk_ctree_set_node_info:
 * @pixmap_closed: (allow-none):
 * @mask_closed: (allow-none):
 * @pixmap_opened: (allow-none):
 * @mask_opened: (allow-none):
 */
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
	  work = GTK_CTREE_ROW (work)->sibling;
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
  GtkCList *clist;
  GtkRequisition requisition;
  gboolean visible = FALSE;

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  clist = GTK_CLIST (ctree);

  if (clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      visible = gtk_ctree_is_viewable (ctree, node);
      if (visible)
	GTK_CLIST_GET_CLASS (clist)->cell_size_request
	  (clist, &GTK_CTREE_ROW (node)->row, column, &requisition);
    }

  GTK_CTREE_ROW (node)->row.cell[column].vertical   = vertical;
  GTK_CTREE_ROW (node)->row.cell[column].horizontal = horizontal;

  if (visible)
    column_auto_resize (clist, &GTK_CTREE_ROW (node)->row,
			column, requisition.width);

  tree_draw_node (ctree, node);
}

static void
remove_grab (GtkCList *clist)
{
  if (gdk_display_pointer_is_grabbed (gtk_widget_get_display (GTK_WIDGET (clist))) && 
      GTK_WIDGET_HAS_GRAB (clist))
    {
      gtk_grab_remove (GTK_WIDGET (clist));
      gdk_display_pointer_ungrab (gtk_widget_get_display (GTK_WIDGET (clist)),
				  GDK_CURRENT_TIME);
    }

  if (clist->htimer)
    {
      g_source_remove (clist->htimer);
      clist->htimer = 0;
    }

  if (clist->vtimer)
    {
      g_source_remove (clist->vtimer);
      clist->vtimer = 0;
    }
}

void
gtk_ctree_node_set_selectable (GtkCTree     *ctree,
			       GtkCTreeNode *node,
			       gboolean      selectable)
{
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
	  clist->selection_mode == GTK_SELECTION_MULTIPLE)
	{
	  clist->drag_button = 0;
	  remove_grab (clist);

	  GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
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
  g_return_val_if_fail (GTK_IS_CTREE (ctree), -1);
  g_return_val_if_fail (node != NULL, -1);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return -1;

  return GTK_CTREE_ROW (node)->row.cell[column].type;
}

gboolean
gtk_ctree_node_get_text (GtkCTree      *ctree,
			 GtkCTreeNode  *node,
			 gint           column,
			 gchar        **text)
{
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return FALSE;

  if (GTK_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_TEXT)
    return FALSE;

  if (text)
    *text = GTK_CELL_TEXT (GTK_CTREE_ROW (node)->row.cell[column])->text;

  return TRUE;
}

gboolean
gtk_ctree_node_get_pixmap (GtkCTree     *ctree,
			   GtkCTreeNode *node,
			   gint          column,
			   GdkPixmap   **pixmap,
			   GdkBitmap   **mask)
{
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return FALSE;

  if (GTK_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_PIXMAP)
    return FALSE;

  if (pixmap)
    *pixmap = GTK_CELL_PIXMAP (GTK_CTREE_ROW (node)->row.cell[column])->pixmap;
  if (mask)
    *mask = GTK_CELL_PIXMAP (GTK_CTREE_ROW (node)->row.cell[column])->mask;

  return TRUE;
}

gboolean
gtk_ctree_node_get_pixtext (GtkCTree      *ctree,
			    GtkCTreeNode  *node,
			    gint           column,
			    gchar        **text,
			    guint8        *spacing,
			    GdkPixmap    **pixmap,
			    GdkBitmap    **mask)
{
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  
  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return FALSE;
  
  if (GTK_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_PIXTEXT)
    return FALSE;
  
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
  
  return TRUE;
}

gboolean
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
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  
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
  
  return TRUE;
}

void
gtk_ctree_node_set_cell_style (GtkCTree     *ctree,
			       GtkCTreeNode *node,
			       gint          column,
			       GtkStyle     *style)
{
  GtkCList *clist;
  GtkRequisition requisition;
  gboolean visible = FALSE;

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);

  if (column < 0 || column >= clist->columns)
    return;

  if (GTK_CTREE_ROW (node)->row.cell[column].style == style)
    return;

  if (clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      visible = gtk_ctree_is_viewable (ctree, node);
      if (visible)
	GTK_CLIST_GET_CLASS (clist)->cell_size_request
	  (clist, &GTK_CTREE_ROW (node)->row, column, &requisition);
    }

  if (GTK_CTREE_ROW (node)->row.cell[column].style)
    {
      if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
        gtk_style_detach (GTK_CTREE_ROW (node)->row.cell[column].style);
      g_object_unref (GTK_CTREE_ROW (node)->row.cell[column].style);
    }

  GTK_CTREE_ROW (node)->row.cell[column].style = style;

  if (GTK_CTREE_ROW (node)->row.cell[column].style)
    {
      g_object_ref (GTK_CTREE_ROW (node)->row.cell[column].style);
      
      if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
        GTK_CTREE_ROW (node)->row.cell[column].style =
	  gtk_style_attach (GTK_CTREE_ROW (node)->row.cell[column].style,
			    clist->clist_window);
    }

  if (visible)
    column_auto_resize (clist, &GTK_CTREE_ROW (node)->row, column,
			requisition.width);

  tree_draw_node (ctree, node);
}

GtkStyle *
gtk_ctree_node_get_cell_style (GtkCTree     *ctree,
			       GtkCTreeNode *node,
			       gint          column)
{
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return NULL;

  return GTK_CTREE_ROW (node)->row.cell[column].style;
}

void
gtk_ctree_node_set_row_style (GtkCTree     *ctree,
			      GtkCTreeNode *node,
			      GtkStyle     *style)
{
  GtkCList *clist;
  GtkRequisition requisition;
  gboolean visible;
  gint *old_width = NULL;
  gint i;

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);

  if (GTK_CTREE_ROW (node)->row.style == style)
    return;
  
  visible = gtk_ctree_is_viewable (ctree, node);
  if (visible && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      old_width = g_new (gint, clist->columns);
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].auto_resize)
	  {
	    GTK_CLIST_GET_CLASS (clist)->cell_size_request
	      (clist, &GTK_CTREE_ROW (node)->row, i, &requisition);
	    old_width[i] = requisition.width;
	  }
    }

  if (GTK_CTREE_ROW (node)->row.style)
    {
      if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
        gtk_style_detach (GTK_CTREE_ROW (node)->row.style);
      g_object_unref (GTK_CTREE_ROW (node)->row.style);
    }

  GTK_CTREE_ROW (node)->row.style = style;

  if (GTK_CTREE_ROW (node)->row.style)
    {
      g_object_ref (GTK_CTREE_ROW (node)->row.style);
      
      if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
        GTK_CTREE_ROW (node)->row.style =
	  gtk_style_attach (GTK_CTREE_ROW (node)->row.style,
			    clist->clist_window);
    }

  if (visible && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].auto_resize)
	  column_auto_resize (clist, &GTK_CTREE_ROW (node)->row, i,
			      old_width[i]);
      g_free (old_width);
    }
  tree_draw_node (ctree, node);
}

GtkStyle *
gtk_ctree_node_get_row_style (GtkCTree     *ctree,
			      GtkCTreeNode *node)
{
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  return GTK_CTREE_ROW (node)->row.style;
}

void
gtk_ctree_node_set_foreground (GtkCTree       *ctree,
			       GtkCTreeNode   *node,
			       const GdkColor *color)
{
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (color)
    {
      GTK_CTREE_ROW (node)->row.foreground = *color;
      GTK_CTREE_ROW (node)->row.fg_set = TRUE;
      if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (ctree)),
                                  &GTK_CTREE_ROW (node)->row.foreground,
                                  FALSE, TRUE);
    }
  else
    GTK_CTREE_ROW (node)->row.fg_set = FALSE;

  tree_draw_node (ctree, node);
}

void
gtk_ctree_node_set_background (GtkCTree       *ctree,
			       GtkCTreeNode   *node,
			       const GdkColor *color)
{
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (color)
    {
      GTK_CTREE_ROW (node)->row.background = *color;
      GTK_CTREE_ROW (node)->row.bg_set = TRUE;
      if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (ctree)),
                                  &GTK_CTREE_ROW (node)->row.background,
                                  FALSE, TRUE);
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
gtk_ctree_node_set_row_data_full (GtkCTree       *ctree,
				  GtkCTreeNode   *node,
				  gpointer        data,
				  GDestroyNotify  destroy)
{
  GDestroyNotify dnotify;
  gpointer ddata;
  
  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  dnotify = GTK_CTREE_ROW (node)->row.destroy;
  ddata = GTK_CTREE_ROW (node)->row.data;
  
  GTK_CTREE_ROW (node)->row.data = data;
  GTK_CTREE_ROW (node)->row.destroy = destroy;

  if (dnotify)
    dnotify (ddata);
}

gpointer
gtk_ctree_node_get_row_data (GtkCTree     *ctree,
			     GtkCTreeNode *node)
{
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

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  while (node && !gtk_ctree_is_viewable (ctree, node))
    node = GTK_CTREE_ROW (node)->parent;

  if (node)
    row = g_list_position (clist->row_list, (GList *)node);
  
  gtk_clist_moveto (clist, row, column, row_align, col_align);
}

GtkVisibility 
gtk_ctree_node_is_visible (GtkCTree     *ctree,
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
  GtkCList *clist;

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (indent >= 0);

  if (indent == ctree->tree_indent)
    return;

  clist = GTK_CLIST (ctree);
  ctree->tree_indent = indent;

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    gtk_clist_set_column_width
      (clist, ctree->tree_column,
       gtk_clist_optimal_column_width (clist, ctree->tree_column));
  else
    CLIST_REFRESH (ctree);
}

void
gtk_ctree_set_spacing (GtkCTree *ctree, 
		       gint      spacing)
{
  GtkCList *clist;
  gint old_spacing;

  g_return_if_fail (GTK_IS_CTREE (ctree));
  g_return_if_fail (spacing >= 0);

  if (spacing == ctree->tree_spacing)
    return;

  clist = GTK_CLIST (ctree);

  old_spacing = ctree->tree_spacing;
  ctree->tree_spacing = spacing;

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    gtk_clist_set_column_width (clist, ctree->tree_column,
				clist->column[ctree->tree_column].width +
				spacing - old_spacing);
  else
    CLIST_REFRESH (ctree);
}

void
gtk_ctree_set_show_stub (GtkCTree *ctree, 
			 gboolean  show_stub)
{
  g_return_if_fail (GTK_IS_CTREE (ctree));

  show_stub = show_stub != FALSE;

  if (show_stub != ctree->show_stub)
    {
      GtkCList *clist;

      clist = GTK_CLIST (ctree);
      ctree->show_stub = show_stub;

      if (CLIST_UNFROZEN (clist) && clist->rows &&
	  gtk_clist_row_is_visible (clist, 0) != GTK_VISIBILITY_NONE)
	GTK_CLIST_GET_CLASS (clist)->draw_row
	  (clist, NULL, 0, GTK_CLIST_ROW (clist->row_list));
    }
}

void 
gtk_ctree_set_line_style (GtkCTree          *ctree, 
			  GtkCTreeLineStyle  line_style)
{
  GtkCList *clist;
  GtkCTreeLineStyle old_style;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (line_style == ctree->line_style)
    return;

  clist = GTK_CLIST (ctree);

  old_style = ctree->line_style;
  ctree->line_style = line_style;

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      if (old_style == GTK_CTREE_LINES_TABBED)
	gtk_clist_set_column_width
	  (clist, ctree->tree_column,
	   clist->column[ctree->tree_column].width - 3);
      else if (line_style == GTK_CTREE_LINES_TABBED)
	gtk_clist_set_column_width
	  (clist, ctree->tree_column,
	   clist->column[ctree->tree_column].width + 3);
    }

  if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
    {
      gint8 dashes[] = { 1, 1 };

      switch (line_style)
	{
	case GTK_CTREE_LINES_SOLID:
	  if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, GDK_LINE_SOLID, 
					GDK_CAP_BUTT, GDK_JOIN_MITER);
	  break;
	case GTK_CTREE_LINES_DOTTED:
	  if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, 
					GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
	  gdk_gc_set_dashes (ctree->lines_gc, 0, dashes, G_N_ELEMENTS (dashes));
	  break;
	case GTK_CTREE_LINES_TABBED:
	  if (gtk_widget_get_realized (GTK_WIDGET (ctree)))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, GDK_LINE_SOLID, 
					GDK_CAP_BUTT, GDK_JOIN_MITER);
	  break;
	case GTK_CTREE_LINES_NONE:
	  break;
	default:
	  return;
	}
      CLIST_REFRESH (ctree);
    }
}

void 
gtk_ctree_set_expander_style (GtkCTree              *ctree, 
			      GtkCTreeExpanderStyle  expander_style)
{
  GtkCList *clist;
  GtkCTreeExpanderStyle old_style;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (expander_style == ctree->expander_style)
    return;

  clist = GTK_CLIST (ctree);

  old_style = ctree->expander_style;
  ctree->expander_style = expander_style;

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      gint new_width;

      new_width = clist->column[ctree->tree_column].width;
      switch (old_style)
	{
	case GTK_CTREE_EXPANDER_NONE:
	  break;
	case GTK_CTREE_EXPANDER_TRIANGLE:
	  new_width -= PM_SIZE + 3;
	  break;
	case GTK_CTREE_EXPANDER_SQUARE:
	case GTK_CTREE_EXPANDER_CIRCULAR:
	  new_width -= PM_SIZE + 1;
	  break;
	}

      switch (expander_style)
	{
	case GTK_CTREE_EXPANDER_NONE:
	  break;
	case GTK_CTREE_EXPANDER_TRIANGLE:
	  new_width += PM_SIZE + 3;
	  break;
	case GTK_CTREE_EXPANDER_SQUARE:
	case GTK_CTREE_EXPANDER_CIRCULAR:
	  new_width += PM_SIZE + 1;
	  break;
	}

      gtk_clist_set_column_width (clist, ctree->tree_column, new_width);
    }

  if (GTK_WIDGET_DRAWABLE (clist))
    CLIST_REFRESH (clist);
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

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  if (clist->selection_mode == GTK_SELECTION_MULTIPLE)
    {
      GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
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

  g_return_if_fail (GTK_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  if (clist->selection_mode == GTK_SELECTION_MULTIPLE)
    {
      GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
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
	  
	  if (CLIST_UNFROZEN (clist) &&
	      gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	    GTK_CLIST_GET_CLASS (clist)->draw_row (clist, NULL, row,
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
  gboolean unselect;

  g_return_if_fail (GTK_IS_CTREE (clist));

  if (clist->selection_mode != GTK_SELECTION_MULTIPLE)
    return;

  if (clist->anchor < 0 || clist->drag_pos < 0)
    return;

  ctree = GTK_CTREE (clist);
  
  clist->freeze_count++;

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

  if (clist->anchor < clist->drag_pos)
    {
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
		    clist->undo_selection =
		      g_list_prepend (clist->undo_selection, node);
		  }
	      }
	    else if (GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
	      {
		GTK_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;
		clist->undo_unselection =
		  g_list_prepend (clist->undo_unselection, node);
	      }
	  }
    }
  else
    {
      for (node = GTK_CTREE_NODE (g_list_nth (clist->row_list, e)); i <= e;
	   e--, node = GTK_CTREE_NODE_PREV (node))
	if (GTK_CTREE_ROW (node)->row.selectable)
	  {
	    if (g_list_find (clist->selection, node))
	      {
		if (GTK_CTREE_ROW (node)->row.state == GTK_STATE_NORMAL)
		  {
		    GTK_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;
		    gtk_ctree_unselect (ctree, node);
		    clist->undo_selection =
		      g_list_prepend (clist->undo_selection, node);
		  }
	      }
	    else if (GTK_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
	      {
		GTK_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;
		clist->undo_unselection =
		  g_list_prepend (clist->undo_unselection, node);
	      }
	  }
    }

  clist->undo_unselection = g_list_reverse (clist->undo_unselection);
  for (list = clist->undo_unselection; list; list = list->next)
    gtk_ctree_select (ctree, list->data);

  clist->anchor = -1;
  clist->drag_pos = -1;

  if (!CLIST_UNFROZEN (clist))
    clist->freeze_count--;
}

static void
real_undo_selection (GtkCList *clist)
{
  GtkCTree *ctree;
  GList *work;

  g_return_if_fail (GTK_IS_CTREE (clist));

  if (clist->selection_mode != GTK_SELECTION_MULTIPLE)
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

  if (gtk_widget_has_focus (GTK_WIDGET (clist)) && clist->focus_row != clist->undo_anchor)
    {
      clist->focus_row = clist->undo_anchor;
      gtk_widget_queue_draw (GTK_WIDGET (clist));
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
  g_return_if_fail (GTK_IS_CTREE (ctree));

  ctree->drag_compare = cmp_func;
}

static gboolean
check_drag (GtkCTree        *ctree,
	    GtkCTreeNode    *drag_source,
	    GtkCTreeNode    *drag_target,
	    GtkCListDragPos  insert_pos)
{
  g_return_val_if_fail (GTK_IS_CTREE (ctree), FALSE);

  if (drag_source && drag_source != drag_target &&
      (!GTK_CTREE_ROW (drag_source)->children ||
       !gtk_ctree_is_ancestor (ctree, drag_source, drag_target)))
    {
      switch (insert_pos)
	{
	case GTK_CLIST_DRAG_NONE:
	  return FALSE;
	case GTK_CLIST_DRAG_AFTER:
	  if (GTK_CTREE_ROW (drag_target)->sibling != drag_source)
	    return (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 drag_source,
					 GTK_CTREE_ROW (drag_target)->parent,
					 GTK_CTREE_ROW (drag_target)->sibling));
	  break;
	case GTK_CLIST_DRAG_BEFORE:
	  if (GTK_CTREE_ROW (drag_source)->sibling != drag_target)
	    return (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 drag_source,
					 GTK_CTREE_ROW (drag_target)->parent,
					 drag_target));
	  break;
	case GTK_CLIST_DRAG_INTO:
	  if (!GTK_CTREE_ROW (drag_target)->is_leaf &&
	      GTK_CTREE_ROW (drag_target)->children != drag_source)
	    return (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 drag_source,
					 drag_target,
					 GTK_CTREE_ROW (drag_target)->children));
	  break;
	}
    }
  return FALSE;
}



/************************************/
static void
drag_dest_info_destroy (gpointer data)
{
  GtkCListDestInfo *info = data;

  g_free (info);
}

static void
drag_dest_cell (GtkCList         *clist,
		gint              x,
		gint              y,
		GtkCListDestInfo *dest_info)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (clist);

  dest_info->insert_pos = GTK_CLIST_DRAG_NONE;

  y -= (GTK_CONTAINER (widget)->border_width +
	widget->style->ythickness + clist->column_title_area.height);
  dest_info->cell.row = ROW_FROM_YPIXEL (clist, y);

  if (dest_info->cell.row >= clist->rows)
    {
      dest_info->cell.row = clist->rows - 1;
      y = ROW_TOP_YPIXEL (clist, dest_info->cell.row) + clist->row_height;
    }
  if (dest_info->cell.row < -1)
    dest_info->cell.row = -1;
  
  x -= GTK_CONTAINER (widget)->border_width + widget->style->xthickness;

  dest_info->cell.column = COLUMN_FROM_XPIXEL (clist, x);

  if (dest_info->cell.row >= 0)
    {
      gint y_delta;
      gint h = 0;

      y_delta = y - ROW_TOP_YPIXEL (clist, dest_info->cell.row);
      
      if (GTK_CLIST_DRAW_DRAG_RECT(clist) &&
	  !GTK_CTREE_ROW (g_list_nth (clist->row_list,
				      dest_info->cell.row))->is_leaf)
	{
	  dest_info->insert_pos = GTK_CLIST_DRAG_INTO;
	  h = clist->row_height / 4;
	}
      else if (GTK_CLIST_DRAW_DRAG_LINE(clist))
	{
	  dest_info->insert_pos = GTK_CLIST_DRAG_BEFORE;
	  h = clist->row_height / 2;
	}

      if (GTK_CLIST_DRAW_DRAG_LINE(clist))
	{
	  if (y_delta < h)
	    dest_info->insert_pos = GTK_CLIST_DRAG_BEFORE;
	  else if (clist->row_height - y_delta < h)
	    dest_info->insert_pos = GTK_CLIST_DRAG_AFTER;
	}
    }
}

static void
gtk_ctree_drag_begin (GtkWidget	     *widget,
		      GdkDragContext *context)
{
  GtkCList *clist;
  GtkCTree *ctree;
  gboolean use_icons;

  g_return_if_fail (GTK_IS_CTREE (widget));
  g_return_if_fail (context != NULL);

  clist = GTK_CLIST (widget);
  ctree = GTK_CTREE (widget);

  use_icons = GTK_CLIST_USE_DRAG_ICONS (clist);
  GTK_CLIST_UNSET_FLAG (clist, CLIST_USE_DRAG_ICONS);
  GTK_WIDGET_CLASS (parent_class)->drag_begin (widget, context);

  if (use_icons)
    {
      GtkCTreeNode *node;

      GTK_CLIST_SET_FLAG (clist, CLIST_USE_DRAG_ICONS);
      node = GTK_CTREE_NODE (g_list_nth (clist->row_list,
					 clist->click_cell.row));
      if (node)
	{
	  if (GTK_CELL_PIXTEXT
	      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
	    {
	      gtk_drag_set_icon_pixmap
		(context,
		 gtk_widget_get_colormap (widget),
		 GTK_CELL_PIXTEXT
		 (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap,
		 GTK_CELL_PIXTEXT
		 (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask,
		 -2, -2);
	      return;
	    }
	}
      gtk_drag_set_icon_default (context);
    }
}

static gint
gtk_ctree_drag_motion (GtkWidget      *widget,
		       GdkDragContext *context,
		       gint            x,
		       gint            y,
		       guint           time)
{
  GtkCList *clist;
  GtkCTree *ctree;
  GtkCListDestInfo new_info;
  GtkCListDestInfo *dest_info;

  g_return_val_if_fail (GTK_IS_CTREE (widget), FALSE);

  clist = GTK_CLIST (widget);
  ctree = GTK_CTREE (widget);

  dest_info = g_dataset_get_data (context, "gtk-clist-drag-dest");

  if (!dest_info)
    {
      dest_info = g_new (GtkCListDestInfo, 1);
	  
      dest_info->cell.row    = -1;
      dest_info->cell.column = -1;
      dest_info->insert_pos  = GTK_CLIST_DRAG_NONE;

      g_dataset_set_data_full (context, "gtk-clist-drag-dest", dest_info,
			       drag_dest_info_destroy);
    }

  drag_dest_cell (clist, x, y, &new_info);

  if (GTK_CLIST_REORDERABLE (clist))
    {
      GList *list;
      GdkAtom atom = gdk_atom_intern_static_string ("gtk-clist-drag-reorder");

      list = context->targets;
      while (list)
	{
	  if (atom == GDK_POINTER_TO_ATOM (list->data))
	    break;
	  list = list->next;
	}

      if (list)
	{
	  GtkCTreeNode *drag_source;
	  GtkCTreeNode *drag_target;

	  drag_source = GTK_CTREE_NODE (g_list_nth (clist->row_list,
						    clist->click_cell.row));
	  drag_target = GTK_CTREE_NODE (g_list_nth (clist->row_list,
						    new_info.cell.row));

	  if (gtk_drag_get_source_widget (context) != widget ||
	      !check_drag (ctree, drag_source, drag_target,
			   new_info.insert_pos))
	    {
	      if (dest_info->cell.row < 0)
		{
		  gdk_drag_status (context, GDK_ACTION_DEFAULT, time);
		  return FALSE;
		}
	      return TRUE;
	    }

	  if (new_info.cell.row != dest_info->cell.row ||
	      (new_info.cell.row == dest_info->cell.row &&
	       dest_info->insert_pos != new_info.insert_pos))
	    {
	      if (dest_info->cell.row >= 0)
		GTK_CLIST_GET_CLASS (clist)->draw_drag_highlight
		  (clist,
		   g_list_nth (clist->row_list, dest_info->cell.row)->data,
		   dest_info->cell.row, dest_info->insert_pos);

	      dest_info->insert_pos  = new_info.insert_pos;
	      dest_info->cell.row    = new_info.cell.row;
	      dest_info->cell.column = new_info.cell.column;

	      GTK_CLIST_GET_CLASS (clist)->draw_drag_highlight
		(clist,
		 g_list_nth (clist->row_list, dest_info->cell.row)->data,
		 dest_info->cell.row, dest_info->insert_pos);

	      clist->drag_highlight_row = dest_info->cell.row;
	      clist->drag_highlight_pos = dest_info->insert_pos;

	      gdk_drag_status (context, context->suggested_action, time);
	    }
	  return TRUE;
	}
    }

  dest_info->insert_pos  = new_info.insert_pos;
  dest_info->cell.row    = new_info.cell.row;
  dest_info->cell.column = new_info.cell.column;
  return TRUE;
}

static void
gtk_ctree_drag_data_received (GtkWidget        *widget,
			      GdkDragContext   *context,
			      gint              x,
			      gint              y,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint32           time)
{
  GtkCTree *ctree;
  GtkCList *clist;

  g_return_if_fail (GTK_IS_CTREE (widget));
  g_return_if_fail (context != NULL);
  g_return_if_fail (selection_data != NULL);

  ctree = GTK_CTREE (widget);
  clist = GTK_CLIST (widget);

  if (GTK_CLIST_REORDERABLE (clist) &&
      gtk_drag_get_source_widget (context) == widget &&
      selection_data->target ==
      gdk_atom_intern_static_string ("gtk-clist-drag-reorder") &&
      selection_data->format == 8 &&
      selection_data->length == sizeof (GtkCListCellInfo))
    {
      GtkCListCellInfo *source_info;

      source_info = (GtkCListCellInfo *)(selection_data->data);
      if (source_info)
	{
	  GtkCListDestInfo dest_info;
	  GtkCTreeNode *source_node;
	  GtkCTreeNode *dest_node;

	  drag_dest_cell (clist, x, y, &dest_info);
	  
	  source_node = GTK_CTREE_NODE (g_list_nth (clist->row_list,
						    source_info->row));
	  dest_node = GTK_CTREE_NODE (g_list_nth (clist->row_list,
						  dest_info.cell.row));

	  if (!source_node || !dest_node)
	    return;

	  switch (dest_info.insert_pos)
	    {
	    case GTK_CLIST_DRAG_NONE:
	      break;
	    case GTK_CLIST_DRAG_INTO:
	      if (check_drag (ctree, source_node, dest_node,
			      dest_info.insert_pos))
		gtk_ctree_move (ctree, source_node, dest_node,
				GTK_CTREE_ROW (dest_node)->children);
	      g_dataset_remove_data (context, "gtk-clist-drag-dest");
	      break;
	    case GTK_CLIST_DRAG_BEFORE:
	      if (check_drag (ctree, source_node, dest_node,
			      dest_info.insert_pos))
		gtk_ctree_move (ctree, source_node,
				GTK_CTREE_ROW (dest_node)->parent, dest_node);
	      g_dataset_remove_data (context, "gtk-clist-drag-dest");
	      break;
	    case GTK_CLIST_DRAG_AFTER:
	      if (check_drag (ctree, source_node, dest_node,
			      dest_info.insert_pos))
		gtk_ctree_move (ctree, source_node,
				GTK_CTREE_ROW (dest_node)->parent, 
				GTK_CTREE_ROW (dest_node)->sibling);
	      g_dataset_remove_data (context, "gtk-clist-drag-dest");
	      break;
	    }
	}
    }
}

GType
gtk_ctree_node_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_pointer_type_register_static ("GtkCTreeNode");

  return our_type;
}

#include "gtkaliasdef.c"
