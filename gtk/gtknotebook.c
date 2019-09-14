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

#include "gtknotebook.h"

#include "gtkbindings.h"
#include "gtkbox.h"
#include "gtkboxlayout.h"
#include "gtkbuildable.h"
#include "gtkbutton.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkdnd.h"
#include "gtkdragdest.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtkgizmoprivate.h"
#include "gtkiconprivate.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkpopovermenu.h"
#include "gtkorientable.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkprivate.h"
#include "gtkstack.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtknotebookaccessible.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

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
 * “type” attribute of a <child> element. Note that the content
 * of the tab must be created before the tab can be filled.
 * A tab child can be specified without specifying a <child>
 * type attribute.
 *
 * To add a child widget in the notebooks action area, specify
 * "action-start" or “action-end” as the “type” attribute of the
 * <child> element.
 *
 * An example of a UI definition fragment with GtkNotebook:
 * |[
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
 * The nodes are always arranged from left-to-right, regardless of text direction.
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

#define GTK_NOTEBOOK_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_NOTEBOOK, GtkNotebookClass))
#define GTK_NOTEBOOK_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_NOTEBOOK, GtkNotebookClass))

typedef struct _GtkNotebookPrivate       GtkNotebookPrivate;
typedef struct _GtkNotebookClass         GtkNotebookClass;

struct _GtkNotebook
{
  GtkContainer container;

  GtkNotebookPrivate *priv;
};

struct _GtkNotebookClass
{
  GtkContainerClass parent_class;

  void (* switch_page)       (GtkNotebook     *notebook,
                              GtkWidget       *page,
                              guint            page_num);

  /* Action signals for keybindings */
  gboolean (* select_page)     (GtkNotebook       *notebook,
                                gboolean           move_focus);
  gboolean (* focus_tab)       (GtkNotebook       *notebook,
                                GtkNotebookTab     type);
  gboolean (* change_current_page) (GtkNotebook   *notebook,
                                gint               offset);
  void (* move_focus_out)      (GtkNotebook       *notebook,
                                GtkDirectionType   direction);
  gboolean (* reorder_tab)     (GtkNotebook       *notebook,
                                GtkDirectionType   direction,
                                gboolean           move_to_last);
  /* More vfuncs */
  gint (* insert_page)         (GtkNotebook       *notebook,
                                GtkWidget         *child,
                                GtkWidget         *tab_label,
                                GtkWidget         *menu_label,
                                gint               position);

  GtkNotebook * (* create_window) (GtkNotebook    *notebook,
                                   GtkWidget      *page);

  void (* page_reordered)      (GtkNotebook     *notebook,
                                GtkWidget       *child,
                                guint            page_num);

  void (* page_removed)        (GtkNotebook     *notebook,
                                GtkWidget       *child,
                                guint            page_num);

  void (* page_added)          (GtkNotebook     *notebook,
                                GtkWidget       *child,
                                guint            page_num);
};

struct _GtkNotebookPrivate
{
  GtkNotebookDragOperation   operation;
  GtkNotebookPage           *cur_page;
  GtkNotebookPage           *detached_tab;
  GdkContentFormats             *source_targets;
  GtkWidget                 *action_widget[N_ACTION_WIDGETS];
  GtkWidget                 *dnd_child;
  GtkWidget                 *menu;
  GtkWidget                 *menu_box;

  GtkWidget                 *stack_widget;
  GtkWidget                 *header_widget;
  GtkWidget                 *tabs_widget;
  GtkWidget                 *arrow_widget[4];

  GListModel    *pages;

  GList         *children;
  GList         *first_tab;             /* The first tab visible (for scrolling notebooks) */
  GList         *focus_tab;

  gint           drag_begin_x;
  gint           drag_begin_y;
  gint           drag_offset_x;
  gint           drag_offset_y;
  gint           drag_surface_x;
  gint           drag_surface_y;
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
  guint          need_timer         : 1;
  guint          show_border        : 1;
  guint          show_tabs          : 1;
  guint          scrollable         : 1;
  guint          tab_pos            : 2;
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
  PROP_PAGES,
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
  CHILD_PROP_DETACHABLE,
  CHILD_PROP_CHILD,
  CHILD_PROP_TAB,
  CHILD_PROP_MENU,
};

#define GTK_NOTEBOOK_PAGE_FROM_LIST(_glist_)         ((GtkNotebookPage *)(_glist_)->data)

#define NOTEBOOK_IS_TAB_LABEL_PARENT(_notebook_,_page_) \
  (g_object_get_data (G_OBJECT ((_page_)->tab_label), "notebook") == _notebook_)

struct _GtkNotebookPage
{
  GObject instance;

  GtkWidget *child;
  GtkWidget *tab_label;
  GtkWidget *menu_label;
  GtkWidget *last_focus_child;  /* Last descendant of the page that had focus */

  GtkWidget *tab_widget;        /* widget used for the tab itself */

  char *tab_text;
  char *menu_text;

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

typedef struct _GtkNotebookPageClass GtkNotebookPageClass;
struct _GtkNotebookPageClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkNotebookPage, gtk_notebook_page, G_TYPE_OBJECT)

static void
gtk_notebook_page_init (GtkNotebookPage *page)
{
  page->default_tab = TRUE;
  page->default_menu = TRUE;
  page->fill = TRUE;
}

static void
gtk_notebook_page_finalize (GObject *object)
{
  GtkNotebookPage *page = GTK_NOTEBOOK_PAGE (object);

  g_clear_object (&page->child);
  g_clear_object (&page->tab_label);
  g_clear_object (&page->menu_label);

  g_free (page->tab_text);
  g_free (page->menu_text);

  G_OBJECT_CLASS (gtk_notebook_page_parent_class)->finalize (object);
}


static void
gtk_notebook_page_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkNotebookPage *page = GTK_NOTEBOOK_PAGE (object);

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_set_object (&page->child, g_value_get_object (value));
      break;

    case CHILD_PROP_TAB:
      g_set_object (&page->tab_label, g_value_get_object (value));
      page->default_tab = page->tab_label == NULL;
      break;

    case CHILD_PROP_MENU:
      g_set_object (&page->menu_label, g_value_get_object (value));
      page->default_menu = page->menu_label == NULL;
      break;

    case CHILD_PROP_TAB_LABEL:
      g_free (page->tab_text);
      page->tab_text = g_value_dup_string (value);
      if (page->default_tab && GTK_IS_LABEL (page->tab_label))
        gtk_label_set_label (GTK_LABEL (page->tab_label), page->tab_text);
      break;

    case CHILD_PROP_MENU_LABEL:
      g_free (page->menu_text);
      page->menu_text = g_value_dup_string (value);
      if (page->default_menu && GTK_IS_LABEL (page->menu_label))
        gtk_label_set_label (GTK_LABEL (page->menu_label), page->menu_text);
      break;

    case CHILD_PROP_POSITION:
      {
        GtkNotebook *notebook = NULL;
        if (page->tab_widget)
          notebook = GTK_NOTEBOOK (g_object_get_data (G_OBJECT (page->tab_widget), "notebook"));

        if (notebook)
          gtk_notebook_reorder_child (notebook, page->child, g_value_get_int (value));
      }
      break;

    case CHILD_PROP_TAB_EXPAND:
      if (page->expand != g_value_get_boolean (value))
        {
          page->expand = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case CHILD_PROP_TAB_FILL:
      if (page->fill != g_value_get_boolean (value))
        {
          page->fill = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case CHILD_PROP_REORDERABLE:
      if (page->reorderable != g_value_get_boolean (value))
        {
          page->reorderable = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case CHILD_PROP_DETACHABLE:
      if (page->detachable != g_value_get_boolean (value))
        {
          page->detachable = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_page_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkNotebookPage *page = GTK_NOTEBOOK_PAGE (object);

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_value_set_object (value, page->child);
      break;

    case CHILD_PROP_TAB:
      g_value_set_object (value, page->tab_label);
      break;

    case CHILD_PROP_MENU:
      g_value_set_object (value, page->menu_label);
      break;

    case CHILD_PROP_TAB_LABEL:
      g_value_set_string (value, page->tab_text);
      break;

    case CHILD_PROP_MENU_LABEL:
      g_value_set_string (value, page->menu_text);
      break;

    case CHILD_PROP_POSITION:
      {
        GtkNotebook *notebook = NULL;
        if (page->tab_widget)
          notebook = GTK_NOTEBOOK (g_object_get_data (G_OBJECT (page->tab_widget), "notebook"));

        if (notebook)
          g_value_set_int (value, g_list_index (notebook->priv->children, page));
      }
      break;

    case CHILD_PROP_TAB_EXPAND:
        g_value_set_boolean (value, page->expand);
      break;

    case CHILD_PROP_TAB_FILL:
        g_value_set_boolean (value, page->fill);
      break;

    case CHILD_PROP_REORDERABLE:
      g_value_set_boolean (value, page->reorderable);
      break;

    case CHILD_PROP_DETACHABLE:
      g_value_set_boolean (value, page->detachable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_page_class_init (GtkNotebookPageClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_notebook_page_finalize;
  object_class->get_property = gtk_notebook_page_get_property;
  object_class->set_property = gtk_notebook_page_set_property;

  g_object_class_install_property (object_class,
                                   CHILD_PROP_CHILD,
                                   g_param_spec_object ("child",
                                                        P_("Child"),
                                                        P_("The child for this page"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_TAB,
                                   g_param_spec_object ("tab",
                                                        P_("Tab"),
                                                        P_("The tab widget for this page"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        P_("Menu"),
                                                        P_("The label widget displayed in the child’s menu entry"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_TAB_LABEL,
                                   g_param_spec_string ("tab-label",
                                                        P_("Tab label"),
                                                        P_("The text of the tab widget"),
                                                        NULL,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_MENU_LABEL,
                                   g_param_spec_string ("menu-label",
                                                        P_("Menu label"),
                                                        P_("The text of the menu widget"),
                                                        NULL,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_POSITION,
                                   g_param_spec_int ("position",
                                                     P_("Position"),
                                                     P_("The index of the child in the parent"),
                                                     -1, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_TAB_EXPAND,
                                   g_param_spec_boolean ("tab-expand",
                                                         P_("Tab expand"),
                                                         P_("Whether to expand the child’s tab"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_TAB_FILL,
                                   g_param_spec_boolean ("tab-fill",
                                                         P_("Tab fill"),
                                                         P_("Whether the child’s tab should fill the allocated area"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
                                                         P_("Tab reorderable"),
                                                         P_("Whether the tab is reorderable by user action"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
                                   CHILD_PROP_DETACHABLE,
                                   g_param_spec_boolean ("detachable",
                                                         P_("Tab detachable"),
                                                         P_("Whether the tab is detachable"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

}

static const char *src_notebook_targets [] = {
  "GTK_NOTEBOOK_TAB",
  "application/x-rootwindow-drop"
};

static const char *dst_notebook_targets [] = {
  "GTK_NOTEBOOK_TAB"
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
static void gtk_notebook_dispose             (GObject         *object);

/*** GtkWidget Methods ***/
static void gtk_notebook_destroy             (GtkWidget        *widget);
static void gtk_notebook_unmap               (GtkWidget        *widget);
static gboolean gtk_notebook_popup_menu      (GtkWidget        *widget);
static void gtk_notebook_motion              (GtkEventController *controller,
                                              double              x,
                                              double              y,
                                              gpointer            user_data);
static void gtk_notebook_grab_notify         (GtkWidget          *widget,
                                              gboolean            was_grabbed);
static void gtk_notebook_state_flags_changed (GtkWidget          *widget,
                                              GtkStateFlags       previous_state);
static gboolean gtk_notebook_focus           (GtkWidget        *widget,
                                              GtkDirectionType  direction);

/*** Drag and drop Methods ***/
static void gtk_notebook_drag_begin          (GtkWidget        *widget,
                                              GdkDrag          *drag);
static void gtk_notebook_drag_end            (GtkWidget        *widget,
                                              GdkDrag          *drag);
static gboolean gtk_notebook_drag_failed     (GtkWidget        *widget,
                                              GdkDrag          *drag,
                                              GtkDragResult     result);
static gboolean gtk_notebook_drag_motion     (GtkWidget        *widget,
                                              GdkDrop          *drop,
                                              gint              x,
                                              gint              y);
static void gtk_notebook_drag_leave          (GtkWidget        *widget,
                                              GdkDrop          *drop);
static gboolean gtk_notebook_drag_drop       (GtkWidget        *widget,
                                              GdkDrop          *drop,
                                              gint              x,
                                              gint              y);
static void gtk_notebook_drag_data_get       (GtkWidget        *widget,
                                              GdkDrag          *drag,
                                              GtkSelectionData *data);
static void gtk_notebook_drag_data_received  (GtkWidget        *widget,
                                              GdkDrop          *drop,
                                              GtkSelectionData *data);

/*** GtkContainer Methods ***/
static void gtk_notebook_add                 (GtkContainer     *container,
                                              GtkWidget        *widget);
static void gtk_notebook_remove              (GtkContainer     *container,
                                              GtkWidget        *widget);
static void gtk_notebook_set_focus_child     (GtkContainer     *container,
                                              GtkWidget        *child);
static GType gtk_notebook_child_type       (GtkContainer     *container);
static void gtk_notebook_forall              (GtkContainer     *container,
                                              GtkCallback       callback,
                                              gpointer          callback_data);

/*** GtkNotebook Methods ***/
static gint gtk_notebook_real_insert_page    (GtkNotebook      *notebook,
                                              GtkWidget        *child,
                                              GtkWidget        *tab_label,
                                              GtkWidget        *menu_label,
                                              gint              position);

static GtkNotebook *gtk_notebook_create_window (GtkNotebook    *notebook,
                                                GtkWidget      *page);

static void gtk_notebook_measure_tabs        (GtkGizmo         *gizmo,
                                              GtkOrientation    orientation,
                                              gint              for_size,
                                              gint             *minimum,
                                              gint             *natural,
                                              gint             *minimum_baseline,
                                              gint             *natural_baseline);
static void gtk_notebook_allocate_tabs       (GtkGizmo         *gizmo,
                                              int               width,
                                              int               height,
                                              int               baseline);
static void     gtk_notebook_snapshot_tabs   (GtkGizmo         *gizmo,
                                              GtkSnapshot      *snapshot);

/*** GtkNotebook Private Functions ***/
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
static gint gtk_notebook_insert_notebook_page (GtkNotebook     *notebook,
                                               GtkNotebookPage *page,
                                               int              position);

/*** GtkNotebook Size Allocate Functions ***/
static void gtk_notebook_pages_allocate      (GtkNotebook      *notebook,
                                              int               width,
                                              int               height);
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
                                              GtkNotebookPage  *page);
static void gtk_notebook_menu_item_recreate  (GtkNotebook      *notebook,
                                              GList            *list);
static void gtk_notebook_menu_label_unparent (GtkWidget        *widget,
                                              gpointer          data);

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
                            GtkWidget   *child);

/* GtkBuildable */
static void gtk_notebook_buildable_init           (GtkBuildableIface *iface);
static void gtk_notebook_buildable_add_child      (GtkBuildable *buildable,
                                                   GtkBuilder   *builder,
                                                   GObject      *child,
                                                   const gchar  *type);

static void gtk_notebook_gesture_pressed (GtkGestureClick *gesture,
                                          int                   n_press,
                                          double                x,
                                          double                y,
                                          gpointer              user_data);
static void gtk_notebook_gesture_released (GtkGestureClick *gesture,
                                           int                   n_press,
                                           double                x,
                                           double                y,
                                           gpointer              user_data);


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
  gobject_class->dispose = gtk_notebook_dispose;

  widget_class->destroy = gtk_notebook_destroy;
  widget_class->unmap = gtk_notebook_unmap;
  widget_class->popup_menu = gtk_notebook_popup_menu;
  widget_class->grab_notify = gtk_notebook_grab_notify;
  widget_class->state_flags_changed = gtk_notebook_state_flags_changed;
  widget_class->focus = gtk_notebook_focus;
  widget_class->drag_begin = gtk_notebook_drag_begin;
  widget_class->drag_end = gtk_notebook_drag_end;
  widget_class->drag_motion = gtk_notebook_drag_motion;
  widget_class->drag_leave = gtk_notebook_drag_leave;
  widget_class->drag_drop = gtk_notebook_drag_drop;
  widget_class->drag_data_get = gtk_notebook_drag_data_get;
  widget_class->drag_data_received = gtk_notebook_drag_data_received;
  widget_class->drag_failed = gtk_notebook_drag_failed;
  widget_class->compute_expand = gtk_notebook_compute_expand;

  container_class->add = gtk_notebook_add;
  container_class->remove = gtk_notebook_remove;
  container_class->forall = gtk_notebook_forall;
  container_class->set_focus_child = gtk_notebook_set_focus_child;
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
   */
  properties[PROP_GROUP_NAME] =
      g_param_spec_string ("group-name",
                           P_("Group Name"),
                           P_("Group name for tab drag and drop"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_PAGES] = 
      g_param_spec_object ("pages",
                           P_("Pages"),
                           P_("The pages of the notebook."),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, LAST_PROP, properties);

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
   */
  notebook_signals[CREATE_WINDOW] =
    g_signal_new (I_("create-window"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNotebookClass, create_window),
                  gtk_object_handled_accumulator, NULL,
                  _gtk_marshal_OBJECT__OBJECT,
                  GTK_TYPE_NOTEBOOK, 1,
                  GTK_TYPE_WIDGET);
  g_signal_set_va_marshaller (notebook_signals[CREATE_WINDOW],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_OBJECT__OBJECTv);

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

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_NOTEBOOK_ACCESSIBLE);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("notebook"));
}

static void
gtk_notebook_init (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;
  GdkContentFormats *targets;
  GtkEventController *controller;
  GtkGesture *gesture;
  GtkLayoutManager *layout;

  gtk_widget_set_can_focus (GTK_WIDGET (notebook), TRUE);

  notebook->priv = gtk_notebook_get_instance_private (notebook);
  priv = notebook->priv;

  priv->cur_page = NULL;
  priv->children = NULL;
  priv->first_tab = NULL;
  priv->focus_tab = NULL;
  priv->menu = NULL;

  priv->show_tabs = TRUE;
  priv->show_border = TRUE;
  priv->tab_pos = GTK_POS_TOP;
  priv->scrollable = FALSE;
  priv->click_child = ARROW_NONE;
  priv->need_timer = 0;
  priv->child_has_focus = FALSE;
  priv->focus_out = FALSE;

  priv->group = 0;
  priv->pressed_button = 0;
  priv->dnd_timer = 0;
  priv->switch_tab_timer = 0;
  priv->source_targets = gdk_content_formats_new (src_notebook_targets,
                                              G_N_ELEMENTS (src_notebook_targets));
  priv->operation = DRAG_OPERATION_NONE;
  priv->detached_tab = NULL;
  priv->has_scrolled = FALSE;

  targets = gdk_content_formats_new (dst_notebook_targets, G_N_ELEMENTS (dst_notebook_targets));
  gtk_drag_dest_set (GTK_WIDGET (notebook), 0,
                     targets,
                     GDK_ACTION_MOVE);
  gdk_content_formats_unref (targets);

  gtk_drag_dest_set_track_motion (GTK_WIDGET (notebook), TRUE);

  priv->header_widget = g_object_new (GTK_TYPE_BOX,
                                      "css-name", "header",
                                      NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->header_widget),
                               GTK_STYLE_CLASS_TOP);
  gtk_widget_hide (priv->header_widget);
  gtk_widget_set_parent (priv->header_widget, GTK_WIDGET (notebook));

  priv->tabs_widget = gtk_gizmo_new ("tabs",
                                     gtk_notebook_measure_tabs,
                                     gtk_notebook_allocate_tabs,
                                     gtk_notebook_snapshot_tabs,
                                     NULL);
  gtk_widget_set_hexpand (priv->tabs_widget, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->header_widget), priv->tabs_widget);

  priv->stack_widget = gtk_stack_new ();
  gtk_widget_set_hexpand (priv->stack_widget, TRUE);
  gtk_widget_set_vexpand (priv->stack_widget, TRUE);
  gtk_widget_set_parent (priv->stack_widget, GTK_WIDGET (notebook));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (gtk_notebook_gesture_pressed), notebook);
  g_signal_connect (gesture, "released", G_CALLBACK (gtk_notebook_gesture_released), notebook);
  gtk_widget_add_controller (GTK_WIDGET (notebook), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (gtk_notebook_motion), notebook);
  gtk_widget_add_controller (GTK_WIDGET (notebook), controller);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (notebook)),
                               GTK_STYLE_CLASS_FRAME);

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (notebook));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_VERTICAL);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_notebook_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_notebook_buildable_add_child;
}

static void
gtk_notebook_buildable_add_child (GtkBuildable  *buildable,
                                  GtkBuilder    *builder,
                                  GObject       *child,
                                  const gchar   *type)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (buildable);

  if (GTK_IS_NOTEBOOK_PAGE (child))
    {
      gtk_notebook_insert_notebook_page (notebook, GTK_NOTEBOOK_PAGE (child), -1);
    }
  else if (GTK_IS_WIDGET (child))
    {
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
  else
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
    }
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

        default:
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

  if (gtk_widget_get_focus_child (GTK_WIDGET (notebook)) && effective_direction == GTK_DIR_UP)
    if (focus_tabs_in (notebook))
      return;
  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && effective_direction == GTK_DIR_DOWN)
    if (focus_child_in (notebook, GTK_DIR_TAB_FORWARD))
      return;

  /* At this point, we know we should be focusing out of the notebook entirely. We
   * do this by setting a flag, then propagating the focus motion to the notebook.
   */
  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (notebook)));
  if (!GTK_IS_ROOT (toplevel))
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
        g_object_notify (G_OBJECT (element->data), "position");
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
    case PROP_PAGES:
      g_value_set_object (value, gtk_notebook_get_pages (notebook));
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
 * gtk_notebook_snapshot
 * gtk_notebook_popup_menu
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

  if (priv->pages)
    g_list_model_items_changed (G_LIST_MODEL (priv->pages), 0, g_list_length (priv->children), 0);

  if (priv->menu)
    gtk_notebook_popup_disable (notebook);

  if (priv->source_targets)
    {
      gdk_content_formats_unref (priv->source_targets);
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

  gtk_widget_unparent (priv->header_widget);
  gtk_widget_unparent (priv->stack_widget);

  G_OBJECT_CLASS (gtk_notebook_parent_class)->finalize (object);
}

static void
gtk_notebook_dispose (GObject *object)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (object);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *l = priv->children;

  while (l != NULL)
    {
      GtkNotebookPage *page = l->data;
      l = l->next;

      gtk_notebook_remove (GTK_CONTAINER (notebook), page->child);
    }

  G_OBJECT_CLASS (gtk_notebook_parent_class)->dispose (object);
}

static gboolean
gtk_notebook_get_tab_area_position (GtkNotebook     *notebook,
                                    graphene_rect_t *rectangle)
{
  GtkNotebookPrivate *priv = notebook->priv;

  if (priv->show_tabs && gtk_notebook_has_current_page (notebook))
    {
      return gtk_widget_compute_bounds (priv->header_widget,
                                        GTK_WIDGET (notebook),
                                        rectangle);
    }
  else
    {
      graphene_rect_init_from_rect (rectangle, graphene_rect_zero ());
    }

  return FALSE;
}

static void
gtk_notebook_unmap (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  stop_scrolling (notebook);

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->unmap (widget);
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

  if (priv->arrow_widget[2 * type + 1] == NULL)
    {
      if (priv->arrow_widget[2 * type] == NULL)
        *out_left = 0;
      else
        *out_left = size;
      *out_right = 0;
    }
  else if (priv->arrow_widget[2 * type] == NULL)
    {
      *out_left = 0;
      *out_right = size;
    }
  else
    {
      gtk_widget_measure (priv->arrow_widget[2 * type],
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          &sizes[0].minimum_size, &sizes[0].natural_size,
                          NULL, NULL);
      gtk_widget_measure (priv->arrow_widget[2 * type + 1],
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
      if (priv->arrow_widget[2 * type])
        {
          gtk_widget_measure (priv->arrow_widget[2 * type],
                              orientation,
                              for_size,
                              &child1_min, &child1_nat,
                              NULL, NULL);
        }
      else
        {
          child1_min = child1_nat = 0;
        }
      if (priv->arrow_widget[2 * type + 1])
        {
          gtk_widget_measure (priv->arrow_widget[2 * type + 1],
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

      if (priv->arrow_widget[2 * type])
        {
          gtk_widget_measure (priv->arrow_widget[2 * type],
                              orientation,
                              child1_size,
                              &child1_min, &child1_nat,
                              NULL, NULL);
        }
      else
        {
          child1_min = child1_nat = 0;
        }
      if (priv->arrow_widget[2 * type + 1])
        {
          gtk_widget_measure (priv->arrow_widget[2 * type + 1],
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

          gtk_widget_measure (page->tab_widget,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &page->requisition.width, NULL,
                              NULL, NULL);
          gtk_widget_measure (page->tab_widget,
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
            default:
              g_assert_not_reached ();
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
gtk_notebook_measure_tabs (GtkGizmo       *gizmo,
                           GtkOrientation  orientation,
                           gint            size,
                           gint           *minimum,
                           gint           *natural,
                           gint           *minimum_baseline,
                           gint           *natural_baseline)
{
  GtkWidget *widget = gtk_widget_get_parent (GTK_WIDGET (gizmo));
  GtkNotebook *notebook = GTK_NOTEBOOK (gtk_widget_get_parent (widget));
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
gtk_notebook_allocate_tabs (GtkGizmo *gizmo,
                            int       width,
                            int       height,
                            int       baseline)
{
  GtkWidget *widget = gtk_widget_get_parent (GTK_WIDGET (gizmo));
  GtkNotebook *notebook = GTK_NOTEBOOK (gtk_widget_get_parent (widget));

  gtk_notebook_pages_allocate (notebook, width, height);
}

static gboolean
gtk_notebook_show_arrows (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;

  if (!priv->scrollable)
    return FALSE;

  children = priv->children;
  while (children)
    {
      GtkNotebookPage *page = children->data;

      if (!gtk_widget_get_child_visible (page->tab_widget))
        return TRUE;

      children = children->next;
    }

  return FALSE;
}

static GtkNotebookArrow
gtk_notebook_get_arrow (GtkNotebook *notebook,
                        gint         x,
                        gint         y)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gint i;

  if (gtk_notebook_show_arrows (notebook))
    {
      for (i = 0; i < 4; i++)
        {
          graphene_rect_t arrow_bounds;

          if (priv->arrow_widget[i] == NULL)
            continue;

          if (!gtk_widget_compute_bounds (priv->arrow_widget[i],
                                          GTK_WIDGET (notebook),
                                          &arrow_bounds))
            continue;

          if (graphene_rect_contains_point (&arrow_bounds,
                                            &(graphene_point_t){x, y}))
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

  return TRUE;
}

static gboolean
gtk_notebook_page_tab_label_is_visible (GtkNotebookPage *page)
{
  return page->tab_label &&
         gtk_widget_get_visible (page->tab_widget) &&
         gtk_widget_get_child_visible (page->tab_widget) &&
         gtk_widget_get_visible (page->tab_label) &&
         gtk_widget_get_child_visible (page->tab_label);
}

static gboolean
in_tabs (GtkNotebook *notebook,
         gdouble      x,
         gdouble      y)
{
  GtkNotebookPrivate *priv = notebook->priv;
  graphene_rect_t tabs_bounds;

  if (!gtk_widget_compute_bounds (priv->tabs_widget, GTK_WIDGET (notebook), &tabs_bounds))
    return FALSE;

  return graphene_rect_contains_point (&tabs_bounds,
                                       &(graphene_point_t){x, y});
}

static GList*
get_tab_at_pos (GtkNotebook *notebook,
                gdouble      x,
                gdouble      y)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GList *children;

  for (children = priv->children; children; children = children->next)
    {
      graphene_rect_t bounds;

      page = children->data;

      if (!gtk_notebook_page_tab_label_is_visible (page))
        continue;

      if (!gtk_widget_compute_bounds (page->tab_widget, GTK_WIDGET (notebook), &bounds))
        continue;

      if (graphene_rect_contains_point (&bounds, &(graphene_point_t){x, y}))
        return children;
    }

  return NULL;
}

static void
gtk_notebook_gesture_pressed (GtkGestureClick *gesture,
                              int                   n_press,
                              double                x,
                              double                y,
                              gpointer              user_data)
{
  GtkNotebook *notebook = user_data;
  GtkWidget *widget = user_data;
  GtkNotebookPrivate *priv = notebook->priv;
  GdkEventSequence *sequence;
  GtkNotebookArrow arrow;
  GtkNotebookPage *page;
  const GdkEvent *event;
  guint button;
  GList *tab;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (!priv->children)
    return;

  arrow = gtk_notebook_get_arrow (notebook, x, y);
  if (arrow != ARROW_NONE)
    {
      gtk_notebook_arrow_button_press (notebook, arrow, button);
      return;
    }

  if (in_tabs (notebook, x, y) && priv->menu && gdk_event_triggers_context_menu (event))
    {
      GdkRectangle rect;

      rect.x = x;
      rect.y = y;
      rect.width = 1;
      rect.height = 1;
      gtk_popover_set_pointing_to (GTK_POPOVER (priv->menu), &rect);
      gtk_popover_popup (GTK_POPOVER (priv->menu));
      return;
    }

  if (button != GDK_BUTTON_PRIMARY)
    return;

  if ((tab = get_tab_at_pos (notebook, x, y)) != NULL)
    {
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
          graphene_rect_t tab_bounds;

          priv->pressed_button = button;

          priv->mouse_x = x;
          priv->mouse_y = y;

          priv->drag_begin_x = priv->mouse_x;
          priv->drag_begin_y = priv->mouse_y;

          /* tab bounds get set to empty, which is fine */
          priv->drag_offset_x = priv->drag_begin_x;
          priv->drag_offset_y = priv->drag_begin_y;
          if (gtk_widget_compute_bounds (page->tab_widget, GTK_WIDGET (notebook), &tab_bounds))
            {
              priv->drag_offset_x -= tab_bounds.origin.x;
              priv->drag_offset_y -= tab_bounds.origin.y;
            }
        }
    }
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
  last_child = NULL;

  for (children = priv->children; children; children = children->next)
    {
      page = children->data;

      if ((priv->operation != DRAG_OPERATION_REORDER || page != priv->cur_page) &&
          gtk_widget_get_visible (page->child) &&
          page->tab_label &&
          gtk_widget_get_mapped (page->tab_label))
        {
          graphene_rect_t tab_bounds;

          if (!gtk_widget_compute_bounds (page->tab_widget, GTK_WIDGET (notebook), &tab_bounds))
            continue;

          switch (priv->tab_pos)
            {
            case GTK_POS_TOP:
            case GTK_POS_BOTTOM:
              if (!is_rtl)
                {
                  if (tab_bounds.origin.x + tab_bounds.size.width / 2 > x)
                    return children;
                }
              else
                {
                  if (tab_bounds.origin.x + tab_bounds.size.width / 2 < x)
                    return children;
                }
              break;

            case GTK_POS_LEFT:
            case GTK_POS_RIGHT:
              if (tab_bounds.origin.y + tab_bounds.size.height / 2 > y)
                return children;
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          last_child = children->next;
        }
    }

  return last_child;
}

static void
tab_drag_begin (GtkNotebook     *notebook,
                GtkNotebookPage *page)
{
  gtk_style_context_add_class (gtk_widget_get_style_context (page->tab_widget), GTK_STYLE_CLASS_DND);
}

/* This function undoes the reparenting that happens both when drag_surface
 * is shown for reordering and when the DnD icon is shown for detaching
 */
static void
tab_drag_end (GtkNotebook     *notebook,
              GtkNotebookPage *page)
{
  if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
    {
      g_object_ref (page->tab_label);
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (page->tab_label)), page->tab_label);
      gtk_widget_set_parent (page->tab_label, page->tab_widget);
      g_object_unref (page->tab_label);
    }

  gtk_style_context_remove_class (gtk_widget_get_style_context (page->tab_widget), GTK_STYLE_CLASS_DND);
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
                    g_object_notify (G_OBJECT (element->data), "position");
                }
              g_signal_emit (notebook,
                             notebook_signals[PAGE_REORDERED], 0,
                             page->child, page_num);
            }
        }

      priv->has_scrolled = FALSE;

      tab_drag_end (notebook, page);

      priv->operation = DRAG_OPERATION_NONE;

      if (priv->dnd_timer)
        {
          g_source_remove (priv->dnd_timer);
          priv->dnd_timer = 0;
        }

      gtk_widget_queue_allocate (GTK_WIDGET (notebook));
    }
}

static void
gtk_notebook_gesture_released (GtkGestureClick *gesture,
                               int              n_press,
                               double           x,
                               double           y,
                               gpointer         user_data)
{
  GtkNotebook *notebook = user_data;
  GtkNotebookPrivate *priv = notebook->priv;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  guint button;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (!event)
    return;

  if (priv->pressed_button != button)
    return;

  if (priv->operation == DRAG_OPERATION_REORDER &&
      priv->cur_page &&
      priv->cur_page->reorderable)
    gtk_notebook_stop_reorder (notebook);

  stop_scrolling (notebook);
}

static GtkNotebookPointerPosition
get_pointer_position (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *widget = GTK_WIDGET (notebook);
  graphene_rect_t area;
  gint width, height;
  gboolean is_rtl;

  if (!priv->scrollable)
    return POINTER_BETWEEN;

  gtk_notebook_get_tab_area_position (notebook, &area);
  width = area.size.width;
  height = area.size.height;

  if (priv->tab_pos == GTK_POS_TOP ||
      priv->tab_pos == GTK_POS_BOTTOM)
    {
      gint x = priv->mouse_x;

      is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

      if (x > width - SCROLL_THRESHOLD)
        return (is_rtl) ? POINTER_BEFORE : POINTER_AFTER;
      else if (x < SCROLL_THRESHOLD)
        return (is_rtl) ? POINTER_AFTER : POINTER_BEFORE;
      else
        return POINTER_BETWEEN;
    }
  else
    {
      gint y = priv->mouse_y;

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

      gtk_widget_queue_allocate (priv->tabs_widget);
    }

  return TRUE;
}

static gboolean
check_threshold (GtkNotebook *notebook,
                 gint         current_x,
                 gint         current_y)
{
  gint dnd_threshold;
  graphene_rect_t rectangle;
  GtkSettings *settings;

  settings = gtk_widget_get_settings (GTK_WIDGET (notebook));
  g_object_get (G_OBJECT (settings), "gtk-dnd-drag-threshold", &dnd_threshold, NULL);

  /* we want a large threshold */
  dnd_threshold *= DND_THRESHOLD_MULTIPLIER;

  gtk_notebook_get_tab_area_position (notebook, &rectangle);
  graphene_rect_inset (&rectangle, -dnd_threshold, -dnd_threshold);

  /* The negation here is important! */
  return !graphene_rect_contains_point (&rectangle, &(graphene_point_t){current_x, current_y});
}

static void
gtk_notebook_motion (GtkEventController *controller,
                     double              x,
                     double              y,
                     gpointer            user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  guint state;

  page = priv->cur_page;
  if (!page)
    return;

  if (!gtk_get_current_event_state (&state))
    return;

  if (!(state & GDK_BUTTON1_MASK) &&
      priv->pressed_button != 0)
    {
      gtk_notebook_stop_reorder (notebook);
      stop_scrolling (notebook);
    }

  priv->mouse_x = x;
  priv->mouse_y = y;

  if (priv->pressed_button == 0)
    return;

  if (page->detachable &&
      check_threshold (notebook, priv->mouse_x, priv->mouse_y))
    {
      priv->detached_tab = priv->cur_page;

      gtk_drag_begin (widget,
                      gtk_get_current_event_device (),
                      priv->source_targets, GDK_ACTION_MOVE,
                      priv->drag_begin_x, priv->drag_begin_y);
      return;
    }

  if (page->reorderable &&
      (priv->operation == DRAG_OPERATION_REORDER ||
       gtk_drag_check_threshold (widget, priv->drag_begin_x, priv->drag_begin_y,
                                 priv->mouse_x, priv->mouse_y)))
    {
      GtkNotebookPointerPosition pointer_position = get_pointer_position (notebook);

      if (pointer_position != POINTER_BETWEEN &&
          gtk_notebook_show_arrows (notebook))
        {
          /* scroll tabs */
          if (!priv->dnd_timer)
            {
              priv->has_scrolled = TRUE;
              priv->dnd_timer = g_timeout_add (TIMEOUT_REPEAT * SCROLL_DELAY_FACTOR,
                                               scroll_notebook_timer,
                                               notebook);
              g_source_set_name_by_id (priv->dnd_timer, "[gtk] scroll_notebook_timer");
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

      if (priv->operation != DRAG_OPERATION_REORDER)
        {
          priv->operation = DRAG_OPERATION_REORDER;
          tab_drag_begin (notebook, page);
        }
    }

  if (priv->operation == DRAG_OPERATION_REORDER)
    gtk_widget_queue_allocate (priv->tabs_widget);
}

static void
gtk_notebook_grab_notify (GtkWidget *widget,
                          gboolean   was_grabbed)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->grab_notify (widget, was_grabbed);

  if (!was_grabbed)
    {
      gtk_notebook_stop_reorder (notebook);
      stop_scrolling (notebook);
    }
}

static void
update_arrow_state (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gint i;
  gboolean is_rtl, left;

  is_rtl = gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL;

  for (i = 0; i < 4; i++)
    {
      gboolean sensitive = TRUE;

      if (priv->arrow_widget[i] == NULL)
        continue;

      left = (ARROW_IS_LEFT (i) && !is_rtl) ||
             (!ARROW_IS_LEFT (i) && is_rtl);

      if (priv->focus_tab &&
          !gtk_notebook_search_page (notebook, priv->focus_tab,
                                     left ? STEP_PREV : STEP_NEXT, TRUE))
        {
          sensitive = FALSE;
        }

      gtk_widget_set_sensitive (priv->arrow_widget[i], sensitive);
    }
}

static void
gtk_notebook_state_flags_changed (GtkWidget     *widget,
                                  GtkStateFlags  previous_state)
{
  if (!gtk_widget_is_sensitive (widget))
    stop_scrolling (GTK_NOTEBOOK (widget));
}

static void
update_arrow_nodes (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gboolean arrow[4];
  const char *up_icon_name;
  const char *down_icon_name;
  gint i;

  if (priv->tab_pos == GTK_POS_LEFT ||
      priv->tab_pos == GTK_POS_RIGHT)
    {
      up_icon_name = "pan-down-symbolic";
      down_icon_name = "pan-up-symbolic";
    }
  else
    {
      up_icon_name = "pan-end-symbolic";
      down_icon_name = "pan-start-symbolic";
    }

  arrow[0] = TRUE;
  arrow[1] = FALSE;
  arrow[2] = FALSE;
  arrow[3] = TRUE;

  for (i = 0; i < 4; i++)
    {
      if (priv->scrollable && arrow[i])
        {
          if (priv->arrow_widget[i] == NULL)
            {
              GtkWidget *next_widget;
              GtkStyleContext *context;

              switch (i)
                {
                case 0:
                  if (priv->arrow_widget[1])
                    {
                      next_widget = priv->arrow_widget[1];
                      break;
                    }
                  G_GNUC_FALLTHROUGH;
                case 1:
                  if (priv->children)
                    {
                      GtkNotebookPage *page = priv->children->data;
                      next_widget = page->tab_widget;
                      break;
                    }
                  if (priv->arrow_widget[2])
                    {
                      next_widget = priv->arrow_widget[2];
                      break;
                    }
                  G_GNUC_FALLTHROUGH;
                case 2:
                  if (priv->arrow_widget[3])
                    {
                      next_widget = priv->arrow_widget[3];
                      break;
                    }
                  G_GNUC_FALLTHROUGH;
                case 3:
                  next_widget = NULL;
                  break;

                default:
                  g_assert_not_reached ();
                  next_widget = NULL;
                  break;
                }

              priv->arrow_widget[i] = g_object_new (GTK_TYPE_BUTTON,
                                                    "css-name", "arrow",
                                                    NULL);

              context = gtk_widget_get_style_context (priv->arrow_widget[i]);

              if (i == ARROW_LEFT_BEFORE || i == ARROW_LEFT_AFTER)
                {
                  gtk_style_context_add_class (context, "down");
                  gtk_widget_insert_after (priv->arrow_widget[i], priv->tabs_widget, next_widget);
                }
              else
                {
                  gtk_style_context_add_class (context, "up");
                  gtk_widget_insert_before (priv->arrow_widget[i], priv->tabs_widget, next_widget);
                }
           }

          if (i == ARROW_LEFT_BEFORE || i == ARROW_LEFT_AFTER)
            gtk_button_set_icon_name (GTK_BUTTON (priv->arrow_widget[i]), down_icon_name);
          else
            gtk_button_set_icon_name (GTK_BUTTON (priv->arrow_widget[i]), up_icon_name);
        }
      else
        {
          g_clear_pointer (&priv->arrow_widget[i], gtk_widget_unparent);
        }
    }
}

static void
gtk_notebook_drag_begin (GtkWidget        *widget,
                         GdkDrag          *drag)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  graphene_rect_t bounds;
  GtkWidget *tab_label;

  if (priv->dnd_timer)
    {
      g_source_remove (priv->dnd_timer);
      priv->dnd_timer = 0;
    }

  g_assert (priv->cur_page != NULL);

  priv->operation = DRAG_OPERATION_DETACH;

  tab_label = priv->detached_tab->tab_label;

  tab_drag_end (notebook, priv->cur_page);
  g_object_ref (tab_label);
  gtk_widget_unparent (tab_label);

  priv->dnd_child = tab_label;
  if (gtk_widget_compute_bounds (priv->dnd_child, priv->dnd_child, &bounds))
    gtk_widget_set_size_request (priv->dnd_child,
                                 ceilf (bounds.size.width),
                                 ceilf (bounds.size.height));

  gtk_style_context_add_class (gtk_widget_get_style_context (priv->dnd_child), "background");

  gtk_drag_set_icon_widget (drag, tab_label, -2, -2);
  g_object_set_data (G_OBJECT (priv->dnd_child), "drag-context", drag);
  g_object_unref (tab_label);
}

static void
gtk_notebook_drag_end (GtkWidget      *widget,
                       GdkDrag        *drag)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  gtk_notebook_stop_reorder (notebook);

  if (priv->rootwindow_drop)
    {
      GtkNotebook *dest_notebook = NULL;

      g_signal_emit (notebook, notebook_signals[CREATE_WINDOW], 0,
                     priv->detached_tab->child, &dest_notebook);

      if (dest_notebook)
        do_detach_tab (notebook, dest_notebook, priv->detached_tab->child);

      priv->rootwindow_drop = FALSE;
    }
  else if (priv->detached_tab)
    {
      gtk_widget_set_size_request (priv->dnd_child, -1, -1);
      g_object_ref (priv->dnd_child);
      gtk_widget_unparent (priv->dnd_child);
      gtk_widget_set_parent (priv->dnd_child, priv->detached_tab->tab_widget);
      g_object_unref (priv->dnd_child);
      gtk_notebook_switch_page (notebook, priv->detached_tab);
    }

  gtk_style_context_remove_class (gtk_widget_get_style_context (priv->dnd_child), "background");
  priv->dnd_child = NULL;

  priv->operation = DRAG_OPERATION_NONE;
}

static GtkNotebook *
gtk_notebook_create_window (GtkNotebook *notebook,
                            GtkWidget   *page)
{
  return NULL;
}

static gboolean
gtk_notebook_drag_failed (GtkWidget      *widget,
                          GdkDrag        *drag,
                          GtkDragResult   result)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;

  priv->rootwindow_drop = FALSE;

  if (result == GTK_DRAG_RESULT_NO_TARGET)
    {
      GtkNotebook *dest_notebook = NULL;

      g_signal_emit (notebook, notebook_signals[CREATE_WINDOW], 0,
                     priv->detached_tab->child, &dest_notebook);

      if (dest_notebook)
        do_detach_tab (notebook, dest_notebook, priv->detached_tab->child);

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
gtk_notebook_drag_motion (GtkWidget *widget,
                          GdkDrop   *drop,
                          gint       x,
                          gint       y)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  graphene_rect_t position;
  GtkNotebookArrow arrow;
  GdkAtom target, tab_target;
  GList *tab;
  gboolean retval = FALSE;

  arrow = gtk_notebook_get_arrow (notebook, x, y);
  if (arrow != ARROW_NONE)
    {
      priv->click_child = arrow;
      gtk_notebook_set_scroll_timer (notebook);
      gdk_drop_status (drop, 0);

      retval = TRUE;
      goto out;
    }

  stop_scrolling (notebook);
  target = gtk_drag_dest_find_target (widget, drop, NULL);
  tab_target = g_intern_static_string ("GTK_NOTEBOOK_TAB");

  if (target == tab_target)
    {
      GQuark group, source_group;
      GtkNotebook *source;
      GtkWidget *source_child;

      retval = TRUE;

      source = GTK_NOTEBOOK (gtk_drag_get_source_widget (gdk_drop_get_drag (drop)));
      g_assert (source->priv->cur_page != NULL);
      source_child = source->priv->cur_page->child;

      group = notebook->priv->group;
      source_group = source->priv->group;

      if (group != 0 && group == source_group &&
          !(widget == source_child ||
            gtk_widget_is_ancestor (widget, source_child)))
        {
          gdk_drop_status (drop, GDK_ACTION_MOVE);
          goto out;
        }
      else
        {
          /* it's a tab, but doesn't share
           * ID with this notebook */
          gdk_drop_status (drop, 0);
        }
    }

  if (gtk_notebook_get_tab_area_position (notebook, &position) &&
      graphene_rect_contains_point (&position, &(graphene_point_t){x, y}) &&
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
          priv->switch_tab_timer = g_timeout_add (TIMEOUT_EXPAND, gtk_notebook_switch_tab_timeout, widget);
          g_source_set_name_by_id (priv->switch_tab_timer, "[gtk] gtk_notebook_switch_tab_timeout");
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
gtk_notebook_drag_leave (GtkWidget *widget,
                         GdkDrop   *drop)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  remove_switch_tab_timer (notebook);
  stop_scrolling (notebook);
}

static gboolean
gtk_notebook_drag_drop (GtkWidget *widget,
                        GdkDrop   *drop,
                        gint       x,
                        gint       y)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GdkAtom target, tab_target;

  target = gtk_drag_dest_find_target (widget, drop, NULL);
  tab_target = g_intern_static_string ("GTK_NOTEBOOK_TAB");

  if (target == tab_target)
    {
      notebook->priv->mouse_x = x;
      notebook->priv->mouse_y = y;
      gtk_drag_get_data (widget, drop, target);
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
do_detach_tab (GtkNotebook *from,
               GtkNotebook *to,
               GtkWidget   *child)
{
  GtkNotebookPrivate *to_priv = to->priv;
  GtkWidget *tab_label, *menu_label;
  gboolean tab_expand, tab_fill, reorderable, detachable;
  GList *element;
  gint page_num;
  GtkNotebookPage *page;

  menu_label = gtk_notebook_get_menu_label (from, child);

  if (menu_label)
    g_object_ref (menu_label);

  tab_label = gtk_notebook_get_tab_label (from, child);

  if (tab_label)
    g_object_ref (tab_label);

  g_object_ref (child);

  page = gtk_notebook_get_page (from, child);
  g_object_get (page,
                "tab-expand", &tab_expand,
                "tab-fill", &tab_fill,
                "reorderable", &reorderable,
                "detachable", &detachable,
                NULL);

  gtk_notebook_detach_tab (from, child);

  element = get_drop_position (to);
  page_num = g_list_position (to_priv->children, element);
  gtk_notebook_insert_page_menu (to, child, tab_label, menu_label, page_num);

  page = gtk_notebook_get_page (to, child);
  g_object_set (page,
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
                            GdkDrag          *drag,
                            GtkSelectionData *data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = notebook->priv;
  GdkAtom target;

  target = gtk_selection_data_get_target (data);
  if (target == g_intern_static_string ("GTK_NOTEBOOK_TAB"))
    {
      gtk_selection_data_set (data,
                              target,
                              8,
                              (void*) &priv->detached_tab->child,
                              sizeof (gpointer));
      priv->rootwindow_drop = FALSE;
    }
  else if (target == g_intern_static_string ("application/x-rootwindow-drop"))
    {
      gtk_selection_data_set (data, target, 8, NULL, 0);
      priv->rootwindow_drop = TRUE;
    }
}

static void
gtk_notebook_drag_data_received (GtkWidget        *widget,
                                 GdkDrop          *drop,
                                 GtkSelectionData *data)
{
  GtkNotebook *notebook;
  GdkDrag *drag;
  GtkWidget *source_widget;
  GtkWidget **child;

  notebook = GTK_NOTEBOOK (widget);
  drag = gdk_drop_get_drag (drop);
  source_widget = gtk_drag_get_source_widget (drag);

  if (source_widget &&
      (gdk_drop_get_actions (drop) & GDK_ACTION_MOVE) &&
      gtk_selection_data_get_target (data) == g_intern_static_string ("GTK_NOTEBOOK_TAB"))
    {
      child = (void*) gtk_selection_data_get_data (data);

      do_detach_tab (GTK_NOTEBOOK (source_widget), notebook, *child);
      gdk_drop_finish (drop, GDK_ACTION_MOVE);
    }
  else
    gdk_drop_finish (drop, 0);
}

/* Private GtkContainer Methods :
 *
 * gtk_notebook_add
 * gtk_notebook_remove
 * gtk_notebook_focus
 * gtk_notebook_set_focus_child
 * gtk_notebook_child_type
 * gtk_notebook_forall
 */
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
      g_object_notify (G_OBJECT (list->data), "position");
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
  old_focus_child = gtk_widget_get_focus_child (widget);

  effective_direction = get_effective_direction (notebook, direction);

  if (old_focus_child)          /* Focus on page child or action widget */
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
        return TRUE;

      if (old_focus_child == priv->action_widget[ACTION_WIDGET_START])
        {
          switch ((guint) effective_direction)
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
              switch ((guint) direction)
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
                  break;
                }
            }
        }
      else if (old_focus_child == priv->action_widget[ACTION_WIDGET_END])
        {
          switch ((guint) effective_direction)
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
              switch ((guint) direction)
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
                  break;
                }
            }
        }
      else
        {
          switch ((guint) effective_direction)
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
            default:
              break;
            }
        }
    }
  else if (widget_is_focus)     /* Focus was on tabs */
    {
      switch ((guint) effective_direction)
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
        default:
          break;
        }
    }
  else /* Focus was not on widget */
    {
      switch ((guint) effective_direction)
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
          return FALSE;
        case GTK_DIR_UP:
        case GTK_DIR_LEFT:
        case GTK_DIR_RIGHT:
          return focus_child_in (notebook, direction);
        default:
          break;
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

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (container)));
  if (GTK_IS_WINDOW (toplevel))
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
                     GtkCallback   callback,
                     gpointer      callback_data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;

  children = priv->children;
  while (children)
    {
      GtkNotebookPage *page;

      page = children->data;
      children = children->next;
      (* callback) (page->child, callback_data);
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

  gtk_widget_set_visible (page->tab_widget, gtk_widget_get_visible (child));

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
            gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE_FROM_LIST (next));
        }
      gtk_widget_set_visible (priv->header_widget, priv->show_tabs && gtk_notebook_has_current_page (notebook));
    }

  if (!gtk_notebook_has_current_page (notebook) && gtk_widget_get_visible (child))
    {
      gtk_notebook_switch_page (notebook, page);
      /* focus_tab is set in the switch_page method */
      gtk_notebook_switch_focus_tab (notebook, priv->focus_tab);
    }
}

static void
measure_tab (GtkGizmo       *gizmo,
             GtkOrientation  orientation,
             gint            for_size,
             gint           *minimum,
             gint           *natural,
             gint           *minimum_baseline,
             gint           *natural_baseline)
{
  GtkNotebook *notebook = g_object_get_data (G_OBJECT (gizmo), "notebook");
  GtkNotebookPrivate *priv = notebook->priv;
  GList *l;
  GtkNotebookPage *page = NULL;

  for (l = priv->children; l; l = l->next)
    {
      GtkNotebookPage *p = GTK_NOTEBOOK_PAGE_FROM_LIST (l);
      if (p->tab_widget == GTK_WIDGET (gizmo))
        {
          page = p;
          break;
        }
    }

  g_assert (page != NULL);

  gtk_widget_measure (page->tab_label,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
allocate_tab (GtkGizmo *gizmo,
              int       width,
              int       height,
              int       baseline)
{
  GtkNotebook *notebook = g_object_get_data (G_OBJECT (gizmo), "notebook");
  GtkNotebookPrivate *priv = notebook->priv;
  GList *l;
  GtkNotebookPage *page = NULL;
  GtkAllocation child_allocation;

  for (l = priv->children; l; l = l->next)
    {
      GtkNotebookPage *p = GTK_NOTEBOOK_PAGE_FROM_LIST (l);
      if (p->tab_widget == GTK_WIDGET (gizmo))
        {
          page = p;
          break;
        }
    }

  g_assert (page != NULL);

  child_allocation = (GtkAllocation) {0, 0, width, height};

  if (!page->fill)
    {
      if (priv->tab_pos == GTK_POS_TOP || priv->tab_pos == GTK_POS_BOTTOM)
        {
          gtk_widget_measure (page->tab_label, GTK_ORIENTATION_HORIZONTAL, height,
                              NULL, &child_allocation.width, NULL, NULL);
          if (child_allocation.width > width)
            child_allocation.width = width;
          else
            child_allocation.x += (width - child_allocation.width) / 2;

        }
      else
        {
          gtk_widget_measure (page->tab_label, GTK_ORIENTATION_VERTICAL, width,
                              NULL, &child_allocation.height, NULL, NULL);

          if (child_allocation.height > height)
            child_allocation.height = height;
          else
            child_allocation.y += (height - child_allocation.height) / 2;
        }
    }

  gtk_widget_size_allocate (page->tab_label, &child_allocation, baseline);
}

static gint
gtk_notebook_insert_notebook_page (GtkNotebook *notebook,
                                   GtkNotebookPage *page,
                                   int position)
{
  GtkNotebookPrivate *priv = notebook->priv;
  gint nchildren;
  GList *list;
  GtkWidget *sibling;

  nchildren = g_list_length (priv->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  priv->children = g_list_insert (priv->children, g_object_ref (page), position);

  if (position < nchildren)
    sibling = GTK_NOTEBOOK_PAGE_FROM_LIST (g_list_nth (priv->children, position))->tab_widget;
  else if (priv->arrow_widget[ARROW_LEFT_AFTER])
    sibling = priv->arrow_widget[ARROW_LEFT_AFTER];
  else
  sibling = priv->arrow_widget[ARROW_RIGHT_AFTER];

  page->tab_widget = gtk_gizmo_new ("tab", measure_tab, allocate_tab, NULL, NULL);
  g_object_set_data (G_OBJECT (page->tab_widget), "notebook", notebook);
  gtk_widget_insert_before (page->tab_widget, priv->tabs_widget, sibling);

  page->expand = FALSE;
  page->fill = TRUE;

  if (priv->menu)
    gtk_notebook_menu_item_create (notebook, page);

  gtk_container_add (GTK_CONTAINER (priv->stack_widget), page->child);

  if (page->tab_label)
    {
      gtk_widget_set_parent (page->tab_label, page->tab_widget);
      g_object_set_data (G_OBJECT (page->tab_label), "notebook", notebook);
    }

  gtk_notebook_update_labels (notebook);

  if (!priv->first_tab)
    priv->first_tab = priv->children;

  if (page->tab_label)
    {
      if (priv->show_tabs && gtk_widget_get_visible (page->child))
        gtk_widget_show (page->tab_label);
      else
        gtk_widget_hide (page->tab_label);

    page->mnemonic_activate_signal =
      g_signal_connect (page->tab_label,
                        "mnemonic-activate",
                        G_CALLBACK (gtk_notebook_mnemonic_activate_switch_page),
                        notebook);
    }

  page->notify_visible_handler = g_signal_connect (page->child, "notify::visible",
                                                   G_CALLBACK (page_visible_cb), notebook);

  g_signal_emit (notebook, notebook_signals[PAGE_ADDED], 0, page->child, position);

  if (!gtk_notebook_has_current_page (notebook))
    {
      gtk_notebook_switch_page (notebook, page);
      /* focus_tab is set in the switch_page method */
      gtk_notebook_switch_focus_tab (notebook, priv->focus_tab);
    }

  g_object_notify (G_OBJECT (page), "tab-expand");
  g_object_notify (G_OBJECT (page), "tab-fill");
  g_object_notify (G_OBJECT (page), "tab-label");
  g_object_notify (G_OBJECT (page), "menu-label");

  list = g_list_nth (priv->children, position);
  while (list)
    {
      g_object_notify (G_OBJECT (list->data), "position");
      list = list->next;
    }

  update_arrow_state (notebook);

  if (priv->pages)
    g_list_model_items_changed (priv->pages, position, 0, 1);

  /* The page-added handler might have reordered the pages, re-get the position */
  return gtk_notebook_page_num (notebook, page->child);
}

static gint
gtk_notebook_real_insert_page (GtkNotebook *notebook,
                               GtkWidget   *child,
                               GtkWidget   *tab_label,
                               GtkWidget   *menu_label,
                               gint         position)
{
  GtkNotebookPage *page;
  int ret;

  page = g_object_new (GTK_TYPE_NOTEBOOK_PAGE,
                       "child", child,
                       "tab", tab_label,
                       "menu", menu_label,
                       NULL);

  ret = gtk_notebook_insert_notebook_page (notebook, page, position);

  g_object_unref (page);

  return ret;
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
          priv->timer = g_timeout_add (TIMEOUT_REPEAT * SCROLL_DELAY_FACTOR,
                                       (GSourceFunc) gtk_notebook_timer,
                                       notebook);
          g_source_set_name_by_id (priv->timer, "[gtk] gtk_notebook_timer");
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
      priv->timer = g_timeout_add (TIMEOUT_INITIAL,
                                   (GSourceFunc) gtk_notebook_timer,
                                   notebook);
      g_source_set_name_by_id (priv->timer, "[gtk] gtk_notebook_timer");
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

      if (gtk_widget_get_native (page->tab_label) != gtk_widget_get_native (GTK_WIDGET (notebook)) ||
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
  int position;

  destroying = gtk_widget_in_destruction (GTK_WIDGET (notebook));

  next_list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
  if (!next_list)
    next_list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);

  priv->children = g_list_remove_link (priv->children, list);

  if (priv->cur_page == list->data)
    {
      priv->cur_page = NULL;
      if (next_list && !destroying)
        gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE_FROM_LIST (next_list));
      if (priv->operation == DRAG_OPERATION_REORDER && !priv->remove_in_detach)
        gtk_notebook_stop_reorder (notebook);
    }

  if (priv->detached_tab == list->data)
    {
      priv->detached_tab = NULL;

      if (priv->operation == DRAG_OPERATION_DETACH && !priv->remove_in_detach)
        {
          GdkDrag *drag;

          drag = (GdkDrag *)g_object_get_data (G_OBJECT (priv->dnd_child), "drag-context");
          gtk_drag_cancel (drag);
        }
    }
  if (priv->switch_tab == list)
    priv->switch_tab = NULL;

  if (list == priv->first_tab)
    priv->first_tab = next_list;
  if (list == priv->focus_tab && !destroying)
    gtk_notebook_switch_focus_tab (notebook, next_list);

  page = list->data;

  position = g_list_index (priv->children, page);

  g_signal_handler_disconnect (page->child, page->notify_visible_handler);

  if (gtk_widget_get_visible (page->child) &&
      gtk_widget_get_visible (GTK_WIDGET (notebook)))
    need_resize = TRUE;

  gtk_container_remove (GTK_CONTAINER (priv->stack_widget), page->child);

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

  g_list_free (list);

  if (page->last_focus_child)
    {
      g_object_remove_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
      page->last_focus_child = NULL;
    }

  gtk_widget_unparent (page->tab_widget);

  g_object_unref (page);

  gtk_notebook_update_labels (notebook);
  if (need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  if (priv->pages)
    g_list_model_items_changed (priv->pages, position, 1, 0);
}

static void
gtk_notebook_update_labels (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkNotebookPage *page;
  GList *list;
  char string[32];
  gint page_num = 1;

  if (!priv->show_tabs && !priv->menu)
    return;

  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    {
      const char *text;
      page = list->data;
      g_snprintf (string, sizeof (string), _("Page %u"), page_num++);
      if (page->tab_text)
        text = page->tab_text;
      else
        text = string;
      if (priv->show_tabs)
        {
          if (page->default_tab)
            {
              if (!page->tab_label)
                {
                  page->tab_label = gtk_label_new ("");
                  g_object_ref_sink (page->tab_label);
                  g_object_set_data (G_OBJECT (page->tab_label), "notebook", notebook);
                  gtk_widget_set_parent (page->tab_label, page->tab_widget);
                }
              gtk_label_set_text (GTK_LABEL (page->tab_label), text);
            }

          if (page->child && page->tab_label)
            gtk_widget_set_visible (page->tab_label, gtk_widget_get_visible (page->child));
        }

      if (priv->menu && page->default_menu)
        {
          if (page->menu_text)
            text = page->menu_text;
          else if (GTK_IS_LABEL (page->tab_label))
            text = gtk_label_get_text (GTK_LABEL (page->tab_label));
          else
            text = string;
          gtk_label_set_text (GTK_LABEL (page->menu_label), text);
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

static void
gtk_notebook_snapshot_tabs (GtkGizmo    *gizmo,
                            GtkSnapshot *snapshot)
{
  GtkWidget *widget = gtk_widget_get_parent (GTK_WIDGET (gizmo));
  GtkNotebook *notebook = GTK_NOTEBOOK (gtk_widget_get_parent (widget));
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
    return;

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
        default:
          g_assert_not_reached ();
          break;
        }
    }

  for (children = priv->children; children; children = children->next)
    {
      page = children->data;

      if (!gtk_widget_get_visible (page->child) ||
          page == priv->detached_tab)
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

      gtk_widget_snapshot_child (GTK_WIDGET (gizmo), page->tab_widget, snapshot);
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
          gtk_widget_snapshot_child (GTK_WIDGET (gizmo), page->tab_widget, snapshot);
        }

      g_list_free (other_order);
    }

  if (showarrow && priv->scrollable)
    {
      for (i = 0; i < 4; i++)
        {
          if (priv->arrow_widget[i] == NULL)
            continue;

          gtk_widget_snapshot_child (GTK_WIDGET (gizmo), priv->arrow_widget[i], snapshot);
        }
    }

  if (priv->operation != DRAG_OPERATION_DETACH)
    gtk_widget_snapshot_child (GTK_WIDGET (gizmo), priv->cur_page->tab_widget, snapshot);
}

/* Private GtkNotebook Size Allocate Functions:
 *
 * gtk_notebook_calculate_shown_tabs
 * gtk_notebook_calculate_tabs_allocation
 * gtk_notebook_calc_tabs
 */
static void
gtk_notebook_allocate_arrows (GtkNotebook   *notebook,
                              GtkAllocation *allocation)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkAllocation arrow_allocation;
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

          if (priv->arrow_widget[ii] == NULL)
            continue;

          gtk_widget_measure (priv->arrow_widget[ii],
                              GTK_ORIENTATION_HORIZONTAL,
                              allocation->height,
                              &min, &nat,
                              NULL, NULL);
          if (i < 2)
            {
              arrow_allocation.x = allocation->x;
              arrow_allocation.width = min;
              gtk_widget_size_allocate (priv->arrow_widget[ii],
                                        &arrow_allocation,
                                        -1);
              allocation->x += min;
              allocation->width -= min;
            }
          else
            {
              arrow_allocation.x = allocation->x + allocation->width - min;
              arrow_allocation.width = min;
              gtk_widget_size_allocate (priv->arrow_widget[ii],
                                        &arrow_allocation,
                                        -1);
              allocation->width -= min;
            }
        }
      break;

    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      if (priv->arrow_widget[0] || priv->arrow_widget[1])
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
          if (priv->arrow_widget[0])
            gtk_widget_size_allocate (priv->arrow_widget[0], &arrow_allocation, -1);
          arrow_allocation.x += size1;
          arrow_allocation.width = size2;
          if (priv->arrow_widget[1])
            gtk_widget_size_allocate (priv->arrow_widget[1], &arrow_allocation, -1);
          allocation->y += min;
          allocation->height -= min;
        }
      if (priv->arrow_widget[2] || priv->arrow_widget[3])
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
          if (priv->arrow_widget[2])
            gtk_widget_size_allocate (priv->arrow_widget[2], &arrow_allocation, -1);
          arrow_allocation.x += size1;
          arrow_allocation.width = size2;
          if (priv->arrow_widget[3])
            gtk_widget_size_allocate (priv->arrow_widget[3], &arrow_allocation, -1);
          allocation->height -= min;
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}


static void
gtk_notebook_tab_space (GtkNotebook   *notebook,
                        int            notebook_width,
                        int            notebook_height,
                        gboolean      *show_arrows,
                        GtkAllocation *tabs_allocation,
                        gint          *tab_space)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GList *children;
  GtkPositionType tab_pos = get_effective_tab_pos (notebook);

  children = priv->children;

  *tabs_allocation = (GtkAllocation) {0, 0, notebook_width, notebook_height};

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

    default:
      g_assert_not_reached ();
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

        default:
          g_assert_not_reached ();
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
            gtk_widget_set_child_visible (page->tab_widget, FALSE);
        }

      for (children = *last_child; children;
           children = gtk_notebook_search_page (notebook, children,
                                                STEP_NEXT, TRUE))
        {
          page = children->data;

          if (page->tab_label &&
              NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
            gtk_widget_set_child_visible (page->tab_widget, FALSE);
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

    case GTK_POS_RIGHT:
    case GTK_POS_LEFT:
      return (search_direction == STEP_PREV);

    default:
      g_assert_not_reached ();
      return FALSE;
    }
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
  GtkAllocation child_allocation;
  GtkOrientation tab_expand_orientation;
  graphene_rect_t drag_bounds;

  g_assert (priv->cur_page != NULL);

  widget = GTK_WIDGET (notebook);
  tab_pos = get_effective_tab_pos (notebook);
  allocate_at_bottom = get_allocate_at_bottom (widget, direction);

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

    default:
      g_assert_not_reached ();
      anchor = 0;
      break;
    }

  if (!gtk_widget_compute_bounds (priv->cur_page->tab_widget, priv->cur_page->tab_widget, &drag_bounds))
    graphene_rect_init_from_rect (&drag_bounds, graphene_rect_zero ());

  left_x   = CLAMP (priv->mouse_x - priv->drag_offset_x,
                    allocation->x, allocation->x + allocation->width - drag_bounds.size.width);
  top_y    = CLAMP (priv->mouse_y - priv->drag_offset_y,
                    allocation->y, allocation->y + allocation->height - drag_bounds.size.height);
  right_x  = left_x + drag_bounds.size.width;
  bottom_y = top_y + drag_bounds.size.height;
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
                      left_x = priv->drag_surface_x = anchor;
                      anchor += drag_bounds.size.width;
                    }
                }
              else
                {
                  if (right_x <= anchor)
                    {
                      anchor -= drag_bounds.size.width;
                      left_x = priv->drag_surface_x = anchor;
                    }
                }

              gap_left = TRUE;
            }

          if (priv->operation == DRAG_OPERATION_REORDER && page == priv->cur_page)
            {
              priv->drag_surface_x = left_x;
              priv->drag_surface_y = child_allocation.y;
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
                    anchor += drag_bounds.size.width;
                  else if (allocate_at_bottom &&
                           right_x >= anchor + child_allocation.width / 2 &&
                           right_x <= anchor + child_allocation.width)
                    anchor -= drag_bounds.size.width;
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
                  top_y = priv->drag_surface_y = anchor;
                  anchor += drag_bounds.size.height;
                }

              gap_left = TRUE;
            }

          if (priv->operation == DRAG_OPERATION_REORDER && page == priv->cur_page)
            {
              priv->drag_surface_x = child_allocation.x;
              priv->drag_surface_y = top_y;
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
                    anchor += drag_bounds.size.height;
                  else if (allocate_at_bottom &&
                           bottom_y >= anchor + child_allocation.height / 2 &&
                           bottom_y <= anchor + child_allocation.height)
                    anchor -= drag_bounds.size.height;
                }

              child_allocation.y = anchor;
            }
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      /* set child visible */
      if (page->tab_label)
        gtk_widget_set_child_visible (page->tab_widget, TRUE);

      if (page == priv->cur_page && priv->operation == DRAG_OPERATION_REORDER)
        {
          GtkAllocation fixed_allocation = { priv->drag_surface_x, priv->drag_surface_y,
                                             child_allocation.width, child_allocation.height };
          gtk_widget_size_allocate (page->tab_widget, &fixed_allocation, -1);
        }
      else if (page == priv->detached_tab && priv->operation == DRAG_OPERATION_DETACH)
        {
          /* needs to be allocated at 0,0
           * to be shown in the drag window */
          GtkAllocation fixed_allocation = { 0, 0, child_allocation.width, child_allocation.height };
          gtk_widget_size_allocate (page->tab_widget, &fixed_allocation, -1);
        }
      else if (gtk_notebook_page_tab_label_is_visible (page))
        {
          gtk_widget_size_allocate (page->tab_widget, &child_allocation, -1);
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
                    anchor += drag_bounds.size.width;
                  else if (allocate_at_bottom &&
                           right_x >= anchor &&
                           right_x <= anchor + child_allocation.width / 2)
                    anchor -= drag_bounds.size.width;
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
                    anchor += drag_bounds.size.height;
                  else if (allocate_at_bottom &&
                           bottom_y >= anchor &&
                           bottom_y <= anchor + child_allocation.height / 2)
                    anchor -= drag_bounds.size.height;
                }

              if (!allocate_at_bottom)
                anchor += child_allocation.height;
            }

          break;
        default:
          g_assert_not_reached ();
      break;
        }
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
            anchor -= drag_bounds.size.width;

          if ((!allocate_at_bottom && priv->drag_surface_x > anchor) ||
              (allocate_at_bottom && priv->drag_surface_x < anchor))
            priv->drag_surface_x = anchor;
          break;
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          if (allocate_at_bottom)
            anchor -= drag_bounds.size.height;

          if ((!allocate_at_bottom && priv->drag_surface_y > anchor) ||
              (allocate_at_bottom && priv->drag_surface_y < anchor))
            priv->drag_surface_y = anchor;
          break;
        default:
          g_assert_not_reached ();
          break;
        }
    }
}

