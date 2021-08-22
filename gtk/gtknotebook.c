/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp:ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gtknotebook.h"

#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtklabel.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkdnd.h"
#include "gtkbuildable.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpath.h"
#include "gtkboxgadgetprivate.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"
#include "a11y/gtknotebookaccessible.h"


/**
 * SECTION:gtknotebook
 * @Short_description: A tabbed notebook container
 * @Title: GtkNotebook
 * @See_also: #GtkContainer
 *
 * The #GtkNotebook widget is a #GtkContainer whose children are pages that
 * can be switched between using tab labels along one edge.
 *
 * There are many configuration options for GtkNotebook. Among other
 * things, you can choose on which edge the tabs appear
 * (see gtk_notebook_set_tab_pos()), whether, if there are too many
 * tabs to fit the notebook should be made bigger or scrolling
 * arrows added (see gtk_notebook_set_scrollable()), and whether there
 * will be a popup menu allowing the users to switch pages.
 * (see gtk_notebook_popup_enable(), gtk_notebook_popup_disable())
 *
 * # GtkNotebook as GtkBuildable
 * 
 * The GtkNotebook implementation of the #GtkBuildable interface
 * supports placing children into tabs by specifying “tab” as the
 * “type” attribute of a `<child>` element. Note that the content
 * of the tab must be created before the tab can be filled.
 * A tab child can be specified without specifying a `<child>`
 * type attribute.
 *
 * To add a child widget in the notebooks action area, specify
 * "action-start" or “action-end” as the “type” attribute of the
 * `<child>` element.
 *
 * An example of a UI definition fragment with GtkNotebook:
 *
 * |[<!-- language="xml" -->
 * <object class="GtkNotebook">
 *   <child>
 *     <object class="GtkLabel" id="notebook-content">
 *       <property name="label">Content</property>
 *     </object>
 *   </child>
 *   <child type="tab">
 *     <object class="GtkLabel" id="notebook-tab">
 *       <property name="label">Tab</property>
 *     </object>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * notebook
 * ├── header.top
 * │   ├── [<action widget>]
 * │   ├── tabs
 * │   │   ├── [arrow]
 * │   │   ├── tab
 * │   │   │   ╰── <tab label>
 * ┊   ┊   ┊
 * │   │   ├── tab[.reorderable-page]
 * │   │   │   ╰── <tab label>
 * │   │   ╰── [arrow]
 * │   ╰── [<action widget>]
 * │
 * ╰── stack
 *     ├── <child>
 *     ┊
 *     ╰── <child>
 * ]|
 *
 * GtkNotebook has a main CSS node with name notebook, a subnode
 * with name header and below that a subnode with name tabs which
 * contains one subnode per tab with name tab.
 *
 * If action widgets are present, their CSS nodes are placed next
 * to the tabs node. If the notebook is scrollable, CSS nodes with
 * name arrow are placed as first and last child of the tabs node.
 *
 * The main node gets the .frame style class when the notebook
 * has a border (see gtk_notebook_set_show_border()).
 *
 * The header node gets one of the style class .top, .bottom,
 * .left or .right, depending on where the tabs are placed. For
 * reorderable pages, the tab node gets the .reorderable-page class.
 *
 * A tab node gets the .dnd style class while it is moved with drag-and-drop.
 *
 * The nodes are always arranged from left-to-right, regarldess of text direction.
 */


#define SCROLL_DELAY_FACTOR   5
#define SCROLL_THRESHOLD      12
#define DND_THRESHOLD_MULTIPLIER 4

#define TIMEOUT_INITIAL  500
#define TIMEOUT_REPEAT    50
#define TIMEOUT_EXPAND   500

typedef struct _GtkNotebookPage GtkNotebookPage;

typedef enum
{
  DRAG_OPERATION_NONE,
  DRAG_OPERATION_REORDER,
  DRAG_OPERATION_DETACH
} GtkNotebookDragOperation;

enum {
  ACTION_WIDGET_START,
  ACTION_WIDGET_END,
  N_ACTION_WIDGETS
};

struct _GtkNotebookPrivate
{
  GtkNotebookDragOperation   operation;
  GtkNotebookPage           *cur_page;
  GtkNotebookPage           *detached_tab;
  GtkNotebookPage           *prelight_tab;
  GtkTargetList             *source_targets;
  GtkWidget                 *action_widget[N_ACTION_WIDGETS];
  GtkWidget                 *dnd_window;
  GtkWidget                 *menu;

  GdkWindow               *drag_window;
  GdkWindow               *event_window;

  GtkCssGadget              *gadget;
  GtkCssGadget              *stack_gadget;
  GtkCssGadget              *header_gadget;
  GtkCssGadget              *tabs_gadget;
  GtkCssGadget              *arrow_gadget[4];

  GList         *children;
  GList         *first_tab;             /* The first tab visible (for scrolling notebooks) */
  GList         *focus_tab;

  gint           drag_begin_x;
  gint           drag_begin_y;
  gint           drag_offset_x;
  gint           drag_offset_y;
  gint           drag_window_x;
  gint           drag_window_y;
  gint           mouse_x;
  gint           mouse_y;
  gint           pressed_button;

  GQuark         group;

  guint          dnd_timer;
  guint          switch_tab_timer;
  GList         *switch_tab;

  guint32        timer;

  guint          child_has_focus    : 1;
  guint          click_child        : 3;
  guint          remove_in_detach   : 1;
  guint          focus_out          : 1; /* Flag used by ::move-focus-out implementation */
  guint          has_scrolled       : 1;
  guint          in_child           : 3;
  guint          need_timer         : 1;
  guint          show_border        : 1;
  guint          show_tabs          : 1;
  guint          scrollable         : 1;
  guint          tab_pos            : 2;
  guint          tabs_reversed      : 1;
  guint          rootwindow_drop    : 1;
};

enum {
  SWITCH_PAGE,
  FOCUS_TAB,
  SELECT_PAGE,
  CHANGE_CURRENT_PAGE,
  MOVE_FOCUS_OUT,
  REORDER_TAB,
  PAGE_REORDERED,
  PAGE_REMOVED,
  PAGE_ADDED,
  CREATE_WINDOW,
  LAST_SIGNAL
};

enum {
  STEP_PREV,
  STEP_NEXT
};

typedef enum
{
  ARROW_LEFT_BEFORE,
  ARROW_RIGHT_BEFORE,
  ARROW_LEFT_AFTER,
  ARROW_RIGHT_AFTER,
  ARROW_NONE
} GtkNotebookArrow;

typedef enum
{
  POINTER_BEFORE,
  POINTER_AFTER,
  POINTER_BETWEEN
} GtkNotebookPointerPosition;

#define ARROW_IS_LEFT(arrow)  ((arrow) == ARROW_LEFT_BEFORE || (arrow) == ARROW_LEFT_AFTER)
#define ARROW_IS_BEFORE(arrow) ((arrow) == ARROW_LEFT_BEFORE || (arrow) == ARROW_RIGHT_BEFORE)

enum {
  PROP_0,
  PROP_TAB_POS,
  PROP_SHOW_TABS,
  PROP_SHOW_BORDER,
  PROP_SCROLLABLE,
  PROP_PAGE,
  PROP_ENABLE_POPUP,
  PROP_GROUP_NAME,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

enum {
  CHILD_PROP_0,
  CHILD_PROP_TAB_LABEL,
  CHILD_PROP_MENU_LABEL,
  CHILD_PROP_POSITION,
  CHILD_PROP_TAB_EXPAND,
  CHILD_PROP_TAB_FILL,
  CHILD_PROP_REORDERABLE,
  CHILD_PROP_DETACHABLE
};

#define GTK_NOTEBOOK_PAGE(_glist_)         ((GtkNotebookPage *)(_glist_)->data)

/* some useful defines for calculating coords */
#define NOTEBOOK_IS_TAB_LABEL_PARENT(_notebook_,_page_) (gtk_widget_get_parent ((_page_)->tab_label) == (GTK_WIDGET (_notebook_)))

struct _GtkNotebookPage
{
  GtkWidget *child;
  GtkWidget *tab_label;
  GtkWidget *menu_label;
  GtkWidget *last_focus_child;  /* Last descendant of the page that had focus */

  GtkCssGadget *gadget;         /* gadget used for the tab itself */

  guint default_menu : 1;       /* If true, we create the menu label ourself */
  guint default_tab  : 1;       /* If true, we create the tab label ourself */
  guint expand       : 1;
  guint fill         : 1;
  guint reorderable  : 1;
  guint detachable   : 1;

  GtkRequisition requisition;

  gulong mnemonic_activate_signal;
  gulong notify_visible_handler;
};

static const GtkTargetEntry src_notebook_targets [] = {
  { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, 0 },
  { "application/x-rootwindow-drop", 0, 0 },
};

static const GtkTargetEntry dst_notebook_targets [] = {
  { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, 0 },
};

/*** GtkNotebook Methods ***/
static gboolean gtk_notebook_select_page         (GtkNotebook      *notebook,
                                                  gboolean          move_focus);
static gboolean gtk_notebook_focus_tab           (GtkNotebook      *notebook,
                                                  GtkNotebookTab    type);
static gboolean gtk_notebook_change_current_page (GtkNotebook      *notebook,
                                                  gint              offset);
static void     gtk_notebook_move_focus_out      (GtkNotebook      *notebook,
                                                  GtkDirectionType  direction_type);
static gboolean gtk_notebook_reorder_tab         (GtkNotebook      *notebook,
                                                  GtkDirectionType  direction_type,
                                                  gboolean          move_to_last);
static void     gtk_notebook_remove_tab_label    (GtkNotebook      *notebook,
                                                  GtkNotebookPage  *page);
static void     gtk_notebook_set_tab_label_packing   (GtkNotebook  *notebook,
                                                      GtkWidget    *child,
                                                      gboolean      expand,
                                                      gboolean      fill);
static void     gtk_notebook_query_tab_label_packing (GtkNotebook  *notebook,
                                                      GtkWidget    *child,
                                                      gboolean     *expand,
                                                      gboolean     *fill);

/*** GObject Methods ***/
static void gtk_notebook_set_property        (GObject         *object,
                                              guint            prop_id,
                                              const GValue    *value,
                                              GParamSpec      *pspec);
static void gtk_notebook_get_property        (GObject         *object,
                                              guint            prop_id,
                                              GValue          *value,
                                              GParamSpec      *pspec);
static void gtk_notebook_finalize            (GObject         *object);

/*** GtkWidget Methods ***/
static void gtk_notebook_destroy             (GtkWidget        *widget);
static void gtk_notebook_map                 (GtkWidget        *widget);
static void gtk_notebook_unmap               (GtkWidget        *widget);
static void gtk_notebook_realize             (GtkWidget        *widget);
static void gtk_notebook_unrealize           (GtkWidget        *widget);
static void gtk_notebook_get_preferred_width (GtkWidget        *widget,
                                              gint             *minimum,
                                              gint             *natural);
static void gtk_notebook_get_preferred_height(GtkWidget        *widget,
                                              gint             *minimum,
                                              gint             *natural);
static void gtk_notebook_get_preferred_width_for_height
                                             (GtkWidget        *widget,
                                              gint              height,
                                              gint             *minimum,
                                              gint             *natural);
static void gtk_notebook_get_preferred_height_for_width
                                             (GtkWidget        *widget,
                                              gint              width,
                                              gint             *minimum,
                                              gint             *natural);
static void gtk_notebook_size_allocate       (GtkWidget        *widget,
                                              GtkAllocation    *allocation);
static gboolean gtk_notebook_draw            (GtkWidget        *widget,
                                              cairo_t          *cr);
static gboolean gtk_notebook_button_press    (GtkWidget        *widget,
                                              GdkEventButton   *event);
static gboolean gtk_notebook_button_release  (GtkWidget        *widget,
                                              GdkEventButton   *event);
static gboolean gtk_notebook_popup_menu      (GtkWidget        *widget);
static gboolean gtk_notebook_enter_notify    (GtkWidget        *widget,
                                              GdkEventCrossing *event);
static gboolean gtk_notebook_leave_notify    (GtkWidget        *widget,
                                              GdkEventCrossing *event);
static gboolean gtk_notebook_motion_notify   (GtkWidget        *widget,
                                              GdkEventMotion   *event);
static gboolean gtk_notebook_focus_in        (GtkWidget        *widget,
                                              GdkEventFocus    *event);
static gboolean gtk_notebook_focus_out       (GtkWidget        *widget,
                                              GdkEventFocus    *event);
static void gtk_notebook_grab_notify         (GtkWidget          *widget,
                                              gboolean            was_grabbed);
static void gtk_notebook_state_flags_changed (GtkWidget          *widget,
                                              GtkStateFlags       previous_state);
static gboolean gtk_notebook_focus           (GtkWidget        *widget,
                                              GtkDirectionType  direction);
static void gtk_notebook_style_updated       (GtkWidget        *widget);

/*** Drag and drop Methods ***/
static void gtk_notebook_drag_begin          (GtkWidget        *widget,
                                              GdkDragContext   *context);
static void gtk_notebook_drag_end            (GtkWidget        *widget,
                                              GdkDragContext   *context);
static gboolean gtk_notebook_drag_failed     (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              GtkDragResult     result);
static gboolean gtk_notebook_drag_motion     (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              gint              x,
                                              gint              y,
                                              guint             time);
static void gtk_notebook_drag_leave          (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              guint             time);
static gboolean gtk_notebook_drag_drop       (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              gint              x,
                                              gint              y,
                                              guint             time);
static void gtk_notebook_drag_data_get       (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              GtkSelectionData *data,
                                              guint             info,
                                              guint             time);
static void gtk_notebook_drag_data_received  (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              gint              x,
                                              gint              y,
                                              GtkSelectionData *data,
                                              guint             info,
                                              guint             time);
static void gtk_notebook_direction_changed   (GtkWidget        *widget,
                                              GtkTextDirection  previous_direction);

/*** GtkContainer Methods ***/
static void gtk_notebook_set_child_property  (GtkContainer     *container,
                                              GtkWidget        *child,
                                              guint             property_id,
                                              const GValue     *value,
                                              GParamSpec       *pspec);
static void gtk_notebook_get_child_property  (GtkContainer     *container,
                                              GtkWidget        *child,
                                              guint             property_id,
                                              GValue           *value,
                                              GParamSpec       *pspec);
static void gtk_notebook_add                 (GtkContainer     *container,
                                              GtkWidget        *widget);
static void gtk_notebook_remove              (GtkContainer     *container,
                                              GtkWidget        *widget);
static void gtk_notebook_set_focus_child     (GtkContainer     *container,
                                              GtkWidget        *child);
static GType gtk_notebook_child_type       (GtkContainer     *container);
static void gtk_notebook_forall              (GtkContainer     *container,
                                              gboolean          include_internals,
                                              GtkCallback       callback,
                                              gpointer          callback_data);

/*** GtkNotebook Methods ***/
static gint gtk_notebook_real_insert_page    (GtkNotebook      *notebook,
                                              GtkWidget        *child,
                                              GtkWidget        *tab_label,
                                              GtkWidget        *menu_label,
                                              gint              position);

static GtkNotebook *gtk_notebook_create_window (GtkNotebook    *notebook,
                                                GtkWidget      *page,
                                                gint            x,
                                                gint            y);

/*** Gadget Functions ***/
static void gtk_notebook_measure_tabs        (GtkCssGadget     *gadget,
                                              GtkOrientation    orientation,
                                              gint              for_size,
                                              gint             *minimum,
                                              gint             *natural,
                                              gint             *minimum_baseline,
                                              gint             *natural_baseline,
                                              gpointer          data);
static void gtk_notebook_allocate_tabs       (GtkCssGadget     *gadget,
                                              const GtkAllocation *allocation,
                                              int               baseline,
                                              GtkAllocation    *out_clip,
                                              gpointer          data);
static gboolean gtk_notebook_draw_tabs       (GtkCssGadget     *gadget,
                                              cairo_t          *cr,
                                              int               x,
                                              int               y,
                                              int               width,
                                              int               height,
                                              gpointer          data);
static void gtk_notebook_measure_stack       (GtkCssGadget     *gadget,
                                              GtkOrientation    orientation,
                                              gint              for_size,
                                              gint             *minimum,
                                              gint             *natural,
                                              gint             *minimum_baseline,
                                              gint             *natural_baseline,
                                              gpointer          data);
static void gtk_notebook_allocate_stack      (GtkCssGadget     *gadget,
                                              const GtkAllocation *allocation,
                                              int               baseline,
                                              GtkAllocation    *out_clip,
                                              gpointer          data);
static gboolean gtk_notebook_draw_stack      (GtkCssGadget     *gadget,
                                              cairo_t          *cr,
                                              int               x,
                                              int               y,
                                              int               width,
                                              int               height,
                                              gpointer          data);

/*** GtkNotebook Private Functions ***/
static void gtk_notebook_redraw_arrows       (GtkNotebook      *notebook);
static void gtk_notebook_real_remove         (GtkNotebook      *notebook,
                                              GList            *list);
static void gtk_notebook_update_labels       (GtkNotebook      *notebook);
static gint gtk_notebook_timer               (GtkNotebook      *notebook);
static void gtk_notebook_set_scroll_timer    (GtkNotebook *notebook);
static gint gtk_notebook_page_compare        (gconstpointer     a,
                                              gconstpointer     b);
static GList* gtk_notebook_find_child        (GtkNotebook      *notebook,
                                              GtkWidget        *child);
static GList * gtk_notebook_search_page      (GtkNotebook      *notebook,
                                              GList            *list,
                                              gint              direction,
                                              gboolean          find_visible);
static void  gtk_notebook_child_reordered    (GtkNotebook      *notebook,
                                              GtkNotebookPage  *page);

/*** GtkNotebook Size Allocate Functions ***/
static void gtk_notebook_pages_allocate      (GtkNotebook      *notebook,
                                              const GtkAllocation *allocation);
static void gtk_notebook_calc_tabs           (GtkNotebook      *notebook,
                                              GList            *start,
                                              GList           **end,
                                              gint             *tab_space,
                                              guint             direction);

/*** GtkNotebook Page Switch Methods ***/
static void gtk_notebook_real_switch_page    (GtkNotebook      *notebook,
                                              GtkWidget        *child,
                                              guint             page_num);

/*** GtkNotebook Page Switch Functions ***/
static void gtk_notebook_switch_page         (GtkNotebook      *notebook,
                                              GtkNotebookPage  *page);
static gint gtk_notebook_page_select         (GtkNotebook      *notebook,
                                              gboolean          move_focus);
static void gtk_notebook_switch_focus_tab    (GtkNotebook      *notebook,
                                              GList            *new_child);
static void gtk_notebook_menu_switch_page    (GtkWidget        *widget,
                                              GtkNotebookPage  *page);

/*** GtkNotebook Menu Functions ***/
static void gtk_notebook_menu_item_create    (GtkNotebook      *notebook,
                                              GList            *list);
static void gtk_notebook_menu_item_recreate  (GtkNotebook      *notebook,
                                              GList            *list);
static void gtk_notebook_menu_label_unparent (GtkWidget        *widget,
                                              gpointer          data);
static void gtk_notebook_menu_detacher       (GtkWidget        *widget,
                                              GtkMenu          *menu);

static void gtk_notebook_update_tab_pos      (GtkNotebook      *notebook);

/*** GtkNotebook Private Setters ***/
static gboolean gtk_notebook_mnemonic_activate_switch_page (GtkWidget *child,
                                                            gboolean overload,
                                                            gpointer data);

static gboolean focus_tabs_in  (GtkNotebook      *notebook);
static gboolean focus_child_in (GtkNotebook      *notebook,
                                GtkDirectionType  direction);

static void stop_scrolling (GtkNotebook *notebook);
static void do_detach_tab  (GtkNotebook *from,
                            GtkNotebook *to,
                            GtkWidget   *child,
                            gint         x,
                            gint         y);

/* GtkBuildable */
static void gtk_notebook_buildable_init           (GtkBuildableIface *iface);
static void gtk_notebook_buildable_add_child      (GtkBuildable *buildable,
                                                   GtkBuilder   *builder,
                                                   GObject      *child,
                                                   const gchar  *type);

static guint notebook_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkNotebook, gtk_notebook, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkNotebook)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_notebook_buildable_init))

