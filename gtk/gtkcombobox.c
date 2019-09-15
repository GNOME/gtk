/* gtkcombobox.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
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

#include "gtkcombobox.h"

#include "gtkadjustment.h"
#include "gtkcellareabox.h"
#include "gtktreemenu.h"
#include "gtkbindings.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcellview.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkeventbox.h"
#include "gtkframe.h"
#include "gtkiconprivate.h"
#include "gtkbox.h"
#include "gtkliststore.h"
#include "gtkmain.h"
#include "gtkmenuprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkscrolledwindow.h"
#include "deprecated/gtktearoffmenuitem.h"
#include "gtktogglebutton.h"
#include "gtktreeselection.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtktooltipprivate.h"
#include "gtkcomboboxprivate.h"

#include <gobject/gvaluecollector.h>

#include <string.h>
#include <stdarg.h>

#include "gtkmarshalers.h"
#include "gtkintl.h"

#include "gtkentryprivate.h"
#include "gtktreeprivate.h"
#include "a11y/gtkcomboboxaccessible.h"


/**
 * SECTION:gtkcombobox
 * @Short_description: A widget used to choose from a list of items
 * @Title: GtkComboBox
 * @See_also: #GtkComboBoxText, #GtkTreeModel, #GtkCellRenderer
 *
 * A GtkComboBox is a widget that allows the user to choose from a list of
 * valid choices. The GtkComboBox displays the selected choice. When
 * activated, the GtkComboBox displays a popup which allows the user to
 * make a new choice. The style in which the selected value is displayed,
 * and the style of the popup is determined by the current theme. It may
 * be similar to a Windows-style combo box.
 *
 * The GtkComboBox uses the model-view pattern; the list of valid choices
 * is specified in the form of a tree model, and the display of the choices
 * can be adapted to the data in the model by using cell renderers, as you
 * would in a tree view. This is possible since GtkComboBox implements the
 * #GtkCellLayout interface. The tree model holding the valid choices is
 * not restricted to a flat list, it can be a real tree, and the popup will
 * reflect the tree structure.
 *
 * To allow the user to enter values not in the model, the “has-entry”
 * property allows the GtkComboBox to contain a #GtkEntry. This entry
 * can be accessed by calling gtk_bin_get_child() on the combo box.
 *
 * For a simple list of textual choices, the model-view API of GtkComboBox
 * can be a bit overwhelming. In this case, #GtkComboBoxText offers a
 * simple alternative. Both GtkComboBox and #GtkComboBoxText can contain
 * an entry.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * combobox
 * ├── box.linked
 * │   ╰── button.combo
 * │       ╰── box
 * │           ├── cellview
 * │           ╰── arrow
 * ╰── window.popup
 * ]|
 *
 * A normal combobox contains a box with the .linked class, a button
 * with the .combo class and inside those buttons, there are a cellview and
 * an arrow.
 *
 * |[<!-- language="plain" -->
 * combobox
 * ├── box.linked
 * │   ├── entry.combo
 * │   ╰── button.combo
 * │       ╰── box
 * │           ╰── arrow
 * ╰── window.popup
 * ]|
 *
 * A GtkComboBox with an entry has a single CSS node with name combobox. It
 * contains a box with the .linked class. That box contains an entry and a
 * button, both with the .combo class added.
 * The button also contains another node with name arrow.
 */


/* WELCOME, to THE house of evil code */
struct _GtkComboBoxPrivate
{
  GtkTreeModel *model;

  GtkCellArea *area;

  gint col_column;
  gint row_column;

  gint wrap_width;

  gint active; /* Only temporary */
  GtkTreeRowReference *active_row;

  GtkWidget *tree_view;

  GtkWidget *cell_view;

  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *arrow;

  GtkWidget *popup_widget;
  GtkWidget *popup_window;
  GtkWidget *scrolled_window;

  GtkCssGadget *gadget;

  guint popup_idle_id;
  GdkEvent *trigger_event;
  guint scroll_timer;
  guint resize_idle_id;

  /* For "has-entry" specific behavior we track
   * an automated cell renderer and text column
   */
  gint  text_column;
  GtkCellRenderer *text_renderer;

  gint id_column;

  guint popup_in_progress : 1;
  guint popup_shown : 1;
  guint add_tearoffs : 1;
  guint has_frame : 1;
  guint is_cell_renderer : 1;
  guint editing_canceled : 1;
  guint auto_scroll : 1;
  guint button_sensitivity : 2;
  guint has_entry : 1;
  guint popup_fixed_width : 1;

  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer                    row_separator_data;
  GDestroyNotify              row_separator_destroy;

  GdkDevice *grab_pointer;

  gchar *tearoff_title;
};

/* While debugging this evil code, I have learned that
 * there are actually 4 modes to this widget, which can
 * be characterized as follows
 *
 * 1) menu mode, no child added
 *
 * tree_view -> NULL
 * cell_view -> GtkCellView, regular child
 * button -> GtkToggleButton set_parent to combo
 * arrow -> GtkArrow set_parent to button
 * popup_widget -> GtkMenu
 * popup_window -> NULL
 * scrolled_window -> NULL
 *
 * 2) menu mode, child added
 *
 * tree_view -> NULL
 * cell_view -> NULL
 * button -> GtkToggleButton set_parent to combo
 * arrow -> GtkArrow, child of button
 * popup_widget -> GtkMenu
 * popup_window -> NULL
 * scrolled_window -> NULL
 *
 * 3) list mode, no child added
 *
 * tree_view -> GtkTreeView, child of scrolled_window
 * cell_view -> GtkCellView, regular child
 * button -> GtkToggleButton, set_parent to combo
 * arrow -> GtkArrow, child of button
 * popup_widget -> tree_view
 * popup_window -> GtkWindow
 * scrolled_window -> GtkScrolledWindow, child of popup_window
 *
 * 4) list mode, child added
 *
 * tree_view -> GtkTreeView, child of scrolled_window
 * cell_view -> NULL
 * button -> GtkToggleButton, set_parent to combo
 * arrow -> GtkArrow, child of button
 * popup_widget -> tree_view
 * popup_window -> GtkWindow
 * scrolled_window -> GtkScrolledWindow, child of popup_window
 *
 */

enum {
  CHANGED,
  MOVE_ACTIVE,
  POPUP,
  POPDOWN,
  FORMAT_ENTRY_TEXT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_WRAP_WIDTH,
  PROP_ROW_SPAN_COLUMN,
  PROP_COLUMN_SPAN_COLUMN,
  PROP_ACTIVE,
  PROP_ADD_TEAROFFS,
  PROP_TEAROFF_TITLE,
  PROP_HAS_FRAME,
  PROP_POPUP_SHOWN,
  PROP_BUTTON_SENSITIVITY,
  PROP_EDITING_CANCELED,
  PROP_HAS_ENTRY,
  PROP_ENTRY_TEXT_COLUMN,
  PROP_POPUP_FIXED_WIDTH,
  PROP_ID_COLUMN,
  PROP_ACTIVE_ID,
  PROP_CELL_AREA
};

static guint combo_box_signals[LAST_SIGNAL] = {0,};

#define SCROLL_TIME  100

/* common */

static void     gtk_combo_box_cell_layout_init     (GtkCellLayoutIface *iface);
static void     gtk_combo_box_cell_editable_init   (GtkCellEditableIface *iface);
static void     gtk_combo_box_constructed          (GObject          *object);
static void     gtk_combo_box_dispose              (GObject          *object);
static void     gtk_combo_box_finalize             (GObject          *object);
static void     gtk_combo_box_unmap                (GtkWidget        *widget);
static void     gtk_combo_box_destroy              (GtkWidget        *widget);

static void     gtk_combo_box_set_property         (GObject         *object,
                                                    guint            prop_id,
                                                    const GValue    *value,
                                                    GParamSpec      *spec);
static void     gtk_combo_box_get_property         (GObject         *object,
                                                    guint            prop_id,
                                                    GValue          *value,
                                                    GParamSpec      *spec);

static void     gtk_combo_box_grab_focus           (GtkWidget       *widget);
static void     gtk_combo_box_style_updated        (GtkWidget       *widget);
static void     gtk_combo_box_button_toggled       (GtkWidget       *widget,
                                                    gpointer         data);
static void     gtk_combo_box_button_state_flags_changed (GtkWidget     *widget,
                                                          GtkStateFlags  previous,
                                                          gpointer       data);
static void     gtk_combo_box_add                  (GtkContainer    *container,
                                                    GtkWidget       *widget);
static void     gtk_combo_box_remove               (GtkContainer    *container,
                                                    GtkWidget       *widget);