static void
gtk_notebook_pages_allocate (GtkNotebook *notebook,
                             int          width,
                             int          height)
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

  gtk_notebook_tab_space (notebook, width, height,
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
    default:
      g_assert_not_reached ();
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
  GtkNotebookPage *page = GTK_NOTEBOOK_PAGE_FROM_LIST (list);
  gboolean child_has_focus;

  if (priv->cur_page == page || !gtk_widget_get_visible (GTK_WIDGET (child)))
    return;

  /* save the value here, changing visibility changes focus */
  child_has_focus = priv->child_has_focus;

  if (priv->cur_page)
    {
      GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (notebook));
      GtkWidget *focus = gtk_root_get_focus (root);
      if (focus)
        child_has_focus = gtk_widget_is_ancestor (focus, priv->cur_page->child);
      gtk_widget_unset_state_flags (priv->cur_page->tab_widget, GTK_STATE_FLAG_CHECKED);
    }

  priv->cur_page = page;
  gtk_widget_set_state_flags (page->tab_widget, GTK_STATE_FLAG_CHECKED, FALSE);
  gtk_widget_set_visible (priv->header_widget, priv->show_tabs);

  if (!priv->focus_tab ||
      priv->focus_tab->data != (gpointer) priv->cur_page)
    priv->focus_tab =
      g_list_find (priv->children, priv->cur_page);

  gtk_stack_set_visible_child (GTK_STACK (priv->stack_widget), priv->cur_page->child);
  gtk_widget_set_child_visible (priv->cur_page->tab_widget, TRUE);

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

  update_arrow_state (notebook);

  gtk_widget_queue_resize (GTK_WIDGET (notebook));
  gtk_widget_queue_resize (priv->tabs_widget);
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
  GtkDirectionType dir;
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
        default:
          g_assert_not_reached ();
          dir = GTK_DIR_DOWN;
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
  GList *children;
  guint page_num;

  notebook = GTK_NOTEBOOK (gtk_widget_get_ancestor (widget, GTK_TYPE_NOTEBOOK));
  priv = notebook->priv;

  gtk_popover_popdown (GTK_POPOVER (priv->menu));

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
 */