static void
add_tab_bindings (GtkBindingSet    *binding_set,
                  GdkModifierType   modifiers,
                  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
add_arrow_bindings (GtkBindingSet    *binding_set,
                    guint             keysym,
                    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_binding_entry_add_signal (binding_set, keysym, GDK_CONTROL_MASK,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, GDK_CONTROL_MASK,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
add_reorder_bindings (GtkBindingSet    *binding_set,
                      guint             keysym,
                      GtkDirectionType  direction,
                      gboolean          move_to_last)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_binding_entry_add_signal (binding_set, keysym, GDK_MOD1_MASK,
                                "reorder_tab", 2,
                                GTK_TYPE_DIRECTION_TYPE, direction,
                                G_TYPE_BOOLEAN, move_to_last);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, GDK_MOD1_MASK,
                                "reorder_tab", 2,
                                GTK_TYPE_DIRECTION_TYPE, direction,
                                G_TYPE_BOOLEAN, move_to_last);
}

static gboolean
gtk_object_handled_accumulator (GSignalInvocationHint *ihint,
                                GValue                *return_accu,
                                const GValue          *handler_return,
                                gpointer               dummy)
{
  gboolean continue_emission;
  GObject *object;

  object = g_value_get_object (handler_return);
  g_value_set_object (return_accu, object);
  continue_emission = !object;

  return continue_emission;
}

static void
gtk_notebook_compute_expand (GtkWidget *widget,
                             gboolean  *hexpand_p,
                             gboolean  *vexpand_p)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  gboolean hexpand;
  gboolean vexpand;
  GList *list;
  GtkNotebookPage *page;

  hexpand = FALSE;
  vexpand = FALSE;

  for (list = priv->children; list; list = list->next)
    {
      page = list->data;

      hexpand = hexpand ||
        gtk_widget_compute_expand (page->child, GTK_ORIENTATION_HORIZONTAL);

      vexpand = vexpand ||
        gtk_widget_compute_expand (page->child, GTK_ORIENTATION_VERTICAL);

      if (hexpand & vexpand)
        break;
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static void
gtk_notebook_class_init (GtkNotebookClass *class)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkBindingSet *binding_set;

  gobject_class->set_property = gtk_notebook_set_property;
  gobject_class->get_property = gtk_notebook_get_property;
  gobject_class->finalize = gtk_notebook_finalize;

  widget_class->destroy = gtk_notebook_destroy;
  widget_class->map = gtk_notebook_map;
  widget_class->unmap = gtk_notebook_unmap;
  widget_class->realize = gtk_notebook_realize;
  widget_class->unrealize = gtk_notebook_unrealize;
  widget_class->get_preferred_width = gtk_notebook_get_preferred_width;
  widget_class->get_preferred_height = gtk_notebook_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_notebook_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_notebook_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_notebook_size_allocate;
  widget_class->draw = gtk_notebook_draw;
  widget_class->button_press_event = gtk_notebook_button_press;
  widget_class->button_release_event = gtk_notebook_button_release;
  widget_class->popup_menu = gtk_notebook_popup_menu;
  widget_class->enter_notify_event = gtk_notebook_enter_notify;
  widget_class->leave_notify_event = gtk_notebook_leave_notify;
  widget_class->motion_notify_event = gtk_notebook_motion_notify;
  widget_class->grab_notify = gtk_notebook_grab_notify;
  widget_class->state_flags_changed = gtk_notebook_state_flags_changed;
  widget_class->focus_in_event = gtk_notebook_focus_in;
  widget_class->focus_out_event = gtk_notebook_focus_out;
  widget_class->focus = gtk_notebook_focus;
  widget_class->style_updated = gtk_notebook_style_updated;
  widget_class->drag_begin = gtk_notebook_drag_begin;
  widget_class->drag_end = gtk_notebook_drag_end;
  widget_class->drag_motion = gtk_notebook_drag_motion;
  widget_class->drag_leave = gtk_notebook_drag_leave;
  widget_class->drag_drop = gtk_notebook_drag_drop;
  widget_class->drag_data_get = gtk_notebook_drag_data_get;
  widget_class->drag_data_received = gtk_notebook_drag_data_received;
  widget_class->drag_failed = gtk_notebook_drag_failed;
  widget_class->compute_expand = gtk_notebook_compute_expand;
  widget_class->direction_changed = gtk_notebook_direction_changed;

  container_class->add = gtk_notebook_add;
  container_class->remove = gtk_notebook_remove;
  container_class->forall = gtk_notebook_forall;
  container_class->set_focus_child = gtk_notebook_set_focus_child;
  container_class->get_child_property = gtk_notebook_get_child_property;
  container_class->set_child_property = gtk_notebook_set_child_property;
  container_class->child_type = gtk_notebook_child_type;

  class->switch_page = gtk_notebook_real_switch_page;
  class->insert_page = gtk_notebook_real_insert_page;

  class->focus_tab = gtk_notebook_focus_tab;
  class->select_page = gtk_notebook_select_page;
  class->change_current_page = gtk_notebook_change_current_page;
  class->move_focus_out = gtk_notebook_move_focus_out;
  class->reorder_tab = gtk_notebook_reorder_tab;
  class->create_window = gtk_notebook_create_window;

  properties[PROP_PAGE] =
      g_param_spec_int ("page",
                        P_("Page"),
                        P_("The index of the current page"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_TAB_POS] =
      g_param_spec_enum ("tab-pos",
                         P_("Tab Position"),
                         P_("Which side of the notebook holds the tabs"),
                         GTK_TYPE_POSITION_TYPE,
                         GTK_POS_TOP,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_TABS] =
      g_param_spec_boolean ("show-tabs",
                            P_("Show Tabs"),
                            P_("Whether tabs should be shown"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_BORDER] =
      g_param_spec_boolean ("show-border",
                            P_("Show Border"),
                            P_("Whether the border should be shown"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SCROLLABLE] =
      g_param_spec_boolean ("scrollable",
                            P_("Scrollable"),
                            P_("If TRUE, scroll arrows are added if there are too many tabs to fit"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ENABLE_POPUP] =
      g_param_spec_boolean ("enable-popup",
                            P_("Enable Popup"),
                            P_("If TRUE, pressing the right mouse button on the notebook pops up a menu that you can use to go to a page"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkNotebook:group-name:
   *
   * Group name for tab drag and drop.
   *
   * Since: 2.24
   */
  properties[PROP_GROUP_NAME] =
      g_param_spec_string ("group-name",
                           P_("Group Name"),
                           P_("Group name for tab drag and drop"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, properties);

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_TAB_LABEL,
                                              g_param_spec_string ("tab-label",
                                                                   P_("Tab label"),
                                                                   P_("The string displayed on the child's tab label"),
                                                                   NULL,
                                                                   GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_MENU_LABEL,
                                              g_param_spec_string ("menu-label",
                                                                   P_("Menu label"),
                                                                   P_("The string displayed in the child's menu entry"),
                                                                   NULL,
                                                                   GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_POSITION,
                                              g_param_spec_int ("position",
                                                                P_("Position"),
                                                                P_("The index of the child in the parent"),
                                                                -1, G_MAXINT, 0,
                                                                GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_TAB_EXPAND,
                                              g_param_spec_boolean ("tab-expand",
                                                                    P_("Tab expand"),
                                                                    P_("Whether to expand the child's tab"),
                                                                    FALSE,
                                                                    GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_TAB_FILL,
                                              g_param_spec_boolean ("tab-fill",
                                                                    P_("Tab fill"),
                                                                    P_("Whether the child's tab should fill the allocated area"),
                                                                    TRUE,
                                                                    GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_REORDERABLE,
                                              g_param_spec_boolean ("reorderable",
                                                                    P_("Tab reorderable"),
                                                                    P_("Whether the tab is reorderable by user action"),
                                                                    FALSE,
                                                                    GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_DETACHABLE,
                                              g_param_spec_boolean ("detachable",
                                                                    P_("Tab detachable"),
                                                                    P_("Whether the tab is detachable"),
                                                                    FALSE,
                                                                    GTK_PARAM_READWRITE));

/**
 * GtkNotebook:has-secondary-backward-stepper:
 *
 * The “has-secondary-backward-stepper” property determines whether
 * a second backward arrow button is displayed on the opposite end
 * of the tab area.
 *
 * Since: 2.4
 */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-secondary-backward-stepper",
                                                                 P_("Secondary backward stepper"),
                                                                 P_("Display a second backward arrow button on the opposite end of the tab area"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

/**
 * GtkNotebook:has-secondary-forward-stepper:
 *
 * The “has-secondary-forward-stepper” property determines whether
 * a second forward arrow button is displayed on the opposite end
 * of the tab area.
 *
 * Since: 2.4
 */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-secondary-forward-stepper",
                                                                 P_("Secondary forward stepper"),
                                                                 P_("Display a second forward arrow button on the opposite end of the tab area"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

/**
 * GtkNotebook:has-backward-stepper:
 *
 * The “has-backward-stepper” property determines whether
 * the standard backward arrow button is displayed.
 *
 * Since: 2.4
 */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-backward-stepper",
                                                                 P_("Backward stepper"),
                                                                 P_("Display the standard backward arrow button"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

/**
 * GtkNotebook:has-forward-stepper:
 *
 * The “has-forward-stepper” property determines whether
 * the standard forward arrow button is displayed.
 *
 * Since: 2.4
 */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-forward-stepper",
                                                                 P_("Forward stepper"),
                                                                 P_("Display the standard forward arrow button"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

/**
 * GtkNotebook:tab-overlap:
 *
 * The “tab-overlap” property defines size of tab overlap
 * area.
 *
 * Since: 2.10
 *
 * Deprecated: 3.20: This property is ignored. Use margins on tab nodes
 *     to achieve the same effect.
 */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("tab-overlap",
                                                             P_("Tab overlap"),
                                                             P_("Size of tab overlap area"),
                                                             G_MININT,
                                                             G_MAXINT,
                                                             2,
                                                             GTK_PARAM_READABLE | G_PARAM_DEPRECATED));

/**
 * GtkNotebook:tab-curvature:
 *
 * The “tab-curvature” property defines size of tab curvature.
 *
 * Since: 2.10
 *
 * Deprecated: 3.20: This property is ignored. Use margins on tab nodes
 *     to achieve the same effect.
 */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("tab-curvature",
                                                             P_("Tab curvature"),
                                                             P_("Size of tab curvature"),
                                                             0,
                                                             G_MAXINT,
                                                             1,
                                                             GTK_PARAM_READABLE | G_PARAM_DEPRECATED));

  /**
   * GtkNotebook:arrow-spacing:
   *
   * The "arrow-spacing" property defines the spacing between the scroll
   * arrows and the tabs.
   *
   * Since: 2.10
   *
   * Deprecated: 3.20: This property is ignored. Use margins on arrows or
   *     the "tabs" node to achieve the same effect.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("arrow-spacing",
                                                             P_("Arrow spacing"),
                                                             P_("Scroll arrow spacing"),
                                                             0,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE | G_PARAM_DEPRECATED));

  /**
   * GtkNotebook:initial-gap:
   *
   * The "initial-gap" property defines the minimum size for the initial
   * gap between the first tab.
   *
   * Since: 3.2
   *
   * Deprecated: 3.20: The intial gap is ignored. Use margins on the header node
   *     to achieve the same effect.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("initial-gap",
                                                             P_("Initial gap"),
                                                             P_("Initial gap before the first tab"),
                                                             0,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE | G_PARAM_DEPRECATED));

  /**
   * GtkNotebook:has-tab-gap:
   *
   * The "has-tab-gap" property defines whether the active tab is draw
   * with a gap at the bottom. When %TRUE the theme engine uses
   * gtk_render_extension to draw the active tab. When %FALSE
   * gtk_render_background and gtk_render_frame are used.
   *
   * Since: 3.12
   *
   * Deprecated: 3.20: This function always behaves as if it was set to %FALSE.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-tab-gap",
                                                                 P_("Tab gap"),
                                                                 P_("Active tab is drawn with a gap at the bottom"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE | G_PARAM_DEPRECATED));

  /**
   * GtkNotebook::switch-page:
   * @notebook: the object which received the signal.
   * @page: the new current page
   * @page_num: the index of the page
   *
   * Emitted when the user or a function changes the current page.
   */
  notebook_signals[SWITCH_PAGE] =
    g_signal_new (I_("switch-page"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNotebookClass, switch_page),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_WIDGET,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (notebook_signals[SWITCH_PAGE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_VOID__OBJECT_UINTv);
  notebook_signals[FOCUS_TAB] =
    g_signal_new (I_("focus-tab"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, focus_tab),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_NOTEBOOK_TAB);
  g_signal_set_va_marshaller (notebook_signals[FOCUS_TAB],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__ENUMv);
  notebook_signals[SELECT_PAGE] =
    g_signal_new (I_("select-page"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, select_page),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (notebook_signals[SELECT_PAGE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);
  notebook_signals[CHANGE_CURRENT_PAGE] =
    g_signal_new (I_("change-current-page"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, change_current_page),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (notebook_signals[CHANGE_CURRENT_PAGE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__INTv);
  notebook_signals[MOVE_FOCUS_OUT] =
    g_signal_new (I_("move-focus-out"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, move_focus_out),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_DIRECTION_TYPE);
  notebook_signals[REORDER_TAB] =
    g_signal_new (I_("reorder-tab"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, reorder_tab),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_BOOLEAN,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_DIRECTION_TYPE,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (notebook_signals[REORDER_TAB],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__ENUM_BOOLEANv);
  /**
   * GtkNotebook::page-reordered:
   * @notebook: the #GtkNotebook
   * @child: the child #GtkWidget affected
   * @page_num: the new page number for @child
   *
   * the ::page-reordered signal is emitted in the notebook
   * right after a page has been reordered.
   *
   * Since: 2.10
   */
  notebook_signals[PAGE_REORDERED] =
    g_signal_new (I_("page-reordered"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNotebookClass, page_reordered),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_WIDGET,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (notebook_signals[PAGE_REORDERED],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_VOID__OBJECT_UINTv);
  /**
   * GtkNotebook::page-removed:
   * @notebook: the #GtkNotebook
   * @child: the child #GtkWidget affected
   * @page_num: the @child page number
   *
   * the ::page-removed signal is emitted in the notebook
   * right after a page is removed from the notebook.
   *
   * Since: 2.10
   */
  notebook_signals[PAGE_REMOVED] =
    g_signal_new (I_("page-removed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNotebookClass, page_removed),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_WIDGET,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (notebook_signals[PAGE_REMOVED],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_VOID__OBJECT_UINTv);
  /**
   * GtkNotebook::page-added:
   * @notebook: the #GtkNotebook
   * @child: the child #GtkWidget affected
   * @page_num: the new page number for @child
   *
   * the ::page-added signal is emitted in the notebook
   * right after a page is added to the notebook.
   *
   * Since: 2.10
   */
  notebook_signals[PAGE_ADDED] =
    g_signal_new (I_("page-added"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNotebookClass, page_added),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_WIDGET,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (notebook_signals[PAGE_ADDED],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_VOID__OBJECT_UINTv);

  /**
   * GtkNotebook::create-window:
   * @notebook: the #GtkNotebook emitting the signal
   * @page: the tab of @notebook that is being detached
   * @x: the X coordinate where the drop happens
   * @y: the Y coordinate where the drop happens
   *
   * The ::create-window signal is emitted when a detachable
   * tab is dropped on the root window.
   *
   * A handler for this signal can create a window containing
   * a notebook where the tab will be attached. It is also
   * responsible for moving/resizing the window and adding the
   * necessary properties to the notebook (e.g. the
   * #GtkNotebook:group-name ).
   *
   * Returns: (transfer none): a #GtkNotebook that @page should be
   *     added to, or %NULL.
   *
   * Since: 2.12
   */
  notebook_signals[CREATE_WINDOW] =
    g_signal_new (I_("create-window"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNotebookClass, create_window),
                  gtk_object_handled_accumulator, NULL,
                  _gtk_marshal_OBJECT__OBJECT_INT_INT,
                  GTK_TYPE_NOTEBOOK, 3,
                  GTK_TYPE_WIDGET, G_TYPE_INT, G_TYPE_INT);
  g_signal_set_va_marshaller (notebook_signals[CREATE_WINDOW],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_OBJECT__OBJECT_INT_INTv);

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_space, 0,
                                "select-page", 1,
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Space, 0,
                                "select-page", 1,
                                G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Home, 0,
                                "focus-tab", 1,
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_FIRST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_Home, 0,
                                "focus-tab", 1,
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_FIRST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_End, 0,
                                "focus-tab", 1,
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_LAST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_KP_End, 0,
                                "focus-tab", 1,
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_LAST);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Up, GDK_CONTROL_MASK | GDK_MOD1_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Down, GDK_CONTROL_MASK | GDK_MOD1_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, 1);

  add_arrow_bindings (binding_set, GDK_KEY_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_KEY_Down, GTK_DIR_DOWN);
  add_arrow_bindings (binding_set, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_KEY_Right, GTK_DIR_RIGHT);

  add_reorder_bindings (binding_set, GDK_KEY_Up, GTK_DIR_UP, FALSE);
  add_reorder_bindings (binding_set, GDK_KEY_Down, GTK_DIR_DOWN, FALSE);
  add_reorder_bindings (binding_set, GDK_KEY_Left, GTK_DIR_LEFT, FALSE);
  add_reorder_bindings (binding_set, GDK_KEY_Right, GTK_DIR_RIGHT, FALSE);
  add_reorder_bindings (binding_set, GDK_KEY_Home, GTK_DIR_LEFT, TRUE);
  add_reorder_bindings (binding_set, GDK_KEY_Home, GTK_DIR_UP, TRUE);
  add_reorder_bindings (binding_set, GDK_KEY_End, GTK_DIR_RIGHT, TRUE);
  add_reorder_bindings (binding_set, GDK_KEY_End, GTK_DIR_DOWN, TRUE);

  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_container_class_handle_border_width (container_class);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_NOTEBOOK_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "notebook");
}

static void
gtk_notebook_init (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;
  GtkCssNode *widget_node;

  gtk_widget_set_can_focus (GTK_WIDGET (notebook), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (notebook), FALSE);

  notebook->priv = gtk_notebook_get_instance_private (notebook);
  priv = notebook->priv;

  priv->cur_page = NULL;
  priv->children = NULL;
  priv->first_tab = NULL;
  priv->focus_tab = NULL;
  priv->event_window = NULL;
  priv->menu = NULL;

  priv->show_tabs = TRUE;
  priv->show_border = TRUE;
  priv->tab_pos = GTK_POS_TOP;
  priv->scrollable = FALSE;
  priv->in_child = ARROW_NONE;
  priv->click_child = ARROW_NONE;
  priv->need_timer = 0;
  priv->child_has_focus = FALSE;
  priv->focus_out = FALSE;

  priv->group = 0;
  priv->pressed_button = 0;
  priv->dnd_timer = 0;
  priv->switch_tab_timer = 0;
  priv->source_targets = gtk_target_list_new (src_notebook_targets,
                                              G_N_ELEMENTS (src_notebook_targets));
  priv->operation = DRAG_OPERATION_NONE;
  priv->detached_tab = NULL;
  priv->has_scrolled = FALSE;

  if (gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL)
    priv->tabs_reversed = TRUE;
  else
    priv->tabs_reversed = FALSE;

  gtk_drag_dest_set (GTK_WIDGET (notebook), 0,
                     dst_notebook_targets, G_N_ELEMENTS (dst_notebook_targets),
                     GDK_ACTION_MOVE);

  gtk_drag_dest_set_track_motion (GTK_WIDGET (notebook), TRUE);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (notebook));
  priv->gadget = gtk_box_gadget_new_for_node (widget_node,
                                              GTK_WIDGET (notebook));
  gtk_css_gadget_add_class (priv->gadget, GTK_STYLE_CLASS_FRAME);
  gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_VERTICAL);
  gtk_box_gadget_set_draw_reverse (GTK_BOX_GADGET (priv->gadget), TRUE);

  priv->stack_gadget = gtk_css_custom_gadget_new ("stack",
                                                  GTK_WIDGET (notebook),
                                                  priv->gadget,
                                                  NULL,
                                                  gtk_notebook_measure_stack,
                                                  gtk_notebook_allocate_stack,
                                                  gtk_notebook_draw_stack,
                                                  NULL,
                                                  NULL);
  gtk_css_gadget_set_state (priv->stack_gadget, gtk_css_node_get_state (widget_node));
  gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->gadget), -1, priv->stack_gadget, TRUE, GTK_ALIGN_FILL);

  priv->header_gadget = gtk_box_gadget_new ("header",
                                            GTK_WIDGET (notebook),
                                            priv->gadget,
                                            priv->stack_gadget);
  gtk_css_gadget_add_class (priv->header_gadget, GTK_STYLE_CLASS_TOP);
  gtk_css_gadget_set_state (priv->header_gadget, gtk_css_node_get_state (widget_node));
  gtk_css_gadget_set_visible (priv->header_gadget, FALSE);
  gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->gadget), 0, priv->header_gadget, FALSE, GTK_ALIGN_FILL);

  priv->tabs_gadget = gtk_css_custom_gadget_new ("tabs",
                                                 GTK_WIDGET (notebook),
                                                 priv->header_gadget,
                                                 NULL,
                                                 gtk_notebook_measure_tabs,
                                                 gtk_notebook_allocate_tabs,
                                                 gtk_notebook_draw_tabs,
                                                 NULL,
                                                 NULL);
  gtk_css_gadget_set_state (priv->tabs_gadget, gtk_css_node_get_state (widget_node));
  gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->header_gadget), 0, priv->tabs_gadget, TRUE, GTK_ALIGN_FILL);
}

static void
gtk_notebook_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_notebook_buildable_add_child;
}

static void
gtk_notebook_buildable_add_child (GtkBuildable  *buildable,
                                  GtkBuilder    *builder,
                                  GObject       *child,
                                  const gchar   *type)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (buildable);

  if (type && strcmp (type, "tab") == 0)
    {
      GtkWidget * page;

      page = gtk_notebook_get_nth_page (notebook, -1);
      /* To set the tab label widget, we must have already a child
       * inside the tab container. */
      g_assert (page != NULL);
      /* warn when Glade tries to overwrite label */
      if (gtk_notebook_get_tab_label (notebook, page))
        g_warning ("Overriding tab label for notebook");
      gtk_notebook_set_tab_label (notebook, page, GTK_WIDGET (child));
    }
  else if (type && strcmp (type, "action-start") == 0)
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (child), GTK_PACK_START);
    }
  else if (type && strcmp (type, "action-end") == 0)
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (child), GTK_PACK_END);
    }
  else if (!type)
    gtk_notebook_append_page (notebook, GTK_WIDGET (child), NULL);
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (notebook, type);
}

static gboolean
gtk_notebook_has_current_page (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  return priv->cur_page &&
         gtk_widget_get_visible (priv->cur_page->child);
}

static gboolean
gtk_notebook_select_page (GtkNotebook *notebook,
                          gboolean     move_focus)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && priv->show_tabs)
    {
      gtk_notebook_page_select (notebook, move_focus);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_focus_tab (GtkNotebook       *notebook,
                        GtkNotebookTab     type)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *list;

  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && priv->show_tabs)
    {
      switch (type)
        {
        case GTK_NOTEBOOK_TAB_FIRST:
          list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);
          if (list)
            gtk_notebook_switch_focus_tab (notebook, list);
          break;
        case GTK_NOTEBOOK_TAB_LAST:
          list = gtk_notebook_search_page (notebook, NULL, STEP_PREV, TRUE);
          if (list)
            gtk_notebook_switch_focus_tab (notebook, list);
          break;
        }

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_change_current_page (GtkNotebook *notebook,
                                  gint         offset)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *current = NULL;

  if (!priv->show_tabs)
    return FALSE;

  if (priv->cur_page)
    current = g_list_find (priv->children, priv->cur_page);

  while (offset != 0)
    {
      current = gtk_notebook_search_page (notebook, current,
                                          offset < 0 ? STEP_PREV : STEP_NEXT,
                                          TRUE);

      if (!current)
        {
          current = gtk_notebook_search_page (notebook, NULL,
                                              offset < 0 ? STEP_PREV : STEP_NEXT,
                                              TRUE);
        }

      offset += offset < 0 ? 1 : -1;
    }

  if (current)
    gtk_notebook_switch_page (notebook, current->data);
  else
    gtk_widget_error_bell (GTK_WIDGET (notebook));

  return TRUE;
}

static GtkDirectionType
get_effective_direction (GtkNotebook      *notebook,
                         GtkDirectionType  direction)
{
  GtkNotebookPrivate *priv = notebook->priv;

  /* Remap the directions into the effective direction it would be for a
   * GTK_POS_TOP notebook
   */

#define D(rest) GTK_DIR_##rest

  static const GtkDirectionType translate_direction[2][4][6] = {
    /* LEFT */   {{ D(TAB_FORWARD),  D(TAB_BACKWARD), D(LEFT), D(RIGHT), D(UP),   D(DOWN) },
    /* RIGHT */  { D(TAB_BACKWARD), D(TAB_FORWARD),  D(LEFT), D(RIGHT), D(DOWN), D(UP)   },
    /* TOP */    { D(TAB_FORWARD),  D(TAB_BACKWARD), D(UP),   D(DOWN),  D(LEFT), D(RIGHT) },
    /* BOTTOM */ { D(TAB_BACKWARD), D(TAB_FORWARD),  D(DOWN), D(UP),    D(LEFT), D(RIGHT) }},
    /* LEFT */  {{ D(TAB_BACKWARD), D(TAB_FORWARD),  D(LEFT), D(RIGHT), D(DOWN), D(UP)   },
    /* RIGHT */  { D(TAB_FORWARD),  D(TAB_BACKWARD), D(LEFT), D(RIGHT), D(UP),   D(DOWN) },
    /* TOP */    { D(TAB_FORWARD),  D(TAB_BACKWARD), D(UP),   D(DOWN),  D(RIGHT), D(LEFT) },
    /* BOTTOM */ { D(TAB_BACKWARD), D(TAB_FORWARD),  D(DOWN), D(UP),    D(RIGHT), D(LEFT) }},
  };

#undef D

  int text_dir = gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL ? 1 : 0;

  return translate_direction[text_dir][priv->tab_pos][direction];
}

static GtkPositionType
get_effective_tab_pos (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL)
    {
      switch (priv->tab_pos)
        {
        case GTK_POS_LEFT:
          return GTK_POS_RIGHT;
        case GTK_POS_RIGHT:
          return GTK_POS_LEFT;
        default: ;
        }
    }

  return priv->tab_pos;
}

static void
gtk_notebook_move_focus_out (GtkNotebook      *notebook,
                             GtkDirectionType  direction_type)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkDirectionType effective_direction = get_effective_direction (notebook, direction_type);
  GtkWidget *toplevel;

  if (gtk_container_get_focus_child (GTK_CONTAINER (notebook)) && effective_direction == GTK_DIR_UP)
    if (focus_tabs_in (notebook))
      return;
  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && effective_direction == GTK_DIR_DOWN)
    if (focus_child_in (notebook, GTK_DIR_TAB_FORWARD))
      return;

  /* At this point, we know we should be focusing out of the notebook entirely. We
   * do this by setting a flag, then propagating the focus motion to the notebook.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (notebook));
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  g_object_ref (notebook);

  priv->focus_out = TRUE;
  g_signal_emit_by_name (toplevel, "move-focus", direction_type);
  priv->focus_out = FALSE;

  g_object_unref (notebook);
}

static gint
reorder_tab (GtkNotebook *notebook, GList *position, GList *tab)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *elem;

  if (position == tab)
    return g_list_position (priv->children, tab);

  /* check that we aren't inserting the tab in the
   * same relative position, taking packing into account */
  elem = (position) ? position->prev : g_list_last (priv->children);

  if (elem == tab)
    return g_list_position (priv->children, tab);

  /* now actually reorder the tab */
  if (priv->first_tab == tab)
    priv->first_tab = gtk_notebook_search_page (notebook, priv->first_tab,
                                                    STEP_NEXT, TRUE);

  priv->children = g_list_remove_link (priv->children, tab);

  if (!position)
    elem = g_list_last (priv->children);
  else
    {
      elem = position->prev;
      position->prev = tab;
    }

  if (elem)
    elem->next = tab;
  else
    priv->children = tab;

  tab->prev = elem;
  tab->next = position;

  return g_list_position (priv->children, tab);
}

static gboolean
gtk_notebook_reorder_tab (GtkNotebook      *notebook,
                          GtkDirectionType  direction_type,
                          gboolean          move_to_last)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkDirectionType effective_direction = get_effective_direction (notebook, direction_type);
  GList *last, *child, *element;
  gint page_num, old_page_num, i;

  if (!gtk_widget_is_focus (GTK_WIDGET (notebook)) || !priv->show_tabs)
    return FALSE;

  if (!gtk_notebook_has_current_page (notebook) ||
      !priv->cur_page->reorderable)
    return FALSE;

  if (effective_direction != GTK_DIR_LEFT &&
      effective_direction != GTK_DIR_RIGHT)
    return FALSE;

  if (move_to_last)
    {
      child = priv->focus_tab;

      do
        {
          last = child;
          child = gtk_notebook_search_page (notebook, last,
                                            (effective_direction == GTK_DIR_RIGHT) ? STEP_NEXT : STEP_PREV,
                                            TRUE);
        }
      while (child);

      child = last;
    }
  else
    child = gtk_notebook_search_page (notebook, priv->focus_tab,
                                      (effective_direction == GTK_DIR_RIGHT) ? STEP_NEXT : STEP_PREV,
                                      TRUE);

  if (!child || child->data == priv->cur_page)
    return FALSE;

  old_page_num = g_list_position (priv->children, priv->focus_tab);
  if (effective_direction == GTK_DIR_RIGHT)
    page_num = reorder_tab (notebook, child->next, priv->focus_tab);
  else
    page_num = reorder_tab (notebook, child, priv->focus_tab);
  
  gtk_notebook_child_reordered (notebook, priv->focus_tab->data);
  for (element = priv->children, i = 0; element; element = element->next, i++)
    {
      if (MIN (old_page_num, page_num) <= i && i <= MAX (old_page_num, page_num))
        gtk_widget_child_notify (((GtkNotebookPage *) element->data)->child, "position");
    }
  g_signal_emit (notebook,
                 notebook_signals[PAGE_REORDERED],
                 0,
                 ((GtkNotebookPage *) priv->focus_tab->data)->child,
                 page_num);

  return TRUE;
}

/**
 * gtk_notebook_new:
 *
 * Creates a new #GtkNotebook widget with no pages.

 * Returns: the newly created #GtkNotebook
 */
GtkWidget*
gtk_notebook_new (void)
{
  return g_object_new (GTK_TYPE_NOTEBOOK, NULL);
}

/* Private GObject Methods :
 *
 * gtk_notebook_set_property
 * gtk_notebook_get_property
 */
static void
gtk_notebook_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (object);

  switch (prop_id)
    {
    case PROP_SHOW_TABS:
      gtk_notebook_set_show_tabs (notebook, g_value_get_boolean (value));
      break;
    case PROP_SHOW_BORDER:
      gtk_notebook_set_show_border (notebook, g_value_get_boolean (value));
      break;
    case PROP_SCROLLABLE:
      gtk_notebook_set_scrollable (notebook, g_value_get_boolean (value));
      break;
    case PROP_ENABLE_POPUP:
      if (g_value_get_boolean (value))
        gtk_notebook_popup_enable (notebook);
      else
        gtk_notebook_popup_disable (notebook);
      break;
    case PROP_PAGE:
      gtk_notebook_set_current_page (notebook, g_value_get_int (value));
      break;
    case PROP_TAB_POS:
      gtk_notebook_set_tab_pos (notebook, g_value_get_enum (value));
      break;
    case PROP_GROUP_NAME:
      gtk_notebook_set_group_name (notebook, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_notebook_get_property (GObject         *object,
                           guint            prop_id,
                           GValue          *value,
                           GParamSpec      *pspec)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (object);
  GtkNotebookPrivate *priv = notebook->priv;

  switch (prop_id)
    {
    case PROP_SHOW_TABS:
      g_value_set_boolean (value, priv->show_tabs);
      break;
    case PROP_SHOW_BORDER:
      g_value_set_boolean (value, priv->show_border);
      break;
    case PROP_SCROLLABLE:
      g_value_set_boolean (value, priv->scrollable);
      break;
    case PROP_ENABLE_POPUP:
      g_value_set_boolean (value, priv->menu != NULL);
      break;
    case PROP_PAGE:
      g_value_set_int (value, gtk_notebook_get_current_page (notebook));
      break;
    case PROP_TAB_POS:
      g_value_set_enum (value, priv->tab_pos);
      break;
    case PROP_GROUP_NAME:
      g_value_set_string (value, gtk_notebook_get_group_name (notebook));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Private GtkWidget Methods :
 *
 * gtk_notebook_destroy
 * gtk_notebook_map
 * gtk_notebook_unmap
 * gtk_notebook_realize
 * gtk_notebook_size_allocate
 * gtk_notebook_draw
 * gtk_notebook_scroll
 * gtk_notebook_button_press
 * gtk_notebook_button_release
 * gtk_notebook_popup_menu
 * gtk_notebook_enter_notify
 * gtk_notebook_leave_notify
 * gtk_notebook_motion_notify
 * gtk_notebook_focus_in
 * gtk_notebook_focus_out
 * gtk_notebook_style_updated
 * gtk_notebook_drag_begin
 * gtk_notebook_drag_end
 * gtk_notebook_drag_failed
 * gtk_notebook_drag_motion
 * gtk_notebook_drag_drop
 * gtk_notebook_drag_data_get
 * gtk_notebook_drag_data_received
 */
static void
remove_switch_tab_timer (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->switch_tab_timer)
    {
      g_source_remove (priv->switch_tab_timer);
      priv->switch_tab_timer = 0;
    }
}

static void
gtk_notebook_destroy (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->action_widget[GTK_PACK_START])
    {
      gtk_widget_unparent (priv->action_widget[GTK_PACK_START]);
      priv->action_widget[GTK_PACK_START] = NULL;
    }

  if (priv->action_widget[GTK_PACK_END])
    {
      gtk_widget_unparent (priv->action_widget[GTK_PACK_END]);
      priv->action_widget[GTK_PACK_END] = NULL;
    }

  if (priv->menu)
    gtk_notebook_popup_disable (notebook);

  if (priv->source_targets)
    {
      gtk_target_list_unref (priv->source_targets);
      priv->source_targets = NULL;
    }

  remove_switch_tab_timer (notebook);

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->destroy (widget);
}

static void
gtk_notebook_finalize (GObject *object)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (object);
  GtkNotebookPrivate *priv = notebook->priv;

  g_clear_object (&priv->gadget);
  g_clear_object (&priv->header_gadget);
  g_clear_object (&priv->tabs_gadget);
  g_clear_object (&priv->arrow_gadget[0]);
  g_clear_object (&priv->arrow_gadget[1]);
  g_clear_object (&priv->arrow_gadget[2]);
  g_clear_object (&priv->arrow_gadget[3]);
  g_clear_object (&priv->stack_gadget);

  G_OBJECT_CLASS (gtk_notebook_parent_class)->finalize (object);
}

static void
update_node_ordering (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gboolean reverse_tabs;

  reverse_tabs = (priv->tab_pos == GTK_POS_TOP || priv->tab_pos == GTK_POS_BOTTOM) &&
                 gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL;

  if ((reverse_tabs && !priv->tabs_reversed) ||
      (!reverse_tabs && priv->tabs_reversed))
    {
      gtk_box_gadget_reverse_children (GTK_BOX_GADGET (priv->header_gadget));
      gtk_css_node_reverse_children (gtk_css_gadget_get_node (priv->tabs_gadget));
      priv->tabs_reversed = reverse_tabs;
    }
}

static void
gtk_notebook_direction_changed (GtkWidget        *widget,
                                GtkTextDirection  previous_dir)
{
  update_node_ordering (GTK_NOTEBOOK (widget));

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->direction_changed (widget, previous_dir);
}

static gboolean
gtk_notebook_get_event_window_position (GtkNotebook  *notebook,
                                        GdkRectangle *rectangle)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->show_tabs && gtk_notebook_has_current_page (notebook))
    {
      if (rectangle)
        gtk_css_gadget_get_border_allocation (priv->header_gadget, rectangle, NULL);

      return TRUE;
    }
  else
    {
      if (rectangle)
        {
          rectangle->x = rectangle->y = 0;
          rectangle->width = rectangle->height = 10;
        }
    }

  return FALSE;
}

static void
gtk_notebook_map (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->map (widget);

  if (gtk_notebook_get_event_window_position (notebook, NULL))
    gdk_window_show_unraised (priv->event_window);
}

static void
gtk_notebook_unmap (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  stop_scrolling (notebook);

  gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->unmap (widget);
}

static void
gtk_notebook_realize (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkRectangle event_window_pos;

  gtk_widget_set_realized (widget, TRUE);

  gtk_css_gadget_get_border_allocation (priv->header_gadget, &event_window_pos, NULL);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = event_window_pos.x;
  attributes.y = event_window_pos.y;
  attributes.width = event_window_pos.width;
  attributes.height = event_window_pos.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK |
                            GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                           &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_notebook_unrealize (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_widget_unregister_window (widget, priv->event_window);
  gdk_window_destroy (priv->event_window);
  priv->event_window = NULL;

  if (priv->drag_window)
    {
      gtk_widget_unregister_window (widget, priv->drag_window);
      gdk_window_destroy (priv->drag_window);
      priv->drag_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->unrealize (widget);
}

static void
gtk_notebook_distribute_arrow_width (GtkNotebook *notebook,
                                     GtkPackType  type,
                                     gint         size,
                                     gint        *out_left,
                                     gint        *out_right)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkRequestedSize sizes[2];

  if (priv->arrow_gadget[2 * type + 1] == NULL)
    {
      if (priv->arrow_gadget[2 * type] == NULL)
        *out_left = 0;
      else
        *out_left = size;
      *out_right = 0;
    }
  else if (priv->arrow_gadget[2 * type] == NULL)
    {
      *out_left = 0;
      *out_right = size;
    }
  else
    {
      gtk_css_gadget_get_preferred_size (priv->arrow_gadget[2 * type],
                                         GTK_ORIENTATION_HORIZONTAL,
                                         -1,
                                         &sizes[0].minimum_size, &sizes[0].natural_size,
                                         NULL, NULL);
      gtk_css_gadget_get_preferred_size (priv->arrow_gadget[2 * type + 1],
                                         GTK_ORIENTATION_HORIZONTAL,
                                         -1,
                                         &sizes[1].minimum_size, &sizes[1].natural_size,
                                         NULL, NULL);

      size -= sizes[0].minimum_size + sizes[1].minimum_size;
      size = gtk_distribute_natural_allocation (size, G_N_ELEMENTS (sizes), sizes);

      *out_left = sizes[0].minimum_size + size / 2;
      *out_right = sizes[1].minimum_size + (size + 1) / 2;
    }
}

static void
gtk_notebook_measure_arrows (GtkNotebook    *notebook,
                             GtkPackType     type,
                             GtkOrientation  orientation,
                             gint            for_size,
                             gint           *minimum,
                             gint           *natural,
                             gint           *minimum_baseline,
                             gint           *natural_baseline)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gint child1_min, child1_nat;
  gint child2_min, child2_nat;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->arrow_gadget[2 * type])
        {
          gtk_css_gadget_get_preferred_size (priv->arrow_gadget[2 * type],
                                             orientation,
                                             for_size,
                                             &child1_min, &child1_nat,
                                             NULL, NULL);
        }
      else
        {
          child1_min = child1_nat = 0;
        }
      if (priv->arrow_gadget[2 * type + 1])
        {
          gtk_css_gadget_get_preferred_size (priv->arrow_gadget[2 * type + 1],
                                             orientation,
                                             for_size,
                                             &child2_min, &child2_nat,
                                             NULL, NULL);
        }
      else
        {
          child2_min = child2_nat = 0;
        }
      *minimum = child1_min + child2_min;
      *natural = child1_nat + child2_nat;
      if (minimum_baseline)
        *minimum_baseline = -1;
      if (natural_baseline)
        *natural_baseline = -1;
    }
  else
    {
      gint child1_size, child2_size;

      if (for_size > -1)
        gtk_notebook_distribute_arrow_width (notebook, type, for_size, &child1_size, &child2_size);
      else
        child1_size = child2_size = for_size;

      if (priv->arrow_gadget[2 * type])
        {
          gtk_css_gadget_get_preferred_size (priv->arrow_gadget[2 * type],
                                             orientation,
                                             child1_size,
                                             &child1_min, &child1_nat,
                                             NULL, NULL);
        }
      else
        {
          child1_min = child1_nat = 0;
        }
      if (priv->arrow_gadget[2 * type + 1])
        {
          gtk_css_gadget_get_preferred_size (priv->arrow_gadget[2 * type + 1],
                                             orientation,
                                             child2_size,
                                             &child2_min, &child2_nat,
                                             NULL, NULL);
        }
      else
        {
          child2_min = child2_nat = 0;
        }
      *minimum = MAX (child1_min, child2_min);
      *natural = MAX (child1_nat, child2_nat);
      if (minimum_baseline)
        *minimum_baseline = -1;
      if (natural_baseline)
        *natural_baseline = -1;
    }
}