static void     gtk_combo_box_menu_show            (GtkWidget        *menu,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_hide            (GtkWidget        *menu,
                                                    gpointer          user_data);

static void     gtk_combo_box_set_popup_widget     (GtkComboBox      *combo_box,
                                                    GtkWidget        *popup);

static void     gtk_combo_box_unset_model          (GtkComboBox      *combo_box);

static void     gtk_combo_box_forall               (GtkContainer     *container,
                                                    gboolean          include_internals,
                                                    GtkCallback       callback,
                                                    gpointer          callback_data);
static gboolean gtk_combo_box_scroll_event         (GtkWidget        *widget,
                                                    GdkEventScroll   *event);
static void     gtk_combo_box_set_active_internal  (GtkComboBox      *combo_box,
                                                    GtkTreePath      *path);

static void     gtk_combo_box_check_appearance     (GtkComboBox      *combo_box);
static void     gtk_combo_box_real_move_active     (GtkComboBox      *combo_box,
                                                    GtkScrollType     scroll);
static void     gtk_combo_box_real_popup           (GtkComboBox      *combo_box);
static gboolean gtk_combo_box_real_popdown         (GtkComboBox      *combo_box);

/* listening to the model */
static void     gtk_combo_box_model_row_inserted   (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          user_data);
static void     gtk_combo_box_model_row_deleted    (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    gpointer          user_data);
static void     gtk_combo_box_model_rows_reordered (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gint             *new_order,
                                                    gpointer          user_data);
static void     gtk_combo_box_model_row_changed    (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);
static void     gtk_combo_box_model_row_expanded   (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);

/* list */
static void     gtk_combo_box_list_position        (GtkComboBox      *combo_box,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *width,
                                                    gint             *height);
static void     gtk_combo_box_list_setup           (GtkComboBox      *combo_box);
static void     gtk_combo_box_list_destroy         (GtkComboBox      *combo_box);

static gboolean gtk_combo_box_list_button_released (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_key_press       (GtkWidget        *widget,
                                                    GdkEventKey      *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_enter_notify    (GtkWidget        *widget,
                                                    GdkEventCrossing *event,
                                                    gpointer          data);
static void     gtk_combo_box_list_auto_scroll     (GtkComboBox   *combo,
                                                    gint           x,
                                                    gint           y);
static gboolean gtk_combo_box_list_scroll_timeout  (GtkComboBox   *combo);
static gboolean gtk_combo_box_list_button_pressed  (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);

static gboolean gtk_combo_box_list_select_func     (GtkTreeSelection *selection,
                                                    GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    gboolean          path_currently_selected,
                                                    gpointer          data);

static void     gtk_combo_box_list_row_changed     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);
static void     gtk_combo_box_list_popup_resize    (GtkComboBox      *combo_box);

/* menu */
static void     gtk_combo_box_menu_setup           (GtkComboBox      *combo_box);
static void     gtk_combo_box_update_title         (GtkComboBox      *combo_box);
static void     gtk_combo_box_menu_destroy         (GtkComboBox      *combo_box);


static gboolean gtk_combo_box_menu_button_press    (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_activate        (GtkWidget        *menu,
                                                    const gchar      *path,
                                                    GtkComboBox      *combo_box);
static void     gtk_combo_box_update_sensitivity   (GtkComboBox      *combo_box);
static gboolean gtk_combo_box_menu_key_press       (GtkWidget        *widget,
                                                    GdkEventKey      *event,
                                                    gpointer          data);
static void     gtk_combo_box_menu_popup           (GtkComboBox      *combo_box,
                                                    const GdkEvent   *trigger_event);

/* cell layout */
static GtkCellArea *gtk_combo_box_cell_layout_get_area       (GtkCellLayout    *cell_layout);

static gboolean gtk_combo_box_mnemonic_activate              (GtkWidget    *widget,
                                                              gboolean      group_cycling);

static void     gtk_combo_box_child_show                     (GtkWidget       *widget,
                                                              GtkComboBox     *combo_box);
static void     gtk_combo_box_child_hide                     (GtkWidget       *widget,
                                                              GtkComboBox     *combo_box);

/* GtkComboBox:has-entry callbacks */
static void     gtk_combo_box_entry_contents_changed         (GtkEntry        *entry,
                                                              gpointer         user_data);
static void     gtk_combo_box_entry_active_changed           (GtkComboBox     *combo_box,
                                                              gpointer         user_data);
static gchar   *gtk_combo_box_format_entry_text              (GtkComboBox     *combo_box,
                                                              const gchar     *path);

/* GtkBuildable method implementation */
static GtkBuildableIface *parent_buildable_iface;

static void     gtk_combo_box_buildable_init                 (GtkBuildableIface *iface);
static void     gtk_combo_box_buildable_add_child            (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *type);
static gboolean gtk_combo_box_buildable_custom_tag_start     (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              GMarkupParser *parser,
                                                              gpointer      *data);
static void     gtk_combo_box_buildable_custom_tag_end       (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              gpointer      *data);
static GObject *gtk_combo_box_buildable_get_internal_child   (GtkBuildable *buildable,
                                                              GtkBuilder   *builder,
                                                              const gchar  *childname);


/* GtkCellEditable method implementations */
static void     gtk_combo_box_start_editing                  (GtkCellEditable *cell_editable,
                                                              GdkEvent        *event);

G_DEFINE_TYPE_WITH_CODE (GtkComboBox, gtk_combo_box, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkComboBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_combo_box_cell_layout_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                gtk_combo_box_cell_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_combo_box_buildable_init))


/* common */
static void
gtk_combo_box_measure (GtkCssGadget   *gadget,
                       GtkOrientation  orientation,
                       int             size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline,
                       gpointer        data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  _gtk_widget_get_preferred_size_for_size (priv->box,
                                           orientation,
                                           size,
                                           minimum, natural,
                                           minimum_baseline, natural_baseline);
}

static void
gtk_combo_box_allocate (GtkCssGadget        *gadget,
                        const GtkAllocation *allocation,
                        int                  baseline,
                        GtkAllocation       *out_clip,
                        gpointer             data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  gtk_widget_size_allocate_with_baseline (priv->box, (GtkAllocation *) allocation, baseline);
  gtk_widget_get_clip (priv->box, out_clip);

  if (!priv->tree_view)
    {
      if (gtk_widget_get_visible (priv->popup_widget))
        {
          gint menu_width;

          if (priv->wrap_width == 0)
            {
              gtk_widget_set_size_request (priv->popup_widget, -1, -1);

              if (priv->popup_fixed_width)
                gtk_widget_get_preferred_width (priv->popup_widget, &menu_width, NULL);
              else
                gtk_widget_get_preferred_width (priv->popup_widget, NULL, &menu_width);

              gtk_widget_set_size_request (priv->popup_widget,
                                           MAX (allocation->width, menu_width), -1);
            }

          /* reposition the menu after giving it a new width */
          gtk_menu_reposition (GTK_MENU (priv->popup_widget));
        }
    }
  else
    {
      if (gtk_widget_get_visible (priv->popup_window))
        {
          gint x, y, width, height;
          gtk_combo_box_list_position (combo_box, &x, &y, &width, &height);
          gtk_window_move (GTK_WINDOW (priv->popup_window), x, y);
          gtk_widget_set_size_request (priv->popup_window, width, height);
        }
    }
}

static gboolean
gtk_combo_box_render (GtkCssGadget *gadget,
                      cairo_t      *cr,
                      int           x,
                      int           y,
                      int           width,
                      int           height,
                      gpointer      data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                priv->box, cr);

  return FALSE;
}

static void
gtk_combo_box_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  gint dummy;

  /* https://bugzilla.gnome.org/show_bug.cgi?id=729496 */
  if (natural_size == NULL)
    natural_size = &dummy;

  gtk_css_gadget_get_preferred_size (GTK_COMBO_BOX (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_combo_box_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  gint min_width;

  /* Combo box is height-for-width only
   * (so we always just reserve enough height for the minimum width) */
  gtk_css_gadget_get_preferred_size (GTK_COMBO_BOX (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     &min_width, NULL,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (GTK_COMBO_BOX (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     min_width,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_combo_box_get_preferred_width_for_height (GtkWidget *widget,
                                              gint       avail_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  /* Combo box is height-for-width only
   * (so we assume we always reserved enough height for the minimum width) */
  gtk_css_gadget_get_preferred_size (GTK_COMBO_BOX (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     avail_size,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_combo_box_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       avail_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  gtk_css_gadget_get_preferred_size (GTK_COMBO_BOX (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     avail_size,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_combo_box_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (GTK_COMBO_BOX (widget)->priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static gboolean
gtk_combo_box_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  gtk_css_gadget_draw (GTK_COMBO_BOX (widget)->priv->gadget, cr);
  return FALSE;
}

static void
gtk_combo_box_compute_expand (GtkWidget *widget,
                              gboolean  *hexpand,
                              gboolean  *vexpand)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (combo_box));
  if (child && child != priv->cell_view)
    {
      *hexpand = gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static void
gtk_combo_box_class_init (GtkComboBoxClass *klass)
{
  GObjectClass *object_class;
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  container_class = (GtkContainerClass *)klass;
  container_class->forall = gtk_combo_box_forall;
  container_class->add = gtk_combo_box_add;
  container_class->remove = gtk_combo_box_remove;

  gtk_container_class_handle_border_width (container_class);

  widget_class = (GtkWidgetClass *)klass;
  widget_class->size_allocate = gtk_combo_box_size_allocate;
  widget_class->draw = gtk_combo_box_draw;
  widget_class->scroll_event = gtk_combo_box_scroll_event;
  widget_class->mnemonic_activate = gtk_combo_box_mnemonic_activate;
  widget_class->grab_focus = gtk_combo_box_grab_focus;
  widget_class->style_updated = gtk_combo_box_style_updated;
  widget_class->get_preferred_width = gtk_combo_box_get_preferred_width;
  widget_class->get_preferred_height = gtk_combo_box_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_combo_box_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_combo_box_get_preferred_width_for_height;
  widget_class->unmap = gtk_combo_box_unmap;
  widget_class->destroy = gtk_combo_box_destroy;
  widget_class->compute_expand = gtk_combo_box_compute_expand;

  object_class = (GObjectClass *)klass;
  object_class->constructed = gtk_combo_box_constructed;
  object_class->dispose = gtk_combo_box_dispose;
  object_class->finalize = gtk_combo_box_finalize;
  object_class->set_property = gtk_combo_box_set_property;
  object_class->get_property = gtk_combo_box_get_property;

  klass->format_entry_text = gtk_combo_box_format_entry_text;

  /* signals */
  /**
   * GtkComboBox::changed:
   * @widget: the object which received the signal
   *
   * The changed signal is emitted when the active
   * item is changed. The can be due to the user selecting
   * a different item from the list, or due to a
   * call to gtk_combo_box_set_active_iter().
   * It will also be emitted while typing into the entry of a combo box
   * with an entry.
   *
   * Since: 2.4
   */
  combo_box_signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkComboBoxClass, changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  /**
   * GtkComboBox::move-active:
   * @widget: the object that received the signal
   * @scroll_type: a #GtkScrollType
   *
   * The ::move-active signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to move the active selection.
   *
   * Since: 2.12
   */
  combo_box_signals[MOVE_ACTIVE] =
    g_signal_new_class_handler (I_("move-active"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_move_active),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1,
                                GTK_TYPE_SCROLL_TYPE);

  /**
   * GtkComboBox::popup:
   * @widget: the object that received the signal
   *
   * The ::popup signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to popup the combo box list.
   *
   * The default binding for this signal is Alt+Down.
   *
   * Since: 2.12
   */
  combo_box_signals[POPUP] =
    g_signal_new_class_handler (I_("popup"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_popup),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);
  /**
   * GtkComboBox::popdown:
   * @button: the object which received the signal
   *
   * The ::popdown signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to popdown the combo box list.
   *
   * The default bindings for this signal are Alt+Up and Escape.
   *
   * Since: 2.12
   */
  combo_box_signals[POPDOWN] =
    g_signal_new_class_handler (I_("popdown"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_popdown),
                                NULL, NULL,
                                _gtk_marshal_BOOLEAN__VOID,
                                G_TYPE_BOOLEAN, 0);

  /**
   * GtkComboBox::format-entry-text:
   * @combo: the object which received the signal
   * @path: the GtkTreePath string from the combo box's current model to format text for
   *
   * For combo boxes that are created with an entry (See GtkComboBox:has-entry).
   *
   * A signal which allows you to change how the text displayed in a combo box's
   * entry is displayed.
   *
   * Connect a signal handler which returns an allocated string representing
   * @path. That string will then be used to set the text in the combo box's entry.
   * The default signal handler uses the text from the GtkComboBox::entry-text-column
   * model column.
   *
   * Here's an example signal handler which fetches data from the model and
   * displays it in the entry.
   * |[<!-- language="C" -->
   * static gchar*
   * format_entry_text_callback (GtkComboBox *combo,
   *                             const gchar *path,
   *                             gpointer     user_data)
   * {
   *   GtkTreeIter iter;
   *   GtkTreeModel model;
   *   gdouble      value;
   *
   *   model = gtk_combo_box_get_model (combo);
   *
   *   gtk_tree_model_get_iter_from_string (model, &iter, path);
   *   gtk_tree_model_get (model, &iter,
   *                       THE_DOUBLE_VALUE_COLUMN, &value,
   *                       -1);
   *
   *   return g_strdup_printf ("%g", value);
   * }
   * ]|
   *
   * Returns: (transfer full): a newly allocated string representing @path
   * for the current GtkComboBox model.
   *
   * Since: 3.4
   */
  combo_box_signals[FORMAT_ENTRY_TEXT] =
    g_signal_new (I_("format-entry-text"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkComboBoxClass, format_entry_text),
                  _gtk_single_string_accumulator, NULL,
                  _gtk_marshal_STRING__STRING,
                  G_TYPE_STRING, 1, G_TYPE_STRING);

  /* key bindings */
  binding_set = gtk_binding_set_by_class (widget_class);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, GDK_MOD1_MASK,
                                "popup", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Down, GDK_MOD1_MASK,
                                "popup", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, GDK_MOD1_MASK,
                                "popdown", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Up, GDK_MOD1_MASK,
                                "popdown", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
                                "popdown", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Home, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_START);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Home, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_START);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_End, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_END);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_End, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_END);

  /* properties */
  g_object_class_override_property (object_class,
                                    PROP_EDITING_CANCELED,
                                    "editing-canceled");

  /**
   * GtkComboBox:model:
   *
   * The model from which the combo box takes the values shown
   * in the list.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("ComboBox model"),
                                                        P_("The model for the combo box"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkComboBox:wrap-width:
   *
   * If wrap-width is set to a positive value, items in the popup will be laid
   * out along multiple columns, starting a new row on reaching the wrap width.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_WRAP_WIDTH,
                                   g_param_spec_int ("wrap-width",
                                                     P_("Wrap width"),
                                                     P_("Wrap width for laying out the items in a grid"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkComboBox:row-span-column:
   *
   * If this is set to a non-negative value, it must be the index of a column
   * of type %G_TYPE_INT in the model. The value in that column for each item
   * will determine how many rows that item will span in the popup. Therefore,
   * values in this column must be greater than zero.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_ROW_SPAN_COLUMN,
                                   g_param_spec_int ("row-span-column",
                                                     P_("Row span column"),
                                                     P_("TreeModel column containing the row span values"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkComboBox:column-span-column:
   *
   * If this is set to a non-negative value, it must be the index of a column
   * of type %G_TYPE_INT in the model. The value in that column for each item
   * will determine how many columns that item will span in the popup.
   * Therefore, values in this column must be greater than zero, and the sum of
   * an item’s column position + span should not exceed #GtkComboBox:wrap-width.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_COLUMN_SPAN_COLUMN,
                                   g_param_spec_int ("column-span-column",
                                                     P_("Column span column"),
                                                     P_("TreeModel column containing the column span values"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkComboBox:active:
   *
   * The item which is currently active. If the model is a non-flat treemodel,
   * and the active item is not an immediate child of the root of the tree,
   * this property has the value
   * `gtk_tree_path_get_indices (path)[0]`,
   * where `path` is the #GtkTreePath of the active item.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_int ("active",
                                                     P_("Active item"),
                                                     P_("The item which is currently active"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkComboBox:add-tearoffs:
   *
   * The add-tearoffs property controls whether generated menus
   * have tearoff menu items.
   *
   * Note that this only affects menu style combo boxes.
   *
   * Since: 2.6
   *
   * Deprecated: 3.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ADD_TEAROFFS,
                                   g_param_spec_boolean ("add-tearoffs",
                                                         P_("Add tearoffs to menus"),
                                                         P_("Whether dropdowns should have a tearoff menu item"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED));

  /**
   * GtkComboBox:has-frame:
   *
   * The has-frame property controls whether a frame
   * is drawn around the entry.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_HAS_FRAME,
                                   g_param_spec_boolean ("has-frame",
                                                         P_("Has Frame"),
                                                         P_("Whether the combo box draws a frame around the child"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkComboBox:tearoff-title:
   *
   * A title that may be displayed by the window manager
   * when the popup is torn-off.
   *
   * Since: 2.10
   *
   * Deprecated: 3.10
   */
  g_object_class_install_property (object_class,
                                   PROP_TEAROFF_TITLE,
                                   g_param_spec_string ("tearoff-title",
                                                        P_("Tearoff Title"),
                                                        P_("A title that may be displayed by the window manager when the popup is torn-off"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED));


  /**
   * GtkComboBox:popup-shown:
   *
   * Whether the combo boxes dropdown is popped up.
   * Note that this property is mainly useful, because
   * it allows you to connect to notify::popup-shown.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_POPUP_SHOWN,
                                   g_param_spec_boolean ("popup-shown",
                                                         P_("Popup shown"),
                                                         P_("Whether the combo's dropdown is shown"),
                                                         FALSE,
                                                         GTK_PARAM_READABLE));


   /**
    * GtkComboBox:button-sensitivity:
    *
    * Whether the dropdown button is sensitive when
    * the model is empty.
    *
    * Since: 2.14
    */
   g_object_class_install_property (object_class,
                                    PROP_BUTTON_SENSITIVITY,
                                    g_param_spec_enum ("button-sensitivity",
                                                       P_("Button Sensitivity"),
                                                       P_("Whether the dropdown button is sensitive when the model is empty"),
                                                       GTK_TYPE_SENSITIVITY_TYPE,
                                                       GTK_SENSITIVITY_AUTO,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:has-entry:
    *
    * Whether the combo box has an entry.
    *
    * Since: 2.24
    */
   g_object_class_install_property (object_class,
                                    PROP_HAS_ENTRY,
                                    g_param_spec_boolean ("has-entry",
                                                          P_("Has Entry"),
                                                          P_("Whether combo box has an entry"),
                                                          FALSE,
                                                          GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

   /**
    * GtkComboBox:entry-text-column:
    *
    * The column in the combo box's model to associate with strings from the entry
    * if the combo was created with #GtkComboBox:has-entry = %TRUE.
    *
    * Since: 2.24
    */
   g_object_class_install_property (object_class,
                                    PROP_ENTRY_TEXT_COLUMN,
                                    g_param_spec_int ("entry-text-column",
                                                      P_("Entry Text Column"),
                                                      P_("The column in the combo box's model to associate "
                                                         "with strings from the entry if the combo was "
                                                         "created with #GtkComboBox:has-entry = %TRUE"),
                                                      -1, G_MAXINT, -1,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:id-column:
    *
    * The column in the combo box's model that provides string
    * IDs for the values in the model, if != -1.
    *
    * Since: 3.0
    */
   g_object_class_install_property (object_class,
                                    PROP_ID_COLUMN,
                                    g_param_spec_int ("id-column",
                                                      P_("ID Column"),
                                                      P_("The column in the combo box's model that provides "
                                                      "string IDs for the values in the model"),
                                                      -1, G_MAXINT, -1,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:active-id:
    *
    * The value of the ID column of the active row.
    *
    * Since: 3.0
    */
   g_object_class_install_property (object_class,
                                    PROP_ACTIVE_ID,
                                    g_param_spec_string ("active-id",
                                                         P_("Active id"),
                                                         P_("The value of the id column "
                                                         "for the active row"),
                                                         NULL,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:popup-fixed-width:
    *
    * Whether the popup's width should be a fixed width matching the
    * allocated width of the combo box.
    *
    * Since: 3.0
    */
   g_object_class_install_property (object_class,
                                    PROP_POPUP_FIXED_WIDTH,
                                    g_param_spec_boolean ("popup-fixed-width",
                                                          P_("Popup Fixed Width"),
                                                          P_("Whether the popup's width should be a "
                                                             "fixed width matching the allocated width "
                                                             "of the combo box"),
                                                          TRUE,
                                                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:cell-area:
    *
    * The #GtkCellArea used to layout cell renderers for this combo box.
    *
    * If no area is specified when creating the combo box with gtk_combo_box_new_with_area()
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
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("appears-as-list",
                                                                 P_("Appears as list"),
                                                                 P_("Whether dropdowns should look like lists rather than menus"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkComboBox:arrow-size:
   *
   * Sets the minimum size of the arrow in the combo box.  Note
   * that the arrow size is coupled to the font size, so in case
   * a larger font is used, the arrow will be larger than set
   * by arrow size.
   *
   * Since: 2.12
   *
   * Deprecated: 3.20: use the standard min-width/min-height CSS properties on
   *   the arrow node; the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("arrow-size",
                                                             P_("Arrow Size"),
                                                             P_("The minimum size of the arrow in the combo box"),
                                                             0,
                                                             G_MAXINT,
                                                             15,
                                                             GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkComboBox:arrow-scaling:
   *
   * Sets the amount of space used up by the combobox arrow,
   * proportional to the font size.
   *
   * Deprecated: 3.20: use the standard min-width/min-height CSS properties on
   *   the arrow node; the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("The amount of space used by the arrow"),
                                                             0,
                                                             2.0,
                                                             1.0,
                                                             GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkComboBox:shadow-type:
   *
   * Which kind of shadow to draw around the combo box.
   *
   * Since: 2.12
   *
   * Deprecated: 3.20: use CSS styling to change the appearance of the combobox
   *   frame; the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow-type",
                                                              P_("Shadow type"),
                                                              P_("Which kind of shadow to draw around the combo box"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_NONE,
                                                              GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkcombobox.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, box);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, button);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, arrow);
  gtk_widget_class_bind_template_callback (widget_class, gtk_combo_box_button_toggled);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_COMBO_BOX_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "combobox");
}

static void
gtk_combo_box_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_combo_box_buildable_add_child;
  iface->custom_tag_start = gtk_combo_box_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_combo_box_buildable_custom_tag_end;
  iface->get_internal_child = gtk_combo_box_buildable_get_internal_child;
}

static void
gtk_combo_box_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_combo_box_cell_layout_get_area;
}

static void
gtk_combo_box_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = gtk_combo_box_start_editing;
}

static void
gtk_combo_box_init (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv;
  GtkCssNode *widget_node;
  GtkStyleContext *context;

  combo_box->priv = gtk_combo_box_get_instance_private (combo_box);
  priv = combo_box->priv;

  priv->wrap_width = 0;

  priv->active = -1;
  priv->active_row = NULL;
  priv->col_column = -1;
  priv->row_column = -1;

  priv->popup_shown = FALSE;
  priv->add_tearoffs = FALSE;
  priv->has_frame = TRUE;
  priv->is_cell_renderer = FALSE;
  priv->editing_canceled = FALSE;
  priv->auto_scroll = FALSE;
  priv->button_sensitivity = GTK_SENSITIVITY_AUTO;
  priv->has_entry = FALSE;
  priv->popup_fixed_width = TRUE;

  priv->text_column = -1;
  priv->text_renderer = NULL;
  priv->id_column = -1;

  g_type_ensure (GTK_TYPE_ICON);
  gtk_widget_init_template (GTK_WIDGET (combo_box));

  gtk_widget_add_events (priv->button, GDK_SCROLL_MASK);

  context = gtk_widget_get_style_context (priv->button);
  gtk_style_context_remove_class (context, "toggle");
  gtk_style_context_add_class (context, "combo");

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (combo_box));
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     GTK_WIDGET (combo_box),
                                                     gtk_combo_box_measure,
                                                     gtk_combo_box_allocate,
                                                     gtk_combo_box_render,
                                                     NULL, NULL);
}

static void
gtk_combo_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkCellArea *area;

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_combo_box_set_model (combo_box, g_value_get_object (value));
      break;

    case PROP_WRAP_WIDTH:
      gtk_combo_box_set_wrap_width (combo_box, g_value_get_int (value));
      break;

    case PROP_ROW_SPAN_COLUMN:
      gtk_combo_box_set_row_span_column (combo_box, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPAN_COLUMN:
      gtk_combo_box_set_column_span_column (combo_box, g_value_get_int (value));
      break;

    case PROP_ACTIVE:
      gtk_combo_box_set_active (combo_box, g_value_get_int (value));
      break;

    case PROP_ADD_TEAROFFS:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_combo_box_set_add_tearoffs (combo_box, g_value_get_boolean (value));
G_GNUC_END_IGNORE_DEPRECATIONS;
      break;

    case PROP_HAS_FRAME:
      if (priv->has_frame != g_value_get_boolean (value))
        {
          priv->has_frame = g_value_get_boolean (value);
          if (priv->has_entry)
            gtk_entry_set_has_frame (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box))),
                                     priv->has_frame);
          g_object_notify (object, "has-frame");
        }
      break;

    case PROP_TEAROFF_TITLE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_combo_box_set_title (combo_box, g_value_get_string (value));
G_GNUC_END_IGNORE_DEPRECATIONS;
      break;

    case PROP_POPUP_SHOWN:
      if (g_value_get_boolean (value))
        gtk_combo_box_popup (combo_box);
      else
        gtk_combo_box_popdown (combo_box);
      break;

    case PROP_BUTTON_SENSITIVITY:
      gtk_combo_box_set_button_sensitivity (combo_box,
                                            g_value_get_enum (value));
      break;

    case PROP_POPUP_FIXED_WIDTH:
      gtk_combo_box_set_popup_fixed_width (combo_box,
                                           g_value_get_boolean (value));
      break;

    case PROP_EDITING_CANCELED:
      if (priv->editing_canceled != g_value_get_boolean (value))
        {
          priv->editing_canceled = g_value_get_boolean (value);
          g_object_notify (object, "editing-canceled");
        }
      break;

    case PROP_HAS_ENTRY:
      priv->has_entry = g_value_get_boolean (value);
      break;

    case PROP_ENTRY_TEXT_COLUMN:
      gtk_combo_box_set_entry_text_column (combo_box, g_value_get_int (value));
      break;

    case PROP_ID_COLUMN:
      gtk_combo_box_set_id_column (combo_box, g_value_get_int (value));
      break;

    case PROP_ACTIVE_ID:
      gtk_combo_box_set_active_id (combo_box, g_value_get_string (value));
      break;

    case PROP_CELL_AREA:
      /* Construct-only, can only be assigned once */
      area = g_value_get_object (value);
      if (area)
        {
          if (priv->area != NULL)
            {
              g_warning ("cell-area has already been set, ignoring construct property");
              g_object_ref_sink (area);
              g_object_unref (area);
            }
          else
            priv->area = g_object_ref_sink (area);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_combo_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = combo_box->priv;

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value, priv->model);
        break;

      case PROP_WRAP_WIDTH:
        g_value_set_int (value, priv->wrap_width);
        break;

      case PROP_ROW_SPAN_COLUMN:
        g_value_set_int (value, priv->row_column);
        break;

      case PROP_COLUMN_SPAN_COLUMN:
        g_value_set_int (value, priv->col_column);
        break;

      case PROP_ACTIVE:
        g_value_set_int (value, gtk_combo_box_get_active (combo_box));
        break;

      case PROP_ADD_TEAROFFS:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        g_value_set_boolean (value, gtk_combo_box_get_add_tearoffs (combo_box));
G_GNUC_END_IGNORE_DEPRECATIONS;
        break;

      case PROP_HAS_FRAME:
        g_value_set_boolean (value, priv->has_frame);
        break;

      case PROP_TEAROFF_TITLE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        g_value_set_string (value, gtk_combo_box_get_title (combo_box));
G_GNUC_END_IGNORE_DEPRECATIONS;
        break;

      case PROP_POPUP_SHOWN:
        g_value_set_boolean (value, priv->popup_shown);
        break;

      case PROP_BUTTON_SENSITIVITY:
        g_value_set_enum (value, priv->button_sensitivity);
        break;

      case PROP_POPUP_FIXED_WIDTH:
        g_value_set_boolean (value, priv->popup_fixed_width);
        break;

      case PROP_EDITING_CANCELED:
        g_value_set_boolean (value, priv->editing_canceled);
        break;

      case PROP_HAS_ENTRY:
        g_value_set_boolean (value, priv->has_entry);
        break;

      case PROP_ENTRY_TEXT_COLUMN:
        g_value_set_int (value, priv->text_column);
        break;

      case PROP_ID_COLUMN:
        g_value_set_int (value, priv->id_column);
        break;

      case PROP_ACTIVE_ID:
        g_value_set_string (value, gtk_combo_box_get_active_id (combo_box));
        break;

      case PROP_CELL_AREA:
        g_value_set_object (value, priv->area);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_combo_box_button_state_flags_changed (GtkWidget     *widget,
                                          GtkStateFlags  previous,
                                          gpointer       data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (gtk_widget_get_realized (widget))
    {
      if (!priv->tree_view && priv->cell_view)
        gtk_widget_set_state_flags (priv->cell_view,
                                    gtk_widget_get_state_flags (widget),
                                    TRUE);
    }
}

static void
gtk_combo_box_check_appearance (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  gboolean appears_as_list;

  /* if wrap_width > 0, then we are in grid-mode and forced to use
   * unix style
   */
  if (priv->wrap_width)
    appears_as_list = FALSE;
  else
    gtk_widget_style_get (GTK_WIDGET (combo_box),
                          "appears-as-list", &appears_as_list,
                          NULL);
  
  if (appears_as_list)
    {
      /* Destroy all the menu mode widgets, if they exist. */
      if (GTK_IS_MENU (priv->popup_widget))
        gtk_combo_box_menu_destroy (combo_box);

      /* Create the list mode widgets, if they don't already exist. */
      if (!GTK_IS_TREE_VIEW (priv->tree_view))
        gtk_combo_box_list_setup (combo_box);
    }
  else
    {
      /* Destroy all the list mode widgets, if they exist. */
      if (GTK_IS_TREE_VIEW (priv->tree_view))
        gtk_combo_box_list_destroy (combo_box);

      /* Create the menu mode widgets, if they don't already exist. */
      if (!GTK_IS_MENU (priv->popup_widget))
        gtk_combo_box_menu_setup (combo_box);
    }
}

static void
gtk_combo_box_style_updated (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  GTK_WIDGET_CLASS (gtk_combo_box_parent_class)->style_updated (widget);

  gtk_combo_box_check_appearance (combo_box);
}

static void
gtk_combo_box_button_toggled (GtkWidget *widget,
                              gpointer   data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      if (!combo_box->priv->popup_in_progress)
        gtk_combo_box_popup (combo_box);
    }
  else
    gtk_combo_box_popdown (combo_box);
}

static void
gtk_combo_box_create_child (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child;

  if (priv->has_entry)
    {
      GtkWidget *entry;
      GtkStyleContext *context;

      entry = gtk_entry_new ();
      gtk_widget_show (entry);
      gtk_container_add (GTK_CONTAINER (combo_box), entry);

      context = gtk_widget_get_style_context (GTK_WIDGET (entry));
      gtk_style_context_add_class (context, "combo");

      g_signal_connect (combo_box, "changed",
                        G_CALLBACK (gtk_combo_box_entry_active_changed), NULL);
    }
  else
    {
      child = gtk_cell_view_new_with_context (priv->area, NULL);
      priv->cell_view = child;
      gtk_widget_set_hexpand (child, TRUE);
      gtk_cell_view_set_fit_model (GTK_CELL_VIEW (priv->cell_view), TRUE);
      gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view), priv->model);
      gtk_container_add (GTK_CONTAINER (gtk_widget_get_parent (priv->arrow)),
                         priv->cell_view);
      _gtk_bin_set_child (GTK_BIN (combo_box), priv->cell_view);
      gtk_widget_show (priv->cell_view);
    }
}

static void
gtk_combo_box_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->box == NULL)
    {
      gtk_widget_set_parent (widget, GTK_WIDGET (container));
      return;
    }

  if (priv->has_entry && !GTK_IS_ENTRY (widget))
    {
      g_warning ("Attempting to add a widget with type %s to a GtkComboBox that needs an entry "
                 "(need an instance of GtkEntry or of a subclass)",
                 G_OBJECT_TYPE_NAME (widget));
      return;
    }

  if (priv->cell_view)
    {
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (priv->cell_view)),
                            priv->cell_view);
      _gtk_bin_set_child (GTK_BIN (container), NULL);
      priv->cell_view = NULL;
    }
  
  gtk_box_pack_start (GTK_BOX (priv->box), widget, TRUE, TRUE, 0);
  _gtk_bin_set_child (GTK_BIN (container), widget);

  if (priv->has_entry)
    {
      g_signal_connect (widget, "changed",
                        G_CALLBACK (gtk_combo_box_entry_contents_changed),
                        combo_box);

      gtk_entry_set_has_frame (GTK_ENTRY (widget), priv->has_frame);
    }
}

static void
gtk_combo_box_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreePath *path;
  gboolean appears_as_list;

  if (priv->has_entry)
    {
      GtkWidget *child_widget;

      child_widget = gtk_bin_get_child (GTK_BIN (container));
      if (widget && widget == child_widget)
        {
          g_signal_handlers_disconnect_by_func (widget,
                                                gtk_combo_box_entry_contents_changed,
                                                container);
        }
    }

  gtk_container_remove (GTK_CONTAINER (priv->box), widget);
  _gtk_bin_set_child (GTK_BIN (container), NULL);

  if (gtk_widget_in_destruction (GTK_WIDGET (combo_box)))
    return;

  gtk_widget_queue_resize (GTK_WIDGET (container));

  if (!priv->tree_view)
    appears_as_list = FALSE;
  else
    appears_as_list = TRUE;
  
  if (appears_as_list)
    gtk_combo_box_list_destroy (combo_box);
  else if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_combo_box_menu_destroy (combo_box);
      gtk_menu_detach (GTK_MENU (priv->popup_widget));
      priv->popup_widget = NULL;
    }

  gtk_combo_box_create_child (combo_box);

  if (appears_as_list)
    gtk_combo_box_list_setup (combo_box);
  else
    gtk_combo_box_menu_setup (combo_box);

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      path = gtk_tree_row_reference_get_path (priv->active_row);
      gtk_combo_box_set_active_internal (combo_box, path);
      gtk_tree_path_free (path);
    }
  else
    gtk_combo_box_set_active_internal (combo_box, NULL);
}

static void
gtk_combo_box_menu_show (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  gtk_combo_box_child_show (menu, user_data);

  priv->popup_in_progress = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                TRUE);
  priv->popup_in_progress = FALSE;
}

static void
gtk_combo_box_menu_hide (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_combo_box_child_hide (menu,user_data);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                FALSE);
}

static void
gtk_combo_box_detacher (GtkWidget *widget,
                        GtkMenu          *menu)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  g_return_if_fail (priv->popup_widget == (GtkWidget *) menu);

  if (menu->priv->toplevel)
    {
      g_signal_handlers_disconnect_by_func (menu->priv->toplevel,
                                            gtk_combo_box_menu_show,
                                            combo_box);
      g_signal_handlers_disconnect_by_func (menu->priv->toplevel,
                                            gtk_combo_box_menu_hide,
                                            combo_box);
    }

  priv->popup_widget = NULL;
}

static gboolean
gtk_combo_box_grab_broken_event (GtkWidget          *widget,
                                 GdkEventGrabBroken *event,
                                 gpointer            user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (event->grab_window == NULL)
    gtk_combo_box_popdown (combo_box);

  return TRUE;
}

static void
gtk_combo_box_set_popup_widget (GtkComboBox *combo_box,
                                GtkWidget   *popup)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_menu_detach (GTK_MENU (priv->popup_widget));
      priv->popup_widget = NULL;
    }
  else if (priv->popup_widget)
    {
      gtk_container_remove (GTK_CONTAINER (priv->scrolled_window),
                            priv->popup_widget);
      g_object_unref (priv->popup_widget);
      priv->popup_widget = NULL;
    }

  if (GTK_IS_MENU (popup))
    {
      if (priv->popup_window)
        {
          gtk_widget_destroy (priv->popup_window);
          priv->popup_window = NULL;
        }

      priv->popup_widget = popup;

      /*
       * Note that we connect to show/hide on the toplevel, not the
       * menu itself, since the menu is not shown/hidden when it is
       * popped up while torn-off.
       */
      g_signal_connect (GTK_MENU (popup)->priv->toplevel, "show",
                        G_CALLBACK (gtk_combo_box_menu_show), combo_box);
      g_signal_connect (GTK_MENU (popup)->priv->toplevel, "hide",
                        G_CALLBACK (gtk_combo_box_menu_hide), combo_box);

      gtk_menu_attach_to_widget (GTK_MENU (popup),
                                 GTK_WIDGET (combo_box),
                                 gtk_combo_box_detacher);
    }
  else
    {
      if (!priv->popup_window)
        {
          priv->popup_window = gtk_window_new (GTK_WINDOW_POPUP);
          gtk_widget_set_name (priv->popup_window, "gtk-combobox-popup-window");

          gtk_window_set_type_hint (GTK_WINDOW (priv->popup_window),
                                    GDK_WINDOW_TYPE_HINT_COMBO);
          gtk_window_set_modal (GTK_WINDOW (priv->popup_window), TRUE);

          g_signal_connect (priv->popup_window, "show",
                            G_CALLBACK (gtk_combo_box_child_show),
                            combo_box);
          g_signal_connect (priv->popup_window, "hide",
                            G_CALLBACK (gtk_combo_box_child_hide),
                            combo_box);
          g_signal_connect (priv->popup_window, "grab-broken-event",
                            G_CALLBACK (gtk_combo_box_grab_broken_event),
                            combo_box);

          gtk_window_set_resizable (GTK_WINDOW (priv->popup_window), FALSE);

          priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);

          gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                          GTK_POLICY_NEVER,
                                          GTK_POLICY_NEVER);
          gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                               GTK_SHADOW_IN);

          gtk_widget_show (priv->scrolled_window);

          gtk_container_add (GTK_CONTAINER (priv->popup_window),
                             priv->scrolled_window);
        }

      gtk_container_add (GTK_CONTAINER (priv->scrolled_window),
                         popup);

      gtk_widget_show (popup);
      g_object_ref (popup);
      priv->popup_widget = popup;
    }
}