static void
gtk_notebook_menu_item_create (GtkNotebook *notebook,
                               GtkNotebookPage *page)
{
  GtkNotebookPrivate *priv = notebook->priv;
  GtkWidget *menu_item;

  if (page->default_menu)
    {
      if (GTK_IS_LABEL (page->tab_label))
        page->menu_label = gtk_label_new (gtk_label_get_text (GTK_LABEL (page->tab_label)));
      else
        page->menu_label = gtk_label_new ("");
      g_object_ref_sink (page->menu_label);
      gtk_widget_set_halign (page->menu_label, GTK_ALIGN_START);
      gtk_widget_set_valign (page->menu_label, GTK_ALIGN_CENTER);
    }

  menu_item = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (menu_item), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (menu_item), page->menu_label);
  gtk_container_add (GTK_CONTAINER (priv->menu_box), menu_item);
  g_signal_connect (menu_item, "clicked",
                    G_CALLBACK (gtk_notebook_menu_switch_page), page);
  if (!gtk_widget_get_visible (page->child))
    gtk_widget_hide (menu_item);
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
  gtk_notebook_menu_item_create (notebook, page);
}

static void
gtk_notebook_menu_label_unparent (GtkWidget *widget,
                                  gpointer  data)
{
  gtk_widget_unparent (gtk_bin_get_child (GTK_BIN (widget)));
  _gtk_bin_set_child (GTK_BIN (widget), NULL);
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
    gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE_FROM_LIST (list));

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

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE_FROM_LIST (list));
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

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE_FROM_LIST (list));
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

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = notebook->priv;

  if (priv->show_border != show_border)
    {
      GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (notebook));
      priv->show_border = show_border;

      if (show_border)
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
      else
        gtk_style_context_remove_class (context, GTK_STYLE_CLASS_FRAME);

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

      gtk_widget_hide (priv->header_widget);
    }
  else
    {
      gtk_widget_set_can_focus (GTK_WIDGET (notebook), TRUE);
      gtk_notebook_update_labels (notebook);
      gtk_widget_show (priv->header_widget);
    }

  for (i = 0; i < N_ACTION_WIDGETS; i++)
    {
      if (priv->action_widget[i])
        gtk_widget_set_child_visible (priv->action_widget[i], show_tabs);
    }

  gtk_notebook_update_tab_pos (notebook);
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
  GtkLayoutManager *layout;
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
        gtk_style_context_add_class (gtk_widget_get_style_context (priv->header_widget),
                                     tab_pos_names[i]);
      else
        gtk_style_context_remove_class (gtk_widget_get_style_context (priv->header_widget),
                                        tab_pos_names[i]);
    }

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (notebook));

  switch (tab_pos)
    {
    case GTK_POS_TOP:
      gtk_widget_set_hexpand (priv->tabs_widget, TRUE);
      gtk_widget_set_vexpand (priv->tabs_widget, FALSE);
      gtk_widget_set_hexpand (priv->header_widget, TRUE);
      gtk_widget_set_vexpand (priv->header_widget, FALSE);
      if (priv->show_tabs)
        {
          gtk_widget_insert_before (priv->header_widget, GTK_WIDGET (notebook), priv->stack_widget);
          gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (notebook)),
                                      gtk_widget_get_css_node (priv->header_widget),
                                      gtk_widget_get_css_node (priv->stack_widget));
        }

      gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_VERTICAL);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->header_widget), GTK_ORIENTATION_HORIZONTAL);
      break;

    case GTK_POS_BOTTOM:
      gtk_widget_set_hexpand (priv->tabs_widget, TRUE);
      gtk_widget_set_vexpand (priv->tabs_widget, FALSE);
      gtk_widget_set_hexpand (priv->header_widget, TRUE);
      gtk_widget_set_vexpand (priv->header_widget, FALSE);
      if (priv->show_tabs)
        {
          gtk_widget_insert_after (priv->header_widget, GTK_WIDGET (notebook), priv->stack_widget);
          gtk_css_node_insert_after (gtk_widget_get_css_node (GTK_WIDGET (notebook)),
                                     gtk_widget_get_css_node (priv->header_widget),
                                     gtk_widget_get_css_node (priv->stack_widget));
        }

      gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_VERTICAL);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->header_widget), GTK_ORIENTATION_HORIZONTAL);
      break;

    case GTK_POS_LEFT:
      gtk_widget_set_hexpand (priv->tabs_widget, FALSE);
      gtk_widget_set_vexpand (priv->tabs_widget, TRUE);
      gtk_widget_set_hexpand (priv->header_widget, FALSE);
      gtk_widget_set_vexpand (priv->header_widget, TRUE);
      if (priv->show_tabs)
        {
          gtk_widget_insert_before (priv->header_widget, GTK_WIDGET (notebook), priv->stack_widget);
          gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (notebook)),
                                      gtk_widget_get_css_node (priv->header_widget),
                                      gtk_widget_get_css_node (priv->stack_widget));
        }

      gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_HORIZONTAL);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->header_widget), GTK_ORIENTATION_VERTICAL);
      break;

    case GTK_POS_RIGHT:
      gtk_widget_set_hexpand (priv->tabs_widget, FALSE);
      gtk_widget_set_vexpand (priv->tabs_widget, TRUE);
      gtk_widget_set_hexpand (priv->header_widget, FALSE);
      gtk_widget_set_vexpand (priv->header_widget, TRUE);
      if (priv->show_tabs)
        {
          gtk_widget_insert_after (priv->header_widget, GTK_WIDGET (notebook), priv->stack_widget);
          gtk_css_node_insert_after (gtk_widget_get_css_node (GTK_WIDGET (notebook)),
                                     gtk_widget_get_css_node (priv->header_widget),
                                     gtk_widget_get_css_node (priv->stack_widget));
        }

      gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_HORIZONTAL);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->header_widget), GTK_ORIENTATION_VERTICAL);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
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

  priv->menu = gtk_popover_menu_new (priv->tabs_widget);

  priv->menu_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_popover_menu_add_submenu (GTK_POPOVER_MENU (priv->menu), priv->menu_box, "main");

  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    gtk_notebook_menu_item_create (notebook, list->data);

  gtk_notebook_update_labels (notebook);

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
  priv->menu = NULL;
  priv->menu_box = NULL;

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

  if (GTK_NOTEBOOK_PAGE_FROM_LIST (list)->default_tab)
    return NULL;

  return GTK_NOTEBOOK_PAGE_FROM_LIST (list)->tab_label;
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
      g_object_set_data (G_OBJECT (page->tab_label), "notebook", notebook);
      gtk_widget_set_parent (page->tab_label, page->tab_widget);
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
          gtk_widget_set_parent (page->tab_label, page->tab_widget);
          g_object_set_data (G_OBJECT (page->tab_label), "notebook", notebook);
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

  g_object_notify (G_OBJECT (page), "tab-label");
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

  if (GTK_NOTEBOOK_PAGE_FROM_LIST (list)->default_menu)
    return NULL;

  return GTK_NOTEBOOK_PAGE_FROM_LIST (list)->menu_label;
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

      g_clear_object (&page->menu_label);
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
    gtk_notebook_menu_item_create (notebook, page);
  g_object_notify (G_OBJECT (page), "menu-label");
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
    sibling = gtk_widget_get_css_node (GTK_NOTEBOOK_PAGE_FROM_LIST (list->prev)->tab_widget);
  else if (priv->arrow_widget[ARROW_RIGHT_BEFORE])
    sibling = gtk_widget_get_css_node (priv->arrow_widget[ARROW_RIGHT_BEFORE]);
  else if (priv->arrow_widget[ARROW_LEFT_BEFORE])
    sibling = gtk_widget_get_css_node (priv->arrow_widget[ARROW_LEFT_BEFORE]);
  else
    sibling = NULL;

  gtk_css_node_insert_after (gtk_widget_get_css_node (priv->tabs_widget),
                             gtk_widget_get_css_node (page->tab_widget),
                             sibling);
  gtk_notebook_update_labels (notebook);
  gtk_widget_queue_allocate (priv->tabs_widget);
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

  /* Move around the menu items if necessary */
  gtk_notebook_child_reordered (notebook, page);

  for (list = priv->children, i = 0; list; list = list->next, i++)
    {
      if (MIN (old_pos, position) <= i && i <= MAX (old_pos, position))
        g_object_notify (G_OBJECT (list->data), "position");
    }

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

  return GTK_NOTEBOOK_PAGE_FROM_LIST (list)->reorderable;
}