static void
gtk_notebook_get_preferred_tabs_size (GtkNotebook    *notebook,
                                      GtkRequisition *requisition)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gint tab_width = 0;
  gint tab_height = 0;
  gint tab_max = 0;
  guint vis_pages = 0;
  GList *children;
  GtkNotebookPage *page;


  for (children = priv->children; children;
       children = children->next)
    {
      page = children->data;

      if (gtk_widget_get_visible (page->child))
        {
          vis_pages++;

          if (!gtk_widget_get_visible (page->tab_label))
            gtk_widget_show (page->tab_label);

          gtk_css_gadget_get_preferred_size (page->gadget,
                                             GTK_ORIENTATION_HORIZONTAL,
                                             -1,
                                             &page->requisition.width, NULL,
                                             NULL, NULL);
          gtk_css_gadget_get_preferred_size (page->gadget,
                                             GTK_ORIENTATION_VERTICAL,
                                             page->requisition.width,
                                             &page->requisition.height, NULL,
                                             NULL, NULL);

          switch (priv->tab_pos)
            {
            case GTK_POS_TOP:
            case GTK_POS_BOTTOM:
              tab_height = MAX (tab_height, page->requisition.height);
              tab_max = MAX (tab_max, page->requisition.width);
              break;
            case GTK_POS_LEFT:
            case GTK_POS_RIGHT:
              tab_width = MAX (tab_width, page->requisition.width);
              tab_max = MAX (tab_max, page->requisition.height);
              break;
            }
        }
      else if (gtk_widget_get_visible (page->tab_label))
        gtk_widget_hide (page->tab_label);
    }

  children = priv->children;

  if (vis_pages)
    {
      switch (priv->tab_pos)
        {
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          if (tab_height == 0)
            break;

          if (priv->scrollable)
            {
              gint arrow_height, unused;
              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_START,
                                           GTK_ORIENTATION_VERTICAL,
                                           -1,
                                           &arrow_height, &unused,
                                           NULL, NULL);
              tab_height = MAX (tab_height, arrow_height);
              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_END,
                                           GTK_ORIENTATION_VERTICAL,
                                           -1,
                                           &arrow_height, &unused,
                                           NULL, NULL);
              tab_height = MAX (tab_height, arrow_height);
            }

          while (children)
            {
              page = children->data;
              children = children->next;

              if (!gtk_widget_get_visible (page->child))
                continue;

              tab_width += page->requisition.width;
              page->requisition.height = tab_height;
            }

          if (priv->scrollable)
            {
              gint start_arrow_width, end_arrow_width, unused;

              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_START,
                                           GTK_ORIENTATION_HORIZONTAL,
                                           tab_height,
                                           &start_arrow_width, &unused,
                                           NULL, NULL);
              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_END,
                                           GTK_ORIENTATION_HORIZONTAL,
                                           tab_height,
                                           &end_arrow_width, &unused,
                                           NULL, NULL);
              tab_width = MIN (tab_width,
                               tab_max + start_arrow_width + end_arrow_width);
            }

          requisition->width = tab_width;

          requisition->height = tab_height;
          break;
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          if (tab_width == 0)
            break;

          if (priv->scrollable)
            {
              gint arrow_width, unused;
              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_START,
                                           GTK_ORIENTATION_HORIZONTAL,
                                           -1,
                                           &arrow_width, &unused,
                                           NULL, NULL);
              tab_width = MAX (tab_width, arrow_width);
              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_END,
                                           GTK_ORIENTATION_HORIZONTAL,
                                           -1,
                                           &arrow_width, &unused,
                                           NULL, NULL);
              tab_width = MAX (tab_width, arrow_width);
            }

          while (children)
            {
              page = children->data;
              children = children->next;

              if (!gtk_widget_get_visible (page->child))
                continue;

              page->requisition.width = tab_width;

              tab_height += page->requisition.height;
            }

          if (priv->scrollable)
            {
              gint start_arrow_height, end_arrow_height, unused;

              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_START,
                                           GTK_ORIENTATION_VERTICAL,
                                           tab_width,
                                           &start_arrow_height, &unused,
                                           NULL, NULL);
              gtk_notebook_measure_arrows (notebook,
                                           GTK_PACK_END,
                                           GTK_ORIENTATION_VERTICAL,
                                           tab_width,
                                           &end_arrow_height, &unused,
                                           NULL, NULL);
              tab_height = MIN (tab_height, tab_max + start_arrow_height + end_arrow_height);
            }

          requisition->height = tab_height;
          requisition->height = MAX (requisition->height, tab_max);

          requisition->width = tab_width;
          break;
        default:
          g_assert_not_reached ();
          requisition->width = 0;
          requisition->height = 0;
        }
    }
  else
    {
      requisition->width = 0;
      requisition->height = 0;
    }
}

static void
gtk_notebook_measure_tabs (GtkCssGadget   *gadget,
                           GtkOrientation  orientation,
                           gint            size,
                           gint           *minimum,
                           gint           *natural,
                           gint           *minimum_baseline,
                           gint           *natural_baseline,
                           gpointer        unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkRequisition tabs_requisition = { 0 };

  gtk_notebook_get_preferred_tabs_size (notebook, &tabs_requisition);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = tabs_requisition.width;
      *natural = tabs_requisition.width;
    }
  else
    {
      *minimum = tabs_requisition.height;
      *natural = tabs_requisition.height;
    }
}

static void
gtk_notebook_measure_stack (GtkCssGadget   *gadget,
                            GtkOrientation  orientation,
                            gint            size,
                            gint           *minimum,
                            gint           *natural,
                            gint           *minimum_baseline,
                            gint           *natural_baseline,
                            gpointer        unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;
  gint child_minimum, child_natural;

  *minimum = 0;
  *natural = 0;

  for (children = priv->children;
       children;
       children = children->next)
    {
      GtkNotebookPage *page = children->data;

      if (gtk_widget_get_visible (page->child))
        {
          _gtk_widget_get_preferred_size_for_size (page->child,
                                                   orientation,
                                                   size, 
                                                   &child_minimum,
                                                   &child_natural,
                                                   NULL,
                                                   NULL);

          *minimum = MAX (*minimum, child_minimum);
          *natural = MAX (*natural, child_natural);
        }
    }
}

static void
gtk_notebook_get_preferred_width_for_height (GtkWidget *widget,
                                             gint       height,
                                             gint      *minimum,
                                             gint      *natural)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_css_gadget_get_preferred_size (priv->gadget, GTK_ORIENTATION_HORIZONTAL, height, minimum, natural, NULL, NULL);
}

static void
gtk_notebook_get_preferred_height_for_width (GtkWidget *widget,
                                             gint       width,
                                             gint      *minimum,
                                             gint      *natural)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_css_gadget_get_preferred_size (priv->gadget, GTK_ORIENTATION_VERTICAL, width, minimum, natural, NULL, NULL);
}

static void
gtk_notebook_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum,
                                  gint      *natural)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_css_gadget_get_preferred_size (priv->gadget, GTK_ORIENTATION_HORIZONTAL, -1, minimum, natural, NULL, NULL);
}

static void
gtk_notebook_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_css_gadget_get_preferred_size (priv->gadget, GTK_ORIENTATION_VERTICAL, -1, minimum, natural, NULL, NULL);
}

static void
gtk_notebook_allocate_tabs (GtkCssGadget        *gadget,
                            const GtkAllocation *allocation,
                            int                  baseline,
                            GtkAllocation       *out_clip,
                            gpointer             unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  gtk_notebook_pages_allocate (notebook, allocation);
}

static void
gtk_notebook_allocate_stack (GtkCssGadget        *gadget,
                             const GtkAllocation *allocation,
                             int                  baseline,
                             GtkAllocation       *out_clip,
                             gpointer             unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;

  for (children = priv->children;
       children;
       children = children->next)
    {
      GtkNotebookPage *page = children->data;

      if (gtk_widget_get_visible (page->child))
        gtk_widget_size_allocate_with_baseline (page->child, (GtkAllocation *) allocation, baseline);
    }

  if (gtk_notebook_has_current_page (notebook))
    gtk_widget_get_clip (priv->cur_page->child, out_clip);
}

static void
gtk_notebook_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);

  if (gtk_widget_get_realized (widget))
    {
      GdkRectangle position;

      if (gtk_notebook_get_event_window_position (notebook, &position))
        {
          gdk_window_move_resize (priv->event_window,
                                  position.x, position.y,
                                  position.width, position.height);
          if (gtk_widget_get_mapped (GTK_WIDGET (notebook)))
            gdk_window_show_unraised (priv->event_window);
        }
      else
        gdk_window_hide (priv->event_window);
    }
}

static gboolean
gtk_notebook_draw_stack (GtkCssGadget *gadget,
                         cairo_t      *cr,
                         int           x,
                         int           y,
                         int           width,
                         int           height,
                         gpointer      unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  if (gtk_notebook_has_current_page (notebook))
    gtk_container_propagate_draw (GTK_CONTAINER (notebook),
                                  priv->cur_page->child,
                                  cr);

  return FALSE;
}

static gboolean
gtk_notebook_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)))
    gtk_css_gadget_draw (priv->gadget, cr);

  if (priv->operation == DRAG_OPERATION_REORDER &&
      gtk_cairo_should_draw_window (cr, priv->drag_window))
    gtk_css_gadget_draw (priv->cur_page->gadget, cr);

  return FALSE;
}

static gboolean
gtk_notebook_show_arrows (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gboolean show_arrow = FALSE;
  GList *children;

  if (!priv->scrollable)
    return FALSE;

  children = priv->children;
  while (children)
    {
      GtkNotebookPage *page = children->data;

      if (page->tab_label && !gtk_widget_get_child_visible (page->tab_label))
        show_arrow = TRUE;

      children = children->next;
    }

  return show_arrow;
}

static void
gtk_notebook_get_arrow_rect (GtkNotebook      *notebook,
                             GdkRectangle     *rectangle,
                             GtkNotebookArrow  arrow)
{
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_css_gadget_get_border_allocation (priv->arrow_gadget[arrow], rectangle, NULL);
}

static GtkNotebookArrow
gtk_notebook_get_arrow (GtkNotebook *notebook,
                        gint         x,
                        gint         y)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GdkRectangle arrow_rect;
  gint i;
  gint x0, y0;

  if (gtk_notebook_show_arrows (notebook))
    {
      for (i = 0; i < 4; i++)
        {
          if (priv->arrow_gadget[i] == NULL)
            continue;

          gtk_notebook_get_arrow_rect (notebook, &arrow_rect, i);

          x0 = x - arrow_rect.x;
          y0 = y - arrow_rect.y;

          if (y0 >= 0 && y0 < arrow_rect.height &&
              x0 >= 0 && x0 < arrow_rect.width)
            return i;
        }
    }

  return ARROW_NONE;
}

static void
gtk_notebook_do_arrow (GtkNotebook     *notebook,
                       GtkNotebookArrow arrow)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *widget = GTK_WIDGET (notebook);
  gboolean is_rtl, left;

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  left = (ARROW_IS_LEFT (arrow) && !is_rtl) ||
         (!ARROW_IS_LEFT (arrow) && is_rtl);

  if (!priv->focus_tab ||
      gtk_notebook_search_page (notebook, priv->focus_tab,
                                left ? STEP_PREV : STEP_NEXT,
                                TRUE))
    {
      gtk_notebook_change_current_page (notebook, left ? -1 : 1);
      gtk_widget_grab_focus (widget);
    }
}

static gboolean
gtk_notebook_arrow_button_press (GtkNotebook      *notebook,
                                 GtkNotebookArrow  arrow,
                                 gint              button)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *widget = GTK_WIDGET (notebook);
  gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  gboolean left = (ARROW_IS_LEFT (arrow) && !is_rtl) ||
                  (!ARROW_IS_LEFT (arrow) && is_rtl);

  if (priv->pressed_button)
    return FALSE;

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  priv->pressed_button = button;
  priv->click_child = arrow;

  if (button == GDK_BUTTON_PRIMARY)
    {
      gtk_notebook_do_arrow (notebook, arrow);
      gtk_notebook_set_scroll_timer (notebook);
    }
  else if (button == GDK_BUTTON_MIDDLE)
    gtk_notebook_page_select (notebook, TRUE);
  else if (button == GDK_BUTTON_SECONDARY)
    gtk_notebook_switch_focus_tab (notebook,
                                   gtk_notebook_search_page (notebook,
                                                             NULL,
                                                             left ? STEP_NEXT : STEP_PREV,
                                                             TRUE));
  gtk_notebook_redraw_arrows (notebook);

  return TRUE;
}

static gboolean
get_widget_coordinates (GtkWidget *widget,
                        GdkEvent  *event,
                        gdouble   *x,
                        gdouble   *y)
{
  GdkWindow *window = ((GdkEventAny *)event)->window;
  gdouble tx, ty;

  if (!gdk_event_get_coords (event, &tx, &ty))
    return FALSE;

  while (window && window != gtk_widget_get_window (widget))
    {
      gint window_x, window_y;

      gdk_window_get_position (window, &window_x, &window_y);
      tx += window_x;
      ty += window_y;

      window = gdk_window_get_parent (window);
    }

  if (window)
    {
      *x = tx;
      *y = ty;

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_page_tab_label_is_visible (GtkNotebookPage *page)
{
  return page->tab_label
      && gtk_widget_get_visible (page->tab_label)
      && gtk_widget_get_child_visible (page->tab_label);
}

static GList*
get_tab_at_pos (GtkNotebook *notebook,
                gdouble      x,
                gdouble      y)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GtkAllocation allocation;
  GList *children;

  for (children = priv->children; children; children = children->next)
    {
      page = children->data;

      if (!gtk_notebook_page_tab_label_is_visible (page))
        continue;

      gtk_css_gadget_get_border_allocation (page->gadget, &allocation, NULL);
      if ((x >= allocation.x) &&
          (y >= allocation.y) &&
          (x <= (allocation.x + allocation.width)) &&
          (y <= (allocation.y + allocation.height)))
        return children;
    }

  return NULL;
}

static gboolean
gtk_notebook_button_press (GtkWidget      *widget,
                           GdkEventButton *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GList *tab;
  GtkNotebookArrow arrow;
  gdouble x, y;

  if (event->type != GDK_BUTTON_PRESS || !priv->children)
    return FALSE;

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  arrow = gtk_notebook_get_arrow (notebook, x, y);
  if (arrow != ARROW_NONE)
    return gtk_notebook_arrow_button_press (notebook, arrow, event->button);

  if (priv->menu && gdk_event_triggers_context_menu ((GdkEvent *) event))
    {
      gtk_menu_popup_at_pointer (GTK_MENU (priv->menu), (GdkEvent *) event);
      return TRUE;
    }

  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  if ((tab = get_tab_at_pos (notebook, x, y)) != NULL)
    {
      GtkAllocation allocation;
      gboolean page_changed, was_focus;

      page = tab->data;
      page_changed = page != priv->cur_page;
      was_focus = gtk_widget_is_focus (widget);

      gtk_notebook_switch_focus_tab (notebook, tab);
      gtk_widget_grab_focus (widget);

      if (page_changed && !was_focus)
        gtk_widget_child_focus (page->child, GTK_DIR_TAB_FORWARD);

      /* save press to possibly begin a drag */
      if (page->reorderable || page->detachable)
        {
          priv->pressed_button = event->button;

          priv->mouse_x = x;
          priv->mouse_y = y;

          priv->drag_begin_x = priv->mouse_x;
          priv->drag_begin_y = priv->mouse_y;

          gtk_css_gadget_get_margin_allocation (page->gadget, &allocation, NULL);

          priv->drag_offset_x = priv->drag_begin_x - allocation.x;
          priv->drag_offset_y = priv->drag_begin_y - allocation.y;
        }
    }

  return TRUE;
}

static gboolean
gtk_notebook_popup_menu (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GtkWidget *tab_label = NULL;

  if (priv->menu)
    {
      if (priv->focus_tab)
        {
          page = priv->focus_tab->data;
          tab_label = page->tab_label;
        }

      if (tab_label)
        {
          g_object_set (priv->menu,
                        "anchor-hints", (GDK_ANCHOR_FLIP_Y |
                                         GDK_ANCHOR_SLIDE |
                                         GDK_ANCHOR_RESIZE),
                        NULL);

          gtk_menu_popup_at_widget (GTK_MENU (priv->menu),
                                    tab_label,
                                    GDK_GRAVITY_SOUTH_WEST,
                                    GDK_GRAVITY_NORTH_WEST,
                                    NULL);
        }
      else
        {
          g_object_set (priv->menu,
                        "anchor-hints", (GDK_ANCHOR_SLIDE |
                                         GDK_ANCHOR_RESIZE),
                        NULL);

          gtk_menu_popup_at_widget (GTK_MENU (priv->menu),
                                    widget,
                                    GDK_GRAVITY_NORTH_WEST,
                                    GDK_GRAVITY_NORTH_WEST,
                                    NULL);
        }

      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menu), FALSE);
      return TRUE;
    }

  return FALSE;
}

static void
stop_scrolling (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->timer)
    {
      g_source_remove (priv->timer);
      priv->timer = 0;
      priv->need_timer = FALSE;
    }
  priv->click_child = ARROW_NONE;
  priv->pressed_button = 0;
  gtk_notebook_redraw_arrows (notebook);
}

static GList*
get_drop_position (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children, *last_child;
  GtkNotebookPage *page;
  gboolean is_rtl;
  gint x, y;

  x = priv->mouse_x;
  y = priv->mouse_y;

  is_rtl = gtk_widget_get_direction ((GtkWidget *) notebook) == GTK_TEXT_DIR_RTL;
  children = priv->children;
  last_child = NULL;

  while (children)
    {
      page = children->data;

      if ((priv->operation != DRAG_OPERATION_REORDER || page != priv->cur_page) &&
          gtk_widget_get_visible (page->child) &&
          page->tab_label &&
          gtk_widget_get_mapped (page->tab_label))
        {
          GtkAllocation allocation;

          gtk_css_gadget_get_border_allocation (page->gadget, &allocation, NULL);

          switch (priv->tab_pos)
            {
            case GTK_POS_TOP:
            case GTK_POS_BOTTOM:
              if (!is_rtl)
                {
                  if (allocation.x + allocation.width / 2 > x)
                    return children;
                }
              else
                {
                  if (allocation.x + allocation.width / 2 < x)
                    return children;
                }

              break;
            case GTK_POS_LEFT:
            case GTK_POS_RIGHT:
              if (allocation.y + allocation.height / 2 > y)
                return children;

              break;
            }

          last_child = children->next;
        }

      children = children->next;
    }

  return last_child;
}