static void
gtk_combo_box_list_position (GtkComboBox *combo_box,
                             gint        *x,
                             gint        *y,
                             gint        *width,
                             gint        *height)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkAllocation content_allocation;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle area;
  GtkRequisition popup_req;
  GtkPolicyType hpolicy, vpolicy;
  GdkWindow *window;

  /* under windows, the drop down list is as wide as the combo box itself.
     see bug #340204 */
  GtkWidget *widget = GTK_WIDGET (combo_box);

  gtk_css_gadget_get_content_allocation (priv->gadget, &content_allocation, NULL);

  *x = content_allocation.x;
  *y = content_allocation.y;
  *width = content_allocation.width;

  window = gtk_widget_get_window (GTK_WIDGET (combo_box));
  gdk_window_get_root_coords (window, *x, *y, x, y);

  hpolicy = vpolicy = GTK_POLICY_NEVER;
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                  hpolicy, vpolicy);

  if (priv->popup_fixed_width)
    {
      gtk_widget_get_preferred_size (priv->scrolled_window, &popup_req, NULL);

      if (popup_req.width > *width)
        {
          hpolicy = GTK_POLICY_ALWAYS;
          gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                          hpolicy, vpolicy);
        }
    }
  else
    {
      /* XXX This code depends on treeviews properly reporting their natural width
       * list-mode menus won't fill up to their natural width until then */
      gtk_widget_get_preferred_size (priv->scrolled_window, NULL, &popup_req);

      if (popup_req.width > *width)
        *width = popup_req.width;
    }

  *height = popup_req.height;

  display = gtk_widget_get_display (widget);
  monitor = gdk_display_get_monitor_at_window (display, window);
  gdk_monitor_get_workarea (monitor, &area);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    *x = *x + content_allocation.width - *width;

  if (*x < area.x)
    *x = area.x;
  else if (*x + *width > area.x + area.width)
    *x = area.x + area.width - *width;

  if (*y + content_allocation.height + *height <= area.y + area.height)
    *y += content_allocation.height;
  else if (*y - *height >= area.y)
    *y -= *height;
  else if (area.y + area.height - (*y + content_allocation.height) > *y - area.y)
    {
      *y += content_allocation.height;
      *height = area.y + area.height - *y;
    }
  else
    {
      *height = *y - area.y;
      *y = area.y;
    }

  if (popup_req.height > *height)
    {
      vpolicy = GTK_POLICY_ALWAYS;

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                      hpolicy, vpolicy);
    }
}