/**
 * gtk_notebook_set_tab_reorderable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 * @reorderable: whether the tab is reorderable or not
 *
 * Sets whether the notebook tab can be reordered
 * via drag and drop or not.
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

  page = GTK_NOTEBOOK_PAGE_FROM_LIST (list);
  reorderable = reorderable != FALSE;

  if (page->reorderable != reorderable)
    {
      page->reorderable = reorderable;
      if (reorderable)
        gtk_style_context_add_class (gtk_widget_get_style_context (page->tab_widget),
                                     "reorderable-page");
      else
        gtk_style_context_remove_class (gtk_widget_get_style_context (page->tab_widget),
                                        "reorderable-page");
      g_object_notify (G_OBJECT (page), "reorderable");
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

  return GTK_NOTEBOOK_PAGE_FROM_LIST (list)->detachable;
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
 *                         GdkDrag          *drag,
 *                         GtkSelectionData *data,
 *                         guint             time,
 *                         gpointer          user_data)
 *  {
 *    GtkWidget *notebook;
 *    GtkWidget **child;
 *
 *    notebook = gtk_drag_get_source_widget (drag);
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
 */
void
gtk_notebook_set_tab_detachable (GtkNotebook *notebook,
                                 GtkWidget  *child,
                                 gboolean    detachable)
{
  GList *list;
  GtkNotebookPage *page;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = gtk_notebook_find_child (notebook, child);
  g_return_if_fail (list != NULL);

  page = GTK_NOTEBOOK_PAGE_FROM_LIST (list);
  detachable = detachable != FALSE;

  if (page->detachable != detachable)
    {
      page->detachable = detachable;
      g_object_notify (G_OBJECT (page), "detachable");
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
    gtk_container_remove (GTK_CONTAINER (priv->header_widget), priv->action_widget[pack_type]);

  priv->action_widget[pack_type] = widget;

  if (widget)
    {
      gtk_container_add (GTK_CONTAINER (priv->header_widget), widget);
      if (pack_type == GTK_PACK_START)
        gtk_box_reorder_child_after (GTK_BOX (priv->header_widget), widget, NULL);
      else
        gtk_box_reorder_child_after (GTK_BOX (priv->header_widget), widget, gtk_widget_get_last_child (priv->header_widget));
      gtk_widget_set_child_visible (widget, priv->show_tabs);
    }

  gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

/**
 * gtk_notebook_get_page:
 * @notebook: a #GtkNotebook
 * @child: a child of @notebook
 *
 * Returns the #GtkNotebookPage for @child.
 *
 * Returns: (transfer none): the #GtkNotebookPage for @child
 */
GtkNotebookPage *
gtk_notebook_get_page (GtkNotebook *notebook,
                       GtkWidget   *child)
{
  GList *list;
  GtkNotebookPage *page = NULL;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  list = gtk_notebook_find_child (notebook, child);
  if (list != NULL)
    page = list->data;
  
  return page;
}

/**
 * gtk_notebook_page_get_child:
 * @page: a #GtkNotebookPage
 *
 * Returns the notebook child to which @page belongs.
 *
 * Returns: (transfer none): the child to which @page belongs
 */
GtkWidget *
gtk_notebook_page_get_child (GtkNotebookPage *page)
{
  return page->child;
}

#define GTK_TYPE_NOTEBOOK_PAGES (gtk_notebook_pages_get_type ())
G_DECLARE_FINAL_TYPE (GtkNotebookPages, gtk_notebook_pages, GTK, NOTEBOOK_PAGES, GObject)

struct _GtkNotebookPages
{
  GObject parent_instance;
  GtkNotebook *notebook;
};

struct _GtkNotebookPagesClass
{
  GObjectClass parent_class;
};

static GType
gtk_notebook_pages_get_item_type (GListModel *model)
{
  return GTK_TYPE_NOTEBOOK_PAGE;
}

static guint
gtk_notebook_pages_get_n_items (GListModel *model)
{
  GtkNotebookPages *pages = GTK_NOTEBOOK_PAGES (model);

  return g_list_length (pages->notebook->priv->children);
}


static gpointer
gtk_notebook_pages_get_item (GListModel *model,
                             guint       position)
{
  GtkNotebookPages *pages = GTK_NOTEBOOK_PAGES (model);
  GtkNotebookPage *page;

  page = g_list_nth_data (pages->notebook->priv->children, position);

  return g_object_ref (page);
}

static void
gtk_notebook_pages_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_notebook_pages_get_item_type;
  iface->get_n_items = gtk_notebook_pages_get_n_items;
  iface->get_item = gtk_notebook_pages_get_item;
}
G_DEFINE_TYPE_WITH_CODE (GtkNotebookPages, gtk_notebook_pages, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_notebook_pages_list_model_init))

static void
gtk_notebook_pages_init (GtkNotebookPages *pages)
{
}

static void
gtk_notebook_pages_class_init (GtkNotebookPagesClass *class)
{
}

static GtkNotebookPages *
gtk_notebook_pages_new (GtkNotebook *notebook)
{
  GtkNotebookPages *pages;

  pages = g_object_new (GTK_TYPE_NOTEBOOK_PAGES, NULL);
  pages->notebook = notebook;

  return pages;
}

/**
 * gtk_notebook_get_pages:
 * @notebook: a #GtkNotebook
 *
 * Returns a #GListModel that contains the pages of the notebook,
 * and can be used to keep an up-to-date view.
 * 
 * Returns: (transfer full): a #GListModel for the notebook's children
 */
GListModel *
gtk_notebook_get_pages (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv = notebook->priv;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  if (priv->pages)
    return g_object_ref (priv->pages);

  priv->pages = G_LIST_MODEL (gtk_notebook_pages_new (notebook));

  g_object_add_weak_pointer (G_OBJECT (priv->pages), (gpointer *)&priv->pages);

  return priv->pages;
}