static void
prepare_drag_window (GdkSeat   *seat,
                     GdkWindow *window,
                     gpointer   user_data)
{
  gdk_window_show (window);
}

static void
show_drag_window (GtkNotebook        *notebook,
                  GtkNotebookPrivate    *priv,
                  GtkNotebookPage    *page,
                  GdkDevice          *device)
{
  GtkWidget *widget = GTK_WIDGET (notebook);

  if (!priv->drag_window)
    {
      GdkWindowAttr attributes;
      GtkAllocation allocation;
      guint attributes_mask;
      GdkRGBA transparent = {0, 0, 0, 0};

      gtk_css_gadget_get_margin_allocation (page->gadget, &allocation, NULL);
      attributes.x = priv->drag_window_x;
      attributes.y = priv->drag_window_y;
      attributes.width = allocation.width;
      attributes.height = allocation.height;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_POINTER_MOTION_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

      priv->drag_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                          &attributes,
                                          attributes_mask);
      gtk_widget_register_window (widget, priv->drag_window);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gdk_window_set_background_rgba (priv->drag_window, &transparent);
G_GNUC_END_IGNORE_DEPRECATIONS
    }

  gtk_widget_set_child_visible (page->tab_label, FALSE);
  gtk_widget_unrealize (page->tab_label);
  gtk_widget_set_parent_window (page->tab_label, priv->drag_window);
  gtk_widget_set_child_visible (page->tab_label, TRUE);

  gtk_css_gadget_add_class (page->gadget, GTK_STYLE_CLASS_DND);

  /* the grab will dissapear when the window is hidden */
  gdk_seat_grab (gdk_device_get_seat (device), priv->drag_window,
                 GDK_SEAT_CAPABILITY_ALL, FALSE,
                 NULL, NULL, prepare_drag_window, notebook);
}

/* This function undoes the reparenting that happens both when drag_window
 * is shown for reordering and when the DnD icon is shown for detaching
 */
static void
hide_drag_window (GtkNotebook        *notebook,
                  GtkNotebookPrivate    *priv,
                  GtkNotebookPage    *page)
{
  GtkWidget *widget = GTK_WIDGET (notebook);

  if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
    {
      g_object_ref (page->tab_label);
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (page->tab_label)), page->tab_label);
      gtk_css_node_set_parent (gtk_widget_get_css_node (page->tab_label),
                               gtk_css_gadget_get_node (page->gadget));
      gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
      g_object_unref (page->tab_label);
    }
  else if (gtk_widget_get_window (page->tab_label) != gtk_widget_get_window (widget))
    {
      gtk_widget_set_child_visible (page->tab_label, FALSE);
      gtk_widget_unrealize (page->tab_label);
      gtk_widget_set_parent_window (page->tab_label, NULL);
      gtk_widget_set_child_visible (page->tab_label, TRUE);
    }

  gtk_css_gadget_remove_class (page->gadget, GTK_STYLE_CLASS_DND);

  if (priv->drag_window &&
      gdk_window_is_visible (priv->drag_window))
    gdk_window_hide (priv->drag_window);
}

static void
gtk_notebook_stop_reorder (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;

  if (priv->operation == DRAG_OPERATION_DETACH)
    page = priv->detached_tab;
  else
    page = priv->cur_page;

  if (!page || !page->tab_label)
    return;

  priv->pressed_button = 0;

  if (page->reorderable || page->detachable)
    {
      if (priv->operation == DRAG_OPERATION_REORDER)
        {
          gint old_page_num, page_num, i;
          GList *element;

          element = get_drop_position (notebook);
          old_page_num = g_list_position (priv->children, priv->focus_tab);
          page_num = reorder_tab (notebook, element, priv->focus_tab);
          gtk_notebook_child_reordered (notebook, page);

          if (priv->has_scrolled || old_page_num != page_num)
            {
              for (element = priv->children, i = 0; element; element = element->next, i++)
                {
                  if (MIN (old_page_num, page_num) <= i && i <= MAX (old_page_num, page_num))
                    gtk_widget_child_notify (((GtkNotebookPage *) element->data)->child, "position");
                }
              g_signal_emit (notebook,
                             notebook_signals[PAGE_REORDERED], 0,
                             page->child, page_num);
            }
        }

      priv->has_scrolled = FALSE;

      hide_drag_window (notebook, priv, page);

      priv->operation = DRAG_OPERATION_NONE;

      if (priv->dnd_timer)
        {
          g_source_remove (priv->dnd_timer);
          priv->dnd_timer = 0;
        }

      gtk_widget_queue_allocate (GTK_WIDGET (notebook));
    }
}

static gboolean
gtk_notebook_button_release (GtkWidget      *widget,
                             GdkEventButton *event)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  notebook = GTK_NOTEBOOK (widget);
  priv = notebook->priv;

  if (priv->pressed_button != event->button)
    return FALSE;

  if (priv->operation == DRAG_OPERATION_REORDER &&
      priv->cur_page &&
      priv->cur_page->reorderable)
    gtk_notebook_stop_reorder (notebook);

  stop_scrolling (notebook);
  return TRUE;
}

static void
update_prelight_tab (GtkNotebook     *notebook,
                     GtkNotebookPage *page)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->prelight_tab == page)
    return;

  if (priv->prelight_tab)
    gtk_css_gadget_remove_state (priv->prelight_tab->gadget, GTK_STATE_FLAG_PRELIGHT);

  if (page)
    gtk_css_gadget_add_state (page->gadget, GTK_STATE_FLAG_PRELIGHT);

  priv->prelight_tab = page;
}

static void
tab_prelight (GtkNotebook *notebook,
              GdkEvent    *event)
{
  GList *tab;
  gdouble x, y;

  if (get_widget_coordinates (GTK_WIDGET (notebook), (GdkEvent *)event, &x, &y))
    {
      tab = get_tab_at_pos (notebook, x, y);
      update_prelight_tab (notebook, tab == NULL ? NULL : tab->data);
    }
}

static gboolean
gtk_notebook_enter_notify (GtkWidget        *widget,
                           GdkEventCrossing *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  tab_prelight (notebook, (GdkEvent *)event);

  return FALSE;
}

static gboolean
gtk_notebook_leave_notify (GtkWidget        *widget,
                           GdkEventCrossing *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  gdouble x, y;

  if (get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    {
      if (priv->prelight_tab != NULL)
        {
          tab_prelight (notebook, (GdkEvent *)event);
        }

      if (priv->in_child != ARROW_NONE)
        {
          priv->in_child = ARROW_NONE;
          gtk_notebook_redraw_arrows (notebook);
        }
    }

  return FALSE;
}

static GtkNotebookPointerPosition
get_pointer_position (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *widget = GTK_WIDGET (notebook);
  gint wx, wy, width, height;
  gboolean is_rtl;

  if (!priv->scrollable)
    return POINTER_BETWEEN;

  gdk_window_get_position (priv->event_window, &wx, &wy);
  width = gdk_window_get_width (priv->event_window);
  height = gdk_window_get_height (priv->event_window);

  if (priv->tab_pos == GTK_POS_TOP ||
      priv->tab_pos == GTK_POS_BOTTOM)
    {
      gint x;

      is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
      x = priv->mouse_x - wx;

      if (x > width - SCROLL_THRESHOLD)
        return (is_rtl) ? POINTER_BEFORE : POINTER_AFTER;
      else if (x < SCROLL_THRESHOLD)
        return (is_rtl) ? POINTER_AFTER : POINTER_BEFORE;
      else
        return POINTER_BETWEEN;
    }
  else
    {
      gint y;

      y = priv->mouse_y - wy;
      if (y > height - SCROLL_THRESHOLD)
        return POINTER_AFTER;
      else if (y < SCROLL_THRESHOLD)
        return POINTER_BEFORE;
      else
        return POINTER_BETWEEN;
    }
}

static gboolean
scroll_notebook_timer (gpointer data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (data);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPointerPosition pointer_position;
  GList *element, *first_tab;

  pointer_position = get_pointer_position (notebook);

  element = get_drop_position (notebook);
  reorder_tab (notebook, element, priv->focus_tab);
  first_tab = gtk_notebook_search_page (notebook, priv->first_tab,
                                        (pointer_position == POINTER_BEFORE) ? STEP_PREV : STEP_NEXT,
                                        TRUE);
  if (first_tab && priv->cur_page)
    {
      priv->first_tab = first_tab;

      gtk_css_gadget_queue_allocate (priv->tabs_gadget);
    }

  return TRUE;
}

static gboolean
check_threshold (GtkNotebook *notebook,
                 gint         current_x,
                 gint         current_y)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gint dnd_threshold;
  GdkRectangle rectangle = { 0, }; /* shut up gcc */
  GtkSettings *settings;

  settings = gtk_widget_get_settings (GTK_WIDGET (notebook));
  g_object_get (G_OBJECT (settings), "gtk-dnd-drag-threshold", &dnd_threshold, NULL);

  /* we want a large threshold */
  dnd_threshold *= DND_THRESHOLD_MULTIPLIER;

  gdk_window_get_position (priv->event_window, &rectangle.x, &rectangle.y);
  rectangle.width = gdk_window_get_width (priv->event_window);
  rectangle.height = gdk_window_get_height (priv->event_window);

  rectangle.x -= dnd_threshold;
  rectangle.width += 2 * dnd_threshold;
  rectangle.y -= dnd_threshold;
  rectangle.height += 2 * dnd_threshold;

  return (current_x < rectangle.x ||
          current_x > rectangle.x + rectangle.width ||
          current_y < rectangle.y ||
          current_y > rectangle.y + rectangle.height);
}

static gboolean
gtk_notebook_motion_notify (GtkWidget      *widget,
                            GdkEventMotion *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GtkNotebookArrow arrow;
  GtkNotebookPointerPosition pointer_position;
  gint x_win, y_win;

  page = priv->cur_page;

  if (!page)
    return FALSE;

  if (!(event->state & GDK_BUTTON1_MASK) &&
      priv->pressed_button != 0)
    {
      gtk_notebook_stop_reorder (notebook);
      stop_scrolling (notebook);
    }

  tab_prelight (notebook, (GdkEvent *)event);

  /* While animating the move, event->x is relative to the flying tab
   * (priv->drag_window has a pointer grab), but we need coordinates relative to
   * the notebook widget.
   */
  gdk_window_get_origin (gtk_widget_get_window (widget), &x_win, &y_win);
  priv->mouse_x = event->x_root - x_win;
  priv->mouse_y = event->y_root - y_win;

  arrow = gtk_notebook_get_arrow (notebook, priv->mouse_x, priv->mouse_y);
  if (arrow != priv->in_child)
    {
      priv->in_child = arrow;
      gtk_notebook_redraw_arrows (notebook);
    }

  if (priv->pressed_button == 0)
    return FALSE;

  if (page->detachable &&
      check_threshold (notebook, priv->mouse_x, priv->mouse_y))
    {
      priv->detached_tab = priv->cur_page;

      gtk_drag_begin_with_coordinates (widget, priv->source_targets, GDK_ACTION_MOVE,
                                       priv->pressed_button, (GdkEvent*) event,
                                       priv->drag_begin_x, priv->drag_begin_y);
      return TRUE;
    }

  if (page->reorderable &&
      (priv->operation == DRAG_OPERATION_REORDER ||
       gtk_drag_check_threshold (widget, priv->drag_begin_x, priv->drag_begin_y, priv->mouse_x, priv->mouse_y)))
    {
      pointer_position = get_pointer_position (notebook);

      if (event->window == priv->drag_window &&
          pointer_position != POINTER_BETWEEN &&
          gtk_notebook_show_arrows (notebook))
        {
          /* scroll tabs */
          if (!priv->dnd_timer)
            {
              priv->has_scrolled = TRUE;
              priv->dnd_timer = gdk_threads_add_timeout (TIMEOUT_REPEAT * SCROLL_DELAY_FACTOR,
                                               scroll_notebook_timer,
                                               (gpointer) notebook);
              g_source_set_name_by_id (priv->dnd_timer, "[gtk+] scroll_notebook_timer");
            }
        }
      else
        {
          if (priv->dnd_timer)
            {
              g_source_remove (priv->dnd_timer);
              priv->dnd_timer = 0;
            }
        }

      if (event->window == priv->drag_window ||
          priv->operation != DRAG_OPERATION_REORDER)
        {
          /* the drag operation is beginning, create the window */
          if (priv->operation != DRAG_OPERATION_REORDER)
            {
              priv->operation = DRAG_OPERATION_REORDER;
              show_drag_window (notebook, priv, page, event->device);
            }
        }
    }

  if (priv->operation == DRAG_OPERATION_REORDER)
    gtk_widget_queue_allocate (widget);

  return TRUE;
}

static void
gtk_notebook_grab_notify (GtkWidget *widget,
                          gboolean   was_grabbed)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  if (!was_grabbed)
    {
      gtk_notebook_stop_reorder (notebook);
      stop_scrolling (notebook);
    }
}

static void
update_tab_state (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkStateFlags state, tab_state;
  GList *l;

  state = gtk_widget_get_state_flags (GTK_WIDGET (notebook));

  state = state & ~GTK_STATE_FLAG_FOCUSED;

  gtk_css_gadget_set_state (priv->stack_gadget, state);
  gtk_css_gadget_set_state (priv->header_gadget, state);
  gtk_css_gadget_set_state (priv->tabs_gadget, state);

  for (l = priv->children; l; l = l->next)
    {
      GtkNotebookPage *page = l->data;

      tab_state = state & ~(GTK_STATE_FLAG_CHECKED | GTK_STATE_FLAG_PRELIGHT);

      if (page == priv->cur_page)
        tab_state |= GTK_STATE_FLAG_CHECKED;
      if (page == priv->prelight_tab)
        tab_state |= GTK_STATE_FLAG_PRELIGHT;

      gtk_css_gadget_set_state (page->gadget, tab_state);
    }
}

static void
update_arrow_state (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gint i;
  GtkStateFlags state;
  gboolean is_rtl, left;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL;

  for (i = 0; i < 4; i++)
    {
      if (priv->arrow_gadget[i] == NULL)
        continue;

      state = gtk_widget_get_state_flags (GTK_WIDGET (notebook));
      state &= ~GTK_STATE_FLAG_FOCUSED;


      left = (ARROW_IS_LEFT (i) && !is_rtl) ||
             (!ARROW_IS_LEFT (i) && is_rtl);

      if (priv->focus_tab &&
          !gtk_notebook_search_page (notebook, priv->focus_tab,
                                     left ? STEP_PREV : STEP_NEXT, TRUE))
        {
          state |= GTK_STATE_FLAG_INSENSITIVE;
        }
      else if (priv->in_child == i)
        {
          state |= GTK_STATE_FLAG_PRELIGHT;
          if (priv->click_child == i)
            state |= GTK_STATE_FLAG_ACTIVE;
        }

      gtk_css_gadget_set_state (priv->arrow_gadget[i], state);
    }
}

static void
gtk_notebook_state_flags_changed (GtkWidget     *widget,
                                  GtkStateFlags  previous_state)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  update_tab_state (notebook);
  update_arrow_state (notebook);

  if (!gtk_widget_is_sensitive (widget))
    stop_scrolling (notebook);
}

static gboolean
gtk_notebook_focus_in (GtkWidget     *widget,
                       GdkEventFocus *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_css_gadget_queue_draw (priv->tabs_gadget);

  return FALSE;
}

static gboolean
gtk_notebook_focus_out (GtkWidget     *widget,
                        GdkEventFocus *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_css_gadget_queue_draw (priv->tabs_gadget);

  return FALSE;
}

static void
update_arrow_nodes (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gboolean arrow[4];
  GtkCssImageBuiltinType up_image_type, down_image_type;
  const char *style_property_name;
  GtkCssNode *tabs_node;
  gint i;
 
  tabs_node = gtk_css_gadget_get_node (priv->tabs_gadget);

  if (priv->tab_pos == GTK_POS_LEFT ||
      priv->tab_pos == GTK_POS_RIGHT)
    {
      up_image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_UP;
      down_image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_DOWN;
      style_property_name = "scroll-arrow-vlength";
    }
  else
    {
      up_image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT;
      down_image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT;
      style_property_name = "scroll-arrow-hlength";
    }

  gtk_widget_style_get (GTK_WIDGET (notebook),
                        "has-backward-stepper", &arrow[0],
                        "has-secondary-forward-stepper", &arrow[1],
                        "has-secondary-backward-stepper", &arrow[2],
                        "has-forward-stepper", &arrow[3],
                        NULL);

  for (i = 0; i < 4; i++)
    {
      if (priv->scrollable && arrow[i])
        {
          if (priv->arrow_gadget[i] == NULL)
            {
              GtkCssGadget *next_gadget;

              switch (i)
                {
                case 0:
                  if (priv->arrow_gadget[1])
                    {
                      next_gadget = priv->arrow_gadget[1];
                      break;
                    }
                  /* fall through */
                case 1:
                  if (priv->children)
                    {
                      GtkNotebookPage *page = priv->children->data;
                      next_gadget = page->gadget;
                      break;
                    }
                  if (priv->arrow_gadget[2])
                    {
                      next_gadget = priv->arrow_gadget[2];
                      break;
                    }
                  /* fall through */
                case 2:
                  if (priv->arrow_gadget[3])
                    {
                      next_gadget = priv->arrow_gadget[3];
                      break;
                    }
                  /* fall through */
                case 3:
                  next_gadget = NULL;
                  break;

                default:
                  g_assert_not_reached ();
                  next_gadget = NULL;
                  break;
                }

              priv->arrow_gadget[i] = gtk_builtin_icon_new ("arrow",
                                                            GTK_WIDGET (notebook),
                                                            priv->tabs_gadget,
                                                            next_gadget);
              if (i == ARROW_LEFT_BEFORE || i == ARROW_LEFT_AFTER)
                gtk_css_gadget_add_class (priv->arrow_gadget[i], "down");
              else
                gtk_css_gadget_add_class (priv->arrow_gadget[i], "up");
              gtk_css_gadget_set_state (priv->arrow_gadget[i], gtk_css_node_get_state (tabs_node));
           }

          if (i == ARROW_LEFT_BEFORE || i == ARROW_LEFT_AFTER)
            gtk_builtin_icon_set_image (GTK_BUILTIN_ICON (priv->arrow_gadget[i]), down_image_type);
          else
            gtk_builtin_icon_set_image (GTK_BUILTIN_ICON (priv->arrow_gadget[i]), up_image_type);
          
          gtk_builtin_icon_set_default_size_property (GTK_BUILTIN_ICON (priv->arrow_gadget[i]), style_property_name);
        }
      else
        {
          if (priv->arrow_gadget[i])
            {
              gtk_css_node_set_parent (gtk_css_gadget_get_node (priv->arrow_gadget[i]), NULL);
              g_clear_object (&priv->arrow_gadget[i]);
            }
        }
    }
}

static void
gtk_notebook_style_updated (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  update_arrow_nodes (notebook);
  update_arrow_state (notebook);

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->style_updated (widget);
}

static gboolean
on_drag_icon_draw (GtkWidget *widget,
                   cairo_t   *cr,
                   gpointer   data)
{
  GtkWidget *child;
  GtkRequisition requisition;
  GtkStyleContext *context;

  child = gtk_bin_get_child (GTK_BIN (widget));
  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  gtk_widget_get_preferred_size (widget,
                                 &requisition, NULL);

  gtk_render_background (context, cr, 0, 0,
                         requisition.width,
                         requisition.height);

  gtk_render_frame (context, cr, 0, 0,
                    requisition.width,
                    requisition.height);

  if (child)
    gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);

  gtk_style_context_restore (context);

  return TRUE;
}

static void
gtk_notebook_drag_begin (GtkWidget        *widget,
                         GdkDragContext   *context)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkAllocation allocation;
  GtkWidget *tab_label;

  if (priv->dnd_timer)
    {
      g_source_remove (priv->dnd_timer);
      priv->dnd_timer = 0;
    }

  g_assert (priv->cur_page != NULL);

  priv->operation = DRAG_OPERATION_DETACH;

  tab_label = priv->detached_tab->tab_label;

  hide_drag_window (notebook, priv, priv->cur_page);
  g_object_ref (tab_label);
  gtk_widget_unparent (tab_label);

  priv->dnd_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_screen (GTK_WINDOW (priv->dnd_window),
                         gtk_widget_get_screen (widget));
  gtk_container_add (GTK_CONTAINER (priv->dnd_window), tab_label);
  gtk_css_gadget_get_margin_allocation (priv->detached_tab->gadget, &allocation, NULL);
  gtk_widget_set_size_request (priv->dnd_window,
                               allocation.width,
                               allocation.height);
  g_object_unref (tab_label);

  g_signal_connect (G_OBJECT (priv->dnd_window), "draw",
                    G_CALLBACK (on_drag_icon_draw), notebook);

  gtk_drag_set_icon_widget (context, priv->dnd_window, -2, -2);
  g_object_set_data (G_OBJECT (priv->dnd_window), "drag-context", context);
}

static void
gtk_notebook_drag_end (GtkWidget      *widget,
                       GdkDragContext *context)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_notebook_stop_reorder (notebook);

  if (priv->rootwindow_drop)
    {
      GtkNotebook *notebook = GTK_NOTEBOOK (widget);
      GtkNotebookPrivate *priv = notebook->priv;
      GtkNotebook *dest_notebook = NULL;
      gint x, y;

      gdk_device_get_position (gdk_drag_context_get_device (context),
                               NULL, &x, &y);
      g_signal_emit (notebook, notebook_signals[CREATE_WINDOW], 0,
                     priv->detached_tab->child, x, y, &dest_notebook);

      if (dest_notebook)
        do_detach_tab (notebook, dest_notebook, priv->detached_tab->child, 0, 0);

      priv->rootwindow_drop = FALSE;
    }
  else if (priv->detached_tab)
    gtk_notebook_switch_page (notebook, priv->detached_tab);

  _gtk_bin_set_child (GTK_BIN (priv->dnd_window), NULL);
  gtk_widget_destroy (priv->dnd_window);
  priv->dnd_window = NULL;

  priv->operation = DRAG_OPERATION_NONE;
}

static GtkNotebook *
gtk_notebook_create_window (GtkNotebook *notebook,
                            GtkWidget   *page,
                            gint         x,
                            gint         y)
{
  return NULL;
}

static gboolean
gtk_notebook_drag_failed (GtkWidget      *widget,
                          GdkDragContext *context,
                          GtkDragResult   result)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  priv->rootwindow_drop = FALSE;

  if (result == GTK_DRAG_RESULT_NO_TARGET)
    {
      GtkNotebook *dest_notebook = NULL;
      gint x, y;

      gdk_device_get_position (gdk_drag_context_get_device (context),
                               NULL, &x, &y);

      g_signal_emit (notebook, notebook_signals[CREATE_WINDOW], 0,
                     priv->detached_tab->child, x, y, &dest_notebook);

      if (dest_notebook)
        do_detach_tab (notebook, dest_notebook, priv->detached_tab->child, 0, 0);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_notebook_switch_tab_timeout (gpointer data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (data);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *switch_tab;

  priv->switch_tab_timer = 0;

  switch_tab = priv->switch_tab;
  priv->switch_tab = NULL;

  if (switch_tab)
    {
      /* FIXME: hack, we don't want the
       * focus to move fom the source widget
       */
      priv->child_has_focus = FALSE;
      gtk_notebook_switch_focus_tab (notebook, switch_tab);
    }

  return FALSE;
}

static gboolean
gtk_notebook_drag_motion (GtkWidget      *widget,
                          GdkDragContext *context,
                          gint            x,
                          gint            y,
                          guint           time)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkAllocation allocation;
  GdkRectangle position;
  GtkNotebookArrow arrow;
  GdkAtom target, tab_target;
  GList *tab;
  gboolean retval = FALSE;

  gtk_widget_get_allocation (widget, &allocation);

  arrow = gtk_notebook_get_arrow (notebook,
                                  x + allocation.x,
                                  y + allocation.y);
  if (arrow != ARROW_NONE)
    {
      priv->click_child = arrow;
      gtk_notebook_set_scroll_timer (notebook);
      gdk_drag_status (context, 0, time);

      retval = TRUE;
      goto out;
    }

  stop_scrolling (notebook);
  target = gtk_drag_dest_find_target (widget, context, NULL);
  tab_target = gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB");

  if (target == tab_target)
    {
      GQuark group, source_group;
      GtkNotebook *source;
      GtkWidget *source_child;

      retval = TRUE;

      source = GTK_NOTEBOOK (gtk_drag_get_source_widget (context));
      g_assert (source->priv->cur_page != NULL);
      source_child = source->priv->cur_page->child;

      group = notebook->priv->group;
      source_group = source->priv->group;

      if (group != 0 && group == source_group &&
          !(widget == source_child ||
            gtk_widget_is_ancestor (widget, source_child)))
        {
          gdk_drag_status (context, GDK_ACTION_MOVE, time);
          goto out;
        }
      else
        {
          /* it's a tab, but doesn't share
           * ID with this notebook */
          gdk_drag_status (context, 0, time);
        }
    }

  x += allocation.x;
  y += allocation.y;

  if (gtk_notebook_get_event_window_position (notebook, &position) &&
      x >= position.x && x <= position.x + position.width &&
      y >= position.y && y <= position.y + position.height &&
      (tab = get_tab_at_pos (notebook, x, y)))
    {
      priv->mouse_x = x;
      priv->mouse_y = y;

      retval = TRUE;

      if (tab != priv->switch_tab)
        remove_switch_tab_timer (notebook);

      priv->switch_tab = tab;

      if (!priv->switch_tab_timer)
        {
          priv->switch_tab_timer = gdk_threads_add_timeout (TIMEOUT_EXPAND,
                                                  gtk_notebook_switch_tab_timeout,
                                                  widget);
          g_source_set_name_by_id (priv->switch_tab_timer, "[gtk+] gtk_notebook_switch_tab_timeout");
        }
    }
  else
    {
      remove_switch_tab_timer (notebook);
    }

 out:
  return retval;
}

static void
gtk_notebook_drag_leave (GtkWidget      *widget,
                         GdkDragContext *context,
                         guint           time)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  remove_switch_tab_timer (notebook);
  stop_scrolling (notebook);
}