static gboolean
cell_layout_is_sensitive (GtkCellLayout *layout)
{
  GList *cells, *list;
  gboolean sensitive;

  cells = gtk_cell_layout_get_cells (layout);

  sensitive = FALSE;
  for (list = cells; list; list = list->next)
    {
      g_object_get (list->data, "sensitive", &sensitive, NULL);

      if (sensitive)
        break;
    }
  g_list_free (cells);

  return sensitive;
}

static gboolean
cell_is_sensitive (GtkCellRenderer *cell,
                   gpointer         data)
{
  gboolean *sensitive = data;

  g_object_get (cell, "sensitive", sensitive, NULL);

  return *sensitive;
}

static gboolean
tree_column_row_is_sensitive (GtkComboBox *combo_box,
                              GtkTreeIter *iter)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->row_separator_func)
    {
      if (priv->row_separator_func (priv->model, iter,
                                    priv->row_separator_data))
        return FALSE;
    }

  if (priv->area)
    {
      gboolean sensitive;

      gtk_cell_area_apply_attributes (priv->area, priv->model, iter, FALSE, FALSE);

      sensitive = FALSE;

      gtk_cell_area_foreach (priv->area, cell_is_sensitive, &sensitive);

      return sensitive;
    }

  return TRUE;
}

static void
update_menu_sensitivity (GtkComboBox *combo_box,
                         GtkWidget   *menu)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GList *children, *child;
  GtkWidget *item, *submenu;
  GtkWidget *cell_view;
  gboolean sensitive;

  if (!priv->model)
    return;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  for (child = children; child; child = child->next)
    {
      item = GTK_WIDGET (child->data);
      cell_view = gtk_bin_get_child (GTK_BIN (item));

      if (!GTK_IS_CELL_VIEW (cell_view))
        continue;

      submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item));
      if (submenu != NULL)
        {
          gtk_widget_set_sensitive (item, TRUE);
          update_menu_sensitivity (combo_box, submenu);
        }
      else
        {
          sensitive = cell_layout_is_sensitive (GTK_CELL_LAYOUT (cell_view));
          gtk_widget_set_sensitive (item, sensitive);
        }
    }

  g_list_free (children);
}

static void
gtk_combo_box_menu_popup (GtkComboBox    *combo_box,
                          const GdkEvent *trigger_event)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  gint active_item;
  GtkAllocation border_allocation;
  GtkAllocation content_allocation;
  GtkWidget *active;

  update_menu_sensitivity (combo_box, priv->popup_widget);

  active_item = -1;
  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (priv->active_row);
      active_item = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);

      if (priv->add_tearoffs)
        active_item++;
    }

  /* FIXME handle nested menus better */
  gtk_menu_set_active (GTK_MENU (priv->popup_widget), active_item);

  if (priv->wrap_width == 0)
    {
      gint width, min_width, nat_width;

      gtk_css_gadget_get_content_allocation (priv->gadget, &content_allocation, NULL);
      width = content_allocation.width;
      gtk_widget_set_size_request (priv->popup_widget, -1, -1);
      gtk_widget_get_preferred_width (priv->popup_widget, &min_width, &nat_width);

      if (priv->popup_fixed_width)
        width = MAX (width, min_width);
      else
        width = MAX (width, nat_width);

      gtk_widget_set_size_request (priv->popup_widget, width, -1);
    }

  g_signal_handlers_disconnect_by_func (priv->popup_widget,
                                        gtk_menu_update_scroll_offset,
                                        NULL);

  g_object_set (priv->popup_widget, "menu-type-hint", GDK_WINDOW_TYPE_HINT_COMBO, NULL);

  if (priv->wrap_width > 0 || priv->cell_view == NULL)
    {
      gtk_css_gadget_get_border_allocation (priv->gadget, &border_allocation, NULL);
      gtk_css_gadget_get_content_allocation (priv->gadget, &content_allocation, NULL);

      g_object_set (priv->popup_widget,
                    "anchor-hints", (GDK_ANCHOR_FLIP_Y |
                                     GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    "rect-anchor-dx", border_allocation.x - content_allocation.x,
                    NULL);

      gtk_menu_popup_at_widget (GTK_MENU (priv->popup_widget),
                                gtk_bin_get_child (GTK_BIN (combo_box)),
                                GDK_GRAVITY_SOUTH_WEST,
                                GDK_GRAVITY_NORTH_WEST,
                                trigger_event);
    }
  else
    {
      /* FIXME handle nested menus better */
      gint rect_anchor_dy = -2;
      GList *i;
      GtkWidget *child;

      active = gtk_menu_get_active (GTK_MENU (priv->popup_widget));

      if (!(active && gtk_widget_get_visible (active)))
        {
          for (i = GTK_MENU_SHELL (priv->popup_widget)->priv->children; i && !active; i = i->next)
            {
              child = i->data;

              if (child && gtk_widget_get_visible (child))
                active = child;
            }
        }

      if (active)
        {
          gint child_height;

          for (i = GTK_MENU_SHELL (priv->popup_widget)->priv->children; i && i->data != active; i = i->next)
            {
              child = i->data;

              if (child && gtk_widget_get_visible (child))
                {
                  gtk_widget_get_preferred_height (child, &child_height, NULL);
                  rect_anchor_dy -= child_height;
                }
            }

          gtk_widget_get_preferred_height (active, &child_height, NULL);
          rect_anchor_dy -= child_height / 2;
        }

      g_object_set (priv->popup_widget,
                    "anchor-hints", (GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    "rect-anchor-dy", rect_anchor_dy,
                    NULL);

      g_signal_connect (priv->popup_widget,
                        "popped-up",
                        G_CALLBACK (gtk_menu_update_scroll_offset),
                        NULL);

      gtk_menu_popup_at_widget (GTK_MENU (priv->popup_widget),
                                GTK_WIDGET (combo_box),
                                GDK_GRAVITY_WEST,
                                GDK_GRAVITY_NORTH_WEST,
                                trigger_event);
    }

    /* Re-get the active item before selecting it, as a popped-up handler – like
     * that of FileChooserButton in folder mode – can refilter the model, making
     * the original active item pointer invalid. This seems ugly and makes some
     * of the above code pointless in such cases, so hopefully we can FIXME. */
    active = gtk_menu_get_active (GTK_MENU (priv->popup_widget));
    if (active && gtk_widget_get_visible (active))
      gtk_menu_shell_select_item (GTK_MENU_SHELL (priv->popup_widget), active);
}

static gboolean
popup_grab_on_window (GdkWindow *window,
                      GdkDevice *pointer)
{
  GdkGrabStatus status;
  GdkSeat *seat;

  seat = gdk_device_get_seat (pointer);
  status = gdk_seat_grab (seat, window,
                          GDK_SEAT_CAPABILITY_ALL, TRUE,
                          NULL, NULL, NULL, NULL);

  return status == GDK_GRAB_SUCCESS;
}

/**
 * gtk_combo_box_popup:
 * @combo_box: a #GtkComboBox
 *
 * Pops up the menu or dropdown list of @combo_box.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Before calling this, @combo_box must be mapped, or nothing will happen.
 *
 * Since: 2.4
 */
void
gtk_combo_box_popup (GtkComboBox *combo_box)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (gtk_widget_get_mapped (GTK_WIDGET (combo_box)))
    g_signal_emit (combo_box, combo_box_signals[POPUP], 0);
}

/**
 * gtk_combo_box_popup_for_device:
 * @combo_box: a #GtkComboBox
 * @device: a #GdkDevice
 *
 * Pops up the menu or dropdown list of @combo_box, the popup window
 * will be grabbed so only @device and its associated pointer/keyboard
 * are the only #GdkDevices able to send events to it.
 *
 * Since: 3.0
 **/
void
gtk_combo_box_popup_for_device (GtkComboBox *combo_box,
                                GdkDevice   *device)
{
  GtkComboBoxPrivate *priv;
  gint x, y, width, height;
  GtkTreePath *path = NULL, *ppath;
  GtkWidget *toplevel;
  GdkDevice *pointer;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GDK_IS_DEVICE (device));

  priv = combo_box->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (combo_box)))
    return;

  if (gtk_widget_get_mapped (priv->popup_widget))
    return;

  if (priv->grab_pointer)
    return;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    pointer = gdk_device_get_associated_device (device);
  else
    pointer = device;

  if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_combo_box_menu_popup (combo_box, priv->trigger_event);
      return;
    }

  _gtk_tooltip_hide (GTK_WIDGET (combo_box));
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo_box));
  if (GTK_IS_WINDOW (toplevel))
    {
      gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                   GTK_WINDOW (priv->popup_window));
      gtk_window_set_transient_for (GTK_WINDOW (priv->popup_window),
                                    GTK_WINDOW (toplevel));
    }

  gtk_combo_box_list_position (combo_box, &x, &y, &width, &height);

  gtk_widget_set_size_request (priv->popup_window, width, height);
  gtk_window_move (GTK_WINDOW (priv->popup_window), x, y);

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      path = gtk_tree_row_reference_get_path (priv->active_row);
      ppath = gtk_tree_path_copy (path);
      if (gtk_tree_path_up (ppath))
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->tree_view),
                                      ppath);
      gtk_tree_path_free (ppath);
    }
  gtk_tree_view_set_hover_expand (GTK_TREE_VIEW (priv->tree_view),
                                  TRUE);

  /* popup */
  gtk_window_set_screen (GTK_WINDOW (priv->popup_window),
                         gtk_widget_get_screen (GTK_WIDGET (combo_box)));
  gtk_widget_show (priv->popup_window);

  if (path)
    {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view),
                                path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

  gtk_widget_grab_focus (priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                TRUE);

  if (!gtk_widget_has_focus (priv->tree_view))
    gtk_widget_grab_focus (priv->tree_view);

  if (!popup_grab_on_window (gtk_widget_get_window (priv->popup_window), pointer))
    {
      gtk_widget_hide (priv->popup_window);
      return;
    }

  priv->grab_pointer = pointer;
}

static void
gtk_combo_box_real_popup (GtkComboBox *combo_box)
{
  GdkDevice *device;

  device = gtk_get_current_event_device ();

  if (!device)
    {
      GdkDisplay *display;

      /* No device was set, pick the first master device */
      display = gtk_widget_get_display (GTK_WIDGET (combo_box));
      device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
    }

  gtk_combo_box_popup_for_device (combo_box, device);
}

static gboolean
gtk_combo_box_real_popdown (GtkComboBox *combo_box)
{
  if (combo_box->priv->popup_shown)
    {
      gtk_combo_box_popdown (combo_box);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_combo_box_popdown:
 * @combo_box: a #GtkComboBox
 *
 * Hides the menu or dropdown list of @combo_box.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Since: 2.4
 */
void
gtk_combo_box_popdown (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_menu_popdown (GTK_MENU (priv->popup_widget));
      return;
    }

  if (!gtk_widget_get_realized (GTK_WIDGET (combo_box)))
    return;

  if (!priv->popup_window)
    return;

  if (!gtk_widget_is_drawable (priv->popup_window))
    return;

  if (priv->grab_pointer)
    gdk_seat_ungrab (gdk_device_get_seat (priv->grab_pointer));

  gtk_widget_hide (priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                FALSE);

  if (priv->scroll_timer)
    {
      g_source_remove (priv->scroll_timer);
      priv->scroll_timer = 0;
    }

  priv->grab_pointer = NULL;
}

static void
gtk_combo_box_unset_model (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->model)
    {
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_row_inserted,
                                            combo_box);
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_row_deleted,
                                            combo_box);
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_rows_reordered,
                                            combo_box);
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_row_changed,
                                            combo_box);

      g_object_unref (priv->model);
      priv->model = NULL;
    }

  if (priv->active_row)
    {
      gtk_tree_row_reference_free (priv->active_row);
      priv->active_row = NULL;
    }

  if (priv->cell_view)
    gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view), NULL);
}