static gboolean
gtk_notebook_drag_drop (GtkWidget        *widget,
                        GdkDragContext   *context,
                        gint              x,
                        gint              y,
                        guint             time)
{
  GdkAtom target, tab_target;

  target = gtk_drag_dest_find_target (widget, context, NULL);
  tab_target = gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB");

  if (target == tab_target)
    {
      gtk_drag_get_data (widget, context, target, time);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_notebook_detach_tab:
 * @notebook: a #GtkNotebook
 * @child: a child
 *
 * Removes the child from the notebook.
 *
 * This function is very similar to gtk_container_remove(),
 * but additionally informs the notebook that the removal
 * is happening as part of a tab DND operation, which should
 * not be cancelled.
 *
 * Since: 3.16
 */
void
gtk_notebook_detach_tab (GtkNotebook *notebook,
                         GtkWidget   *child)
{
  notebook->priv->remove_in_detach = TRUE;
  gtk_container_remove (GTK_CONTAINER (notebook), child);
  notebook->priv->remove_in_detach = FALSE;
}

static void
do_detach_tab (GtkNotebook     *from,
               GtkNotebook     *to,
               GtkWidget       *child,
               gint             x,
               gint             y)
{
  GtkNotebookPrivate *to_priv = to->priv;
  GtkAllocation to_allocation;
  GtkWidget *tab_label, *menu_label;
  gboolean tab_expand, tab_fill, reorderable, detachable;
  GList *element;
  gint page_num;

  menu_label = gtk_notebook_get_menu_label (from, child);

  if (menu_label)
    g_object_ref (menu_label);

  tab_label = gtk_notebook_get_tab_label (from, child);

  if (tab_label)
    g_object_ref (tab_label);

  g_object_ref (child);

  gtk_container_child_get (GTK_CONTAINER (from),
                           child,
                           "tab-expand", &tab_expand,
                           "tab-fill", &tab_fill,
                           "reorderable", &reorderable,
                           "detachable", &detachable,
                           NULL);

  gtk_notebook_detach_tab (from, child);

  gtk_widget_get_allocation (GTK_WIDGET (to), &to_allocation);
  to_priv->mouse_x = x + to_allocation.x;
  to_priv->mouse_y = y + to_allocation.y;

  element = get_drop_position (to);
  page_num = g_list_position (to_priv->children, element);
  gtk_notebook_insert_page_menu (to, child, tab_label, menu_label, page_num);

  gtk_container_child_set (GTK_CONTAINER (to), child,
                           "tab-expand", tab_expand,
                           "tab-fill", tab_fill,
                           "reorderable", reorderable,
                           "detachable", detachable,
                           NULL);
  if (child)
    g_object_unref (child);

  if (tab_label)
    g_object_unref (tab_label);

  if (menu_label)
    g_object_unref (menu_label);

  gtk_notebook_set_current_page (to, page_num);
}

static void
gtk_notebook_drag_data_get (GtkWidget        *widget,
                            GdkDragContext   *context,
                            GtkSelectionData *data,
                            guint             info,
                            guint             time)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GdkAtom target;

  target = gtk_selection_data_get_target (data);
  if (target == gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB"))
    {
      gtk_selection_data_set (data,
                              target,
                              8,
                              (void*) &priv->detached_tab->child,
                              sizeof (gpointer));
      priv->rootwindow_drop = FALSE;
    }
  else if (target == gdk_atom_intern_static_string ("application/x-rootwindow-drop"))
    {
      gtk_selection_data_set (data, target, 8, NULL, 0);
      priv->rootwindow_drop = TRUE;
    }
}

static void
gtk_notebook_drag_data_received (GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 gint              x,
                                 gint              y,
                                 GtkSelectionData *data,
                                 guint             info,
                                 guint             time)
{
  GtkNotebook *notebook;
  GtkWidget *source_widget;
  GtkWidget **child;

  notebook = GTK_NOTEBOOK (widget);
  source_widget = gtk_drag_get_source_widget (context);

  if (source_widget &&
      gtk_selection_data_get_target (data) == gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB"))
    {
      child = (void*) gtk_selection_data_get_data (data);

      do_detach_tab (GTK_NOTEBOOK (source_widget), notebook, *child, x, y);
      gtk_drag_finish (context, TRUE, FALSE, time);
    }
  else
    gtk_drag_finish (context, FALSE, FALSE, time);
}

/* Private GtkContainer Methods :
 *
 * gtk_notebook_set_child_arg
 * gtk_notebook_get_child_arg
 * gtk_notebook_add
 * gtk_notebook_remove
 * gtk_notebook_focus
 * gtk_notebook_set_focus_child
 * gtk_notebook_child_type
 * gtk_notebook_forall
 */
static void
gtk_notebook_set_child_property (GtkContainer    *container,
                                 GtkWidget       *child,
                                 guint            property_id,
                                 const GValue    *value,
                                 GParamSpec      *pspec)
{
  gboolean expand;
  gboolean fill;

  /* not finding child's page is valid for menus or labels */
  if (!gtk_notebook_find_child (GTK_NOTEBOOK (container), child))
    return;

  switch (property_id)
    {
    case CHILD_PROP_TAB_LABEL:
      /* a NULL pointer indicates a default_tab setting, otherwise
       * we need to set the associated label
       */
      gtk_notebook_set_tab_label_text (GTK_NOTEBOOK (container), child,
                                       g_value_get_string (value));
      break;
    case CHILD_PROP_MENU_LABEL:
      gtk_notebook_set_menu_label_text (GTK_NOTEBOOK (container), child,
                                        g_value_get_string (value));
      break;
    case CHILD_PROP_POSITION:
      gtk_notebook_reorder_child (GTK_NOTEBOOK (container), child,
                                  g_value_get_int (value));
      break;
    case CHILD_PROP_TAB_EXPAND:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
                                            &expand, &fill);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
                                          g_value_get_boolean (value),
                                          fill);
      break;
    case CHILD_PROP_TAB_FILL:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
                                            &expand, &fill);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
                                          expand,
                                          g_value_get_boolean (value));
      break;
    case CHILD_PROP_REORDERABLE:
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (container), child,
                                        g_value_get_boolean (value));
      break;
    case CHILD_PROP_DETACHABLE:
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (container), child,
                                       g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_get_child_property (GtkContainer    *container,
                                 GtkWidget       *child,
                                 guint            property_id,
                                 GValue          *value,
                                 GParamSpec      *pspec)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *list;
  GtkWidget *label;
  gboolean expand;
  gboolean fill;

  /* not finding child's page is valid for menus or labels */
  list = gtk_notebook_find_child (notebook, child);
  if (!list)
    {
      /* nothing to set on labels or menus */
      g_param_value_set_default (pspec, value);
      return;
    }

  switch (property_id)
    {
    case CHILD_PROP_TAB_LABEL:
      label = gtk_notebook_get_tab_label (notebook, child);

      if (GTK_IS_LABEL (label))
        g_value_set_string (value, gtk_label_get_label (GTK_LABEL (label)));
      else
        g_value_set_string (value, NULL);
      break;
    case CHILD_PROP_MENU_LABEL:
      label = gtk_notebook_get_menu_label (notebook, child);

      if (GTK_IS_LABEL (label))
        g_value_set_string (value, gtk_label_get_label (GTK_LABEL (label)));
      else
        g_value_set_string (value, NULL);
      break;
    case CHILD_PROP_POSITION:
      g_value_set_int (value, g_list_position (priv->children, list));
      break;
    case CHILD_PROP_TAB_EXPAND:
        gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
                                              &expand, NULL);
        g_value_set_boolean (value, expand);
      break;
    case CHILD_PROP_TAB_FILL:
        gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
                                              NULL, &fill);
        g_value_set_boolean (value, fill);
      break;
    case CHILD_PROP_REORDERABLE:
      g_value_set_boolean (value,
                           gtk_notebook_get_tab_reorderable (GTK_NOTEBOOK (container), child));
      break;
    case CHILD_PROP_DETACHABLE:
      g_value_set_boolean (value,
                           gtk_notebook_get_tab_detachable (GTK_NOTEBOOK (container), child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_add (GtkContainer *container,
                  GtkWidget    *widget)
{
  gtk_notebook_insert_page_menu (GTK_NOTEBOOK (container), widget,
                                 NULL, NULL, -1);
}

static void
gtk_notebook_remove (GtkContainer *container,
                     GtkWidget    *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GList *children, *list;
  gint page_num = 0;

  children = priv->children;
  while (children)
    {
      page = children->data;

      if (page->child == widget)
        break;

      page_num++;
      children = children->next;
    }

  if (children == NULL)
    return;

  g_object_ref (widget);

  list = children->next;
  gtk_notebook_real_remove (notebook, children);

  while (list)
    {
      gtk_widget_child_notify (((GtkNotebookPage *)list->data)->child, "position");
      list = list->next;
    }

  g_signal_emit (notebook,
                 notebook_signals[PAGE_REMOVED],
                 0,
                 widget,
                 page_num);

  g_object_unref (widget);
}

static gboolean
focus_tabs_in (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->show_tabs && gtk_notebook_has_current_page (notebook))
    {
      gtk_widget_grab_focus (GTK_WIDGET (notebook));
      gtk_notebook_set_focus_child (GTK_CONTAINER (notebook), NULL);
      gtk_notebook_switch_focus_tab (notebook,
                                     g_list_find (priv->children,
                                                  priv->cur_page));

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
focus_tabs_move (GtkNotebook     *notebook,
                 GtkDirectionType direction,
                 gint             search_direction)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *new_page;

  new_page = gtk_notebook_search_page (notebook, priv->focus_tab,
                                       search_direction, TRUE);
  if (!new_page)
    {
      new_page = gtk_notebook_search_page (notebook, NULL,
                                           search_direction, TRUE);
    }

  if (new_page)
    gtk_notebook_switch_focus_tab (notebook, new_page);
  else
    gtk_widget_error_bell (GTK_WIDGET (notebook));

  return TRUE;
}

static gboolean
focus_child_in (GtkNotebook      *notebook,
                GtkDirectionType  direction)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->cur_page)
    return gtk_widget_child_focus (priv->cur_page->child, direction);
  else
    return FALSE;
}

static gboolean
focus_action_in (GtkNotebook      *notebook,
                 gint              action,
                 GtkDirectionType  direction)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->action_widget[action] &&
      gtk_widget_get_visible (priv->action_widget[action]))
    return gtk_widget_child_focus (priv->action_widget[action], direction);
  else
    return FALSE;
}

/* Focus in the notebook can either be on the pages, or on
 * the tabs or on the action_widgets.
 */
static gboolean
gtk_notebook_focus (GtkWidget        *widget,
                    GtkDirectionType  direction)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *old_focus_child;
  GtkDirectionType effective_direction;
  gint first_action;
  gint last_action;

  gboolean widget_is_focus;
  GtkContainer *container;

  container = GTK_CONTAINER (widget);

  if (priv->tab_pos == GTK_POS_TOP ||
      priv->tab_pos == GTK_POS_LEFT)
    {
      first_action = ACTION_WIDGET_START;
      last_action = ACTION_WIDGET_END;
    }
  else
    {
      first_action = ACTION_WIDGET_END;
      last_action = ACTION_WIDGET_START;
    }

  if (priv->focus_out)
    {
      priv->focus_out = FALSE; /* Clear this to catch the wrap-around case */
      return FALSE;
    }

  widget_is_focus = gtk_widget_is_focus (widget);
  old_focus_child = gtk_container_get_focus_child (container);

  effective_direction = get_effective_direction (notebook, direction);

  if (old_focus_child)          /* Focus on page child or action widget */
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
        return TRUE;

      if (old_focus_child == priv->action_widget[ACTION_WIDGET_START])
        {
          switch (effective_direction)
            {
            case GTK_DIR_DOWN:
              return focus_child_in (notebook, GTK_DIR_TAB_FORWARD);
            case GTK_DIR_RIGHT:
              return focus_tabs_in (notebook);
            case GTK_DIR_LEFT:
              return FALSE;
            case GTK_DIR_UP:
              return FALSE;
            default:
              switch (direction)
                {
                case GTK_DIR_TAB_FORWARD:
                  if ((priv->tab_pos == GTK_POS_RIGHT || priv->tab_pos == GTK_POS_BOTTOM) &&
                      focus_child_in (notebook, direction))
                    return TRUE;
                  return focus_tabs_in (notebook);
                case GTK_DIR_TAB_BACKWARD:
                  return FALSE;
                default:
                  g_assert_not_reached ();
                }
            }
        }
      else if (old_focus_child == priv->action_widget[ACTION_WIDGET_END])
        {
          switch (effective_direction)
            {
            case GTK_DIR_DOWN:
              return focus_child_in (notebook, GTK_DIR_TAB_FORWARD);
            case GTK_DIR_RIGHT:
              return FALSE;
            case GTK_DIR_LEFT:
              return focus_tabs_in (notebook);
            case GTK_DIR_UP:
              return FALSE;
            default:
              switch (direction)
                {
                case GTK_DIR_TAB_FORWARD:
                  return FALSE;
                case GTK_DIR_TAB_BACKWARD:
                  if ((priv->tab_pos == GTK_POS_TOP || priv->tab_pos == GTK_POS_LEFT) &&
                      focus_child_in (notebook, direction))
                    return TRUE;
                  return focus_tabs_in (notebook);
                default:
                  g_assert_not_reached ();
                }
            }
        }
      else
        {
          switch (effective_direction)
            {
            case GTK_DIR_TAB_BACKWARD:
            case GTK_DIR_UP:
              /* Focus onto the tabs */
              return focus_tabs_in (notebook);
            case GTK_DIR_DOWN:
            case GTK_DIR_LEFT:
            case GTK_DIR_RIGHT:
              return FALSE;
            case GTK_DIR_TAB_FORWARD:
              return focus_action_in (notebook, last_action, direction);
            }
        }
    }
  else if (widget_is_focus)     /* Focus was on tabs */
    {
      switch (effective_direction)
        {
        case GTK_DIR_TAB_BACKWARD:
              return focus_action_in (notebook, first_action, direction);
        case GTK_DIR_UP:
          return FALSE;
        case GTK_DIR_TAB_FORWARD:
          if (focus_child_in (notebook, GTK_DIR_TAB_FORWARD))
            return TRUE;
          return focus_action_in (notebook, last_action, direction);
        case GTK_DIR_DOWN:
          /* We use TAB_FORWARD rather than direction so that we focus a more
           * predictable widget for the user; users may be using arrow focusing
           * in this situation even if they don't usually use arrow focusing.
           */
          return focus_child_in (notebook, GTK_DIR_TAB_FORWARD);
        case GTK_DIR_LEFT:
          return focus_tabs_move (notebook, direction, STEP_PREV);
        case GTK_DIR_RIGHT:
          return focus_tabs_move (notebook, direction, STEP_NEXT);
        }
    }
  else /* Focus was not on widget */
    {
      switch (effective_direction)
        {
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
          if (focus_action_in (notebook, first_action, direction))
            return TRUE;
          if (focus_tabs_in (notebook))
            return TRUE;
          if (focus_action_in (notebook, last_action, direction))
            return TRUE;
          if (focus_child_in (notebook, direction))
            return TRUE;
          return FALSE;
        case GTK_DIR_TAB_BACKWARD:
          if (focus_action_in (notebook, last_action, direction))
            return TRUE;
          if (focus_child_in (notebook, direction))
            return TRUE;
          if (focus_tabs_in (notebook))
            return TRUE;
          if (focus_action_in (notebook, first_action, direction))
            return TRUE;
        case GTK_DIR_UP:
        case GTK_DIR_LEFT:
        case GTK_DIR_RIGHT:
          return focus_child_in (notebook, direction);
        }
    }

  g_assert_not_reached ();
  return FALSE;
}

static void
gtk_notebook_set_focus_child (GtkContainer *container,
                              GtkWidget    *child)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *page_child;
  GtkWidget *toplevel;

  /* If the old focus widget was within a page of the notebook,
   * (child may either be NULL or not in this case), record it
   * for future use if we switch to the page with a mnemonic.
   */

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
  if (toplevel && gtk_widget_is_toplevel (toplevel))
    {
      page_child = gtk_window_get_focus (GTK_WINDOW (toplevel));
      while (page_child)
        {
          if (gtk_widget_get_parent (page_child) == GTK_WIDGET (container))
            {
              GList *list = gtk_notebook_find_child (notebook, page_child);
              if (list != NULL)
                {
                  GtkNotebookPage *page = list->data;

                  if (page->last_focus_child)
                    g_object_remove_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);

                  page->last_focus_child = gtk_window_get_focus (GTK_WINDOW (toplevel));
                  g_object_add_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);

                  break;
                }
            }

          page_child = gtk_widget_get_parent (page_child);
        }
    }

  if (child)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));

      priv->child_has_focus = TRUE;
      if (!priv->focus_tab)
        {
          GList *children;
          GtkNotebookPage *page;

          children = priv->children;
          while (children)
            {
              page = children->data;
              if (page->child == child || page->tab_label == child)
                gtk_notebook_switch_focus_tab (notebook, children);
              children = children->next;
            }
        }
    }
  else
    priv->child_has_focus = FALSE;

  GTK_CONTAINER_CLASS (gtk_notebook_parent_class)->set_focus_child (container, child);
}

static void
gtk_notebook_forall (GtkContainer *container,
                     gboolean      include_internals,
                     GtkCallback   callback,
                     gpointer      callback_data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;
  gint i;

  children = priv->children;
  while (children)
    {
      GtkNotebookPage *page;

      page = children->data;
      children = children->next;
      (* callback) (page->child, callback_data);

      if (include_internals)
        {
          if (page->tab_label)
            (* callback) (page->tab_label, callback_data);
        }
    }

  if (include_internals)
    {
      for (i = 0; i < N_ACTION_WIDGETS; i++)
        {
          if (priv->action_widget[i])
            (* callback) (priv->action_widget[i], callback_data);
        }
    }
}

static GType
gtk_notebook_child_type (GtkContainer     *container)
{
  return GTK_TYPE_WIDGET;
}

/* Private GtkNotebook Methods:
 *
 * gtk_notebook_real_insert_page
 */
static void
page_visible_cb (GtkWidget  *child,
                 GParamSpec *arg,
                 gpointer    data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (data);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *list = gtk_notebook_find_child (notebook, GTK_WIDGET (child));
  GtkNotebookPage *page = list->data;
  GList *next = NULL;

  if (priv->menu && page->menu_label)
    {
      GtkWidget *parent = gtk_widget_get_parent (page->menu_label);
      if (parent)
        gtk_widget_set_visible (parent, gtk_widget_get_visible (child));
    }

  if (priv->cur_page == page)
    {
      if (!gtk_widget_get_visible (child))
        {
          list = g_list_find (priv->children, priv->cur_page);
          if (list)
            {
              next = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
              if (!next)
                next = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
            }

          if (next)
            gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (next));
        }
      gtk_css_gadget_set_visible (priv->header_gadget, priv->show_tabs && gtk_notebook_has_current_page (notebook));
    }
  
  if (!gtk_notebook_has_current_page (notebook) && gtk_widget_get_visible (child))
    {
      gtk_notebook_switch_page (notebook, page);
      /* focus_tab is set in the switch_page method */
      gtk_notebook_switch_focus_tab (notebook, priv->focus_tab);
    }
}

static void
measure_tab (GtkCssGadget           *gadget,
             GtkOrientation          orientation,
             gint                    for_size,
             gint                   *minimum,
             gint                   *natural,
             gint                   *minimum_baseline,
             gint                   *natural_baseline,
             gpointer                data)
{
  GtkNotebookPage *page = data;

  _gtk_widget_get_preferred_size_for_size (page->tab_label,
                                           orientation,
                                           for_size,
                                           minimum, natural,
                                           minimum_baseline, natural_baseline);
}

static void
allocate_tab (GtkCssGadget        *gadget,
              const GtkAllocation *allocation,
              int                  baseline,
              GtkAllocation       *out_clip,
              gpointer             data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (gtk_css_gadget_get_owner (gadget));
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page = data;
  GtkAllocation child_allocation;

  child_allocation = *allocation;

  if (page == priv->cur_page && priv->operation == DRAG_OPERATION_REORDER)
    {
      /* needs to be allocated for the drag window */
      child_allocation.x -= priv->drag_window_x;
      child_allocation.y -= priv->drag_window_y;
    }

  if (!page->fill)
    {
      if (priv->tab_pos == GTK_POS_TOP || priv->tab_pos == GTK_POS_BOTTOM)
        {
          gtk_widget_get_preferred_width_for_height (page->tab_label,
                                                     allocation->height,
                                                     NULL,
                                                     &child_allocation.width);
          if (child_allocation.width > allocation->width)
            child_allocation.width = allocation->width;
          else
            child_allocation.x += (allocation->width - child_allocation.width) / 2;

        }
      else
        {
          gtk_widget_get_preferred_height_for_width (page->tab_label,
                                                     allocation->width,
                                                     NULL,
                                                     &child_allocation.height);
          if (child_allocation.height > allocation->height)
            child_allocation.height = allocation->height;
          else
            child_allocation.y += (allocation->height - child_allocation.height) / 2;
        }
    }

  gtk_widget_size_allocate_with_baseline (page->tab_label,
                                          &child_allocation,
                                          baseline);

  gtk_widget_get_clip (page->tab_label, out_clip);
}

static gboolean
draw_tab (GtkCssGadget *gadget,
          cairo_t      *cr,
          int           x,
          int           y,
          int           width,
          int           height,
          gpointer      data)
{
  GtkNotebookPage *page = data;
  GtkWidget *widget;

  widget = gtk_css_gadget_get_owner (gadget);

  gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                page->tab_label,
                                cr);

  return gtk_widget_has_visible_focus (widget) &&
         GTK_NOTEBOOK (widget)->priv->cur_page == page;
}

static gint
gtk_notebook_real_insert_page (GtkNotebook *notebook,
                               GtkWidget   *child,
                               GtkWidget   *tab_label,
                               GtkWidget   *menu_label,
                               gint         position)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  gint nchildren;
  GList *list;
  GtkCssGadget *sibling;

  gtk_widget_freeze_child_notify (child);

  page = g_slice_new0 (GtkNotebookPage);
  page->child = child;

  nchildren = g_list_length (priv->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  priv->children = g_list_insert (priv->children, page, position);

  if (position < nchildren)
    sibling = GTK_NOTEBOOK_PAGE (g_list_nth (priv->children, position))->gadget;
  else if (priv->arrow_gadget[ARROW_LEFT_AFTER])
    sibling = priv->arrow_gadget[ARROW_LEFT_AFTER];
  else
    sibling = priv->arrow_gadget[ARROW_RIGHT_AFTER];

  if (priv->tabs_reversed)
    gtk_css_node_reverse_children (gtk_css_gadget_get_node (priv->tabs_gadget));

  page->gadget = gtk_css_custom_gadget_new ("tab",
                                            GTK_WIDGET (notebook),
                                            priv->tabs_gadget,
                                            sibling,
                                            measure_tab,
                                            allocate_tab,
                                            draw_tab,
                                            page,
                                            NULL);
  if (priv->tabs_reversed)
    gtk_css_node_reverse_children (gtk_css_gadget_get_node (priv->tabs_gadget));

  gtk_css_gadget_set_state (page->gadget, gtk_css_node_get_state (gtk_css_gadget_get_node (priv->tabs_gadget)));

  if (!tab_label)
    page->default_tab = TRUE;

  page->tab_label = tab_label;
  page->menu_label = menu_label;
  page->expand = FALSE;
  page->fill = TRUE;

  if (!menu_label)
    page->default_menu = TRUE;
  else
    g_object_ref_sink (page->menu_label);

  if (priv->menu)
    gtk_notebook_menu_item_create (notebook,
                                   g_list_find (priv->children, page));

  /* child visible will be turned on by switch_page below */
  gtk_widget_set_child_visible (child, FALSE);

  gtk_css_node_set_parent (gtk_widget_get_css_node (child), gtk_css_gadget_get_node (priv->stack_gadget));
  gtk_widget_set_parent (child, GTK_WIDGET (notebook));
  if (tab_label)
    {
      gtk_css_node_set_parent (gtk_widget_get_css_node (tab_label),
                               gtk_css_gadget_get_node (page->gadget));
      gtk_widget_set_parent (tab_label, GTK_WIDGET (notebook));
    }

  gtk_notebook_update_labels (notebook);

  if (!priv->first_tab)
    priv->first_tab = priv->children;

  if (tab_label)
    {
      if (priv->show_tabs && gtk_widget_get_visible (child))
        gtk_widget_show (tab_label);
      else
        gtk_widget_hide (tab_label);

    page->mnemonic_activate_signal =
      g_signal_connect (tab_label,
                        "mnemonic-activate",
                        G_CALLBACK (gtk_notebook_mnemonic_activate_switch_page),
                        notebook);
    }

  page->notify_visible_handler = g_signal_connect (child, "notify::visible",
                                                   G_CALLBACK (page_visible_cb), notebook);

  g_signal_emit (notebook,
                 notebook_signals[PAGE_ADDED],
                 0,
                 child,
                 position);

  if (!gtk_notebook_has_current_page (notebook))
    {
      gtk_notebook_switch_page (notebook, page);
      /* focus_tab is set in the switch_page method */
      gtk_notebook_switch_focus_tab (notebook, priv->focus_tab);
    }

  if (priv->scrollable)
    gtk_notebook_redraw_arrows (notebook);

  gtk_widget_child_notify (child, "tab-expand");
  gtk_widget_child_notify (child, "tab-fill");
  gtk_widget_child_notify (child, "tab-label");
  gtk_widget_child_notify (child, "menu-label");

  list = g_list_nth (priv->children, position);
  while (list)
    {
      gtk_widget_child_notify (((GtkNotebookPage *)list->data)->child, "position");
      list = list->next;
    }

  gtk_widget_thaw_child_notify (child);

  /* The page-added handler might have reordered the pages, re-get the position */
  return gtk_notebook_page_num (notebook, child);
}

/* Private GtkNotebook Functions:
 *
 * gtk_notebook_real_remove
 * gtk_notebook_update_labels
 * gtk_notebook_timer
 * gtk_notebook_set_scroll_timer
 * gtk_notebook_page_compare
 * gtk_notebook_search_page
 */
static void
gtk_notebook_redraw_arrows (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  update_arrow_state (notebook);

  if (gtk_widget_get_mapped (GTK_WIDGET (notebook)) &&
      gtk_notebook_show_arrows (notebook))
    {
      GdkRectangle rect;
      gint i;

      for (i = 0; i < 4; i++)
        {
          if (priv->arrow_gadget[i] == NULL)
            continue;

          gtk_notebook_get_arrow_rect (notebook, &rect, i);
          gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (notebook)),
                                      &rect, FALSE);
        }
    }
}