static void
gtk_combo_box_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child;

  if (include_internals)
    {
      if (priv->box)
        (* callback) (priv->box, callback_data);
    }

  child = gtk_bin_get_child (GTK_BIN (container));
  if (child && child != priv->cell_view)
    (* callback) (child, callback_data);
}

static void
gtk_combo_box_child_show (GtkWidget *widget,
                          GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  priv->popup_shown = TRUE;
  g_object_notify (G_OBJECT (combo_box), "popup-shown");
}

static void
gtk_combo_box_child_hide (GtkWidget *widget,
                          GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  priv->popup_shown = FALSE;
  g_object_notify (G_OBJECT (combo_box), "popup-shown");
}

typedef struct {
  GtkComboBox *combo;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean found;
  gboolean set;
} SearchData;

static gboolean
tree_next_func (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (search_data->found)
    {
      if (!tree_column_row_is_sensitive (search_data->combo, iter))
        return FALSE;

      search_data->set = TRUE;
      search_data->iter = *iter;

      return TRUE;
    }

  if (gtk_tree_path_compare (path, search_data->path) == 0)
    search_data->found = TRUE;

  return FALSE;
}

static gboolean
tree_next (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *iter,
           GtkTreeIter  *next)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.path = gtk_tree_model_get_path (model, iter);
  search_data.found = FALSE;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_next_func, &search_data);

  *next = search_data.iter;

  gtk_tree_path_free (search_data.path);

  return search_data.set;
}

static gboolean
tree_prev_func (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (gtk_tree_path_compare (path, search_data->path) == 0)
    {
      search_data->found = TRUE;
      return TRUE;
    }

  if (!tree_column_row_is_sensitive (search_data->combo, iter))
    return FALSE;

  search_data->set = TRUE;
  search_data->iter = *iter;

  return FALSE;
}

static gboolean
tree_prev (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *iter,
           GtkTreeIter  *prev)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.path = gtk_tree_model_get_path (model, iter);
  search_data.found = FALSE;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_prev_func, &search_data);

  *prev = search_data.iter;

  gtk_tree_path_free (search_data.path);

  return search_data.set;
}

static gboolean
tree_last_func (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (!tree_column_row_is_sensitive (search_data->combo, iter))
    return FALSE;

  search_data->set = TRUE;
  search_data->iter = *iter;

  return FALSE;
}

static gboolean
tree_last (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *last)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_last_func, &search_data);

  *last = search_data.iter;

  return search_data.set;
}


static gboolean
tree_first_func (GtkTreeModel *model,
                 GtkTreePath  *path,
                 GtkTreeIter  *iter,
                 gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (!tree_column_row_is_sensitive (search_data->combo, iter))
    return FALSE;

  search_data->set = TRUE;
  search_data->iter = *iter;

  return TRUE;
}

static gboolean
tree_first (GtkComboBox  *combo,
            GtkTreeModel *model,
            GtkTreeIter  *first)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_first_func, &search_data);

  *first = search_data.iter;

  return search_data.set;
}

static gboolean
gtk_combo_box_scroll_event (GtkWidget          *widget,
                            GdkEventScroll     *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;
  gboolean found;
  GtkTreeIter iter;
  GtkTreeIter new_iter;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter))
    return TRUE;

  if (event->direction == GDK_SCROLL_UP)
    found = tree_prev (combo_box, priv->model,
                       &iter, &new_iter);
  else
    found = tree_next (combo_box, priv->model,
                       &iter, &new_iter);

  if (found)
    gtk_combo_box_set_active_iter (combo_box, &new_iter);

  return TRUE;
}

/*
 * menu style
 */
static gboolean
gtk_combo_box_row_separator_func (GtkTreeModel      *model,
                                  GtkTreeIter       *iter,
                                  GtkComboBox       *combo)
{
  GtkComboBoxPrivate *priv = combo->priv;

  if (priv->row_separator_func)
    return priv->row_separator_func (model, iter, priv->row_separator_data);

  return FALSE;
}

static void
gtk_combo_box_menu_setup (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *menu;

  g_signal_connect (priv->button, "button-press-event",
                    G_CALLBACK (gtk_combo_box_menu_button_press),
                    combo_box);
  g_signal_connect (priv->button, "state-flags-changed",
                    G_CALLBACK (gtk_combo_box_button_state_flags_changed),
                    combo_box);

  /* create our funky menu */
  menu = _gtk_tree_menu_new_with_area (priv->area);
  gtk_widget_set_name (menu, "gtk-combobox-popup-menu");

  _gtk_tree_menu_set_model (GTK_TREE_MENU (menu), priv->model);

  _gtk_tree_menu_set_wrap_width (GTK_TREE_MENU (menu), priv->wrap_width);
  _gtk_tree_menu_set_row_span_column (GTK_TREE_MENU (menu), priv->row_column);
  _gtk_tree_menu_set_column_span_column (GTK_TREE_MENU (menu), priv->col_column);
  _gtk_tree_menu_set_tearoff (GTK_TREE_MENU (menu), priv->add_tearoffs);

  g_signal_connect (menu, "menu-activate",
                    G_CALLBACK (gtk_combo_box_menu_activate), combo_box);

  /* Chain our row_separator_func through */
  _gtk_tree_menu_set_row_separator_func (GTK_TREE_MENU (menu),
                                         (GtkTreeViewRowSeparatorFunc)gtk_combo_box_row_separator_func,
                                         combo_box, NULL);

  g_signal_connect (menu, "key-press-event",
                    G_CALLBACK (gtk_combo_box_menu_key_press), combo_box);
  gtk_combo_box_set_popup_widget (combo_box, menu);

  gtk_combo_box_update_title (combo_box);
}

static void
gtk_combo_box_menu_destroy (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  g_signal_handlers_disconnect_by_func (priv->button,
                                        gtk_combo_box_menu_button_press,
                                        combo_box);
  g_signal_handlers_disconnect_by_func (priv->button,
                                        gtk_combo_box_button_state_flags_changed,
                                        combo_box);
  g_signal_handlers_disconnect_by_data (priv->popup_widget, combo_box);

  /* changing the popup window will unref the menu and the children */
}

/* callbacks */
static gboolean
gtk_combo_box_menu_button_press (GtkWidget      *widget,
                                 GdkEventButton *event,
                                 gpointer        user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (GTK_IS_MENU (priv->popup_widget) &&
      event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY)
    {
      if (gtk_widget_get_focus_on_click (GTK_WIDGET (combo_box)) &&
          !gtk_widget_has_focus (priv->button))
        gtk_widget_grab_focus (priv->button);

      gtk_combo_box_menu_popup (combo_box, (const GdkEvent *) event);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_menu_activate (GtkWidget   *menu,
                             const gchar *path,
                             GtkComboBox *combo_box)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_from_string (combo_box->priv->model, &iter, path))
    gtk_combo_box_set_active_iter (combo_box, &iter);

  g_object_set (combo_box,
                "editing-canceled", FALSE,
                NULL);
}


static void
gtk_combo_box_update_sensitivity (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeIter iter;
  gboolean sensitive = TRUE; /* fool code checkers */

  if (!priv->button)
    return;

  switch (priv->button_sensitivity)
    {
      case GTK_SENSITIVITY_ON:
        sensitive = TRUE;
        break;
      case GTK_SENSITIVITY_OFF:
        sensitive = FALSE;
        break;
      case GTK_SENSITIVITY_AUTO:
        sensitive = priv->model &&
                    gtk_tree_model_get_iter_first (priv->model, &iter);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  gtk_widget_set_sensitive (priv->button, sensitive);
}

static void
gtk_combo_box_model_row_inserted (GtkTreeModel     *model,
                                  GtkTreePath      *path,
                                  GtkTreeIter      *iter,
                                  gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (combo_box->priv->tree_view)
    gtk_combo_box_list_popup_resize (combo_box);

  gtk_combo_box_update_sensitivity (combo_box);
}

static void
gtk_combo_box_model_row_deleted (GtkTreeModel     *model,
                                 GtkTreePath      *path,
                                 gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (!gtk_tree_row_reference_valid (priv->active_row))
    {
      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view), NULL);
      g_signal_emit (combo_box, combo_box_signals[CHANGED], 0);
    }
  
  if (priv->tree_view)
    gtk_combo_box_list_popup_resize (combo_box);

  gtk_combo_box_update_sensitivity (combo_box);
}

static void
gtk_combo_box_model_rows_reordered (GtkTreeModel    *model,
                                    GtkTreePath     *path,
                                    GtkTreeIter     *iter,
                                    gint            *new_order,
                                    gpointer         user_data)
{
  gtk_tree_row_reference_reordered (G_OBJECT (user_data), path, iter, new_order);
}

static void
gtk_combo_box_model_row_changed (GtkTreeModel     *model,
                                 GtkTreePath      *path,
                                 GtkTreeIter      *iter,
                                 gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreePath *active_path;

  /* FIXME this belongs to GtkCellView */
  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      active_path = gtk_tree_row_reference_get_path (priv->active_row);
      if (gtk_tree_path_compare (path, active_path) == 0 &&
          priv->cell_view)
        gtk_widget_queue_resize (GTK_WIDGET (priv->cell_view));
      gtk_tree_path_free (active_path);
    }

  if (priv->tree_view)
    gtk_combo_box_list_row_changed (model, path, iter, user_data);
}

static gboolean
list_popup_resize_idle (gpointer user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;
  gint x, y, width, height;

  if (priv->tree_view && gtk_widget_get_mapped (priv->popup_window))
    {
      gtk_combo_box_list_position (combo_box, &x, &y, &width, &height);

      gtk_widget_set_size_request (priv->popup_window, width, height);
      gtk_window_move (GTK_WINDOW (priv->popup_window), x, y);
    }

  priv->resize_idle_id = 0;

  return FALSE;
}

static void
gtk_combo_box_list_popup_resize (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (!priv->resize_idle_id)
    {
      priv->resize_idle_id =
        gdk_threads_add_idle (list_popup_resize_idle, combo_box);
      g_source_set_name_by_id (priv->resize_idle_id, "[gtk+] list_popup_resize_idle");
    }
}

static void
gtk_combo_box_model_row_expanded (GtkTreeModel     *model,
                                  GtkTreePath      *path,
                                  GtkTreeIter      *iter,
                                  gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_combo_box_list_popup_resize (combo_box);
}


/*
 * list style
 */

static void
gtk_combo_box_list_setup (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeSelection *sel;

  g_signal_connect (priv->button, "button-press-event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed), combo_box);

  priv->tree_view = gtk_tree_view_new ();
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);
  gtk_tree_selection_set_select_function (sel,
                                          gtk_combo_box_list_select_func,
                                          NULL, NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view),
                                     FALSE);
  gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (priv->tree_view),
                                     TRUE);

  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->tree_view),
                                        (GtkTreeViewRowSeparatorFunc)gtk_combo_box_row_separator_func,
                                        combo_box, NULL);

  if (priv->model)
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), priv->model);

  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view),
                               gtk_tree_view_column_new_with_area (priv->area));

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (priv->active_row);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view),
                                path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

  /* set sample/popup widgets */
  gtk_combo_box_set_popup_widget (combo_box, priv->tree_view);

  g_signal_connect (priv->tree_view, "key-press-event",
                    G_CALLBACK (gtk_combo_box_list_key_press),
                    combo_box);
  g_signal_connect (priv->tree_view, "enter-notify-event",
                    G_CALLBACK (gtk_combo_box_list_enter_notify),
                    combo_box);
  g_signal_connect (priv->tree_view, "row-expanded",
                    G_CALLBACK (gtk_combo_box_model_row_expanded),
                    combo_box);
  g_signal_connect (priv->tree_view, "row-collapsed",
                    G_CALLBACK (gtk_combo_box_model_row_expanded),
                    combo_box);
  g_signal_connect (priv->popup_window, "button-press-event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed),
                    combo_box);
  g_signal_connect (priv->popup_window, "button-release-event",
                    G_CALLBACK (gtk_combo_box_list_button_released),
                    combo_box);

  gtk_widget_show (priv->tree_view);

  gtk_combo_box_update_sensitivity (combo_box);
}

static void
gtk_combo_box_list_destroy (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  /* disconnect signals */
  g_signal_handlers_disconnect_by_func (priv->button,
                                        gtk_combo_box_list_button_pressed,
                                        combo_box);
  g_signal_handlers_disconnect_by_data (priv->tree_view, combo_box);
  g_signal_handlers_disconnect_by_data (priv->popup_window, combo_box);

  if (priv->cell_view)
    {
      g_object_set (priv->cell_view,
                    "background-set", FALSE,
                    NULL);
    }

  if (priv->scroll_timer)
    {
      g_source_remove (priv->scroll_timer);
      priv->scroll_timer = 0;
    }

  if (priv->resize_idle_id)
    {
      g_source_remove (priv->resize_idle_id);
      priv->resize_idle_id = 0;
    }

  gtk_widget_destroy (priv->tree_view);
  priv->tree_view = NULL;

  if (priv->popup_widget)
    {
      g_object_unref (priv->popup_widget);
      priv->popup_widget = NULL;
    }
}

/* callbacks */

static gboolean
gtk_combo_box_list_button_pressed (GtkWidget      *widget,
                                   GdkEventButton *event,
                                   gpointer        data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (ewidget == priv->popup_window)
    return TRUE;

  if (ewidget != priv->button ||
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button)))
    return FALSE;

  if (gtk_widget_get_focus_on_click (GTK_WIDGET (combo_box)) &&
      !gtk_widget_has_focus (priv->button))
    gtk_widget_grab_focus (priv->button);

  gtk_combo_box_popup_for_device (combo_box, event->device);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button), TRUE);

  priv->auto_scroll = FALSE;
  if (priv->scroll_timer == 0) {
    priv->scroll_timer = gdk_threads_add_timeout (SCROLL_TIME,
                                                  (GSourceFunc) gtk_combo_box_list_scroll_timeout,
                                                   combo_box);
    g_source_set_name_by_id (priv->scroll_timer, "[gtk+] gtk_combo_box_list_scroll_timeout");
  }

  priv->popup_in_progress = TRUE;

  return TRUE;
}