static gboolean
gtk_notebook_timer (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gboolean retval = FALSE;

  if (priv->timer)
    {
      gtk_notebook_do_arrow (notebook, priv->click_child);

      if (priv->need_timer)
        {
          priv->need_timer = FALSE;
          priv->timer = gdk_threads_add_timeout (TIMEOUT_REPEAT * SCROLL_DELAY_FACTOR,
                                           (GSourceFunc) gtk_notebook_timer,
                                           (gpointer) notebook);
          g_source_set_name_by_id (priv->timer, "[gtk+] gtk_notebook_timer");
        }
      else
        retval = TRUE;
    }

  return retval;
}

static void
gtk_notebook_set_scroll_timer (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (!priv->timer)
    {
      priv->timer = gdk_threads_add_timeout (TIMEOUT_INITIAL,
                                       (GSourceFunc) gtk_notebook_timer,
                                       (gpointer) notebook);
      g_source_set_name_by_id (priv->timer, "[gtk+] gtk_notebook_timer");
      priv->need_timer = TRUE;
    }
}

static gint
gtk_notebook_page_compare (gconstpointer a,
                           gconstpointer b)
{
  return (((GtkNotebookPage *) a)->child != b);
}

static GList*
gtk_notebook_find_child (GtkNotebook *notebook,
                         GtkWidget   *child)
{
  return g_list_find_custom (notebook->priv->children,
                             child,
                             gtk_notebook_page_compare);
}

static void
gtk_notebook_remove_tab_label (GtkNotebook     *notebook,
                               GtkNotebookPage *page)
{
  if (page->tab_label)
    {
      if (page->mnemonic_activate_signal)
        g_signal_handler_disconnect (page->tab_label,
                                     page->mnemonic_activate_signal);
      page->mnemonic_activate_signal = 0;

      if (gtk_widget_get_window (page->tab_label) != gtk_widget_get_window (GTK_WIDGET (notebook)) ||
          !NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
        {
          GtkWidget *parent;

          /* we hit this condition during dnd of a detached tab */
          parent = gtk_widget_get_parent (page->tab_label);
          if (GTK_IS_WINDOW (parent))
            gtk_container_remove (GTK_CONTAINER (parent), page->tab_label);
          else
            gtk_widget_unparent (page->tab_label);
        }
      else
        {
          gtk_widget_unparent (page->tab_label);
        }

      page->tab_label = NULL;
    }
}

static void
gtk_notebook_real_remove (GtkNotebook *notebook,
                          GList       *list)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GList * next_list;
  gint need_resize = FALSE;
  GtkWidget *tab_label;
  gboolean destroying;

  destroying = gtk_widget_in_destruction (GTK_WIDGET (notebook));

  next_list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
  if (!next_list)
    next_list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);

  priv->children = g_list_remove_link (priv->children, list);

  if (priv->cur_page == list->data)
    {
      priv->cur_page = NULL;
      if (next_list && !destroying)
        gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (next_list));
      if (priv->operation == DRAG_OPERATION_REORDER && !priv->remove_in_detach)
        gtk_notebook_stop_reorder (notebook);
    }

  if (priv->detached_tab == list->data)
    {
      priv->detached_tab = NULL;

      if (priv->operation == DRAG_OPERATION_DETACH && !priv->remove_in_detach)
        {
          GdkDragContext *context;

          context = (GdkDragContext *)g_object_get_data (G_OBJECT (priv->dnd_window), "drag-context");
          gtk_drag_cancel (context);
        }
    }
  if (priv->prelight_tab == list->data)
    update_prelight_tab (notebook, NULL);
  if (priv->switch_tab == list)
    priv->switch_tab = NULL;

  if (list == priv->first_tab)
    priv->first_tab = next_list;
  if (list == priv->focus_tab && !destroying)
    gtk_notebook_switch_focus_tab (notebook, next_list);

  page = list->data;

  g_signal_handler_disconnect (page->child, page->notify_visible_handler);

  if (gtk_widget_get_visible (page->child) &&
      gtk_widget_get_visible (GTK_WIDGET (notebook)))
    need_resize = TRUE;

  gtk_widget_unparent (page->child);

  tab_label = page->tab_label;
  if (tab_label)
    {
      g_object_ref (tab_label);
      gtk_notebook_remove_tab_label (notebook, page);
      if (destroying)
        gtk_widget_destroy (tab_label);
      g_object_unref (tab_label);
    }

  if (priv->menu)
    {
      GtkWidget *parent = gtk_widget_get_parent (page->menu_label);

      gtk_notebook_menu_label_unparent (parent, NULL);
      gtk_container_remove (GTK_CONTAINER (priv->menu), parent);

      gtk_widget_queue_resize (priv->menu);
    }
  if (!page->default_menu)
    g_object_unref (page->menu_label);

  g_list_free (list);

  if (page->last_focus_child)
    {
      g_object_remove_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
      page->last_focus_child = NULL;
    }

  gtk_css_node_set_parent (gtk_css_gadget_get_node (page->gadget), NULL);
  g_object_unref (page->gadget);

  g_slice_free (GtkNotebookPage, page);

  gtk_notebook_update_labels (notebook);
  if (need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
  if (!destroying && priv->scrollable)
    gtk_notebook_redraw_arrows (notebook);
}

static void
gtk_notebook_update_labels (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GList *list;
  gchar string[32];
  gint page_num = 1;

  if (!priv->show_tabs && !priv->menu)
    return;

  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    {
      page = list->data;
      g_snprintf (string, sizeof(string), _("Page %u"), page_num++);
      if (priv->show_tabs)
        {
          if (page->default_tab)
            {
              if (!page->tab_label)
                {
                  page->tab_label = gtk_label_new (string);
                  gtk_css_node_set_parent (gtk_widget_get_css_node (page->tab_label),
                                           gtk_css_gadget_get_node (page->gadget));
                  gtk_widget_set_parent (page->tab_label,
                                         GTK_WIDGET (notebook));
                }
              else
                gtk_label_set_text (GTK_LABEL (page->tab_label), string);
            }

          if (gtk_widget_get_visible (page->child) &&
              !gtk_widget_get_visible (page->tab_label))
            gtk_widget_show (page->tab_label);
          else if (!gtk_widget_get_visible (page->child) &&
                   gtk_widget_get_visible (page->tab_label))
            gtk_widget_hide (page->tab_label);
        }
      if (priv->menu && page->default_menu)
        {
          if (GTK_IS_LABEL (page->tab_label))
            gtk_label_set_text (GTK_LABEL (page->menu_label),
                                gtk_label_get_text (GTK_LABEL (page->tab_label)));
          else
            gtk_label_set_text (GTK_LABEL (page->menu_label), string);
        }
    }
}

static GList *
gtk_notebook_search_page (GtkNotebook *notebook,
                          GList       *list,
                          gint         direction,
                          gboolean     find_visible)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page = NULL;
  GList *old_list = NULL;

  if (list)
    page = list->data;

  if (!page || direction == STEP_NEXT)
    {
      if (list)
        {
          old_list = list;
          list = list->next;
        }
      else
        list = priv->children;

      while (list)
        {
          page = list->data;
          if (direction == STEP_NEXT &&
              (!find_visible ||
               (gtk_widget_get_visible (page->child) &&
                (!page->tab_label || NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page)))))
            return list;
          old_list = list;
          list = list->next;
        }
      list = old_list;
    }
  else
    {
      list = list->prev;
    }
  while (list)
    {
      page = list->data;
      if (direction == STEP_PREV &&
          (!find_visible ||
           (gtk_widget_get_visible (page->child) &&
            (!page->tab_label || NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page)))))
        return list;
      list = list->prev;
    }
  return NULL;
}

static gboolean
gtk_notebook_draw_tabs (GtkCssGadget *gadget,
                        cairo_t      *cr,
                        int           x,
                        int           y,
                        int           width,
                        int           height,
                        gpointer      unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GList *children;
  gboolean showarrow;
  gint step = STEP_PREV;
  gboolean is_rtl;
  GtkPositionType tab_pos;
  guint i;

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  tab_pos = get_effective_tab_pos (notebook);
  showarrow = FALSE;

  if (!gtk_notebook_has_current_page (notebook))
    return FALSE;

  if (!priv->first_tab)
    priv->first_tab = priv->children;

  if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, priv->cur_page) ||
      !gtk_widget_get_mapped (priv->cur_page->tab_label))
    {
      step = STEP_PREV;
    }
  else
    {
      switch (tab_pos)
        {
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          step = is_rtl ? STEP_PREV : STEP_NEXT;
          break;
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          step = STEP_PREV;
          break;
        }
    }

  for (children = priv->children; children; children = children->next)
    {
      page = children->data;

      if (!gtk_widget_get_visible (page->child))
        continue;

      if (!gtk_widget_get_mapped (page->tab_label))
        showarrow = TRUE;

      /* No point in keeping searching */
      if (showarrow)
        break;
    }

  for (children = gtk_notebook_search_page (notebook, NULL, step, TRUE);
       children;
       children = gtk_notebook_search_page (notebook, children, step, TRUE))
    {
      page = children->data;

      if (page == priv->cur_page)
        break;

      if (!gtk_notebook_page_tab_label_is_visible (page))
        continue;

      gtk_css_gadget_draw (page->gadget, cr);
    }

  if (children != NULL)
    {
      GList *other_order = NULL;

      for (children = gtk_notebook_search_page (notebook, children, step, TRUE);
           children;
           children = gtk_notebook_search_page (notebook, children, step, TRUE))
        {
          page = children->data;

          if (!gtk_notebook_page_tab_label_is_visible (page))
            continue;

          other_order = g_list_prepend (other_order, page);
        }

      /* draw them with the opposite order */
      for (children = other_order; children; children = children->next)
        {
          page = children->data;
          gtk_css_gadget_draw (page->gadget, cr);
        }

      g_list_free (other_order);
    }

  if (showarrow && priv->scrollable)
    {
      for (i = 0; i < 4; i++)
        {
          if (priv->arrow_gadget[i] == NULL)
            continue;
          
          gtk_css_gadget_draw (priv->arrow_gadget[i], cr);
        }
    }

  if (priv->operation != DRAG_OPERATION_DETACH)
    gtk_css_gadget_draw (priv->cur_page->gadget, cr);

  return FALSE;
}

/* Private GtkNotebook Size Allocate Functions:
 *
 * gtk_notebook_tab_space
 * gtk_notebook_calculate_shown_tabs
 * gtk_notebook_calculate_tabs_allocation
 * gtk_notebook_pages_allocate
 * gtk_notebook_calc_tabs
 */
static void
gtk_notebook_allocate_arrows (GtkNotebook   *notebook,
                              GtkAllocation *allocation)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkAllocation arrow_allocation, arrow_clip;
  gint size1, size2, min, nat;
  guint i, ii;

  switch (priv->tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      arrow_allocation.y = allocation->y;
      arrow_allocation.height = allocation->height;
      for (i = 0; i < 4; i++)
        {
          ii = i < 2 ? i : i ^ 1;

          if (priv->arrow_gadget[ii] == NULL)
            continue;

          gtk_css_gadget_get_preferred_size (priv->arrow_gadget[ii],
                                             GTK_ORIENTATION_HORIZONTAL,
                                             allocation->height,
                                             &min, &nat,
                                             NULL, NULL);
          if (i < 2)
            {
              arrow_allocation.x = allocation->x;
              arrow_allocation.width = min;
              gtk_css_gadget_allocate (priv->arrow_gadget[ii], &arrow_allocation, -1, &arrow_clip);
              allocation->x += min;
              allocation->width -= min;
            }
          else
            {
              arrow_allocation.x = allocation->x + allocation->width - min;
              arrow_allocation.width = min;
              gtk_css_gadget_allocate (priv->arrow_gadget[ii], &arrow_allocation, -1, &arrow_clip);
              allocation->width -= min;
            }
        }
      break;

    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      if (priv->arrow_gadget[0] || priv->arrow_gadget[1])
        {
          gtk_notebook_measure_arrows (notebook,
                                       GTK_PACK_START,
                                       GTK_ORIENTATION_VERTICAL,
                                       allocation->width,
                                       &min, &nat,
                                       NULL, NULL);
          gtk_notebook_distribute_arrow_width (notebook, GTK_PACK_START, allocation->width, &size1, &size2);
          arrow_allocation.x = allocation->x;
          arrow_allocation.y = allocation->y;
          arrow_allocation.width = size1;
          arrow_allocation.height = min;
          if (priv->arrow_gadget[0])
            gtk_css_gadget_allocate (priv->arrow_gadget[0], &arrow_allocation, -1, &arrow_clip);
          arrow_allocation.x += size1;
          arrow_allocation.width = size2;
          if (priv->arrow_gadget[1])
            gtk_css_gadget_allocate (priv->arrow_gadget[1], &arrow_allocation, -1, &arrow_clip);
          allocation->y += min;
          allocation->height -= min;
        }
      if (priv->arrow_gadget[2] || priv->arrow_gadget[3])
        {
          gtk_notebook_measure_arrows (notebook,
                                       GTK_PACK_END,
                                       GTK_ORIENTATION_VERTICAL,
                                       allocation->width,
                                       &min, &nat,
                                       NULL, NULL);
          gtk_notebook_distribute_arrow_width (notebook, GTK_PACK_END, allocation->width, &size1, &size2);
          arrow_allocation.x = allocation->x;
          arrow_allocation.y = allocation->y + allocation->height - min;
          arrow_allocation.width = size1;
          arrow_allocation.height = min;
          if (priv->arrow_gadget[2])
            gtk_css_gadget_allocate (priv->arrow_gadget[2], &arrow_allocation, -1, &arrow_clip);
          arrow_allocation.x += size1;
          arrow_allocation.width = size2;
          if (priv->arrow_gadget[3])
            gtk_css_gadget_allocate (priv->arrow_gadget[3], &arrow_allocation, -1, &arrow_clip);
          allocation->height -= min;
        }
      break;

    }
}


static void
gtk_notebook_tab_space (GtkNotebook         *notebook,
                        const GtkAllocation *allocation,
                        gboolean            *show_arrows,
                        GtkAllocation       *tabs_allocation,
                        gint                *tab_space)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;
  GtkPositionType tab_pos = get_effective_tab_pos (notebook);

  children = priv->children;

  *tabs_allocation = *allocation;

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      while (children)
        {
          GtkNotebookPage *page;

          page = children->data;
          children = children->next;

          if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
              gtk_widget_get_visible (page->child))
            *tab_space += page->requisition.width;
        }
      break;
    case GTK_POS_RIGHT:
    case GTK_POS_LEFT:
      while (children)
        {
          GtkNotebookPage *page;

          page = children->data;
          children = children->next;

          if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
              gtk_widget_get_visible (page->child))
            *tab_space += page->requisition.height;
        }
      break;
    }

  if (!priv->scrollable)
    *show_arrows = FALSE;
  else
    {
      switch (tab_pos)
        {
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          if (*tab_space > tabs_allocation->width)
            {
              *show_arrows = TRUE;

              gtk_notebook_allocate_arrows (notebook, tabs_allocation);

              *tab_space = tabs_allocation->width;
            }
          break;
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          if (*tab_space > tabs_allocation->height)
            {
              *show_arrows = TRUE;

              gtk_notebook_allocate_arrows (notebook, tabs_allocation);

              *tab_space = tabs_allocation->height;
            }
          break;
        }
    }
}

static void
gtk_notebook_calculate_shown_tabs (GtkNotebook          *notebook,
                                   gboolean              show_arrows,
                                   const GtkAllocation  *tabs_allocation,
                                   gint                  tab_space,
                                   GList               **last_child,
                                   gint                 *n,
                                   gint                 *remaining_space)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;
  GtkNotebookPage *page;

  if (show_arrows) /* first_tab <- focus_tab */
    {
      *remaining_space = tab_space;

      if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, priv->cur_page) &&
          gtk_widget_get_visible (priv->cur_page->child))
        {
          gtk_notebook_calc_tabs (notebook,
                                  priv->focus_tab,
                                  &(priv->focus_tab),
                                  remaining_space, STEP_NEXT);
        }

      if (tab_space <= 0 || *remaining_space <= 0)
        {
          /* show 1 tab */
          priv->first_tab = priv->focus_tab;
          *last_child = gtk_notebook_search_page (notebook, priv->focus_tab,
                                                  STEP_NEXT, TRUE);
          *n = 1;
        }
      else
        {
          children = NULL;

          if (priv->first_tab && priv->first_tab != priv->focus_tab)
            {
              /* Is first_tab really predecessor of focus_tab? */
              page = priv->first_tab->data;
              if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
                  gtk_widget_get_visible (page->child))
                for (children = priv->focus_tab;
                     children && children != priv->first_tab;
                     children = gtk_notebook_search_page (notebook,
                                                          children,
                                                          STEP_PREV,
                                                          TRUE));
            }

          if (!children)
            {
              if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, priv->cur_page))
                priv->first_tab = priv->focus_tab;
              else
                priv->first_tab = gtk_notebook_search_page (notebook, priv->focus_tab,
                                                            STEP_NEXT, TRUE);
            }
          else
            /* calculate shown tabs counting backwards from the focus tab */
            gtk_notebook_calc_tabs (notebook,
                                    gtk_notebook_search_page (notebook,
                                                              priv->focus_tab,
                                                              STEP_PREV,
                                                              TRUE),
                                    &(priv->first_tab),
                                    remaining_space,
                                    STEP_PREV);

          if (*remaining_space < 0)
            {
              priv->first_tab =
                gtk_notebook_search_page (notebook, priv->first_tab,
                                          STEP_NEXT, TRUE);
              if (!priv->first_tab)
                priv->first_tab = priv->focus_tab;

              *last_child = gtk_notebook_search_page (notebook, priv->focus_tab,
                                                      STEP_NEXT, TRUE);
            }
          else /* focus_tab -> end */
            {
              if (!priv->first_tab)
                priv->first_tab = gtk_notebook_search_page (notebook,
                                                            NULL,
                                                            STEP_NEXT,
                                                            TRUE);
              children = NULL;
              gtk_notebook_calc_tabs (notebook,
                                      gtk_notebook_search_page (notebook,
                                                                priv->focus_tab,
                                                                STEP_NEXT,
                                                                TRUE),
                                      &children,
                                      remaining_space,
                                      STEP_NEXT);

              if (*remaining_space <= 0)
                *last_child = children;
              else /* start <- first_tab */
                {
                  *last_child = NULL;
                  children = NULL;

                  gtk_notebook_calc_tabs (notebook,
                                          gtk_notebook_search_page (notebook,
                                                                    priv->first_tab,
                                                                    STEP_PREV,
                                                                    TRUE),
                                          &children,
                                          remaining_space,
                                          STEP_PREV);

                  if (*remaining_space == 0)
                    priv->first_tab = children;
                  else
                    priv->first_tab = gtk_notebook_search_page(notebook,
                                                               children,
                                                               STEP_NEXT,
                                                               TRUE);
                }
            }

          if (*remaining_space < 0)
            {
              /* calculate number of tabs */
              *remaining_space = - (*remaining_space);
              *n = 0;

              for (children = priv->first_tab;
                   children && children != *last_child;
                   children = gtk_notebook_search_page (notebook, children,
                                                        STEP_NEXT, TRUE))
                (*n)++;
            }
          else
            *remaining_space = 0;
        }

      /* unmap all non-visible tabs */
      for (children = gtk_notebook_search_page (notebook, NULL,
                                                STEP_NEXT, TRUE);
           children && children != priv->first_tab;
           children = gtk_notebook_search_page (notebook, children,
                                                STEP_NEXT, TRUE))
        {
          page = children->data;

          if (page->tab_label &&
              NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
            gtk_widget_set_child_visible (page->tab_label, FALSE);
        }

      for (children = *last_child; children;
           children = gtk_notebook_search_page (notebook, children,
                                                STEP_NEXT, TRUE))
        {
          page = children->data;

          if (page->tab_label &&
              NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
            gtk_widget_set_child_visible (page->tab_label, FALSE);
        }
    }
  else /* !show_arrows */
    {
      GtkOrientation tab_expand_orientation;
      gint c = 0;
      *n = 0;

      if (priv->tab_pos == GTK_POS_TOP || priv->tab_pos == GTK_POS_BOTTOM)
        {
          tab_expand_orientation = GTK_ORIENTATION_HORIZONTAL;
          *remaining_space = tabs_allocation->width - tab_space;
        }
      else
        {
          tab_expand_orientation = GTK_ORIENTATION_VERTICAL;
          *remaining_space = tabs_allocation->height - tab_space;
        }
      children = priv->children;
      priv->first_tab = gtk_notebook_search_page (notebook, NULL,
                                                  STEP_NEXT, TRUE);
      while (children)
        {
          page = children->data;
          children = children->next;

          if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) ||
              !gtk_widget_get_visible (page->child))
            continue;

          c++;

          if (page->expand ||
              (gtk_widget_compute_expand (page->tab_label, tab_expand_orientation)))
            (*n)++;
        }
    }
}

static gboolean
get_allocate_at_bottom (GtkWidget *widget,
                        gint       search_direction)
{
  gboolean is_rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  GtkPositionType tab_pos = get_effective_tab_pos (GTK_NOTEBOOK (widget));

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      if (!is_rtl)
        return (search_direction == STEP_PREV);
      else
        return (search_direction == STEP_NEXT);

      break;
    case GTK_POS_RIGHT:
    case GTK_POS_LEFT:
      return (search_direction == STEP_PREV);
      break;
    }

  return FALSE;
}

static void
gtk_notebook_calculate_tabs_allocation (GtkNotebook          *notebook,
                                        GList               **children,
                                        GList                *last_child,
                                        gboolean              showarrow,
                                        gint                  direction,
                                        gint                 *remaining_space,
                                        gint                 *expanded_tabs,
                                        const GtkAllocation  *allocation)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *widget;
  GtkNotebookPage *page;
  gboolean allocate_at_bottom;
  gint tab_extra_space;
  GtkPositionType tab_pos;
  gint left_x, right_x, top_y, bottom_y, anchor;
  gboolean gap_left, packing_changed;
  GtkAllocation child_allocation, drag_allocation, page_clip;
  GtkOrientation tab_expand_orientation;

  g_assert (priv->cur_page != NULL);

  widget = GTK_WIDGET (notebook);
  tab_pos = get_effective_tab_pos (notebook);
  allocate_at_bottom = get_allocate_at_bottom (widget, direction);
  anchor = 0;

  child_allocation = *allocation;

  switch (tab_pos)
    {
    case GTK_POS_BOTTOM:
    case GTK_POS_TOP:
      if (allocate_at_bottom)
        child_allocation.x += allocation->width;
      anchor = child_allocation.x;
      break;

    case GTK_POS_RIGHT:
    case GTK_POS_LEFT:
      if (allocate_at_bottom)
        child_allocation.y += allocation->height;
      anchor = child_allocation.y;
      break;
    }

  gtk_css_gadget_get_margin_allocation (priv->cur_page->gadget, &drag_allocation, NULL);
  left_x   = CLAMP (priv->mouse_x - priv->drag_offset_x,
                    allocation->x, allocation->x + allocation->width - drag_allocation.width);
  top_y    = CLAMP (priv->mouse_y - priv->drag_offset_y,
                    allocation->y, allocation->y + allocation->height - drag_allocation.height);
  right_x  = left_x + drag_allocation.width;
  bottom_y = top_y + drag_allocation.height;
  gap_left = packing_changed = FALSE;

  if (priv->tab_pos == GTK_POS_TOP || priv->tab_pos == GTK_POS_BOTTOM)
    tab_expand_orientation = GTK_ORIENTATION_HORIZONTAL;
  else
    tab_expand_orientation = GTK_ORIENTATION_VERTICAL;

  while (*children && *children != last_child)
    {
      page = (*children)->data;

      if (direction == STEP_NEXT)
        *children = gtk_notebook_search_page (notebook, *children, direction, TRUE);
      else
        {
          *children = (*children)->next;
          continue;
        }

      if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
        continue;

      tab_extra_space = 0;
      if (*expanded_tabs && (showarrow || page->expand || gtk_widget_compute_expand (page->tab_label, tab_expand_orientation)))
        {
          tab_extra_space = *remaining_space / *expanded_tabs;
          *remaining_space -= tab_extra_space;
          (*expanded_tabs)--;
        }

      switch (tab_pos)
        {
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          child_allocation.width = MAX (1, page->requisition.width + tab_extra_space);

          /* make sure that the reordered tab doesn't go past the last position */
          if (priv->operation == DRAG_OPERATION_REORDER &&
              !gap_left && packing_changed)
            {
              if (!allocate_at_bottom)
                {
                  if (left_x >= anchor)
                    {
                      left_x = priv->drag_window_x = anchor;
                      anchor += drag_allocation.width;
                    }
                }
              else
                {
                  if (right_x <= anchor)
                    {
                      anchor -= drag_allocation.width;
                      left_x = priv->drag_window_x = anchor;
                    }
                }

              gap_left = TRUE;
            }

          if (priv->operation == DRAG_OPERATION_REORDER && page == priv->cur_page)
            {
              priv->drag_window_x = left_x;
              priv->drag_window_y = child_allocation.y;
            }
          else
            {
              if (allocate_at_bottom)
                anchor -= child_allocation.width;

              if (priv->operation == DRAG_OPERATION_REORDER)
                {
                  if (!allocate_at_bottom &&
                      left_x >= anchor &&
                      left_x <= anchor + child_allocation.width / 2)
                    anchor += drag_allocation.width;
                  else if (allocate_at_bottom &&
                           right_x >= anchor + child_allocation.width / 2 &&
                           right_x <= anchor + child_allocation.width)
                    anchor -= drag_allocation.width;
                }

              child_allocation.x = anchor;
            }

          break;
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          child_allocation.height = MAX (1, page->requisition.height + tab_extra_space);

          /* make sure that the reordered tab doesn't go past the last position */
          if (priv->operation == DRAG_OPERATION_REORDER &&
              !gap_left && packing_changed)
            {
              if (!allocate_at_bottom && top_y >= anchor)
                {
                  top_y = priv->drag_window_y = anchor;
                  anchor += drag_allocation.height;
                }

              gap_left = TRUE;
            }

          if (priv->operation == DRAG_OPERATION_REORDER && page == priv->cur_page)
            {
              priv->drag_window_x = child_allocation.x;
              priv->drag_window_y = top_y;
            }
          else
            {
              if (allocate_at_bottom)
                anchor -= child_allocation.height;

              if (priv->operation == DRAG_OPERATION_REORDER)
                {
                  if (!allocate_at_bottom &&
                      top_y >= anchor &&
                      top_y <= anchor + child_allocation.height / 2)
                    anchor += drag_allocation.height;
                  else if (allocate_at_bottom &&
                           bottom_y >= anchor + child_allocation.height / 2 &&
                           bottom_y <= anchor + child_allocation.height)
                    anchor -= drag_allocation.height;
                }

              child_allocation.y = anchor;
            }

          break;
        }

      if (page == priv->cur_page && priv->operation == DRAG_OPERATION_REORDER)
        {
          GtkAllocation fixed_allocation = { priv->drag_window_x, priv->drag_window_y,
                                             child_allocation.width, child_allocation.height };
          gdk_window_move_resize (priv->drag_window,
                                  priv->drag_window_x, priv->drag_window_y,
                                  child_allocation.width, child_allocation.height);
          gtk_css_gadget_allocate (page->gadget, &fixed_allocation, -1, &page_clip);
        }
      else if (page == priv->detached_tab && priv->operation == DRAG_OPERATION_DETACH)
        {
          /* needs to be allocated at 0,0
           * to be shown in the drag window */
          GtkAllocation fixed_allocation = { 0, 0, child_allocation.width, child_allocation.height };
          gtk_css_gadget_allocate (page->gadget, &fixed_allocation, -1, &page_clip);
        }
      else 
        {
          gtk_css_gadget_allocate (page->gadget, &child_allocation, -1, &page_clip);
        }

      /* calculate whether to leave a gap based on reorder operation or not */
      switch (tab_pos)
        {
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          if (priv->operation != DRAG_OPERATION_REORDER || page != priv->cur_page)
            {
              if (priv->operation == DRAG_OPERATION_REORDER)
                {
                  if (!allocate_at_bottom &&
                      left_x >  anchor + child_allocation.width / 2 &&
                      left_x <= anchor + child_allocation.width)
                    anchor += drag_allocation.width;
                  else if (allocate_at_bottom &&
                           right_x >= anchor &&
                           right_x <= anchor + child_allocation.width / 2)
                    anchor -= drag_allocation.width;
                }

              if (!allocate_at_bottom)
                anchor += child_allocation.width;
            }

          break;
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          if (priv->operation != DRAG_OPERATION_REORDER || page != priv->cur_page)
            {
              if (priv->operation == DRAG_OPERATION_REORDER)
                {
                  if (!allocate_at_bottom &&
                      top_y >= anchor + child_allocation.height / 2 &&
                      top_y <= anchor + child_allocation.height)
                    anchor += drag_allocation.height;
                  else if (allocate_at_bottom &&
                           bottom_y >= anchor &&
                           bottom_y <= anchor + child_allocation.height / 2)
                    anchor -= drag_allocation.height;
                }

              if (!allocate_at_bottom)
                anchor += child_allocation.height;
            }

          break;
        }

      /* set child visible */
      if (page->tab_label)
        gtk_widget_set_child_visible (page->tab_label, TRUE);
    }

  /* Don't move the current tab past the last position during tabs reordering */
  if (priv->operation == DRAG_OPERATION_REORDER &&
      direction == STEP_NEXT)
    {
      switch (tab_pos)
        {
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          if (allocate_at_bottom)
            anchor -= drag_allocation.width;

          if ((!allocate_at_bottom && priv->drag_window_x > anchor) ||
              (allocate_at_bottom && priv->drag_window_x < anchor))
            priv->drag_window_x = anchor;
          break;
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          if (allocate_at_bottom)
            anchor -= drag_allocation.height;

          if ((!allocate_at_bottom && priv->drag_window_y > anchor) ||
              (allocate_at_bottom && priv->drag_window_y < anchor))
            priv->drag_window_y = anchor;
          break;
        }
    }
}