static gboolean
gtk_combo_box_list_button_released (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    gpointer        data)
{
  gboolean ret;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  gint x;
  GdkRectangle cell_area;

  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  gboolean popup_in_progress = FALSE;

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (priv->popup_in_progress)
    {
      popup_in_progress = TRUE;
      priv->popup_in_progress = FALSE;
    }

  gtk_tree_view_set_hover_expand (GTK_TREE_VIEW (priv->tree_view),
                                  FALSE);
  if (priv->scroll_timer)
    {
      g_source_remove (priv->scroll_timer);
      priv->scroll_timer = 0;
    }

  if (ewidget != priv->tree_view)
    {
      GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (priv->scrolled_window);

      if (ewidget == priv->button &&
          !popup_in_progress &&
          gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button)))
        {
          gtk_combo_box_popdown (combo_box);

          return TRUE;
        }

      /* If released outside treeview, pop down, unless finishing a scroll */
      if (ewidget != priv->button &&
          ewidget != gtk_scrolled_window_get_hscrollbar (scrolled_window) &&
          ewidget != gtk_scrolled_window_get_vscrollbar (scrolled_window))
        {
          gtk_combo_box_popdown (combo_box);

          return TRUE;
        }

      return FALSE;
    }

  /* Determine which row was clicked and which column therein */
  ret = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
                                       event->x, event->y,
                                       &path, &column,
                                       &x, NULL);

  if (!ret)
    return TRUE; /* clicked outside window? */

  /* Don’t select/close after clicking row’s expander. cell_area excludes that */
  gtk_tree_view_get_cell_area (GTK_TREE_VIEW (priv->tree_view),
                               path, column, &cell_area);
  if (x >= cell_area.x && x < cell_area.x + cell_area.width)
    {
      gtk_tree_model_get_iter (priv->model, &iter, path);

      /* Use iter before popdown, as mis-users like GtkFileChooserButton alter the
       * model during notify::popped-up, which means the iterator becomes invalid.
       */
      if (tree_column_row_is_sensitive (combo_box, &iter))
        gtk_combo_box_set_active_internal (combo_box, path);

      gtk_combo_box_popdown (combo_box);
    }

  gtk_tree_path_free (path);

  return TRUE;
}

static gboolean
gtk_combo_box_menu_key_press (GtkWidget   *widget,
                              GdkEventKey *event,
                              gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (!gtk_bindings_activate_event (G_OBJECT (widget), event))
    {
      /* The menu hasn't managed the
       * event, forward it to the combobox
       */
      gtk_bindings_activate_event (G_OBJECT (combo_box), event);
    }

  return TRUE;
}

static gboolean
gtk_combo_box_list_key_press (GtkWidget   *widget,
                              GdkEventKey *event,
                              gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeIter iter;

  if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_ISO_Enter || event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_space || event->keyval == GDK_KEY_KP_Space)
  {
    GtkTreeModel *model = NULL;

    gtk_combo_box_popdown (combo_box);

    if (priv->model)
      {
        GtkTreeSelection *sel;

        sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));

        if (gtk_tree_selection_get_selected (sel, &model, &iter))
          gtk_combo_box_set_active_iter (combo_box, &iter);
      }

    return TRUE;
  }

  if (!gtk_bindings_activate_event (G_OBJECT (widget), event))
    {
      /* The list hasn't managed the
       * event, forward it to the combobox
       */
      gtk_bindings_activate_event (G_OBJECT (combo_box), event);
    }

  return TRUE;
}

static void
gtk_combo_box_list_auto_scroll (GtkComboBox *combo_box,
                                gint         x,
                                gint         y)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkAdjustment *adj;
  GtkAllocation allocation;
  gdouble value;

  gtk_widget_get_allocation (priv->tree_view, &allocation);

  adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window));
  if (adj && gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj) > gtk_adjustment_get_page_size (adj))
    {
      if (x <= allocation.x &&
          gtk_adjustment_get_lower (adj) < gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) - (allocation.x - x + 1);
          gtk_adjustment_set_value (adj, value);
        }
      else if (x >= allocation.x + allocation.width &&
               gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj) > gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) + (x - allocation.x - allocation.width + 1);
          gtk_adjustment_set_value (adj, MAX (value, 0.0));
        }
    }

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window));
  if (adj && gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj) > gtk_adjustment_get_page_size (adj))
    {
      if (y <= allocation.y &&
          gtk_adjustment_get_lower (adj) < gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) - (allocation.y - y + 1);
          gtk_adjustment_set_value (adj, value);
        }
      else if (y >= allocation.height &&
               gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj) > gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) + (y - allocation.height + 1);
          gtk_adjustment_set_value (adj, MAX (value, 0.0));
        }
    }
}

static gboolean
gtk_combo_box_list_scroll_timeout (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  gint x, y;

  if (priv->auto_scroll)
    {
      gdk_window_get_device_position (gtk_widget_get_window (priv->tree_view),
                                      priv->grab_pointer,
                                      &x, &y, NULL);
      gtk_combo_box_list_auto_scroll (combo_box, x, y);
    }

  return TRUE;
}

static gboolean
gtk_combo_box_list_enter_notify (GtkWidget        *widget,
                                 GdkEventCrossing *event,
                                 gpointer          data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  combo_box->priv->auto_scroll = TRUE;

  return TRUE;
}

static gboolean
gtk_combo_box_list_select_func (GtkTreeSelection *selection,
                                GtkTreeModel     *model,
                                GtkTreePath      *path,
                                gboolean          path_currently_selected,
                                gpointer          data)
{
  GList *list, *columns;
  gboolean sensitive = FALSE;

  columns = gtk_tree_view_get_columns (gtk_tree_selection_get_tree_view (selection));

  for (list = columns; list && !sensitive; list = list->next)
    {
      GList *cells, *cell;
      gboolean cell_sensitive, cell_visible;
      GtkTreeIter iter;
      GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (list->data);

      if (!gtk_tree_view_column_get_visible (column))
        continue;

      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_view_column_cell_set_cell_data (column, model, &iter,
                                               FALSE, FALSE);

      cell = cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
      while (cell)
        {
          g_object_get (cell->data,
                        "sensitive", &cell_sensitive,
                        "visible", &cell_visible,
                        NULL);

          if (cell_visible && cell_sensitive)
            {
              sensitive = TRUE;
              break;
            }

          cell = cell->next;
        }

      g_list_free (cells);
    }

  g_list_free (columns);

  return sensitive;
}

static void
gtk_combo_box_list_row_changed (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      data)
{
  /* XXX Do nothing ? */
}

/*
 * GtkCellLayout implementation
 */
static GtkCellArea *
gtk_combo_box_cell_layout_get_area (GtkCellLayout *cell_layout)
{
  GtkComboBox *combo = GTK_COMBO_BOX (cell_layout);
  GtkComboBoxPrivate *priv = combo->priv;

  if (G_UNLIKELY (!priv->area))
    {
      priv->area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->area);
    }

  return priv->area;
}

/*
 * public API
 */

/**
 * gtk_combo_box_new:
 *
 * Creates a new empty #GtkComboBox.
 *
 * Returns: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new (void)
{
  return g_object_new (GTK_TYPE_COMBO_BOX, NULL);
}

/**
 * gtk_combo_box_new_with_area:
 * @area: the #GtkCellArea to use to layout cell renderers
 *
 * Creates a new empty #GtkComboBox using @area to layout cells.
 *
 * Returns: A new #GtkComboBox.
 */
GtkWidget *
gtk_combo_box_new_with_area (GtkCellArea  *area)
{
  return g_object_new (GTK_TYPE_COMBO_BOX, "cell-area", area, NULL);
}

/**
 * gtk_combo_box_new_with_area_and_entry:
 * @area: the #GtkCellArea to use to layout cell renderers
 *
 * Creates a new empty #GtkComboBox with an entry.
 *
 * The new combo box will use @area to layout cells.
 *
 * Returns: A new #GtkComboBox.
 */
GtkWidget *
gtk_combo_box_new_with_area_and_entry (GtkCellArea *area)
{
  return g_object_new (GTK_TYPE_COMBO_BOX,
                       "has-entry", TRUE,
                       "cell-area", area,
                       NULL);
}


/**
 * gtk_combo_box_new_with_entry:
 *
 * Creates a new empty #GtkComboBox with an entry.
 *
 * Returns: A new #GtkComboBox.
 *
 * Since: 2.24
 */
GtkWidget *
gtk_combo_box_new_with_entry (void)
{
  return g_object_new (GTK_TYPE_COMBO_BOX, "has-entry", TRUE, NULL);
}

/**
 * gtk_combo_box_new_with_model:
 * @model: A #GtkTreeModel.
 *
 * Creates a new #GtkComboBox with the model initialized to @model.
 *
 * Returns: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new_with_model (GtkTreeModel *model)
{
  GtkComboBox *combo_box;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

  combo_box = g_object_new (GTK_TYPE_COMBO_BOX, "model", model, NULL);

  return GTK_WIDGET (combo_box);
}

/**
 * gtk_combo_box_new_with_model_and_entry:
 * @model: A #GtkTreeModel
 *
 * Creates a new empty #GtkComboBox with an entry
 * and with the model initialized to @model.
 *
 * Returns: A new #GtkComboBox
 *
 * Since: 2.24
 */
GtkWidget *
gtk_combo_box_new_with_model_and_entry (GtkTreeModel *model)
{
  return g_object_new (GTK_TYPE_COMBO_BOX,
                       "has-entry", TRUE,
                       "model", model,
                       NULL);
}

/**
 * gtk_combo_box_get_wrap_width:
 * @combo_box: A #GtkComboBox
 *
 * Returns the wrap width which is used to determine the number of columns
 * for the popup menu. If the wrap width is larger than 1, the combo box
 * is in table mode.
 *
 * Returns: the wrap width.
 *
 * Since: 2.6
 */
gint
gtk_combo_box_get_wrap_width (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), -1);

  return combo_box->priv->wrap_width;
}

/**
 * gtk_combo_box_set_wrap_width:
 * @combo_box: A #GtkComboBox
 * @width: Preferred number of columns
 *
 * Sets the wrap width of @combo_box to be @width. The wrap width is basically
 * the preferred number of columns when you want the popup to be layed out
 * in a table.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_wrap_width (GtkComboBox *combo_box,
                              gint         width)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (width >= 0);

  priv = combo_box->priv;

  if (width != priv->wrap_width)
    {
      priv->wrap_width = width;

      gtk_combo_box_check_appearance (combo_box);

      if (GTK_IS_TREE_MENU (priv->popup_widget))
        _gtk_tree_menu_set_wrap_width (GTK_TREE_MENU (priv->popup_widget), priv->wrap_width);

      g_object_notify (G_OBJECT (combo_box), "wrap-width");
    }
}

/**
 * gtk_combo_box_get_row_span_column:
 * @combo_box: A #GtkComboBox
 *
 * Returns the column with row span information for @combo_box.
 *
 * Returns: the row span column.
 *
 * Since: 2.6
 */
gint
gtk_combo_box_get_row_span_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), -1);

  return combo_box->priv->row_column;
}

/**
 * gtk_combo_box_set_row_span_column:
 * @combo_box: A #GtkComboBox.
 * @row_span: A column in the model passed during construction.
 *
 * Sets the column with row span information for @combo_box to be @row_span.
 * The row span column contains integers which indicate how many rows
 * an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_row_span_column (GtkComboBox *combo_box,
                                   gint         row_span)
{
  GtkComboBoxPrivate *priv;
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  col = gtk_tree_model_get_n_columns (priv->model);
  g_return_if_fail (row_span >= -1 && row_span < col);

  if (row_span != priv->row_column)
    {
      priv->row_column = row_span;

      if (GTK_IS_TREE_MENU (priv->popup_widget))
        _gtk_tree_menu_set_row_span_column (GTK_TREE_MENU (priv->popup_widget), priv->row_column);

      g_object_notify (G_OBJECT (combo_box), "row-span-column");
    }
}

/**
 * gtk_combo_box_get_column_span_column:
 * @combo_box: A #GtkComboBox
 *
 * Returns the column with column span information for @combo_box.
 *
 * Returns: the column span column.
 *
 * Since: 2.6
 */
gint
gtk_combo_box_get_column_span_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), -1);

  return combo_box->priv->col_column;
}

/**
 * gtk_combo_box_set_column_span_column:
 * @combo_box: A #GtkComboBox
 * @column_span: A column in the model passed during construction
 *
 * Sets the column with column span information for @combo_box to be
 * @column_span. The column span column contains integers which indicate
 * how many columns an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_column_span_column (GtkComboBox *combo_box,
                                      gint         column_span)
{
  GtkComboBoxPrivate *priv;
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  col = gtk_tree_model_get_n_columns (priv->model);
  g_return_if_fail (column_span >= -1 && column_span < col);

  if (column_span != priv->col_column)
    {
      priv->col_column = column_span;

      if (GTK_IS_TREE_MENU (priv->popup_widget))
        _gtk_tree_menu_set_column_span_column (GTK_TREE_MENU (priv->popup_widget), priv->col_column);

      g_object_notify (G_OBJECT (combo_box), "column-span-column");
    }
}

/**
 * gtk_combo_box_get_active:
 * @combo_box: A #GtkComboBox
 *
 * Returns the index of the currently active item, or -1 if there’s no
 * active item. If the model is a non-flat treemodel, and the active item
 * is not an immediate child of the root of the tree, this function returns
 * `gtk_tree_path_get_indices (path)[0]`, where
 * `path` is the #GtkTreePath of the active item.
 *
 * Returns: An integer which is the index of the currently active item,
 *     or -1 if there’s no active item.
 *
 * Since: 2.4
 */
gint
gtk_combo_box_get_active (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv;
  gint result;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  priv = combo_box->priv;

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (priv->active_row);
      result = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }
  else
    result = -1;

  return result;
}

/**
 * gtk_combo_box_set_active:
 * @combo_box: A #GtkComboBox
 * @index_: An index in the model passed during construction, or -1 to have
 * no active item
 *
 * Sets the active item of @combo_box to be the item at @index.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_active (GtkComboBox *combo_box,
                          gint         index_)
{
  GtkComboBoxPrivate *priv;
  GtkTreePath *path = NULL;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (index_ >= -1);

  priv = combo_box->priv;

  if (priv->model == NULL)
    {
      /* Save index, in case the model is set after the index */
      priv->active = index_;
      if (index_ != -1)
        return;
    }

  if (index_ != -1)
    path = gtk_tree_path_new_from_indices (index_, -1);

  gtk_combo_box_set_active_internal (combo_box, path);

  if (path)
    gtk_tree_path_free (path);
}

static void
gtk_combo_box_set_active_internal (GtkComboBox *combo_box,
                                   GtkTreePath *path)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreePath *active_path;
  gint path_cmp;

  /* Remember whether the initially active row is valid. */
  gboolean is_valid_row_reference = gtk_tree_row_reference_valid (priv->active_row);

  if (path && is_valid_row_reference)
    {
      active_path = gtk_tree_row_reference_get_path (priv->active_row);
      path_cmp = gtk_tree_path_compare (path, active_path);
      gtk_tree_path_free (active_path);
      if (path_cmp == 0)
        return;
    }

  if (priv->active_row)
    {
      gtk_tree_row_reference_free (priv->active_row);
      priv->active_row = NULL;
    }

  if (!path)
    {
      if (priv->tree_view)
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view)));
      else
        {
          GtkMenu *menu = GTK_MENU (priv->popup_widget);

          if (GTK_IS_MENU (menu))
            gtk_menu_set_active (menu, -1);
        }

      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view), NULL);

      /*
       *  Do not emit a "changed" signal when an already invalid selection was
       *  now set to invalid.
       */
      if (!is_valid_row_reference)
        return;
    }
  else
    {
      priv->active_row =
        gtk_tree_row_reference_new (priv->model, path);

      if (priv->tree_view)
        {
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view),
                                    path, NULL, FALSE);
        }
      else if (GTK_IS_MENU (priv->popup_widget))
        {
          /* FIXME handle nested menus better */
          gtk_menu_set_active (GTK_MENU (priv->popup_widget),
                               gtk_tree_path_get_indices (path)[0]);
        }

      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view),
                                         path);
    }

  g_signal_emit (combo_box, combo_box_signals[CHANGED], 0);
  g_object_notify (G_OBJECT (combo_box), "active");
  if (priv->id_column >= 0)
    g_object_notify (G_OBJECT (combo_box), "active-id");
}


/**
 * gtk_combo_box_get_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: (out): A #GtkTreeIter
 *
 * Sets @iter to point to the currently active item, if any item is active.
 * Otherwise, @iter is left unchanged.
 *
 * Returns: %TRUE if @iter was set, %FALSE otherwise
 *
 * Since: 2.4
 */
gboolean
gtk_combo_box_get_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkComboBoxPrivate *priv;
  GtkTreePath *path;
  gboolean result;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  priv = combo_box->priv;

  if (!gtk_tree_row_reference_valid (priv->active_row))
    return FALSE;

  path = gtk_tree_row_reference_get_path (priv->active_row);
  result = gtk_tree_model_get_iter (priv->model, iter, path);
  gtk_tree_path_free (path);

  return result;
}

/**
 * gtk_combo_box_set_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: (allow-none): The #GtkTreeIter, or %NULL
 *
 * Sets the current active item to be the one referenced by @iter, or
 * unsets the active item if @iter is %NULL.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkTreePath *path = NULL;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (iter)
    path = gtk_tree_model_get_path (gtk_combo_box_get_model (combo_box), iter);

  gtk_combo_box_set_active_internal (combo_box, path);
  gtk_tree_path_free (path);
}

/**
 * gtk_combo_box_set_model:
 * @combo_box: A #GtkComboBox
 * @model: (allow-none): A #GtkTreeModel
 *
 * Sets the model used by @combo_box to be @model. Will unset a previously set
 * model (if applicable). If model is %NULL, then it will unset the model.
 *
 * Note that this function does not clear the cell renderers, you have to
 * call gtk_cell_layout_clear() yourself if you need to set up different
 * cell renderers for the new model.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_model (GtkComboBox  *combo_box,
                         GtkTreeModel *model)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  priv = combo_box->priv;

  if (model == priv->model)
    return;

  gtk_combo_box_unset_model (combo_box);

  if (model == NULL)
    goto out;

  priv->model = model;
  g_object_ref (priv->model);

  g_signal_connect (priv->model, "row-inserted",
                    G_CALLBACK (gtk_combo_box_model_row_inserted),
                    combo_box);
  g_signal_connect (priv->model, "row-deleted",
                    G_CALLBACK (gtk_combo_box_model_row_deleted),
                    combo_box);
  g_signal_connect (priv->model, "rows-reordered",
                    G_CALLBACK (gtk_combo_box_model_rows_reordered),
                    combo_box);
  g_signal_connect (priv->model, "row-changed",
                    G_CALLBACK (gtk_combo_box_model_row_changed),
                    combo_box);

  if (priv->tree_view)
    {
      /* list mode */
      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view),
                               priv->model);
      gtk_combo_box_list_popup_resize (combo_box);
    }

  if (GTK_IS_TREE_MENU (priv->popup_widget))
    {
      /* menu mode */
      _gtk_tree_menu_set_model (GTK_TREE_MENU (priv->popup_widget),
                                priv->model);
    }

  if (priv->cell_view)
    gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view),
                             priv->model);

  if (priv->active != -1)
    {
      /* If an index was set in advance, apply it now */
      gtk_combo_box_set_active (combo_box, priv->active);
      priv->active = -1;
    }

out:
  gtk_combo_box_update_sensitivity (combo_box);

  g_object_notify (G_OBJECT (combo_box), "model");
}

/**
 * gtk_combo_box_get_model:
 * @combo_box: A #GtkComboBox
 *
 * Returns the #GtkTreeModel which is acting as data source for @combo_box.
 *
 * Returns: (transfer none): A #GtkTreeModel which was passed
 *     during construction.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_combo_box_get_model (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->model;
}

static void
gtk_combo_box_real_move_active (GtkComboBox   *combo_box,
                                GtkScrollType  scroll)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeIter iter;
  GtkTreeIter new_iter;
  gboolean    active_iter;
  gboolean    found;

  if (!priv->model)
    {
      gtk_widget_error_bell (GTK_WIDGET (combo_box));
      return;
    }

  active_iter = gtk_combo_box_get_active_iter (combo_box, &iter);

  switch (scroll)
    {
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_UP:
    case GTK_SCROLL_STEP_LEFT:
      if (active_iter)
        {
          found = tree_prev (combo_box, priv->model,
                             &iter, &new_iter);
          break;
        }
      /* else fall through */

    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_PAGE_DOWN:
    case GTK_SCROLL_PAGE_RIGHT:
    case GTK_SCROLL_END:
      found = tree_last (combo_box, priv->model, &new_iter);
      break;

    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_STEP_DOWN:
    case GTK_SCROLL_STEP_RIGHT:
      if (active_iter)
        {
          found = tree_next (combo_box, priv->model,
                             &iter, &new_iter);
          break;
        }
      /* else fall through */

    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_UP:
    case GTK_SCROLL_PAGE_LEFT:
    case GTK_SCROLL_START:
      found = tree_first (combo_box, priv->model, &new_iter);
      break;

    default:
      return;
    }

  if (found && active_iter)
    {
      GtkTreePath *old_path;
      GtkTreePath *new_path;

      old_path = gtk_tree_model_get_path (priv->model, &iter);
      new_path = gtk_tree_model_get_path (priv->model, &new_iter);

      if (gtk_tree_path_compare (old_path, new_path) == 0)
        found = FALSE;

      gtk_tree_path_free (old_path);
      gtk_tree_path_free (new_path);
    }

  if (found)
    {
      gtk_combo_box_set_active_iter (combo_box, &new_iter);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (combo_box));
    }
}

static gboolean
gtk_combo_box_mnemonic_activate (GtkWidget *widget,
                                 gboolean   group_cycling)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (combo_box->priv->has_entry)
    {
      GtkWidget* child;

      child = gtk_bin_get_child (GTK_BIN (combo_box));
      if (child)
        gtk_widget_grab_focus (child);
    }
  else
    gtk_widget_grab_focus (combo_box->priv->button);

  return TRUE;
}

static void
gtk_combo_box_grab_focus (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (combo_box->priv->has_entry)
    {
      GtkWidget *child;

      child = gtk_bin_get_child (GTK_BIN (combo_box));
      if (child)
        gtk_widget_grab_focus (child);
    }
  else
    gtk_widget_grab_focus (combo_box->priv->button);
}

static void
gtk_combo_box_unmap (GtkWidget *widget)
{
  gtk_combo_box_popdown (GTK_COMBO_BOX (widget));

  GTK_WIDGET_CLASS (gtk_combo_box_parent_class)->unmap (widget);
}

static void
gtk_combo_box_destroy (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->popup_idle_id > 0)
    {
      g_source_remove (priv->popup_idle_id);
      priv->popup_idle_id = 0;
    }

  g_clear_pointer (&priv->trigger_event, gdk_event_free);

  if (priv->box)
    {
      /* destroy things (unparent will kill the latest ref from us)
       * last unref on button will destroy the arrow
       */
      gtk_widget_unparent (priv->box);
      priv->box = NULL;
      priv->button = NULL;
      priv->arrow = NULL;
      priv->cell_view = NULL;
      _gtk_bin_set_child (GTK_BIN (combo_box), NULL);
    }

  if (priv->row_separator_destroy)
    priv->row_separator_destroy (priv->row_separator_data);

  priv->row_separator_func = NULL;
  priv->row_separator_data = NULL;
  priv->row_separator_destroy = NULL;

  GTK_WIDGET_CLASS (gtk_combo_box_parent_class)->destroy (widget);
  priv->cell_view = NULL;
}

static void
gtk_combo_box_entry_contents_changed (GtkEntry *entry,
                                      gpointer  user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  /*
   *  Fixes regression reported in bug #574059. The old functionality relied on
   *  bug #572478.  As a bugfix, we now emit the "changed" signal ourselves
   *  when the selection was already set to -1.
   */
  if (gtk_combo_box_get_active(combo_box) == -1)
    g_signal_emit_by_name (combo_box, "changed");
  else
    gtk_combo_box_set_active (combo_box, -1);
}

static void
gtk_combo_box_entry_active_changed (GtkComboBox *combo_box,
                                    gpointer     user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      GtkEntry *entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box)));

      if (entry)
        {
          GtkTreePath *path;
          gchar       *path_str;
          gchar       *text = NULL;

          model    = gtk_combo_box_get_model (combo_box);
          path     = gtk_tree_model_get_path (model, &iter);
          path_str = gtk_tree_path_to_string (path);

          g_signal_handlers_block_by_func (entry,
                                           gtk_combo_box_entry_contents_changed,
                                           combo_box);


          g_signal_emit (combo_box, combo_box_signals[FORMAT_ENTRY_TEXT], 0,
                         path_str, &text);

          gtk_entry_set_text (entry, text);

          g_signal_handlers_unblock_by_func (entry,
                                             gtk_combo_box_entry_contents_changed,
                                             combo_box);

          gtk_tree_path_free (path);
          g_free (text);
          g_free (path_str);
        }
    }
}

static gchar *
gtk_combo_box_format_entry_text (GtkComboBox     *combo_box,
                                 const gchar     *path)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  gchar              *text = NULL;

  if (priv->text_column >= 0)
    {
      model = gtk_combo_box_get_model (combo_box);
      gtk_tree_model_get_iter_from_string (model, &iter, path);

      gtk_tree_model_get (model, &iter,
                          priv->text_column, &text,
                          -1);
    }

  return text;
}

static void
gtk_combo_box_constructed (GObject *object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = combo_box->priv;

  G_OBJECT_CLASS (gtk_combo_box_parent_class)->constructed (object);

  if (!priv->area)
    {
      priv->area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->area);
    }

  gtk_combo_box_create_child (combo_box);

  gtk_combo_box_check_appearance (combo_box);

  if (priv->has_entry)
    {
      priv->text_renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box),
                                  priv->text_renderer, TRUE);

      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), -1);
    }
}


static void
gtk_combo_box_dispose(GObject* object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_combo_box_menu_destroy (combo_box);
      gtk_menu_detach (GTK_MENU (priv->popup_widget));
      priv->popup_widget = NULL;
    }

  if (priv->area)
    {
      g_object_unref (priv->area);
      priv->area = NULL;
    }

  if (GTK_IS_TREE_VIEW (priv->tree_view))
    gtk_combo_box_list_destroy (combo_box);

  if (priv->popup_window)
    {
      gtk_widget_destroy (priv->popup_window);
      priv->popup_window = NULL;
    }

  gtk_combo_box_unset_model (combo_box);

  G_OBJECT_CLASS (gtk_combo_box_parent_class)->dispose (object);
}

static void
gtk_combo_box_finalize (GObject *object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  g_free (combo_box->priv->tearoff_title);
  g_clear_object (&combo_box->priv->gadget);

  G_OBJECT_CLASS (gtk_combo_box_parent_class)->finalize (object);
}

static gboolean
gtk_cell_editable_key_press (GtkWidget   *widget,
                             GdkEventKey *event,
                             gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (event->keyval == GDK_KEY_Escape)
    {
      g_object_set (combo_box,
                    "editing-canceled", TRUE,
                    NULL);
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

      return TRUE;
    }
  else if (event->keyval == GDK_KEY_Return ||
           event->keyval == GDK_KEY_ISO_Enter ||
           event->keyval == GDK_KEY_KP_Enter)
    {
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

      return TRUE;
    }

  return FALSE;
}

static gboolean
popdown_idle (gpointer data)
{
  GtkComboBox *combo_box;

  combo_box = GTK_COMBO_BOX (data);

  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

  g_object_unref (combo_box);

  return FALSE;
}

static void
popdown_handler (GtkWidget *widget,
                 gpointer   data)
{
  guint id;
  id = gdk_threads_add_idle (popdown_idle, g_object_ref (data));
  g_source_set_name_by_id (id, "[gtk+] popdown_idle");
}

static gboolean
popup_idle (gpointer data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (GTK_IS_MENU (priv->popup_widget) &&
      priv->cell_view)
    g_signal_connect_object (priv->popup_widget,
                             "unmap", G_CALLBACK (popdown_handler),
                             combo_box, 0);

  /* we unset this if a menu item is activated */
  g_object_set (combo_box,
                "editing-canceled", TRUE,
                NULL);
  gtk_combo_box_popup (combo_box);

  g_clear_pointer (&priv->trigger_event, gdk_event_free);

  priv->popup_idle_id = 0;

  return FALSE;
}

static void
gtk_combo_box_start_editing (GtkCellEditable *cell_editable,
                             GdkEvent        *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (cell_editable);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child;

  priv->is_cell_renderer = TRUE;

  if (priv->cell_view)
    {
      g_signal_connect_object (priv->button, "key-press-event",
                               G_CALLBACK (gtk_cell_editable_key_press),
                               cell_editable, 0);

      gtk_widget_grab_focus (priv->button);
    }
  else
    {
      child = gtk_bin_get_child (GTK_BIN (combo_box));

      g_signal_connect_object (child, "key-press-event",
                               G_CALLBACK (gtk_cell_editable_key_press),
                               cell_editable, 0);

      gtk_widget_grab_focus (child);
      gtk_widget_set_can_focus (priv->button, FALSE);
    }

  /* we do the immediate popup only for the optionmenu-like
   * appearance
   */
  if (priv->is_cell_renderer &&
      priv->cell_view && !priv->tree_view)
    {
      g_clear_pointer (&priv->trigger_event, gdk_event_free);

      if (event)
        priv->trigger_event = gdk_event_copy (event);
      else
        priv->trigger_event = gtk_get_current_event ();

      priv->popup_idle_id =
          gdk_threads_add_idle (popup_idle, combo_box);
      g_source_set_name_by_id (priv->popup_idle_id, "[gtk+] popup_idle");
    }
}


/**
 * gtk_combo_box_get_add_tearoffs:
 * @combo_box: a #GtkComboBox
 *
 * Gets the current value of the :add-tearoffs property.
 *
 * Returns: the current value of the :add-tearoffs property.
 *
 * Deprecated: 3.10
 */
gboolean
gtk_combo_box_get_add_tearoffs (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->add_tearoffs;
}

/**
 * gtk_combo_box_set_add_tearoffs:
 * @combo_box: a #GtkComboBox
 * @add_tearoffs: %TRUE to add tearoff menu items
 *
 * Sets whether the popup menu should have a tearoff
 * menu item.
 *
 * Since: 2.6
 *
 * Deprecated: 3.10
 */
void
gtk_combo_box_set_add_tearoffs (GtkComboBox *combo_box,
                                gboolean     add_tearoffs)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;
  add_tearoffs = add_tearoffs != FALSE;

  if (priv->add_tearoffs != add_tearoffs)
    {
      priv->add_tearoffs = add_tearoffs;
      gtk_combo_box_check_appearance (combo_box);

      if (GTK_IS_TREE_MENU (priv->popup_widget))
        _gtk_tree_menu_set_tearoff (GTK_TREE_MENU (priv->popup_widget),
                                    priv->add_tearoffs);

      g_object_notify (G_OBJECT (combo_box), "add-tearoffs");
    }
}