static void
gtk_notebook_pages_allocate (GtkNotebook         *notebook,
                             const GtkAllocation *allocation)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children = NULL;
  GList *last_child = NULL;
  gboolean showarrow = FALSE;
  GtkAllocation tabs_allocation;
  gint tab_space, remaining_space;
  gint expanded_tabs;

  if (!priv->show_tabs || !gtk_notebook_has_current_page (notebook))
    return;

  tab_space = remaining_space = 0;
  expanded_tabs = 1;

  gtk_notebook_tab_space (notebook, allocation,
                          &showarrow, &tabs_allocation, &tab_space);

  gtk_notebook_calculate_shown_tabs (notebook, showarrow,
                                     &tabs_allocation, tab_space, &last_child,
                                     &expanded_tabs, &remaining_space);

  children = priv->first_tab;
  gtk_notebook_calculate_tabs_allocation (notebook, &children, last_child,
                                          showarrow, STEP_NEXT,
                                          &remaining_space, &expanded_tabs, &tabs_allocation);
  if (children && children != last_child)
    {
      children = priv->children;
      gtk_notebook_calculate_tabs_allocation (notebook, &children, last_child,
                                              showarrow, STEP_PREV,
                                              &remaining_space, &expanded_tabs, &tabs_allocation);
    }

  if (!priv->first_tab)
    priv->first_tab = priv->children;

  gtk_css_gadget_queue_draw (priv->tabs_gadget);
}

static void
gtk_notebook_calc_tabs (GtkNotebook  *notebook,
                        GList        *start,
                        GList       **end,
                        gint         *tab_space,
                        guint         direction)
{
  GtkNotebookPage *page = NULL;
  GList *children;
  GList *last_calculated_child = NULL;
  GtkPositionType tab_pos = get_effective_tab_pos (notebook);

  if (!start)
    return;

  children = start;

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      while (children)
        {
          page = children->data;
          if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
              gtk_widget_get_visible (page->child))
            {
              *tab_space -= page->requisition.width;
              if (*tab_space < 0 || children == *end)
                {
                  if (*tab_space < 0)
                    {
                      *tab_space = - (*tab_space +
                                      page->requisition.width);

                      if (*tab_space == 0 && direction == STEP_PREV)
                        children = last_calculated_child;

                      *end = children;
                    }
                  return;
                }

              last_calculated_child = children;
            }
          if (direction == STEP_NEXT)
            children = children->next;
          else
            children = children->prev;
        }
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      while (children)
        {
          page = children->data;
          if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
              gtk_widget_get_visible (page->child))
            {
              *tab_space -= page->requisition.height;
              if (*tab_space < 0 || children == *end)
                {
                  if (*tab_space < 0)
                    {
                      *tab_space = - (*tab_space + page->requisition.height);

                      if (*tab_space == 0 && direction == STEP_PREV)
                        children = last_calculated_child;

                      *end = children;
                    }
                  return;
                }

              last_calculated_child = children;
            }
          if (direction == STEP_NEXT)
            children = children->next;
          else
            children = children->prev;
        }
      break;
    }
}

/* Private GtkNotebook Page Switch Methods:
 *
 * gtk_notebook_real_switch_page
 */
static void
gtk_notebook_real_switch_page (GtkNotebook     *notebook,
                               GtkWidget*       child,
                               guint            page_num)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *list = gtk_notebook_find_child (notebook, GTK_WIDGET (child));
  GtkNotebookPage *page = GTK_NOTEBOOK_PAGE (list);
  gboolean child_has_focus;

  if (priv->cur_page == page || !gtk_widget_get_visible (GTK_WIDGET (child)))
    return;

  /* save the value here, changing visibility changes focus */
  child_has_focus = priv->child_has_focus;

  if (priv->cur_page)
    {
      gtk_widget_set_child_visible (priv->cur_page->child, FALSE);
      gtk_css_gadget_remove_state (priv->cur_page->gadget, GTK_STATE_FLAG_CHECKED);
    }

  priv->cur_page = page;
  gtk_css_gadget_add_state (page->gadget, GTK_STATE_FLAG_CHECKED);
  gtk_css_gadget_set_visible (priv->header_gadget, priv->show_tabs);

  if (!priv->focus_tab ||
      priv->focus_tab->data != (gpointer) priv->cur_page)
    priv->focus_tab =
      g_list_find (priv->children, priv->cur_page);

  gtk_widget_set_child_visible (priv->cur_page->child, TRUE);

  /* If the focus was on the previous page, move it to the first
   * element on the new page, if possible, or if not, to the
   * notebook itself.
   */
  if (child_has_focus)
    {
      if (priv->cur_page->last_focus_child &&
          gtk_widget_is_ancestor (priv->cur_page->last_focus_child, priv->cur_page->child))
        gtk_widget_grab_focus (priv->cur_page->last_focus_child);
      else
        if (!gtk_widget_child_focus (priv->cur_page->child, GTK_DIR_TAB_FORWARD))
          gtk_widget_grab_focus (GTK_WIDGET (notebook));
    }

  if (priv->scrollable)
    gtk_notebook_redraw_arrows (notebook);

  gtk_widget_queue_resize (GTK_WIDGET (notebook));
  g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_PAGE]);
}

/* Private GtkNotebook Page Switch Functions:
 *
 * gtk_notebook_switch_page
 * gtk_notebook_page_select
 * gtk_notebook_switch_focus_tab
 * gtk_notebook_menu_switch_page
 */
static void
gtk_notebook_switch_page (GtkNotebook     *notebook,
                          GtkNotebookPage *page)
{
  GtkNotebookPrivate *priv = notebook->priv;
  guint page_num;

  if (priv->cur_page == page)
    return;

  page_num = g_list_index (priv->children, page);

  g_signal_emit (notebook,
                 notebook_signals[SWITCH_PAGE],
                 0,
                 page->child,
                 page_num);
}

static gint
gtk_notebook_page_select (GtkNotebook *notebook,
                          gboolean     move_focus)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GtkDirectionType dir = GTK_DIR_DOWN; /* Quiet GCC */
  GtkPositionType tab_pos = get_effective_tab_pos (notebook);

  if (!priv->focus_tab)
    return FALSE;

  page = priv->focus_tab->data;
  gtk_notebook_switch_page (notebook, page);

  if (move_focus)
    {
      switch (tab_pos)
        {
        case GTK_POS_TOP:
          dir = GTK_DIR_DOWN;
          break;
        case GTK_POS_BOTTOM:
          dir = GTK_DIR_UP;
          break;
        case GTK_POS_LEFT:
          dir = GTK_DIR_RIGHT;
          break;
        case GTK_POS_RIGHT:
          dir = GTK_DIR_LEFT;
          break;
        }

      if (gtk_widget_child_focus (page->child, dir))
        return TRUE;
    }
  return FALSE;
}

static void
gtk_notebook_switch_focus_tab (GtkNotebook *notebook,
                               GList       *new_child)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;

  if (priv->focus_tab == new_child)
    return;

  priv->focus_tab = new_child;

  if (priv->scrollable)
    gtk_notebook_redraw_arrows (notebook);

  if (!priv->show_tabs || !priv->focus_tab)
    return;

  page = priv->focus_tab->data;
  gtk_notebook_switch_page (notebook, page);
}

static void
gtk_notebook_menu_switch_page (GtkWidget       *widget,
                               GtkNotebookPage *page)
{
  GtkNotebookPrivate *priv;
  GtkNotebook *notebook;
  GtkWidget *parent;
  GList *children;
  guint page_num;

  parent = gtk_widget_get_parent (widget);
  notebook = GTK_NOTEBOOK (gtk_menu_get_attach_widget (GTK_MENU (parent)));
  priv = notebook->priv;

  if (priv->cur_page == page)
    return;

  page_num = 0;
  children = priv->children;
  while (children && children->data != page)
    {
      children = children->next;
      page_num++;
    }

  g_signal_emit (notebook,
                 notebook_signals[SWITCH_PAGE],
                 0,
                 page->child,
                 page_num);
}

/* Private GtkNotebook Menu Functions:
 *
 * gtk_notebook_menu_item_create
 * gtk_notebook_menu_item_recreate
 * gtk_notebook_menu_label_unparent
 * gtk_notebook_menu_detacher
 */
static void
gtk_notebook_menu_item_create (GtkNotebook *notebook,
                               GList       *list)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GtkWidget *menu_item;

  page = list->data;
  if (page->default_menu)
    {
      if (GTK_IS_LABEL (page->tab_label))
        page->menu_label = gtk_label_new (gtk_label_get_text (GTK_LABEL (page->tab_label)));
      else
        page->menu_label = gtk_label_new ("");
      gtk_widget_set_halign (page->menu_label, GTK_ALIGN_START);
      gtk_widget_set_valign (page->menu_label, GTK_ALIGN_CENTER);
    }

  gtk_widget_show (page->menu_label);
  menu_item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu_item), page->menu_label);
  gtk_menu_shell_insert (GTK_MENU_SHELL (priv->menu), menu_item,
                         g_list_position (priv->children, list));
  g_signal_connect (menu_item, "activate",
                    G_CALLBACK (gtk_notebook_menu_switch_page), page);
  if (gtk_widget_get_visible (page->child))
    gtk_widget_show (menu_item);
}

static void
gtk_notebook_menu_item_recreate (GtkNotebook *notebook,
                                 GList       *list)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page = list->data;
  GtkWidget *menu_item = gtk_widget_get_parent (page->menu_label);

  gtk_container_remove (GTK_CONTAINER (menu_item), page->menu_label);
  gtk_container_remove (GTK_CONTAINER (priv->menu), menu_item);
  gtk_notebook_menu_item_create (notebook, list);
}

static void
gtk_notebook_menu_label_unparent (GtkWidget *widget,
                                  gpointer  data)
{
  gtk_widget_unparent (gtk_bin_get_child (GTK_BIN (widget)));
  _gtk_bin_set_child (GTK_BIN (widget), NULL);
}

static void
gtk_notebook_menu_detacher (GtkWidget *widget,
                            GtkMenu   *menu)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  g_return_if_fail (priv->menu == (GtkWidget*) menu);

  priv->menu = NULL;
}

/* Public GtkNotebook Page Insert/Remove Methods :
 *
 * gtk_notebook_append_page
 * gtk_notebook_append_page_menu
 * gtk_notebook_prepend_page
 * gtk_notebook_prepend_page_menu
 * gtk_notebook_insert_page
 * gtk_notebook_insert_page_menu
 * gtk_notebook_remove_page
 */
/**
 * gtk_notebook_append_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page
 * @tab_label: (allow-none): the #GtkWidget to be used as the label
 *     for the page, or %NULL to use the default label, “page N”
 *
 * Appends a page to @notebook.
 *
 * Returns: the index (starting from 0) of the appended
 *     page in the notebook, or -1 if function fails
 */
gint
gtk_notebook_append_page (GtkNotebook *notebook,
                          GtkWidget   *child,
                          GtkWidget   *tab_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);

  return gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, -1);
}

/**
 * gtk_notebook_append_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page
 * @tab_label: (allow-none): the #GtkWidget to be used as the label
 *     for the page, or %NULL to use the default label, “page N”
 * @menu_label: (allow-none): the widget to use as a label for the
 *     page-switch menu, if that is enabled. If %NULL, and @tab_label
 *     is a #GtkLabel or %NULL, then the menu label will be a newly
 *     created label with the same text as @tab_label; if @tab_label
 *     is not a #GtkLabel, @menu_label must be specified if the
 *     page-switch menu is to be used.
 *
 * Appends a page to @notebook, specifying the widget to use as the
 * label in the popup menu.
 *
 * Returns: the index (starting from 0) of the appended
 *     page in the notebook, or -1 if function fails
 */
gint
gtk_notebook_append_page_menu (GtkNotebook *notebook,
                               GtkWidget   *child,
                               GtkWidget   *tab_label,
                               GtkWidget   *menu_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  g_return_val_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label), -1);

  return gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, -1);
}

/**
 * gtk_notebook_prepend_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page
 * @tab_label: (allow-none): the #GtkWidget to be used as the label
 *     for the page, or %NULL to use the default label, “page N”
 *
 * Prepends a page to @notebook.
 *
 * Returns: the index (starting from 0) of the prepended
 *     page in the notebook, or -1 if function fails
 */
gint
gtk_notebook_prepend_page (GtkNotebook *notebook,
                           GtkWidget   *child,
                           GtkWidget   *tab_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);

  return gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, 0);
}

/**
 * gtk_notebook_prepend_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page
 * @tab_label: (allow-none): the #GtkWidget to be used as the label
 *     for the page, or %NULL to use the default label, “page N”
 * @menu_label: (allow-none): the widget to use as a label for the
 *     page-switch menu, if that is enabled. If %NULL, and @tab_label
 *     is a #GtkLabel or %NULL, then the menu label will be a newly
 *     created label with the same text as @tab_label; if @tab_label
 *     is not a #GtkLabel, @menu_label must be specified if the
 *     page-switch menu is to be used.
 *
 * Prepends a page to @notebook, specifying the widget to use as the
 * label in the popup menu.
 *
 * Returns: the index (starting from 0) of the prepended
 *     page in the notebook, or -1 if function fails
 */
gint
gtk_notebook_prepend_page_menu (GtkNotebook *notebook,
                                GtkWidget   *child,
                                GtkWidget   *tab_label,
                                GtkWidget   *menu_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  g_return_val_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label), -1);

  return gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, 0);
}

/**
 * gtk_notebook_insert_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page
 * @tab_label: (allow-none): the #GtkWidget to be used as the label
 *     for the page, or %NULL to use the default label, “page N”
 * @position: the index (starting at 0) at which to insert the page,
 *     or -1 to append the page after all other pages
 *
 * Insert a page into @notebook at the given position.
 *
 * Returns: the index (starting from 0) of the inserted
 *     page in the notebook, or -1 if function fails
 */
gint
gtk_notebook_insert_page (GtkNotebook *notebook,
                          GtkWidget   *child,
                          GtkWidget   *tab_label,
                          gint         position)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);

  return gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, position);
}


static gint
gtk_notebook_page_compare_tab (gconstpointer a,
                               gconstpointer b)
{
  return (((GtkNotebookPage *) a)->tab_label != b);
}

static gboolean
gtk_notebook_mnemonic_activate_switch_page (GtkWidget *child,
                                            gboolean overload,
                                            gpointer data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (data);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *list;

  list = g_list_find_custom (priv->children, child,
                             gtk_notebook_page_compare_tab);
  if (list)
    {
      GtkNotebookPage *page = list->data;

      gtk_widget_grab_focus (GTK_WIDGET (notebook));    /* Do this first to avoid focusing new page */
      gtk_notebook_switch_page (notebook, page);
      focus_tabs_in (notebook);
    }

  return TRUE;
}

/**
 * gtk_notebook_insert_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page
 * @tab_label: (allow-none): the #GtkWidget to be used as the label
 *     for the page, or %NULL to use the default label, “page N”
 * @menu_label: (allow-none): the widget to use as a label for the
 *     page-switch menu, if that is enabled. If %NULL, and @tab_label
 *     is a #GtkLabel or %NULL, then the menu label will be a newly
 *     created label with the same text as @tab_label; if @tab_label
 *     is not a #GtkLabel, @menu_label must be specified if the
 *     page-switch menu is to be used.
 * @position: the index (starting at 0) at which to insert the page,
 *     or -1 to append the page after all other pages.
 *
 * Insert a page into @notebook at the given position, specifying
 * the widget to use as the label in the popup menu.
 *
 * Returns: the index (starting from 0) of the inserted
 *     page in the notebook
 */
gint
gtk_notebook_insert_page_menu (GtkNotebook *notebook,
                               GtkWidget   *child,
                               GtkWidget   *tab_label,
                               GtkWidget   *menu_label,
                               gint         position)
{
  GtkNotebookClass *class;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  g_return_val_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label), -1);

  class = GTK_NOTEBOOK_GET_CLASS (notebook);

  return (class->insert_page) (notebook, child, tab_label, menu_label, position);
}

/**
 * gtk_notebook_remove_page:
 * @notebook: a #GtkNotebook
 * @page_num: the index of a notebook page, starting
 *     from 0. If -1, the last page will be removed.
 *
 * Removes a page from the notebook given its index
 * in the notebook.
 */
void
gtk_notebook_remove_page (GtkNotebook *notebook,
                          gint         page_num)
{
  GtkNotebookPrivate *priv;
  GList *list = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  if (page_num >= 0)
    list = g_list_nth (priv->children, page_num);
  else
    list = g_list_last (priv->children);

  if (list)
    gtk_container_remove (GTK_CONTAINER (notebook),
                          ((GtkNotebookPage *) list->data)->child);
}

/* Public GtkNotebook Page Switch Methods :
 * gtk_notebook_get_current_page
 * gtk_notebook_page_num
 * gtk_notebook_set_current_page
 * gtk_notebook_next_page
 * gtk_notebook_prev_page
 */
/**
 * gtk_notebook_get_current_page:
 * @notebook: a #GtkNotebook
 *
 * Returns the page number of the current page.
 *
 * Returns: the index (starting from 0) of the current
 *     page in the notebook. If the notebook has no pages,
 *     then -1 will be returned.
 */
gint
gtk_notebook_get_current_page (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  priv = notebook->priv;

  if (!priv->cur_page)
    return -1;

  return g_list_index (priv->children, priv->cur_page);
}

/**
 * gtk_notebook_get_nth_page:
 * @notebook: a #GtkNotebook
 * @page_num: the index of a page in the notebook, or -1
 *     to get the last page
 *
 * Returns the child widget contained in page number @page_num.
 *
 * Returns: (nullable) (transfer none): the child widget, or %NULL if @page_num
 * is out of bounds
 */
GtkWidget*
gtk_notebook_get_nth_page (GtkNotebook *notebook,
                           gint         page_num)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  priv = notebook->priv;

  if (page_num >= 0)
    list = g_list_nth (priv->children, page_num);
  else
    list = g_list_last (priv->children);

  if (list)
    {
      page = list->data;
      return page->child;
    }

  return NULL;
}

/**
 * gtk_notebook_get_n_pages:
 * @notebook: a #GtkNotebook
 *
 * Gets the number of pages in a notebook.
 *
 * Returns: the number of pages in the notebook
 *
 * Since: 2.2
 */
gint
gtk_notebook_get_n_pages (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), 0);

  priv = notebook->priv;

  return g_list_length (priv->children);
}

/**
 * gtk_notebook_page_num:
 * @notebook: a #GtkNotebook
 * @child: a #GtkWidget
 *
 * Finds the index of the page which contains the given child
 * widget.
 *
 * Returns: the index of the page containing @child, or
 *     -1 if @child is not in the notebook
 */
gint
gtk_notebook_page_num (GtkNotebook      *notebook,
                       GtkWidget        *child)
{
  GtkNotebookPrivate *priv;
  GList *children;
  gint num;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  priv = notebook->priv;

  num = 0;
  children = priv->children;
  while (children)
    {
      GtkNotebookPage *page =  children->data;

      if (page->child == child)
        return num;

      children = children->next;
      num++;
    }

  return -1;
}

/**
 * gtk_notebook_set_current_page:
 * @notebook: a #GtkNotebook
 * @page_num: index of the page to switch to, starting from 0.
 *     If negative, the last page will be used. If greater
 *     than the number of pages in the notebook, nothing
 *     will be done.
 *
 * Switches to the page number @page_num.
 *
 * Note that due to historical reasons, GtkNotebook refuses
 * to switch to a page unless the child widget is visible.
 * Therefore, it is recommended to show child widgets before
 * adding them to a notebook.
 */
void
gtk_notebook_set_current_page (GtkNotebook *notebook,
                               gint         page_num)
{
  GtkNotebookPrivate *priv;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  if (page_num < 0)
    page_num = g_list_length (priv->children) - 1;

  list = g_list_nth (priv->children, page_num);
  if (list)
    gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list));

  g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_PAGE]);
}

/**
 * gtk_notebook_next_page:
 * @notebook: a #GtkNotebook
 *
 * Switches to the next page. Nothing happens if the current page is
 * the last page.
 */
void
gtk_notebook_next_page (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  list = g_list_find (priv->children, priv->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list));
}

/**
 * gtk_notebook_prev_page:
 * @notebook: a #GtkNotebook
 *
 * Switches to the previous page. Nothing happens if the current page
 * is the first page.
 */
void
gtk_notebook_prev_page (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  list = g_list_find (priv->children, priv->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list));
}

/* Public GtkNotebook/Tab Style Functions
 *
 * gtk_notebook_set_show_border
 * gtk_notebook_get_show_border
 * gtk_notebook_set_show_tabs
 * gtk_notebook_get_show_tabs
 * gtk_notebook_set_tab_pos
 * gtk_notebook_get_tab_pos
 * gtk_notebook_set_scrollable
 * gtk_notebook_get_scrollable
 * gtk_notebook_get_tab_hborder
 * gtk_notebook_get_tab_vborder
 */
/**
 * gtk_notebook_set_show_border:
 * @notebook: a #GtkNotebook
 * @show_border: %TRUE if a bevel should be drawn around the notebook
 *
 * Sets whether a bevel will be drawn around the notebook pages.
 * This only has a visual effect when the tabs are not shown.
 * See gtk_notebook_set_show_tabs().
 */
void
gtk_notebook_set_show_border (GtkNotebook *notebook,
                              gboolean     show_border)
{
  GtkNotebookPrivate *priv;
  GtkCssNode *node;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  if (priv->show_border != show_border)
    {
      priv->show_border = show_border;

      node = gtk_widget_get_css_node (GTK_WIDGET (notebook));
      if (show_border)
        gtk_css_node_add_class (node, g_quark_from_static_string (GTK_STYLE_CLASS_FRAME));
      else
        gtk_css_node_remove_class (node, g_quark_from_static_string (GTK_STYLE_CLASS_FRAME));

      if (gtk_widget_get_visible (GTK_WIDGET (notebook)))
        gtk_widget_queue_resize (GTK_WIDGET (notebook));

      g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_SHOW_BORDER]);
    }
}

/**
 * gtk_notebook_get_show_border:
 * @notebook: a #GtkNotebook
 *
 * Returns whether a bevel will be drawn around the notebook pages.
 * See gtk_notebook_set_show_border().
 *
 * Returns: %TRUE if the bevel is drawn
 */
gboolean
gtk_notebook_get_show_border (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->priv->show_border;
}

/**
 * gtk_notebook_set_show_tabs:
 * @notebook: a #GtkNotebook
 * @show_tabs: %TRUE if the tabs should be shown
 *
 * Sets whether to show the tabs for the notebook or not.
 */
void
gtk_notebook_set_show_tabs (GtkNotebook *notebook,
                            gboolean     show_tabs)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList *children;
  gint i;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  show_tabs = show_tabs != FALSE;

  if (priv->show_tabs == show_tabs)
    return;

  priv->show_tabs = show_tabs;
  children = priv->children;

  if (!show_tabs)
    {
      gtk_widget_set_can_focus (GTK_WIDGET (notebook), FALSE);

      while (children)
        {
          page = children->data;
          children = children->next;
          if (page->default_tab)
            {
              gtk_widget_destroy (page->tab_label);
              page->tab_label = NULL;
            }
          else
            gtk_widget_hide (page->tab_label);
        }
      gtk_css_gadget_set_visible (priv->header_gadget,
                                  gtk_notebook_has_current_page (notebook));
    }
  else
    {
      gtk_widget_set_can_focus (GTK_WIDGET (notebook), TRUE);
      gtk_notebook_update_labels (notebook);
      gtk_css_gadget_set_visible (priv->header_gadget, TRUE);
    }

  for (i = 0; i < N_ACTION_WIDGETS; i++)
    {
      if (priv->action_widget[i])
        gtk_widget_set_child_visible (priv->action_widget[i], show_tabs);
    }

  gtk_notebook_update_tab_pos (notebook);
  gtk_widget_reset_style (GTK_WIDGET (notebook));
  gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_SHOW_TABS]);
}