/**
 * gtk_combo_box_get_title:
 * @combo_box: a #GtkComboBox
 *
 * Gets the current title of the menu in tearoff mode. See
 * gtk_combo_box_set_add_tearoffs().
 *
 * Returns: the menu’s title in tearoff mode. This is an internal copy of the
 * string which must not be freed.
 *
 * Since: 2.10
 *
 * Deprecated: 3.10
 */
const gchar*
gtk_combo_box_get_title (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->tearoff_title;
}

static void
gtk_combo_box_update_title (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  gtk_combo_box_check_appearance (combo_box);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (priv->popup_widget && GTK_IS_MENU (priv->popup_widget))
    gtk_menu_set_title (GTK_MENU (priv->popup_widget), priv->tearoff_title);
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gtk_combo_box_set_title:
 * @combo_box: a #GtkComboBox
 * @title: a title for the menu in tearoff mode
 *
 * Sets the menu’s title in tearoff mode.
 *
 * Since: 2.10
 *
 * Deprecated: 3.10
 */
void
gtk_combo_box_set_title (GtkComboBox *combo_box,
                         const gchar *title)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  if (strcmp (title ? title : "",
              priv->tearoff_title ? priv->tearoff_title : "") != 0)
    {
      g_free (priv->tearoff_title);
      priv->tearoff_title = g_strdup (title);

      gtk_combo_box_update_title (combo_box);

      g_object_notify (G_OBJECT (combo_box), "tearoff-title");
    }
}


/**
 * gtk_combo_box_set_popup_fixed_width:
 * @combo_box: a #GtkComboBox
 * @fixed: whether to use a fixed popup width
 *
 * Specifies whether the popup’s width should be a fixed width
 * matching the allocated width of the combo box.
 *
 * Since: 3.0
 **/
void
gtk_combo_box_set_popup_fixed_width (GtkComboBox *combo_box,
                                     gboolean     fixed)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  if (priv->popup_fixed_width != fixed)
    {
      priv->popup_fixed_width = fixed;

      g_object_notify (G_OBJECT (combo_box), "popup-fixed-width");
    }
}

/**
 * gtk_combo_box_get_popup_fixed_width:
 * @combo_box: a #GtkComboBox
 *
 * Gets whether the popup uses a fixed width matching
 * the allocated width of the combo box.
 *
 * Returns: %TRUE if the popup uses a fixed width
 *
 * Since: 3.0
 **/
gboolean
gtk_combo_box_get_popup_fixed_width (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->popup_fixed_width;
}


/**
 * gtk_combo_box_get_popup_accessible:
 * @combo_box: a #GtkComboBox
 *
 * Gets the accessible object corresponding to the combo box’s popup.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Returns: (transfer none): the accessible object corresponding
 *     to the combo box’s popup.
 *
 * Since: 2.6
 */
AtkObject *
gtk_combo_box_get_popup_accessible (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  priv = combo_box->priv;

  if (priv->popup_widget)
    return gtk_widget_get_accessible (priv->popup_widget);

  return NULL;
}

/**
 * gtk_combo_box_get_row_separator_func: (skip)
 * @combo_box: a #GtkComboBox
 *
 * Returns the current row separator function.
 *
 * Returns: the current row separator function.
 *
 * Since: 2.6
 */
GtkTreeViewRowSeparatorFunc
gtk_combo_box_get_row_separator_func (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->row_separator_func;
}

/**
 * gtk_combo_box_set_row_separator_func:
 * @combo_box: a #GtkComboBox
 * @func: a #GtkTreeViewRowSeparatorFunc
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): destroy notifier for @data, or %NULL
 *
 * Sets the row separator function, which is used to determine
 * whether a row should be drawn as a separator. If the row separator
 * function is %NULL, no separators are drawn. This is the default value.
 *
 * Since: 2.6
 */
void
gtk_combo_box_set_row_separator_func (GtkComboBox                 *combo_box,
                                      GtkTreeViewRowSeparatorFunc  func,
                                      gpointer                     data,
                                      GDestroyNotify               destroy)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  if (priv->row_separator_destroy)
    priv->row_separator_destroy (priv->row_separator_data);

  priv->row_separator_func = func;
  priv->row_separator_data = data;
  priv->row_separator_destroy = destroy;

  /* Provoke the underlying treeview/menu to rebuild themselves with the new separator func */
  if (priv->tree_view)
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), NULL);
      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), priv->model);
    }

  if (GTK_IS_TREE_MENU (priv->popup_widget))
    {
      _gtk_tree_menu_set_model (GTK_TREE_MENU (priv->popup_widget), NULL);
      _gtk_tree_menu_set_model (GTK_TREE_MENU (priv->popup_widget), priv->model);
    }

  gtk_widget_queue_draw (GTK_WIDGET (combo_box));
}

/**
 * gtk_combo_box_set_button_sensitivity:
 * @combo_box: a #GtkComboBox
 * @sensitivity: specify the sensitivity of the dropdown button
 *
 * Sets whether the dropdown button of the combo box should be
 * always sensitive (%GTK_SENSITIVITY_ON), never sensitive (%GTK_SENSITIVITY_OFF)
 * or only if there is at least one item to display (%GTK_SENSITIVITY_AUTO).
 *
 * Since: 2.14
 **/
void
gtk_combo_box_set_button_sensitivity (GtkComboBox        *combo_box,
                                      GtkSensitivityType  sensitivity)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  if (priv->button_sensitivity != sensitivity)
    {
      priv->button_sensitivity = sensitivity;
      gtk_combo_box_update_sensitivity (combo_box);

      g_object_notify (G_OBJECT (combo_box), "button-sensitivity");
    }
}

/**
 * gtk_combo_box_get_button_sensitivity:
 * @combo_box: a #GtkComboBox
 *
 * Returns whether the combo box sets the dropdown button
 * sensitive or not when there are no items in the model.
 *
 * Returns: %GTK_SENSITIVITY_ON if the dropdown button
 *    is sensitive when the model is empty, %GTK_SENSITIVITY_OFF
 *    if the button is always insensitive or
 *    %GTK_SENSITIVITY_AUTO if it is only sensitive as long as
 *    the model has one item to be selected.
 *
 * Since: 2.14
 **/
GtkSensitivityType
gtk_combo_box_get_button_sensitivity (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->button_sensitivity;
}


/**
 * gtk_combo_box_get_has_entry:
 * @combo_box: a #GtkComboBox
 *
 * Returns whether the combo box has an entry.
 *
 * Returns: whether there is an entry in @combo_box.
 *
 * Since: 2.24
 **/
gboolean
gtk_combo_box_get_has_entry (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->has_entry;
}

/**
 * gtk_combo_box_set_entry_text_column:
 * @combo_box: A #GtkComboBox
 * @text_column: A column in @model to get the strings from for
 *     the internal entry
 *
 * Sets the model column which @combo_box should use to get strings from
 * to be @text_column. The column @text_column in the model of @combo_box
 * must be of type %G_TYPE_STRING.
 *
 * This is only relevant if @combo_box has been created with
 * #GtkComboBox:has-entry as %TRUE.
 *
 * Since: 2.24
 */
void
gtk_combo_box_set_entry_text_column (GtkComboBox *combo_box,
                                     gint         text_column)
{
  GtkComboBoxPrivate *priv;
  GtkTreeModel *model;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;
  model = gtk_combo_box_get_model (combo_box);

  g_return_if_fail (text_column >= 0);
  g_return_if_fail (model == NULL || text_column < gtk_tree_model_get_n_columns (model));

  if (priv->text_column != text_column)
    {
      priv->text_column = text_column;

      if (priv->text_renderer != NULL)
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box),
                                        priv->text_renderer,
                                        "text", text_column,
                                        NULL);

      g_object_notify (G_OBJECT (combo_box), "entry-text-column");
    }
}

/**
 * gtk_combo_box_get_entry_text_column:
 * @combo_box: A #GtkComboBox.
 *
 * Returns the column which @combo_box is using to get the strings
 * from to display in the internal entry.
 *
 * Returns: A column in the data source model of @combo_box.
 *
 * Since: 2.24
 */
gint
gtk_combo_box_get_entry_text_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return combo_box->priv->text_column;
}

/**
 * gtk_combo_box_set_focus_on_click:
 * @combo: a #GtkComboBox
 * @focus_on_click: whether the combo box grabs focus when clicked
 *    with the mouse
 *
 * Sets whether the combo box will grab focus when it is clicked with
 * the mouse. Making mouse clicks not grab focus is useful in places
 * like toolbars where you don’t want the keyboard focus removed from
 * the main area of the application.
 *
 * Since: 2.6
 *
 * Deprecated: 3.20: Use gtk_widget_set_focus_on_click() instead
 */
void
gtk_combo_box_set_focus_on_click (GtkComboBox *combo_box,
                                  gboolean     focus_on_click)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  gtk_widget_set_focus_on_click (GTK_WIDGET (combo_box), focus_on_click);
}

/**
 * gtk_combo_box_get_focus_on_click:
 * @combo: a #GtkComboBox
 *
 * Returns whether the combo box grabs focus when it is clicked
 * with the mouse. See gtk_combo_box_set_focus_on_click().
 *
 * Returns: %TRUE if the combo box grabs focus when it is
 *     clicked with the mouse.
 *
 * Since: 2.6
 *
 * Deprecated: 3.20: Use gtk_widget_get_focus_on_click() instead
 */
gboolean
gtk_combo_box_get_focus_on_click (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);
  
  return gtk_widget_get_focus_on_click (GTK_WIDGET (combo_box));
}

static void
gtk_combo_box_buildable_add_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   GObject      *child,
                                   const gchar  *type)
{
  if (GTK_IS_WIDGET (child))
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
      return;
    }

  _gtk_cell_layout_buildable_add_child (buildable, builder, child, type);
}

static gboolean
gtk_combo_box_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *data)
{
  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, data))
    return TRUE;

  return _gtk_cell_layout_buildable_custom_tag_start (buildable, builder, child,
                                                      tagname, parser, data);
}

static void
gtk_combo_box_buildable_custom_tag_end (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const gchar  *tagname,
                                        gpointer     *data)
{
  if (!_gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname, data))
    parent_buildable_iface->custom_tag_end (buildable, builder, child, tagname, data);
}

static GObject *
gtk_combo_box_buildable_get_internal_child (GtkBuildable *buildable,
                                            GtkBuilder   *builder,
                                            const gchar  *childname)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (buildable);

  if (combo_box->priv->has_entry && strcmp (childname, "entry") == 0)
    return G_OBJECT (gtk_bin_get_child (GTK_BIN (buildable)));

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

/**
 * gtk_combo_box_set_id_column:
 * @combo_box: A #GtkComboBox
 * @id_column: A column in @model to get string IDs for values from
 *
 * Sets the model column which @combo_box should use to get string IDs
 * for values from. The column @id_column in the model of @combo_box
 * must be of type %G_TYPE_STRING.
 *
 * Since: 3.0
 */
void
gtk_combo_box_set_id_column (GtkComboBox *combo_box,
                             gint         id_column)
{
  GtkComboBoxPrivate *priv;
  GtkTreeModel *model;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  if (id_column != priv->id_column)
    {
      model = gtk_combo_box_get_model (combo_box);

      g_return_if_fail (id_column >= 0);
      g_return_if_fail (model == NULL ||
                        id_column < gtk_tree_model_get_n_columns (model));

      priv->id_column = id_column;

      g_object_notify (G_OBJECT (combo_box), "id-column");
      g_object_notify (G_OBJECT (combo_box), "active-id");
    }
}

/**
 * gtk_combo_box_get_id_column:
 * @combo_box: A #GtkComboBox
 *
 * Returns the column which @combo_box is using to get string IDs
 * for values from.
 *
 * Returns: A column in the data source model of @combo_box.
 *
 * Since: 3.0
 */
gint
gtk_combo_box_get_id_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return combo_box->priv->id_column;
}

/**
 * gtk_combo_box_get_active_id:
 * @combo_box: a #GtkComboBox
 *
 * Returns the ID of the active row of @combo_box.  This value is taken
 * from the active row and the column specified by the #GtkComboBox:id-column
 * property of @combo_box (see gtk_combo_box_set_id_column()).
 *
 * The returned value is an interned string which means that you can
 * compare the pointer by value to other interned strings and that you
 * must not free it.
 *
 * If the #GtkComboBox:id-column property of @combo_box is not set, or if
 * no row is active, or if the active row has a %NULL ID value, then %NULL
 * is returned.
 *
 * Returns: (nullable): the ID of the active row, or %NULL
 *
 * Since: 3.0
 **/
const gchar *
gtk_combo_box_get_active_id (GtkComboBox *combo_box)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint column;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  column = combo_box->priv->id_column;

  if (column < 0)
    return NULL;

  model = gtk_combo_box_get_model (combo_box);
  g_return_val_if_fail (gtk_tree_model_get_column_type (model, column) ==
                        G_TYPE_STRING, NULL);

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      const gchar *interned;
      gchar *id;

      gtk_tree_model_get (model, &iter, column, &id, -1);
      interned = g_intern_string (id);
      g_free (id);

      return interned;
    }

  return NULL;
}

/**
 * gtk_combo_box_set_active_id:
 * @combo_box: a #GtkComboBox
 * @active_id: (allow-none): the ID of the row to select, or %NULL
 *
 * Changes the active row of @combo_box to the one that has an ID equal to
 * @active_id, or unsets the active row if @active_id is %NULL.  Rows having
 * a %NULL ID string cannot be made active by this function.
 *
 * If the #GtkComboBox:id-column property of @combo_box is unset or if no
 * row has the given ID then the function does nothing and returns %FALSE.
 *
 * Returns: %TRUE if a row with a matching ID was found.  If a %NULL
 *          @active_id was given to unset the active row, the function
 *          always returns %TRUE.
 *
 * Since: 3.0
 **/
gboolean
gtk_combo_box_set_active_id (GtkComboBox *combo_box,
                             const gchar *active_id)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean match = FALSE;
  gint column;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  if (active_id == NULL)
    {
      gtk_combo_box_set_active (combo_box, -1);
      return TRUE;  /* active row was successfully unset */
    }

  column = combo_box->priv->id_column;

  if (column < 0)
    return FALSE;

  model = gtk_combo_box_get_model (combo_box);
  g_return_val_if_fail (gtk_tree_model_get_column_type (model, column) ==
                        G_TYPE_STRING, FALSE);

  if (gtk_tree_model_get_iter_first (model, &iter))
    do {
      gchar *id;

      gtk_tree_model_get (model, &iter, column, &id, -1);
      if (id != NULL)
        match = strcmp (id, active_id) == 0;
      g_free (id);

      if (match)
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          break;
        }
    } while (gtk_tree_model_iter_next (model, &iter));

  g_object_notify (G_OBJECT (combo_box), "active-id");

  return match;
}

GtkWidget *
gtk_combo_box_get_popup (GtkComboBox *combo)
{
  if (combo->priv->popup_window)
    return combo->priv->popup_window;
  else
    return combo->priv->popup_widget;
}