/**
 * gtk_notebook_get_show_tabs:
 * @notebook: a #GtkNotebook
 *
 * Returns whether the tabs of the notebook are shown.
 * See gtk_notebook_set_show_tabs().
 *
 * Returns: %TRUE if the tabs are shown
 */
gboolean
gtk_notebook_get_show_tabs (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->priv->show_tabs;
}

static void
gtk_notebook_update_tab_pos (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkPositionType tab_pos;
  const char *tab_pos_names[] = {
    GTK_STYLE_CLASS_LEFT,
    GTK_STYLE_CLASS_RIGHT,
    GTK_STYLE_CLASS_TOP,
    GTK_STYLE_CLASS_BOTTOM
  };
  gint i;

  tab_pos = get_effective_tab_pos (notebook);

  for (i = 0; i < G_N_ELEMENTS (tab_pos_names); i++)
    {
      if (tab_pos == i)
        gtk_css_gadget_add_class (priv->header_gadget, tab_pos_names[i]);
      else
        gtk_css_gadget_remove_class (priv->header_gadget, tab_pos_names[i]);
    }

  gtk_box_gadget_remove_gadget (GTK_BOX_GADGET (priv->gadget), priv->header_gadget);
  switch (tab_pos)
    {
    case GTK_POS_TOP:
      if (priv->show_tabs)
        gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->gadget), 0, priv->header_gadget, FALSE, GTK_ALIGN_FILL);
      gtk_box_gadget_set_draw_reverse (GTK_BOX_GADGET (priv->gadget), TRUE);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_VERTICAL);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->header_gadget), GTK_ORIENTATION_HORIZONTAL);
      break;

    case GTK_POS_BOTTOM:
      if (priv->show_tabs)
        gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->gadget), 1, priv->header_gadget, FALSE, GTK_ALIGN_FILL);
      gtk_box_gadget_set_draw_reverse (GTK_BOX_GADGET (priv->gadget), FALSE);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_VERTICAL);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->header_gadget), GTK_ORIENTATION_HORIZONTAL);
      break;

    case GTK_POS_LEFT:
      if (priv->show_tabs)
        gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->gadget), 0, priv->header_gadget, FALSE, GTK_ALIGN_FILL);
      gtk_box_gadget_set_draw_reverse (GTK_BOX_GADGET (priv->gadget), TRUE);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_HORIZONTAL);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->header_gadget), GTK_ORIENTATION_VERTICAL);
      break;

    case GTK_POS_RIGHT:
      if (priv->show_tabs)
        gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->gadget), 1, priv->header_gadget, FALSE, GTK_ALIGN_FILL);
      gtk_box_gadget_set_draw_reverse (GTK_BOX_GADGET (priv->gadget), FALSE);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_HORIZONTAL);
      gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->header_gadget), GTK_ORIENTATION_VERTICAL);
      break;
    }

  update_node_ordering (notebook);
}

/**
 * gtk_notebook_set_tab_pos:
 * @notebook: a #GtkNotebook.
 * @pos: the edge to draw the tabs at
 *
 * Sets the edge at which the tabs for switching pages in the
 * notebook are drawn.
 */
void
gtk_notebook_set_tab_pos (GtkNotebook     *notebook,
                          GtkPositionType  pos)
{
  GtkNotebookPrivate *priv;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  if (priv->tab_pos != pos)
    {
      priv->tab_pos = pos;
      if (gtk_widget_get_visible (GTK_WIDGET (notebook)))
        gtk_widget_queue_resize (GTK_WIDGET (notebook));

      gtk_notebook_update_tab_pos (notebook);

      g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_TAB_POS]);
    }
}

/**
 * gtk_notebook_get_tab_pos:
 * @notebook: a #GtkNotebook
 *
 * Gets the edge at which the tabs for switching pages in the
 * notebook are drawn.
 *
 * Returns: the edge at which the tabs are drawn
 */
GtkPositionType
gtk_notebook_get_tab_pos (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), GTK_POS_TOP);

  return notebook->priv->tab_pos;
}

/**
 * gtk_notebook_set_scrollable:
 * @notebook: a #GtkNotebook
 * @scrollable: %TRUE if scroll arrows should be added
 *
 * Sets whether the tab label area will have arrows for
 * scrolling if there are too many tabs to fit in the area.
 */
void
gtk_notebook_set_scrollable (GtkNotebook *notebook,
                             gboolean     scrollable)
{
  GtkNotebookPrivate *priv;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  scrollable = (scrollable != FALSE);

  if (priv->scrollable == scrollable)
    return;

  priv->scrollable = scrollable;

  update_arrow_nodes (notebook);
  update_arrow_state (notebook);

  if (gtk_widget_get_visible (GTK_WIDGET (notebook)))
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_SCROLLABLE]);
}

/**
 * gtk_notebook_get_scrollable:
 * @notebook: a #GtkNotebook
 *
 * Returns whether the tab label area has arrows for scrolling.
 * See gtk_notebook_set_scrollable().
 *
 * Returns: %TRUE if arrows for scrolling are present
 */
gboolean
gtk_notebook_get_scrollable (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->priv->scrollable;
}

/**
 * gtk_notebook_get_tab_hborder:
 * @notebook: a #GtkNotebook
 *
 * Returns the horizontal width of a tab border.
 *
 * Returns: horizontal width of a tab border
 *
 * Since: 2.22
 *
 * Deprecated: 3.4: this function returns zero
 */
guint16
gtk_notebook_get_tab_hborder (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return 0;
}

/**
 * gtk_notebook_get_tab_vborder:
 * @notebook: a #GtkNotebook
 *
 * Returns the vertical width of a tab border.
 *
 * Returns: vertical width of a tab border
 *
 * Since: 2.22
 *
 * Deprecated: 3.4: this function returns zero
 */
guint16
gtk_notebook_get_tab_vborder (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return 0;
}


/* Public GtkNotebook Popup Menu Methods:
 *
 * gtk_notebook_popup_enable
 * gtk_notebook_popup_disable
 */


/**
 * gtk_notebook_popup_enable:
 * @notebook: a #GtkNotebook
 *
 * Enables the popup menu: if the user clicks with the right
 * mouse button on the tab labels, a menu with all the pages
 * will be popped up.
 */
void
gtk_notebook_popup_enable (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  if (priv->menu)
    return;

  priv->menu = gtk_menu_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->menu),
                               GTK_STYLE_CLASS_CONTEXT_MENU);

  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    gtk_notebook_menu_item_create (notebook, list);

  gtk_notebook_update_labels (notebook);
  gtk_menu_attach_to_widget (GTK_MENU (priv->menu),
                             GTK_WIDGET (notebook),
                             gtk_notebook_menu_detacher);

  g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_ENABLE_POPUP]);
}

/**
 * gtk_notebook_popup_disable:
 * @notebook: a #GtkNotebook
 *
 * Disables the popup menu.
 */
void
gtk_notebook_popup_disable (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  if (!priv->menu)
    return;

  gtk_container_foreach (GTK_CONTAINER (priv->menu),
                         (GtkCallback) gtk_notebook_menu_label_unparent, NULL);
  gtk_widget_destroy (priv->menu);

  g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_ENABLE_POPUP]);
}

/* Public GtkNotebook Page Properties Functions:
 *
 * gtk_notebook_get_tab_label
 * gtk_notebook_set_tab_label
 * gtk_notebook_set_tab_label_text
 * gtk_notebook_get_menu_label
 * gtk_notebook_set_menu_label
 * gtk_notebook_set_menu_label_text
 * gtk_notebook_get_tab_reorderable
 * gtk_notebook_set_tab_reorderable
 * gtk_notebook_get_tab_detachable
 * gtk_notebook_set_tab_detachable
 */

/**
 * gtk_notebook_get_tab_label:
 * @notebook: a #GtkNotebook
 * @child: the page
 *
 * Returns the tab label widget for the page @child.
 * %NULL is returned if @child is not in @notebook or
 * if no tab label has specifically been set for @child.
 *
 * Returns: (transfer none) (nullable): the tab label
 */
GtkWidget *
gtk_notebook_get_tab_label (GtkNotebook *notebook,
                            GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  list = gtk_notebook_find_child (notebook, child);
  g_return_val_if_fail (list != NULL, NULL);

  if (GTK_NOTEBOOK_PAGE (list)->default_tab)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->tab_label;
}

/**
 * gtk_notebook_set_tab_label:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @tab_label: (allow-none): the tab label widget to use, or %NULL
 *     for default tab label
 *
 * Changes the tab label for @child.
 * If %NULL is specified for @tab_label, then the page will
 * have the label “page N”.
 */
void
gtk_notebook_set_tab_label (GtkNotebook *notebook,
                            GtkWidget   *child,
                            GtkWidget   *tab_label)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = notebook->priv;

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  /* a NULL pointer indicates a default_tab setting, otherwise
   * we need to set the associated label
   */
  page = list->data;

  if (page->tab_label == tab_label)
    return;

  gtk_notebook_remove_tab_label (notebook, page);

  if (tab_label)
    {
      page->default_tab = FALSE;
      page->tab_label = tab_label;
      gtk_css_node_set_parent (gtk_widget_get_css_node (page->tab_label),
                               gtk_css_gadget_get_node (page->gadget));
      gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
    }
  else
    {
      page->default_tab = TRUE;
      page->tab_label = NULL;

      if (priv->show_tabs)
        {
          gchar string[32];

          g_snprintf (string, sizeof(string), _("Page %u"),
                      g_list_position (priv->children, list));
          page->tab_label = gtk_label_new (string);
          gtk_css_node_set_parent (gtk_widget_get_css_node (page->tab_label),
                                   gtk_css_gadget_get_node (page->gadget));
          gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
        }
    }

  if (page->tab_label)
    page->mnemonic_activate_signal =
      g_signal_connect (page->tab_label,
                        "mnemonic-activate",
                        G_CALLBACK (gtk_notebook_mnemonic_activate_switch_page),
                        notebook);

  if (priv->show_tabs && gtk_widget_get_visible (child))
    {
      gtk_widget_show (page->tab_label);
      gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }

  if (priv->menu)
    gtk_notebook_menu_item_recreate (notebook, list);

  gtk_widget_child_notify (child, "tab-label");
}

/**
 * gtk_notebook_set_tab_label_text:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @tab_text: the label text
 *
 * Creates a new label and sets it as the tab label for the page
 * containing @child.
 */
void
gtk_notebook_set_tab_label_text (GtkNotebook *notebook,
                                 GtkWidget   *child,
                                 const gchar *tab_text)
{
  GtkWidget *tab_label = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (tab_text)
    tab_label = gtk_label_new (tab_text);
  gtk_notebook_set_tab_label (notebook, child, tab_label);
}

/**
 * gtk_notebook_get_tab_label_text:
 * @notebook: a #GtkNotebook
 * @child: a widget contained in a page of @notebook
 *
 * Retrieves the text of the tab label for the page containing
 * @child.
 *
 * Returns: (nullable): the text of the tab label, or %NULL if the tab label
 * widget is not a #GtkLabel. The string is owned by the widget and must not be
 * freed.
 */
const gchar *
gtk_notebook_get_tab_label_text (GtkNotebook *notebook,
                                 GtkWidget   *child)
{
  GtkWidget *tab_label;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  tab_label = gtk_notebook_get_tab_label (notebook, child);

  if (GTK_IS_LABEL (tab_label))
    return gtk_label_get_text (GTK_LABEL (tab_label));
  else
    return NULL;
}

/**
 * gtk_notebook_get_menu_label:
 * @notebook: a #GtkNotebook
 * @child: a widget contained in a page of @notebook
 *
 * Retrieves the menu label widget of the page containing @child.
 *
 * Returns: (nullable) (transfer none): the menu label, or %NULL if the
 * notebook page does not have a menu label other than the default (the tab
 * label).
 */
GtkWidget*
gtk_notebook_get_menu_label (GtkNotebook *notebook,
                             GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  list = gtk_notebook_find_child (notebook, child);
  g_return_val_if_fail (list != NULL, NULL);

  if (GTK_NOTEBOOK_PAGE (list)->default_menu)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->menu_label;
}

/**
 * gtk_notebook_set_menu_label:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @menu_label: (allow-none): the menu label, or %NULL for default
 *
 * Changes the menu label for the page containing @child.
 */
void
gtk_notebook_set_menu_label (GtkNotebook *notebook,
                             GtkWidget   *child,
                             GtkWidget   *menu_label)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = notebook->priv;

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  page = list->data;
  if (page->menu_label)
    {
      if (priv->menu)
        gtk_container_remove (GTK_CONTAINER (priv->menu),
                              gtk_widget_get_parent (page->menu_label));

      if (!page->default_menu)
        g_object_unref (page->menu_label);
    }

  if (menu_label)
    {
      page->menu_label = menu_label;
      g_object_ref_sink (page->menu_label);
      page->default_menu = FALSE;
    }
  else
    page->default_menu = TRUE;

  if (priv->menu)
    gtk_notebook_menu_item_create (notebook, list);
  gtk_widget_child_notify (child, "menu-label");
}

/**
 * gtk_notebook_set_menu_label_text:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @menu_text: the label text
 *
 * Creates a new label and sets it as the menu label of @child.
 */
void
gtk_notebook_set_menu_label_text (GtkNotebook *notebook,
                                  GtkWidget   *child,
                                  const gchar *menu_text)
{
  GtkWidget *menu_label = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (menu_text)
    {
      menu_label = gtk_label_new (menu_text);
      gtk_widget_set_halign (menu_label, GTK_ALIGN_START);
      gtk_widget_set_valign (menu_label, GTK_ALIGN_CENTER);
    }
  gtk_notebook_set_menu_label (notebook, child, menu_label);
  gtk_widget_child_notify (child, "menu-label");
}

/**
 * gtk_notebook_get_menu_label_text:
 * @notebook: a #GtkNotebook
 * @child: the child widget of a page of the notebook.
 *
 * Retrieves the text of the menu label for the page containing
 * @child.
 *
 * Returns: (nullable): the text of the tab label, or %NULL if the widget does
 * not have a menu label other than the default menu label, or the menu label
 * widget is not a #GtkLabel. The string is owned by the widget and must not be
 * freed.
 */
const gchar *
gtk_notebook_get_menu_label_text (GtkNotebook *notebook,
                                  GtkWidget *child)
{
  GtkWidget *menu_label;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  menu_label = gtk_notebook_get_menu_label (notebook, child);

  if (GTK_IS_LABEL (menu_label))
    return gtk_label_get_text (GTK_LABEL (menu_label));
  else
    return NULL;
}

/* Helper function called when pages are reordered
 */
static void
gtk_notebook_child_reordered (GtkNotebook     *notebook,
                              GtkNotebookPage *page)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *list;
  GtkCssNode *sibling;

  list = g_list_find (priv->children, page);

  if (priv->menu)
    gtk_notebook_menu_item_recreate (notebook, list);

  if (list->prev)
    sibling = gtk_css_gadget_get_node (GTK_NOTEBOOK_PAGE (list->prev)->gadget);
  else if (priv->arrow_gadget[ARROW_RIGHT_BEFORE])
    sibling = gtk_css_gadget_get_node (priv->arrow_gadget[ARROW_RIGHT_BEFORE]);
  else if (priv->arrow_gadget[ARROW_LEFT_BEFORE])
    sibling = gtk_css_gadget_get_node (priv->arrow_gadget[ARROW_LEFT_BEFORE]);
  else
    sibling = NULL;

  gtk_css_node_insert_after (gtk_css_gadget_get_node (priv->tabs_gadget),
                             gtk_css_gadget_get_node (page->gadget),
                             sibling);
  gtk_notebook_update_labels (notebook);
  gtk_css_gadget_queue_allocate (priv->tabs_gadget);
}

static void
gtk_notebook_set_tab_label_packing (GtkNotebook *notebook,
                                    GtkWidget   *child,
                                    gboolean     expand,
                                    gboolean     fill)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = notebook->priv;

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  page = list->data;
  expand = expand != FALSE;
  fill = fill != FALSE;
  if (page->expand == expand && page->fill == fill)
    return;

  gtk_widget_freeze_child_notify (child);
  page->expand = expand;
  gtk_widget_child_notify (child, "tab-expand");
  page->fill = fill;
  gtk_widget_child_notify (child, "tab-fill");
  gtk_widget_child_notify (child, "position");
  if (priv->show_tabs)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
  gtk_widget_thaw_child_notify (child);
}

static void
gtk_notebook_query_tab_label_packing (GtkNotebook *notebook,
                                      GtkWidget   *child,
                                      gboolean    *expand,
                                      gboolean    *fill)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  if (expand)
    *expand = GTK_NOTEBOOK_PAGE (list)->expand;
  if (fill)
    *fill = GTK_NOTEBOOK_PAGE (list)->fill;
}

/**
 * gtk_notebook_reorder_child:
 * @notebook: a #GtkNotebook
 * @child: the child to move
 * @position: the new position, or -1 to move to the end
 *
 * Reorders the page containing @child, so that it appears in position
 * @position. If @position is greater than or equal to the number of
 * children in the list or negative, @child will be moved to the end
 * of the list.
 */
void
gtk_notebook_reorder_child (GtkNotebook *notebook,
                            GtkWidget   *child,
                            gint         position)
{
  GtkNotebookPrivate *priv;
  GList *list, *new_list;
  GtkNotebookPage *page;
  gint old_pos;
  gint max_pos;
  gint i;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = notebook->priv;

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  max_pos = g_list_length (priv->children) - 1;
  if (position < 0 || position > max_pos)
    position = max_pos;

  old_pos = g_list_position (priv->children, list);

  if (old_pos == position)
    return;

  page = list->data;
  priv->children = g_list_delete_link (priv->children, list);

  priv->children = g_list_insert (priv->children, page, position);
  new_list = g_list_nth (priv->children, position);

  /* Fix up GList references in GtkNotebook structure */
  if (priv->first_tab == list)
    priv->first_tab = new_list;
  if (priv->focus_tab == list)
    priv->focus_tab = new_list;

  gtk_widget_freeze_child_notify (child);

  /* Move around the menu items if necessary */
  gtk_notebook_child_reordered (notebook, page);

  for (list = priv->children, i = 0; list; list = list->next, i++)
    {
      if (MIN (old_pos, position) <= i && i <= MAX (old_pos, position))
	gtk_widget_child_notify (((GtkNotebookPage *) list->data)->child, "position");
    }

  gtk_widget_thaw_child_notify (child);

  g_signal_emit (notebook,
                 notebook_signals[PAGE_REORDERED],
                 0,
                 child,
                 position);
}

/**
 * gtk_notebook_set_group_name:
 * @notebook: a #GtkNotebook
 * @group_name: (allow-none): the name of the notebook group,
 *     or %NULL to unset it
 *
 * Sets a group name for @notebook.
 *
 * Notebooks with the same name will be able to exchange tabs
 * via drag and drop. A notebook with a %NULL group name will
 * not be able to exchange tabs with any other notebook.
 *
 * Since: 2.24
 */
void
gtk_notebook_set_group_name (GtkNotebook *notebook,
                             const gchar *group_name)
{
  GtkNotebookPrivate *priv;
  GQuark group;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  group = g_quark_from_string (group_name);

  if (priv->group != group)
    {
      priv->group = group;

      g_object_notify_by_pspec (G_OBJECT (notebook), properties[PROP_GROUP_NAME]);
    }
}

/**
 * gtk_notebook_get_group_name:
 * @notebook: a #GtkNotebook
 *
 * Gets the current group name for @notebook.
 *
 * Returns: (nullable) (transfer none): the group name, or %NULL if none is set
 *
 * Since: 2.24
 */
const gchar *
gtk_notebook_get_group_name (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  return g_quark_to_string (notebook->priv->group);
}

/**
 * gtk_notebook_get_tab_reorderable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 *
 * Gets whether the tab can be reordered via drag and drop or not.
 *
 * Returns: %TRUE if the tab is reorderable.
 *
 * Since: 2.10
 */
gboolean
gtk_notebook_get_tab_reorderable (GtkNotebook *notebook,
                                  GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  list = gtk_notebook_find_child (notebook, child);
  g_return_val_if_fail (list != NULL, FALSE);

  return GTK_NOTEBOOK_PAGE (list)->reorderable;
}

/**
 * gtk_notebook_set_tab_reorderable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 * @reorderable: whether the tab is reorderable or not
 *
 * Sets whether the notebook tab can be reordered
 * via drag and drop or not.
 *
 * Since: 2.10
 */
void
gtk_notebook_set_tab_reorderable (GtkNotebook *notebook,
                                  GtkWidget   *child,
                                  gboolean     reorderable)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  page = GTK_NOTEBOOK_PAGE (list);
  reorderable = reorderable != FALSE;

  if (page->reorderable != reorderable)
    {
      page->reorderable = reorderable;
      if (reorderable)
        gtk_css_gadget_add_class (page->gadget, "reorderable-page");
      else
        gtk_css_gadget_remove_class (page->gadget, "reorderable-page");
      gtk_widget_child_notify (child, "reorderable");
    }
}

/**
 * gtk_notebook_get_tab_detachable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 *
 * Returns whether the tab contents can be detached from @notebook.
 *
 * Returns: %TRUE if the tab is detachable.
 *
 * Since: 2.10
 */
gboolean
gtk_notebook_get_tab_detachable (GtkNotebook *notebook,
                                 GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  list = gtk_notebook_find_child (notebook, child);
  g_return_val_if_fail (list != NULL, FALSE);

  return GTK_NOTEBOOK_PAGE (list)->detachable;
}

/**
 * gtk_notebook_set_tab_detachable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 * @detachable: whether the tab is detachable or not
 *
 * Sets whether the tab can be detached from @notebook to another
 * notebook or widget.
 *
 * Note that 2 notebooks must share a common group identificator
 * (see gtk_notebook_set_group_name()) to allow automatic tabs
 * interchange between them.
 *
 * If you want a widget to interact with a notebook through DnD
 * (i.e.: accept dragged tabs from it) it must be set as a drop
 * destination and accept the target “GTK_NOTEBOOK_TAB”. The notebook
 * will fill the selection with a GtkWidget** pointing to the child
 * widget that corresponds to the dropped tab.
 *
 * Note that you should use gtk_notebook_detach_tab() instead
 * of gtk_container_remove() if you want to remove the tab from
 * the source notebook as part of accepting a drop. Otherwise,
 * the source notebook will think that the dragged tab was
 * removed from underneath the ongoing drag operation, and
 * will initiate a drag cancel animation.
 *
 * |[<!-- language="C" -->
 *  static void
 *  on_drag_data_received (GtkWidget        *widget,
 *                         GdkDragContext   *context,
 *                         gint              x,
 *                         gint              y,
 *                         GtkSelectionData *data,
 *                         guint             info,
 *                         guint             time,
 *                         gpointer          user_data)
 *  {
 *    GtkWidget *notebook;
 *    GtkWidget **child;
 *
 *    notebook = gtk_drag_get_source_widget (context);
 *    child = (void*) gtk_selection_data_get_data (data);
 *
 *    // process_widget (*child);
 *
 *    gtk_notebook_detach_tab (GTK_NOTEBOOK (notebook), *child);
 *  }
 * ]|
 *
 * If you want a notebook to accept drags from other widgets,
 * you will have to set your own DnD code to do it.
 *
 * Since: 2.10
 */
void
gtk_notebook_set_tab_detachable (GtkNotebook *notebook,
                                 GtkWidget  *child,
                                 gboolean    detachable)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  detachable = detachable != FALSE;

  if (GTK_NOTEBOOK_PAGE (list)->detachable != detachable)
    {
      GTK_NOTEBOOK_PAGE (list)->detachable = detachable;
      gtk_widget_child_notify (child, "detachable");
    }
}

/**
 * gtk_notebook_get_action_widget:
 * @notebook: a #GtkNotebook
 * @pack_type: pack type of the action widget to receive
 *
 * Gets one of the action widgets. See gtk_notebook_set_action_widget().
 *
 * Returns: (nullable) (transfer none): The action widget with the given
 * @pack_type or %NULL when this action widget has not been set
 *
 * Since: 2.20
 */
GtkWidget*
gtk_notebook_get_action_widget (GtkNotebook *notebook,
                                GtkPackType  pack_type)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  return notebook->priv->action_widget[pack_type];
}

/**
 * gtk_notebook_set_action_widget:
 * @notebook: a #GtkNotebook
 * @widget: a #GtkWidget
 * @pack_type: pack type of the action widget
 *
 * Sets @widget as one of the action widgets. Depending on the pack type
 * the widget will be placed before or after the tabs. You can use
 * a #GtkBox if you need to pack more than one widget on the same side.
 *
 * Note that action widgets are “internal” children of the notebook and thus
 * not included in the list returned from gtk_container_foreach().
 *
 * Since: 2.20
 */
void
gtk_notebook_set_action_widget (GtkNotebook *notebook,
                                GtkWidget   *widget,
                                GtkPackType  pack_type)
{
  GtkNotebookPrivate *priv;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));
  g_return_if_fail (!widget || gtk_widget_get_parent (widget) == NULL);

  priv = notebook->priv;

  if (priv->action_widget[pack_type])
    {
      gtk_box_gadget_remove_widget (GTK_BOX_GADGET (priv->header_gadget),
                                    priv->action_widget[pack_type]);
      gtk_widget_unparent (priv->action_widget[pack_type]);
    }

  priv->action_widget[pack_type] = widget;

  if (widget)
    {
      int pos;

      gtk_css_node_set_parent (gtk_widget_get_css_node (widget),
                               gtk_css_gadget_get_node (priv->header_gadget));

      if (priv->tabs_reversed)
        pos = pack_type == GTK_PACK_START ? -1 : 0;
      else
        pos = pack_type == GTK_PACK_START ? 0 : -1;

      gtk_box_gadget_insert_widget (GTK_BOX_GADGET (priv->header_gadget), pos, widget);
      gtk_widget_set_child_visible (widget, priv->show_tabs);
      gtk_widget_set_parent (widget, GTK_WIDGET (notebook));
    }

  gtk_widget_queue_resize (GTK_WIDGET (notebook));
}
