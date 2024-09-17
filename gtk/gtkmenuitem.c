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
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <string.h>

#include "gtkaccellabel.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkmenuitemprivate.h"
#include "gtkmenubar.h"
#include "gtkmenuprivate.h"
#include "gtkseparatormenuitem.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "deprecated/gtkactivatable.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"
#include "a11y/gtkmenuitemaccessible.h"
#include "deprecated/gtktearoffmenuitem.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssstylepropertyprivate.h"

#define MENU_POPUP_DELAY     225

/**
 * SECTION:gtkmenuitem
 * @Short_description: The widget used for item in menus
 * @Title: GtkMenuItem
 * @See_also: #GtkBin, #GtkMenuShell
 *
 * The #GtkMenuItem widget and the derived widgets are the only valid
 * children for menus. Their function is to correctly handle highlighting,
 * alignment, events and submenus.
 *
 * As a GtkMenuItem derives from #GtkBin it can hold any valid child widget,
 * although only a few are really useful.
 *
 * By default, a GtkMenuItem sets a #GtkAccelLabel as its child.
 * GtkMenuItem has direct functions to set the label and its mnemonic.
 * For more advanced label settings, you can fetch the child widget from the GtkBin.
 *
 * An example for setting markup and accelerator on a MenuItem:
 *
 * |[<!-- language="C" -->
 * GtkWidget *menu_item = gtk_menu_item_new_with_label ("Example Menu Item");
 *
 * GtkWidget *child = gtk_bin_get_child (GTK_BIN (menu_item));
 * gtk_label_set_markup (GTK_LABEL (child), "<i>new label</i> with <b>markup</b>");
 * gtk_accel_label_set_accel (GTK_ACCEL_LABEL (child), GDK_KEY_1, 0);
 * ]|
 *
 * # GtkMenuItem as GtkBuildable
 *
 * The GtkMenuItem implementation of the #GtkBuildable interface supports
 * adding a submenu by specifying “submenu” as the “type” attribute of
 * a `<child>` element.
 *
 * An example of UI definition fragment with submenus:
 *
 * |[<!-- language="xml" -->
 * <object class="GtkMenuItem">
 *   <child type="submenu">
 *     <object class="GtkMenu"/>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * menuitem
 * ├── <child>
 * ╰── [arrow.right]
 * ]|
 *
 * GtkMenuItem has a single CSS node with name menuitem. If the menuitem
 * has a submenu, it gets another CSS node with name arrow, which has
 * the .left or .right style class.
 */


enum {
  ACTIVATE,
  ACTIVATE_ITEM,
  TOGGLE_SIZE_REQUEST,
  TOGGLE_SIZE_ALLOCATE,
  SELECT,
  DESELECT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_RIGHT_JUSTIFIED,
  PROP_SUBMENU,
  PROP_ACCEL_PATH,
  PROP_LABEL,
  PROP_USE_UNDERLINE,

  LAST_PROP,

  /* activatable properties */
  PROP_ACTIVATABLE_RELATED_ACTION = LAST_PROP,
  PROP_ACTIVATABLE_USE_ACTION_APPEARANCE,

  PROP_ACTION_NAME,
  PROP_ACTION_TARGET
};


static void gtk_menu_item_dispose        (GObject          *object);
static void gtk_menu_item_set_property   (GObject          *object,
                                          guint             prop_id,
                                          const GValue     *value,
                                          GParamSpec       *pspec);
static void gtk_menu_item_get_property   (GObject          *object,
                                          guint             prop_id,
                                          GValue           *value,
                                          GParamSpec       *pspec);
static void gtk_menu_item_destroy        (GtkWidget        *widget);
static void gtk_menu_item_realize        (GtkWidget        *widget);
static void gtk_menu_item_unrealize      (GtkWidget        *widget);
static void gtk_menu_item_map            (GtkWidget        *widget);
static void gtk_menu_item_unmap          (GtkWidget        *widget);
static gboolean gtk_menu_item_enter      (GtkWidget        *widget,
                                          GdkEventCrossing *event);
static gboolean gtk_menu_item_leave      (GtkWidget        *widget,
                                          GdkEventCrossing *event);
static void gtk_menu_item_parent_set     (GtkWidget        *widget,
                                          GtkWidget        *previous_parent);
static void gtk_menu_item_direction_changed (GtkWidget        *widget,
                                             GtkTextDirection  previous_dir);


static void gtk_real_menu_item_select               (GtkMenuItem *item);
static void gtk_real_menu_item_deselect             (GtkMenuItem *item);
static void gtk_real_menu_item_activate             (GtkMenuItem *item);
static void gtk_real_menu_item_activate_item        (GtkMenuItem *item);
static void gtk_real_menu_item_toggle_size_request  (GtkMenuItem *menu_item,
                                                     gint        *requisition);
static void gtk_real_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
                                                     gint         allocation);
static gboolean gtk_menu_item_mnemonic_activate     (GtkWidget   *widget,
                                                     gboolean     group_cycling);

static void gtk_menu_item_ensure_label   (GtkMenuItem      *menu_item);
static gint gtk_menu_item_popup_timeout  (gpointer          data);
static void gtk_menu_item_show_all       (GtkWidget        *widget);

static void gtk_menu_item_forall         (GtkContainer    *container,
                                          gboolean         include_internals,
                                          GtkCallback      callback,
                                          gpointer         callback_data);

static gboolean gtk_menu_item_can_activate_accel (GtkWidget *widget,
                                                  guint      signal_id);

static void gtk_real_menu_item_set_label (GtkMenuItem     *menu_item,
                                          const gchar     *label);
static const gchar * gtk_real_menu_item_get_label (GtkMenuItem *menu_item);

static void gtk_menu_item_buildable_interface_init (GtkBuildableIface   *iface);
static void gtk_menu_item_buildable_add_child      (GtkBuildable        *buildable,
                                                    GtkBuilder          *builder,
                                                    GObject             *child,
                                                    const gchar         *type);
static void gtk_menu_item_buildable_custom_finished(GtkBuildable        *buildable,
                                                    GtkBuilder          *builder,
                                                    GObject             *child,
                                                    const gchar         *tagname,
                                                    gpointer             user_data);

static void gtk_menu_item_actionable_interface_init  (GtkActionableInterface *iface);
static void gtk_menu_item_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_menu_item_update                     (GtkActivatable       *activatable,
                                                      GtkAction            *action,
                                                      const gchar          *property_name);
static void gtk_menu_item_sync_action_properties     (GtkActivatable       *activatable,
                                                      GtkAction            *action);
static void gtk_menu_item_set_related_action         (GtkMenuItem          *menu_item, 
                                                      GtkAction            *action);
static void gtk_menu_item_set_use_action_appearance  (GtkMenuItem          *menu_item, 
                                                      gboolean              use_appearance);

static guint menu_item_signals[LAST_SIGNAL] = { 0 };
static GParamSpec *menu_item_props[LAST_PROP];

static GtkBuildableIface *parent_buildable_iface;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_CODE (GtkMenuItem, gtk_menu_item, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkMenuItem)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_menu_item_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                gtk_menu_item_activatable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE,
                                                gtk_menu_item_actionable_interface_init))
G_GNUC_END_IGNORE_DEPRECATIONS;

static void
gtk_menu_item_set_action_name (GtkActionable *actionable,
                               const gchar   *action_name)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (actionable);

  if (!menu_item->priv->action_helper)
    menu_item->priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_name (menu_item->priv->action_helper, action_name);
}

static void
gtk_menu_item_set_action_target_value (GtkActionable *actionable,
                                       GVariant      *action_target)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (actionable);

  if (!menu_item->priv->action_helper)
    menu_item->priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (menu_item->priv->action_helper, action_target);
}

static const gchar *
gtk_menu_item_get_action_name (GtkActionable *actionable)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (actionable);

  return gtk_action_helper_get_action_name (menu_item->priv->action_helper);
}

static GVariant *
gtk_menu_item_get_action_target_value (GtkActionable *actionable)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (actionable);

  return gtk_action_helper_get_action_target_value (menu_item->priv->action_helper);
}

static void
gtk_menu_item_actionable_interface_init (GtkActionableInterface *iface)
{
  iface->set_action_name = gtk_menu_item_set_action_name;
  iface->get_action_name = gtk_menu_item_get_action_name;
  iface->set_action_target_value = gtk_menu_item_set_action_target_value;
  iface->get_action_target_value = gtk_menu_item_get_action_target_value;
}

static gboolean
gtk_menu_item_render (GtkCssGadget *gadget,
                      cairo_t      *cr,
                      int           x,
                      int           y,
                      int           width,
                      int           height,
                      gpointer      data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (priv->submenu && !GTK_IS_MENU_BAR (parent))
    gtk_css_gadget_draw (priv->arrow_gadget, cr);

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gtk_menu_item_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  gtk_css_gadget_draw (GTK_MENU_ITEM (widget)->priv->gadget, cr);

  return FALSE;
}

static void
gtk_menu_item_allocate (GtkCssGadget        *gadget,
                        const GtkAllocation *allocation,
                        int                  baseline,
                        GtkAllocation       *out_clip,
                        gpointer             data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkAllocation child_allocation;
  GtkAllocation arrow_clip = { 0 };
  GtkTextDirection direction;
  GtkPackDirection child_pack_dir;
  GtkWidget *child;
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  direction = gtk_widget_get_direction (widget);

  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU_BAR (parent))
    {
      child_pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
    }
  else
    {
      child_pack_dir = GTK_PACK_DIRECTION_LTR;
    }

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    {
      child_allocation = *allocation;

      if (child_pack_dir == GTK_PACK_DIRECTION_LTR ||
          child_pack_dir == GTK_PACK_DIRECTION_RTL)
        {
          if ((direction == GTK_TEXT_DIR_LTR) == (child_pack_dir != GTK_PACK_DIRECTION_RTL))
            child_allocation.x += priv->toggle_size;
          child_allocation.width -= priv->toggle_size;
        }
      else
        {
          if ((direction == GTK_TEXT_DIR_LTR) == (child_pack_dir != GTK_PACK_DIRECTION_BTT))
            child_allocation.y += priv->toggle_size;
          child_allocation.height -= priv->toggle_size;
        }

      if ((priv->submenu && !GTK_IS_MENU_BAR (parent)) || priv->reserve_indicator)
	{
          GtkAllocation arrow_alloc;

          gtk_css_gadget_get_preferred_size (priv->arrow_gadget,
                                             GTK_ORIENTATION_HORIZONTAL,
                                             -1,
                                             &arrow_alloc.width, NULL,
                                             NULL, NULL);
          gtk_css_gadget_get_preferred_size (priv->arrow_gadget,
                                             GTK_ORIENTATION_VERTICAL,
                                             -1,
                                             &arrow_alloc.height, NULL,
                                             NULL, NULL);

          if (direction == GTK_TEXT_DIR_LTR)
            {
              arrow_alloc.x = child_allocation.x +
                child_allocation.width - arrow_alloc.width;
            }
          else
            {
              arrow_alloc.x = 0;
              child_allocation.x += arrow_alloc.width;
            }

          child_allocation.width -= arrow_alloc.width;
          arrow_alloc.y = child_allocation.y +
            (child_allocation.height - arrow_alloc.height) / 2;

          gtk_css_gadget_allocate (priv->arrow_gadget,
                                   &arrow_alloc,
                                   baseline,
                                   &arrow_clip);
	}

      child_allocation.width = MAX (1, child_allocation.width);

      gtk_widget_size_allocate (child, &child_allocation);

      gtk_container_get_children_clip (GTK_CONTAINER (widget), out_clip);
      gdk_rectangle_union (out_clip, &arrow_clip, out_clip);
    }

  if (priv->submenu)
    gtk_menu_reposition (GTK_MENU (priv->submenu));
}

static void
gtk_menu_item_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkAllocation clip;
  
  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_menu_item_accel_width_foreach (GtkWidget *widget,
                                   gpointer   data)
{
  guint *width = data;

  if (GTK_IS_ACCEL_LABEL (widget))
    {
      guint w;

      w = gtk_accel_label_get_accel_width (GTK_ACCEL_LABEL (widget));
      *width = MAX (*width, w);
    }
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
                           gtk_menu_item_accel_width_foreach,
                           data);
}

static void
gtk_menu_item_real_get_width (GtkWidget *widget,
                              gint      *minimum_size,
                              gint      *natural_size)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *child;
  GtkWidget *parent;
  guint accel_width;
  gint  min_width, nat_width;

  min_width = nat_width = 0;

  parent = gtk_widget_get_parent (widget);
  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child != NULL && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_width (child, &child_min, &child_nat);

      if ((priv->submenu && !GTK_IS_MENU_BAR (parent)) || priv->reserve_indicator)
	{
          gint arrow_size;

          gtk_css_gadget_get_preferred_size (priv->arrow_gadget,
                                             GTK_ORIENTATION_HORIZONTAL,
                                             -1,
                                             &arrow_size, NULL,
                                             NULL, NULL);

          min_width += arrow_size;
          nat_width = min_width;
        }

      min_width += child_min;
      nat_width += child_nat;
    }

  accel_width = 0;
  gtk_container_foreach (GTK_CONTAINER (menu_item),
                         gtk_menu_item_accel_width_foreach,
                         &accel_width);
  priv->accelerator_width = accel_width;

  *minimum_size = min_width;
  *natural_size = nat_width;
}

static void
gtk_menu_item_real_get_height (GtkWidget *widget,
                               gint       for_size,
                               gint      *minimum_size,
                               gint      *natural_size)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *child;
  GtkWidget *parent;
  guint accel_width;
  gint min_height, nat_height;
  gint avail_size = 0;

  min_height = nat_height = 0;

  if (for_size != -1)
    avail_size = for_size;

  parent = gtk_widget_get_parent (widget);
  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child != NULL && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;
      gint arrow_size = 0;

      if ((priv->submenu && !GTK_IS_MENU_BAR (parent)) || priv->reserve_indicator)
        gtk_css_gadget_get_preferred_size (priv->arrow_gadget,
                                           GTK_ORIENTATION_VERTICAL,
                                           -1,
                                           &arrow_size, NULL,
                                           NULL, NULL);

      if (for_size != -1)
        {
          avail_size -= arrow_size;
          gtk_widget_get_preferred_height_for_width (child,
                                                     avail_size,
                                                     &child_min,
                                                     &child_nat);
        }
      else
        {
          gtk_widget_get_preferred_height (child, &child_min, &child_nat);
        }

      min_height += child_min;
      nat_height += child_nat;

      min_height = MAX (min_height, arrow_size);
      nat_height = MAX (nat_height, arrow_size);
    }

  accel_width = 0;
  gtk_container_foreach (GTK_CONTAINER (menu_item),
                         gtk_menu_item_accel_width_foreach,
                         &accel_width);
  priv->accelerator_width = accel_width;

  *minimum_size = min_height;
  *natural_size = nat_height;
}

static void
gtk_menu_item_measure (GtkCssGadget   *gadget,
                       GtkOrientation  orientation,
                       int             size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline,
                       gpointer        data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_menu_item_real_get_width (widget, minimum, natural);
  else
    gtk_menu_item_real_get_height (widget, size, minimum, natural);
}

static void
gtk_menu_item_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  gtk_css_gadget_get_preferred_size (GTK_MENU_ITEM (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_menu_item_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  gtk_css_gadget_get_preferred_size (GTK_MENU_ITEM (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_menu_item_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       for_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  gtk_css_gadget_get_preferred_size (GTK_MENU_ITEM (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     for_size,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_menu_item_class_init (GtkMenuItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  gobject_class->dispose = gtk_menu_item_dispose;
  gobject_class->set_property = gtk_menu_item_set_property;
  gobject_class->get_property = gtk_menu_item_get_property;

  widget_class->destroy = gtk_menu_item_destroy;
  widget_class->size_allocate = gtk_menu_item_size_allocate;
  widget_class->draw = gtk_menu_item_draw;
  widget_class->realize = gtk_menu_item_realize;
  widget_class->unrealize = gtk_menu_item_unrealize;
  widget_class->map = gtk_menu_item_map;
  widget_class->unmap = gtk_menu_item_unmap;
  widget_class->enter_notify_event = gtk_menu_item_enter;
  widget_class->leave_notify_event = gtk_menu_item_leave;
  widget_class->show_all = gtk_menu_item_show_all;
  widget_class->mnemonic_activate = gtk_menu_item_mnemonic_activate;
  widget_class->parent_set = gtk_menu_item_parent_set;
  widget_class->can_activate_accel = gtk_menu_item_can_activate_accel;
  widget_class->get_preferred_width = gtk_menu_item_get_preferred_width;
  widget_class->get_preferred_height = gtk_menu_item_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_menu_item_get_preferred_height_for_width;
  widget_class->direction_changed = gtk_menu_item_direction_changed;

  container_class->forall = gtk_menu_item_forall;

  klass->activate = gtk_real_menu_item_activate;
  klass->activate_item = gtk_real_menu_item_activate_item;
  klass->toggle_size_request = gtk_real_menu_item_toggle_size_request;
  klass->toggle_size_allocate = gtk_real_menu_item_toggle_size_allocate;
  klass->set_label = gtk_real_menu_item_set_label;
  klass->get_label = gtk_real_menu_item_get_label;
  klass->select = gtk_real_menu_item_select;
  klass->deselect = gtk_real_menu_item_deselect;

  klass->hide_on_activate = TRUE;

  /**
   * GtkMenuItem::activate:
   * @menuitem: the object which received the signal.
   *
   * Emitted when the item is activated.
   */
  menu_item_signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkMenuItemClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  widget_class->activate_signal = menu_item_signals[ACTIVATE];

  /**
   * GtkMenuItem::activate-item:
   * @menuitem: the object which received the signal.
   *
   * Emitted when the item is activated, but also if the menu item has a
   * submenu. For normal applications, the relevant signal is
   * #GtkMenuItem::activate.
   */
  menu_item_signals[ACTIVATE_ITEM] =
    g_signal_new (I_("activate-item"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, activate_item),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  menu_item_signals[TOGGLE_SIZE_REQUEST] =
    g_signal_new (I_("toggle-size-request"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, toggle_size_request),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  menu_item_signals[TOGGLE_SIZE_ALLOCATE] =
    g_signal_new (I_("toggle-size-allocate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, toggle_size_allocate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);

  menu_item_signals[SELECT] =
    g_signal_new (I_("select"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, select),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  menu_item_signals[DESELECT] =
    g_signal_new (I_("deselect"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, deselect),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkMenuItem:right-justified:
   *
   * Sets whether the menu item appears justified
   * at the right side of a menu bar.
   *
   * Since: 2.14
   */
  menu_item_props[PROP_RIGHT_JUSTIFIED] =
      g_param_spec_boolean ("right-justified",
                            P_("Right Justified"),
                            P_("Sets whether the menu item appears justified at the right side of a menu bar"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED);

  /**
   * GtkMenuItem:submenu:
   *
   * The submenu attached to the menu item, or %NULL if it has none.
   *
   * Since: 2.12
   */
  menu_item_props[PROP_SUBMENU] =
      g_param_spec_object ("submenu",
                           P_("Submenu"),
                           P_("The submenu attached to the menu item, or NULL if it has none"),
                           GTK_TYPE_MENU,
                           GTK_PARAM_READWRITE);

  /**
   * GtkMenuItem:accel-path:
   *
   * Sets the accelerator path of the menu item, through which runtime
   * changes of the menu item's accelerator caused by the user can be
   * identified and saved to persistant storage.
   *
   * Since: 2.14
   */
  menu_item_props[PROP_ACCEL_PATH] =
      g_param_spec_string ("accel-path",
                           P_("Accel Path"),
                           P_("Sets the accelerator path of the menu item"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkMenuItem:label:
   *
   * The text for the child label.
   *
   * Since: 2.16
   */
  menu_item_props[PROP_LABEL] =
      g_param_spec_string ("label",
                           P_("Label"),
                           P_("The text for the child label"),
                           "",
                           GTK_PARAM_READWRITE);

  /**
   * GtkMenuItem:use-underline:
   *
   * %TRUE if underlines in the text indicate mnemonics.
   *
   * Since: 2.16
   */
  menu_item_props[PROP_USE_UNDERLINE] =
      g_param_spec_boolean ("use-underline",
                            P_("Use underline"),
                            P_("If set, an underline in the text indicates "
                               "the next character should be used for the "
                               "mnemonic accelerator key"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, menu_item_props);

  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  /**
   * GtkMenuItem:selected-shadow-type:
   *
   * The shadow type when the item is selected.
   *
   * Deprecated: 3.20: Use CSS to determine the shadow; the value of this
   *     style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("selected-shadow-type",
                                                              "Selected Shadow Type",
                                                              "Shadow type when item is selected",
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_NONE,
                                                              GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkMenuItem:horizontal-padding:
   *
   * Padding to left and right of the menu item.
   *
   * Deprecated: 3.8: use the standard padding CSS property (through objects
   *   like #GtkStyleContext and #GtkCssProvider); the value of this style
   *   property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("horizontal-padding",
                                                             "Horizontal Padding",
                                                             "Padding to left and right of the menu item",
                                                             0,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE |
                                                             G_PARAM_DEPRECATED));

  /**
   * GtkMenuItem:toggle-spacing:
   *
   * Spacing between menu icon and label.
   *
   * Deprecated: 3.20: use the standard margin CSS property on the check or
   *   radio nodes; the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("toggle-spacing",
                                                             "Icon Spacing",
                                                             "Space between icon and label",
                                                             0,
                                                             G_MAXINT,
                                                             5,
                                                             GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkMenuItem:arrow-spacing:
   *
   * Spacing between menu item label and submenu arrow.
   *
   * Deprecated: 3.20: use the standard margin CSS property on the arrow node;
   *   the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("arrow-spacing",
                                                             "Arrow Spacing",
                                                             "Space between label and arrow",
                                                             0,
                                                             G_MAXINT,
                                                             10,
                                                             GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkMenuItem:arrow-scaling:
   *
   * Amount of space used up by the arrow, relative to the menu item's font
   * size.
   *
   * Deprecated: 3.20: use the standard min-width/min-height CSS properties on
   *   the arrow node; the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("Amount of space used up by arrow, relative to the menu item's font size"),
                                                               0.0, 2.0, 0.8,
                                                               GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkMenuItem:width-chars:
   *
   * The minimum desired width of the menu item in characters.
   *
   * Since: 2.14
   *
   * Deprecated: 3.20: Use the standard CSS property min-width; the value of
   *     this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("width-chars",
                                                             P_("Width in Characters"),
                                                             P_("The minimum desired width of the menu item in characters"),
                                                             0, G_MAXINT, 12,
                                                             GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_MENU_ITEM_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "menuitem");

  gtk_container_class_handle_border_width (container_class);
}

static void
gtk_menu_item_init (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv;
  GtkCssNode *widget_node;

  priv = gtk_menu_item_get_instance_private (menu_item);
  menu_item->priv = priv;

  gtk_widget_set_has_window (GTK_WIDGET (menu_item), FALSE);

  priv->action = NULL;
  priv->use_action_appearance = TRUE;
  
  priv->submenu = NULL;
  priv->toggle_size = 0;
  priv->accelerator_width = 0;
  if (gtk_widget_get_direction (GTK_WIDGET (menu_item)) == GTK_TEXT_DIR_RTL)
    priv->submenu_direction = GTK_DIRECTION_LEFT;
  else
    priv->submenu_direction = GTK_DIRECTION_RIGHT;
  priv->submenu_placement = GTK_TOP_BOTTOM;
  priv->right_justify = FALSE;
  priv->use_action_appearance = TRUE;
  priv->timer = 0;
  priv->action = NULL;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (menu_item));
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     GTK_WIDGET (menu_item),
                                                     gtk_menu_item_measure,
                                                     gtk_menu_item_allocate,
                                                     gtk_menu_item_render,
                                                     NULL, NULL);
}

GtkCssGadget *
_gtk_menu_item_get_gadget (GtkMenuItem *menu_item)
{
  return menu_item->priv->gadget;
}

/**
 * gtk_menu_item_new:
 *
 * Creates a new #GtkMenuItem.
 *
 * Returns: the newly created #GtkMenuItem
 */
GtkWidget*
gtk_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_MENU_ITEM, NULL);
}

/**
 * gtk_menu_item_new_with_label:
 * @label: the text for the label
 *
 * Creates a new #GtkMenuItem whose child is a #GtkLabel.
 *
 * Returns: the newly created #GtkMenuItem
 */
GtkWidget*
gtk_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_MENU_ITEM,
                       "label", label,
                       NULL);
}


/**
 * gtk_menu_item_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *     mnemonic character
 *
 * Creates a new #GtkMenuItem containing a label.
 *
 * The label will be created using gtk_label_new_with_mnemonic(),
 * so underscores in @label indicate the mnemonic for the menu item.
 *
 * Returns: a new #GtkMenuItem
 */
GtkWidget*
gtk_menu_item_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_MENU_ITEM,
                       "use-underline", TRUE,
                       "label", label,
                       NULL);
}

static void
gtk_menu_item_dispose (GObject *object)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);
  GtkMenuItemPrivate *priv = menu_item->priv;

  g_clear_object (&priv->action_helper);

  if (priv->action)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_action_disconnect_accelerator (priv->action);
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (menu_item), NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      priv->action = NULL;
    }

  g_clear_object (&priv->arrow_gadget);
  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_menu_item_parent_class)->dispose (object);
}

static void
gtk_menu_item_do_set_right_justified (GtkMenuItem *menu_item,
                                      gboolean     right_justified)
{
  GtkMenuItemPrivate *priv = menu_item->priv;

  right_justified = right_justified != FALSE;

  if (priv->right_justify != right_justified)
    {
      priv->right_justify = right_justified;
      gtk_widget_queue_resize (GTK_WIDGET (menu_item));
      g_object_notify_by_pspec (G_OBJECT (menu_item), menu_item_props[PROP_RIGHT_JUSTIFIED]);
    }
}

static void
gtk_menu_item_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_RIGHT_JUSTIFIED:
      gtk_menu_item_do_set_right_justified (menu_item, g_value_get_boolean (value));
      break;
    case PROP_SUBMENU:
      gtk_menu_item_set_submenu (menu_item, g_value_get_object (value));
      break;
    case PROP_ACCEL_PATH:
      gtk_menu_item_set_accel_path (menu_item, g_value_get_string (value));
      break;
    case PROP_LABEL:
      gtk_menu_item_set_label (menu_item, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_menu_item_set_use_underline (menu_item, g_value_get_boolean (value));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      gtk_menu_item_set_related_action (menu_item, g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      gtk_menu_item_set_use_action_appearance (menu_item, g_value_get_boolean (value));
      break;
    case PROP_ACTION_NAME:
      gtk_menu_item_set_action_name (GTK_ACTIONABLE (menu_item), g_value_get_string (value));
      break;
    case PROP_ACTION_TARGET:
      gtk_menu_item_set_action_target_value (GTK_ACTIONABLE (menu_item), g_value_get_variant (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_item_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);
  GtkMenuItemPrivate *priv = menu_item->priv;

  switch (prop_id)
    {
    case PROP_RIGHT_JUSTIFIED:
      g_value_set_boolean (value, priv->right_justify);
      break;
    case PROP_SUBMENU:
      g_value_set_object (value, gtk_menu_item_get_submenu (menu_item));
      break;
    case PROP_ACCEL_PATH:
      g_value_set_string (value, gtk_menu_item_get_accel_path (menu_item));
      break;
    case PROP_LABEL:
      g_value_set_string (value, gtk_menu_item_get_label (menu_item));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, gtk_menu_item_get_use_underline (menu_item));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      g_value_set_object (value, priv->action);
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, priv->use_action_appearance);
      break;
    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (priv->action_helper));
      break;
    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (priv->action_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_item_destroy (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  if (priv->submenu)
    gtk_widget_destroy (priv->submenu);

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->destroy (widget);
}

static void
gtk_menu_item_detacher (GtkWidget *widget,
                        GtkMenu   *menu)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  g_return_if_fail (priv->submenu == (GtkWidget*) menu);

  priv->submenu = NULL;
  g_clear_object (&priv->arrow_gadget);
}

static void
gtk_menu_item_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_menu_item_buildable_add_child;
  iface->custom_finished = gtk_menu_item_buildable_custom_finished;
}

static void
gtk_menu_item_buildable_add_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   GObject      *child,
                                   const gchar  *type)
{
  if (type && strcmp (type, "submenu") == 0)
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (buildable),
                                   GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}


static void
gtk_menu_item_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer      user_data)
{
  GtkWidget *toplevel;

  if (strcmp (tagname, "accelerator") == 0)
    {
      GtkMenuShell *menu_shell;
      GtkWidget *attach;

      menu_shell = GTK_MENU_SHELL (gtk_widget_get_parent (GTK_WIDGET (buildable)));
      if (menu_shell)
        {
          while (GTK_IS_MENU (menu_shell) &&
                 (attach = gtk_menu_get_attach_widget (GTK_MENU (menu_shell))) != NULL)
            menu_shell = GTK_MENU_SHELL (gtk_widget_get_parent (attach));

          toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu_shell));
        }
      else
        {
          /* Fall back to something ... */
          toplevel = gtk_widget_get_toplevel (GTK_WIDGET (buildable));

          g_warning ("found a GtkMenuItem '%s' without a parent GtkMenuShell, assigned accelerators wont work.",
                     gtk_buildable_get_name (buildable));
        }

      /* Feed the correct toplevel to the GtkWidget accelerator parsing code */
      _gtk_widget_buildable_finish_accelerator (GTK_WIDGET (buildable), toplevel, user_data);
    }
  else
    parent_buildable_iface->custom_finished (buildable, builder, child, tagname, user_data);
}


static void
gtk_menu_item_activatable_interface_init (GtkActivatableIface *iface)
{
  iface->update = gtk_menu_item_update;
  iface->sync_action_properties = gtk_menu_item_sync_action_properties;
}

static void
activatable_update_label (GtkMenuItem *menu_item, GtkAction *action)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (menu_item));

  if (GTK_IS_LABEL (child))
    {
      const gchar *label;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      label = gtk_action_get_label (action);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      gtk_menu_item_set_label (menu_item, label);
    }
}

/*
 * gtk_menu_is_empty:
 * @menu: (allow-none): a #GtkMenu or %NULL
 * 
 * Determines whether @menu is empty. A menu is considered empty if it
 * the only visible children are tearoff menu items or “filler” menu 
 * items which were inserted to mark the menu as empty.
 * 
 * This function is used by #GtkAction.
 *
 * Returns: whether @menu is empty.
 **/
static gboolean
gtk_menu_is_empty (GtkWidget *menu)
{
  GList *children, *cur;
  gboolean result = TRUE;

  g_return_val_if_fail (menu == NULL || GTK_IS_MENU (menu), TRUE);

  if (!menu)
    return FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  cur = children;
  while (cur) 
    {
      if (gtk_widget_get_visible (cur->data))
	{

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

	  if (!GTK_IS_TEAROFF_MENU_ITEM (cur->data) &&
	      !g_object_get_data (cur->data, "gtk-empty-menu-item"))
            {
	      result = FALSE;
              break;
            }

G_GNUC_END_IGNORE_DEPRECATIONS

	}
      cur = cur->next;
    }
  g_list_free (children);

  return result;
}


static void
gtk_menu_item_update (GtkActivatable *activatable,
                      GtkAction      *action,
                      const gchar    *property_name)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (activatable);
  GtkMenuItemPrivate *priv = menu_item->priv;

  if (strcmp (property_name, "visible") == 0)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      _gtk_action_sync_menu_visible (action, GTK_WIDGET (menu_item),
                                     gtk_menu_is_empty (gtk_menu_item_get_submenu (menu_item)));
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  else if (strcmp (property_name, "sensitive") == 0)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_widget_set_sensitive (GTK_WIDGET (menu_item), gtk_action_is_sensitive (action));
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  else if (priv->use_action_appearance)
    {
      if (strcmp (property_name, "label") == 0)
        activatable_update_label (menu_item, action);
    }
}

static void
gtk_menu_item_sync_action_properties (GtkActivatable *activatable,
                                      GtkAction      *action)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (activatable);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *label;

  if (!priv->use_action_appearance || !action)
    {
      label = gtk_bin_get_child (GTK_BIN (menu_item));

      if (GTK_IS_ACCEL_LABEL (label))
        gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), GTK_WIDGET (menu_item));
    }

  if (!action)
    return;

  _gtk_action_sync_menu_visible (action, GTK_WIDGET (menu_item),
                                 gtk_menu_is_empty (gtk_menu_item_get_submenu (menu_item)));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_widget_set_sensitive (GTK_WIDGET (menu_item), gtk_action_is_sensitive (action));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (priv->use_action_appearance)
    {
      label = gtk_bin_get_child (GTK_BIN (menu_item));

      /* make sure label is a label, deleting it otherwise */
      if (label && !GTK_IS_LABEL (label))
        {
          gtk_container_remove (GTK_CONTAINER (menu_item), label);
          label = NULL;
        }
      /* Make sure that menu_item has a label and that any
       * accelerators are set */
      gtk_menu_item_ensure_label (menu_item);
      gtk_menu_item_set_use_underline (menu_item, TRUE);
      /* Make label point to the menu_item's label */
      label = gtk_bin_get_child (GTK_BIN (menu_item));

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      if (GTK_IS_ACCEL_LABEL (label) && gtk_action_get_accel_path (action))
        {
          gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), NULL);
          gtk_accel_label_set_accel_closure (GTK_ACCEL_LABEL (label),
                                             gtk_action_get_accel_closure (action));
        }
      G_GNUC_END_IGNORE_DEPRECATIONS;

      activatable_update_label (menu_item, action);
    }
}

static void
gtk_menu_item_set_related_action (GtkMenuItem *menu_item,
                                  GtkAction   *action)
{
    GtkMenuItemPrivate *priv = menu_item->priv;

    if (priv->action == action)
      return;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

    if (priv->action)
      {
        gtk_action_disconnect_accelerator (priv->action);
      }

    if (action)
      {
        const gchar *accel_path;

        accel_path = gtk_action_get_accel_path (action);
        if (accel_path)
          {
            gtk_action_connect_accelerator (action);
            gtk_menu_item_set_accel_path (menu_item, accel_path);
          }
      }

    gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (menu_item), action);

    G_GNUC_END_IGNORE_DEPRECATIONS;

    priv->action = action;
}

static void
gtk_menu_item_set_use_action_appearance (GtkMenuItem *menu_item,
                                         gboolean     use_appearance)
{
    GtkMenuItemPrivate *priv = menu_item->priv;

    if (priv->use_action_appearance != use_appearance)
      {
        priv->use_action_appearance = use_appearance;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (menu_item), priv->action);
        G_GNUC_END_IGNORE_DEPRECATIONS;
      }
}

static void
update_node_classes (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkCssNode *arrow_node, *widget_node, *node;

  if (!priv->arrow_gadget)
    return;

  arrow_node = gtk_css_gadget_get_node (priv->arrow_gadget);
  widget_node = gtk_widget_get_css_node (GTK_WIDGET (menu_item));

  gtk_css_node_set_state (arrow_node, gtk_css_node_get_state (widget_node));

  if (gtk_widget_get_direction (GTK_WIDGET (menu_item)) == GTK_TEXT_DIR_RTL)
    {
      gtk_css_node_add_class (arrow_node, g_quark_from_static_string (GTK_STYLE_CLASS_LEFT));
      gtk_css_node_remove_class (arrow_node, g_quark_from_static_string (GTK_STYLE_CLASS_RIGHT));

      node = gtk_css_node_get_first_child (widget_node);
      if (node != arrow_node)
        gtk_css_node_insert_before (widget_node, arrow_node, node);
    }
  else
    {
      gtk_css_node_remove_class (arrow_node, g_quark_from_static_string (GTK_STYLE_CLASS_LEFT));
      gtk_css_node_add_class (arrow_node, g_quark_from_static_string (GTK_STYLE_CLASS_RIGHT));

      node = gtk_css_node_get_last_child (widget_node);
      if (node != arrow_node)
        gtk_css_node_insert_after (widget_node, arrow_node, node);
    }
}

static void
update_arrow_gadget (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *widget = GTK_WIDGET (menu_item);
  gboolean should_have_gadget;

  should_have_gadget = priv->reserve_indicator ||
    (priv->submenu && !GTK_IS_MENU_BAR (gtk_widget_get_parent (widget)));

  if (should_have_gadget)
    {
      if (!priv->arrow_gadget)
        {
          priv->arrow_gadget = gtk_builtin_icon_new ("arrow",
                                                     widget,
                                                     priv->gadget,
                                                     NULL);
          update_node_classes (menu_item);
        }
    }
  else
    {
      g_clear_object (&priv->arrow_gadget);
    }
}

/**
 * gtk_menu_item_set_submenu:
 * @menu_item: a #GtkMenuItem
 * @submenu: (allow-none) (type Gtk.Menu): the submenu, or %NULL
 *
 * Sets or replaces the menu item’s submenu, or removes it when a %NULL
 * submenu is passed.
 */
void
gtk_menu_item_set_submenu (GtkMenuItem *menu_item,
                           GtkWidget   *submenu)
{
  GtkWidget *widget;
  GtkMenuItemPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (submenu == NULL || GTK_IS_MENU (submenu));

  widget = GTK_WIDGET (menu_item);
  priv = menu_item->priv;

  if (priv->submenu != submenu)
    {
      if (priv->submenu)
        {
          gtk_menu_detach (GTK_MENU (priv->submenu));
          priv->submenu = NULL;
        }

      if (submenu)
        {
          priv->submenu = submenu;
          gtk_menu_attach_to_widget (GTK_MENU (submenu),
                                     widget,
                                     gtk_menu_item_detacher);
        }

      update_arrow_gadget (menu_item);

      if (gtk_widget_get_parent (widget))
        gtk_widget_queue_resize (widget);

      g_object_notify_by_pspec (G_OBJECT (menu_item), menu_item_props[PROP_SUBMENU]);
    }
}

/**
 * gtk_menu_item_get_submenu:
 * @menu_item: a #GtkMenuItem
 *
 * Gets the submenu underneath this menu item, if any.
 * See gtk_menu_item_set_submenu().
 *
 * Returns: (nullable) (transfer none): submenu for this menu item, or %NULL if none
 */
GtkWidget *
gtk_menu_item_get_submenu (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), NULL);

  return menu_item->priv->submenu;
}

void _gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
                                   GtkSubmenuPlacement  placement);

void
_gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
                             GtkSubmenuPlacement  placement)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->priv->submenu_placement = placement;
}

/**
 * gtk_menu_item_select:
 * @menu_item: the menu item
 *
 * Emits the #GtkMenuItem::select signal on the given item.
 */
void
gtk_menu_item_select (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[SELECT], 0);
}

/**
 * gtk_menu_item_deselect:
 * @menu_item: the menu item
 *
 * Emits the #GtkMenuItem::deselect signal on the given item.
 */
void
gtk_menu_item_deselect (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[DESELECT], 0);
}

/**
 * gtk_menu_item_activate:
 * @menu_item: the menu item
 *
 * Emits the #GtkMenuItem::activate signal on the given item
 */
void
gtk_menu_item_activate (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[ACTIVATE], 0);
}

/**
 * gtk_menu_item_toggle_size_request:
 * @menu_item: the menu item
 * @requisition: (inout): the requisition to use as signal data.
 *
 * Emits the #GtkMenuItem::toggle-size-request signal on the given item.
 */
void
gtk_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                   gint        *requisition)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[TOGGLE_SIZE_REQUEST], 0, requisition);
}

/**
 * gtk_menu_item_toggle_size_allocate:
 * @menu_item: the menu item.
 * @allocation: the allocation to use as signal data.
 *
 * Emits the #GtkMenuItem::toggle-size-allocate signal on the given item.
 */
void
gtk_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
                                    gint         allocation)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[TOGGLE_SIZE_ALLOCATE], 0, allocation);
}

static void
gtk_menu_item_realize (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = (gtk_widget_get_events (widget) |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_ENTER_NOTIFY_MASK |
                           GDK_LEAVE_NOTIFY_MASK |
                           GDK_POINTER_MOTION_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                       &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_menu_item_unrealize (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  gtk_widget_unregister_window (widget, priv->event_window);
  gdk_window_destroy (priv->event_window);
  priv->event_window = NULL;

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->unrealize (widget);
}

static void
gtk_menu_item_map (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->map (widget);

  gdk_window_show (priv->event_window);
}

static void
gtk_menu_item_unmap (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->unmap (widget);
}

static gboolean
gtk_menu_item_enter (GtkWidget        *widget,
                     GdkEventCrossing *event)
{
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (gtk_widget_get_parent (widget), (GdkEvent *) event);
}

static gboolean
gtk_menu_item_leave (GtkWidget        *widget,
                     GdkEventCrossing *event)
{
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (gtk_widget_get_parent (widget), (GdkEvent*) event);
}

static void
gtk_real_menu_item_select (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv = menu_item->priv;
  GdkDevice *source_device = NULL;
  GdkEvent *current_event;

  current_event = gtk_get_current_event ();

  if (current_event)
    {
      source_device = gdk_event_get_source_device (current_event);
      gdk_event_free (current_event);
    }

  if ((!source_device ||
       gdk_device_get_source (source_device) != GDK_SOURCE_TOUCHSCREEN) &&
      priv->submenu &&
      (!gtk_widget_get_mapped (priv->submenu) ||
       GTK_MENU (priv->submenu)->priv->tearoff_active))
    {
      _gtk_menu_item_popup_submenu (GTK_WIDGET (menu_item), TRUE);
    }

  gtk_widget_set_state_flags (GTK_WIDGET (menu_item),
                              GTK_STATE_FLAG_PRELIGHT, FALSE);
  gtk_widget_queue_draw (GTK_WIDGET (menu_item));
}

static void
gtk_real_menu_item_deselect (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv = menu_item->priv;

  if (priv->submenu)
    _gtk_menu_item_popdown_submenu (GTK_WIDGET (menu_item));

  gtk_widget_unset_state_flags (GTK_WIDGET (menu_item),
                                GTK_STATE_FLAG_PRELIGHT);
  gtk_widget_queue_draw (GTK_WIDGET (menu_item));
}

static gboolean
gtk_menu_item_mnemonic_activate (GtkWidget *widget,
                                 gboolean   group_cycling)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_MENU_SHELL (parent))
    _gtk_menu_shell_set_keyboard_mode (GTK_MENU_SHELL (parent), TRUE);

  if (group_cycling &&
      parent &&
      GTK_IS_MENU_SHELL (parent) &&
      GTK_MENU_SHELL (parent)->priv->active)
    {
      gtk_menu_shell_select_item (GTK_MENU_SHELL (parent), widget);
    }
  else
    g_signal_emit (widget, menu_item_signals[ACTIVATE_ITEM], 0);

  return TRUE;
}

static void
gtk_real_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv = menu_item->priv;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);

  if (priv->action)
    gtk_action_activate (priv->action);

  G_GNUC_END_IGNORE_DEPRECATIONS;
}


static void
gtk_real_menu_item_activate_item (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *parent;
  GtkWidget *widget;

  widget = GTK_WIDGET (menu_item);
  parent = gtk_widget_get_parent (widget);

  if (parent && GTK_IS_MENU_SHELL (parent))
    {
      GtkMenuShell *menu_shell = GTK_MENU_SHELL (parent);

      if (priv->submenu == NULL)
        gtk_menu_shell_activate_item (menu_shell, widget, TRUE);
      else
        {
          gtk_menu_shell_select_item (menu_shell, widget);
          _gtk_menu_item_popup_submenu (widget, FALSE);

          gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->submenu), TRUE);
        }
    }
}

static void
gtk_real_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                        gint        *requisition)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  *requisition = 0;
}

static void
gtk_real_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
                                         gint         allocation)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->priv->toggle_size = allocation;
}

static void
gtk_real_menu_item_set_label (GtkMenuItem *menu_item,
                              const gchar *label)
{
  GtkWidget *child;

  gtk_menu_item_ensure_label (menu_item);

  child = gtk_bin_get_child (GTK_BIN (menu_item));
  if (GTK_IS_LABEL (child))
    {
      gtk_label_set_label (GTK_LABEL (child), label ? label : "");

      g_object_notify_by_pspec (G_OBJECT (menu_item), menu_item_props[PROP_LABEL]);
    }
}

static const gchar *
gtk_real_menu_item_get_label (GtkMenuItem *menu_item)
{
  GtkWidget *child;

  gtk_menu_item_ensure_label (menu_item);

  child = gtk_bin_get_child (GTK_BIN (menu_item));
  if (GTK_IS_LABEL (child))
    return gtk_label_get_label (GTK_LABEL (child));

  return NULL;
}

static void
free_timeval (GTimeVal *val)
{
  g_slice_free (GTimeVal, val);
}

static void
popped_up_cb (GtkMenu            *menu,
              const GdkRectangle *flipped_rect,
              const GdkRectangle *final_rect,
              gboolean            flipped_x,
              gboolean            flipped_y,
              GtkMenuItem        *menu_item)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (menu_item));
  GtkMenu *parent_menu = GTK_IS_MENU (parent) ? GTK_MENU (parent) : NULL;

  if (parent_menu && GTK_IS_MENU_ITEM (parent_menu->priv->parent_menu_item))
    menu_item->priv->submenu_direction = GTK_MENU_ITEM (parent_menu->priv->parent_menu_item)->priv->submenu_direction;
  else
    {
      /* this case is stateful, do it at most once */
      g_signal_handlers_disconnect_by_func (menu, popped_up_cb, menu_item);
    }

  if (flipped_x)
    {
      switch (menu_item->priv->submenu_direction)
        {
        case GTK_DIRECTION_LEFT:
          menu_item->priv->submenu_direction = GTK_DIRECTION_RIGHT;
          break;

        case GTK_DIRECTION_RIGHT:
          menu_item->priv->submenu_direction = GTK_DIRECTION_LEFT;
          break;
        }
    }
}

static void
gtk_menu_item_real_popup_submenu (GtkWidget      *widget,
                                  const GdkEvent *trigger_event,
                                  gboolean        remember_exact_time)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkSubmenuDirection submenu_direction;
  GtkStyleContext *context;
  GtkBorder parent_padding;
  GtkBorder menu_padding;
  gint horizontal_offset;
  gint vertical_offset;
  GtkWidget *parent;
  GtkMenu *parent_menu;

  parent = gtk_widget_get_parent (widget);
  parent_menu = GTK_IS_MENU (parent) ? GTK_MENU (parent) : NULL;

  if (gtk_widget_is_sensitive (priv->submenu) && parent)
    {
      gboolean take_focus;

      take_focus = gtk_menu_shell_get_take_focus (GTK_MENU_SHELL (parent));
      gtk_menu_shell_set_take_focus (GTK_MENU_SHELL (priv->submenu), take_focus);

      if (remember_exact_time)
        {
          GTimeVal *popup_time = g_slice_new0 (GTimeVal);

          g_get_current_time (popup_time);

          g_object_set_data_full (G_OBJECT (priv->submenu),
                                  "gtk-menu-exact-popup-time", popup_time,
                                  (GDestroyNotify) free_timeval);
        }
      else
        {
          g_object_set_data (G_OBJECT (priv->submenu),
                             "gtk-menu-exact-popup-time", NULL);
        }

      /* Position the submenu at the menu item if it is mapped.
       * Otherwise, position the submenu at the pointer device.
       */
      if (gtk_widget_get_window (widget))
        {
          switch (priv->submenu_placement)
            {
            case GTK_TOP_BOTTOM:
              g_object_set (priv->submenu,
                            "anchor-hints", (GDK_ANCHOR_FLIP_Y |
                                             GDK_ANCHOR_SLIDE |
                                             GDK_ANCHOR_RESIZE),
                            "menu-type-hint", (priv->from_menubar ?
                                               GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU :
                                               GDK_WINDOW_TYPE_HINT_POPUP_MENU),
                            NULL);

              gtk_menu_popup_at_widget (GTK_MENU (priv->submenu),
                                        widget,
                                        GDK_GRAVITY_SOUTH_WEST,
                                        GDK_GRAVITY_NORTH_WEST,
                                        trigger_event);

              break;

            case GTK_LEFT_RIGHT:
              if (parent_menu && GTK_IS_MENU_ITEM (parent_menu->priv->parent_menu_item))
                submenu_direction = GTK_MENU_ITEM (parent_menu->priv->parent_menu_item)->priv->submenu_direction;
              else
                submenu_direction = priv->submenu_direction;

              g_signal_handlers_disconnect_by_func (priv->submenu, popped_up_cb, menu_item);
              g_signal_connect (priv->submenu, "popped-up", G_CALLBACK (popped_up_cb), menu_item);

              gtk_widget_style_get (priv->submenu,
                                    "horizontal-offset", &horizontal_offset,
                                    "vertical-offset", &vertical_offset,
                                    NULL);

              context = gtk_widget_get_style_context (parent);
              gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &parent_padding);
              context = gtk_widget_get_style_context (priv->submenu);
              gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &menu_padding);

              g_object_set (priv->submenu,
                            "anchor-hints", (GDK_ANCHOR_FLIP_X |
                                             GDK_ANCHOR_SLIDE |
                                             GDK_ANCHOR_RESIZE),
                            "rect-anchor-dy", vertical_offset - menu_padding.top,
                            NULL);

              switch (submenu_direction)
                {
                case GTK_DIRECTION_RIGHT:
                  g_object_set (priv->submenu,
                                "rect-anchor-dx", horizontal_offset + parent_padding.right + menu_padding.left,
                                NULL);

                  gtk_menu_popup_at_widget (GTK_MENU (priv->submenu),
                                            widget,
                                            GDK_GRAVITY_NORTH_EAST,
                                            GDK_GRAVITY_NORTH_WEST,
                                            trigger_event);

                  break;

                case GTK_DIRECTION_LEFT:
                  g_object_set (priv->submenu,
                                "rect-anchor-dx", -(horizontal_offset + parent_padding.left + menu_padding.right),
                                NULL);

                  gtk_menu_popup_at_widget (GTK_MENU (priv->submenu),
                                            widget,
                                            GDK_GRAVITY_NORTH_WEST,
                                            GDK_GRAVITY_NORTH_EAST,
                                            trigger_event);

                  break;
                }

              break;
            }
        }
      else
        gtk_menu_popup_at_pointer (GTK_MENU (priv->submenu), trigger_event);
    }

  /* Enable themeing of the parent menu item depending on whether
   * its submenu is shown or not.
   */
  gtk_widget_queue_draw (widget);
}

typedef struct
{
  GtkMenuItem *menu_item;
  GdkEvent    *trigger_event;
} PopupInfo;

static void
_gtk_menu_item_popup_info_destroy (PopupInfo *info)
{
  g_clear_pointer (&info->trigger_event, gdk_event_free);
  g_slice_free (PopupInfo, info);
}

static gint
gtk_menu_item_popup_timeout (gpointer data)
{
  PopupInfo *info = data;
  GtkMenuItem *menu_item = info->menu_item;
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (menu_item));

  if ((GTK_IS_MENU_SHELL (parent) && GTK_MENU_SHELL (parent)->priv->active) ||
      (GTK_IS_MENU (parent) && GTK_MENU (parent)->priv->torn_off))
    {
      gtk_menu_item_real_popup_submenu (GTK_WIDGET (menu_item), info->trigger_event, TRUE);
      if (info->trigger_event &&
          info->trigger_event->type != GDK_BUTTON_PRESS &&
          info->trigger_event->type != GDK_ENTER_NOTIFY &&
          priv->submenu)
        GTK_MENU_SHELL (priv->submenu)->priv->ignore_enter = TRUE;
    }

  priv->timer = 0;

  return FALSE;
}

static gint
get_popup_delay (GtkWidget *widget)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU_SHELL (parent))
    return _gtk_menu_shell_get_popup_delay (GTK_MENU_SHELL (parent));
  else
    return MENU_POPUP_DELAY;
}

void
_gtk_menu_item_popup_submenu (GtkWidget *widget,
                              gboolean   with_delay)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  if (priv->timer)
    {
      g_source_remove (priv->timer);
      priv->timer = 0;
      with_delay = FALSE;
    }

  if (with_delay)
    {
      gint popup_delay = get_popup_delay (widget);

      if (popup_delay > 0)
        {
          PopupInfo *info = g_slice_new (PopupInfo);

          info->menu_item = menu_item;
          info->trigger_event = gtk_get_current_event ();

          priv->timer = gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                                      popup_delay,
                                                      gtk_menu_item_popup_timeout,
                                                      info,
                                                      (GDestroyNotify) _gtk_menu_item_popup_info_destroy);
          g_source_set_name_by_id (priv->timer, "[gtk+] gtk_menu_item_popup_timeout");

          return;
        }
    }

  gtk_menu_item_real_popup_submenu (widget, NULL, FALSE);
}

void
_gtk_menu_item_popdown_submenu (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  if (priv->submenu)
    {
      g_object_set_data (G_OBJECT (priv->submenu),
                         "gtk-menu-exact-popup-time", NULL);

      if (priv->timer)
        {
          g_source_remove (priv->timer);
          priv->timer = 0;
        }
      else
        gtk_menu_popdown (GTK_MENU (priv->submenu));

      gtk_widget_queue_draw (widget);
    }
}

/**
 * gtk_menu_item_set_right_justified:
 * @menu_item: a #GtkMenuItem.
 * @right_justified: if %TRUE the menu item will appear at the
 *   far right if added to a menu bar
 *
 * Sets whether the menu item appears justified at the right
 * side of a menu bar. This was traditionally done for “Help”
 * menu items, but is now considered a bad idea. (If the widget
 * layout is reversed for a right-to-left language like Hebrew
 * or Arabic, right-justified-menu-items appear at the left.)
 *
 * Deprecated: 3.2: If you insist on using it, use
 *   gtk_widget_set_hexpand() and gtk_widget_set_halign().
 **/
void
gtk_menu_item_set_right_justified (GtkMenuItem *menu_item,
                                   gboolean     right_justified)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_menu_item_do_set_right_justified (menu_item, right_justified);
}

/**
 * gtk_menu_item_get_right_justified:
 * @menu_item: a #GtkMenuItem
 *
 * Gets whether the menu item appears justified at the right
 * side of the menu bar.
 *
 * Returns: %TRUE if the menu item will appear at the
 *   far right if added to a menu bar.
 *
 * Deprecated: 3.2: See gtk_menu_item_set_right_justified()
 **/
gboolean
gtk_menu_item_get_right_justified (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), FALSE);

  return menu_item->priv->right_justify;
}


static void
gtk_menu_item_show_all (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;

  /* show children including submenu */
  if (priv->submenu)
    gtk_widget_show_all (priv->submenu);
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_show_all, NULL);

  gtk_widget_show (widget);
}

static gboolean
gtk_menu_item_can_activate_accel (GtkWidget *widget,
                                  guint      signal_id)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  /* Chain to the parent GtkMenu for further checks */
  return (gtk_widget_is_sensitive (widget) && gtk_widget_get_visible (widget) &&
          parent && gtk_widget_can_activate_accel (parent, signal_id));
}

static void
gtk_menu_item_accel_name_foreach (GtkWidget *widget,
                                  gpointer   data)
{
  const gchar **path_p = data;

  if (!*path_p)
    {
      if (GTK_IS_LABEL (widget))
        {
          *path_p = gtk_label_get_text (GTK_LABEL (widget));
          if (*path_p && (*path_p)[0] == 0)
            *path_p = NULL;
        }
      else if (GTK_IS_CONTAINER (widget))
        gtk_container_foreach (GTK_CONTAINER (widget),
                               gtk_menu_item_accel_name_foreach,
                               data);
    }
}

static void
gtk_menu_item_parent_set (GtkWidget *widget,
                          GtkWidget *previous_parent)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenu *menu;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);
  menu = GTK_IS_MENU (parent) ? GTK_MENU (parent) : NULL;

  if (menu)
    _gtk_menu_item_refresh_accel_path (menu_item,
                                       menu->priv->accel_path,
                                       menu->priv->accel_group,
                                       TRUE);

  update_arrow_gadget (menu_item);

  if (GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->parent_set)
    GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->parent_set (widget, previous_parent);
}

static void
gtk_menu_item_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_dir)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);

  update_node_classes (menu_item);

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->direction_changed (widget, previous_dir);
}

void
_gtk_menu_item_refresh_accel_path (GtkMenuItem   *menu_item,
                                   const gchar   *prefix,
                                   GtkAccelGroup *accel_group,
                                   gboolean       group_changed)
{
  GtkMenuItemPrivate *priv = menu_item->priv;
  const gchar *path;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (!accel_group || GTK_IS_ACCEL_GROUP (accel_group));

  widget = GTK_WIDGET (menu_item);

  if (!accel_group)
    {
      gtk_widget_set_accel_path (widget, NULL, NULL);
      return;
    }

  path = _gtk_widget_get_accel_path (widget, NULL);
  if (!path)  /* no active accel_path yet */
    {
      path = priv->accel_path;
      if (!path && prefix)
        {
          const gchar *postfix = NULL;
          gchar *new_path;

          /* try to construct one from label text */
          gtk_container_foreach (GTK_CONTAINER (menu_item),
                                 gtk_menu_item_accel_name_foreach,
                                 &postfix);
          if (postfix)
            {
              new_path = g_strconcat (prefix, "/", postfix, NULL);
              path = priv->accel_path = g_intern_string (new_path);
              g_free (new_path);
            }
        }
      if (path)
        gtk_widget_set_accel_path (widget, path, accel_group);
    }
  else if (group_changed)    /* reinstall accelerators */
    gtk_widget_set_accel_path (widget, path, accel_group);
}

/**
 * gtk_menu_item_set_accel_path:
 * @menu_item:  a valid #GtkMenuItem
 * @accel_path: (allow-none): accelerator path, corresponding to this menu
 *     item’s functionality, or %NULL to unset the current path.
 *
 * Set the accelerator path on @menu_item, through which runtime
 * changes of the menu item’s accelerator caused by the user can be
 * identified and saved to persistent storage (see gtk_accel_map_save()
 * on this). To set up a default accelerator for this menu item, call
 * gtk_accel_map_add_entry() with the same @accel_path. See also
 * gtk_accel_map_add_entry() on the specifics of accelerator paths,
 * and gtk_menu_set_accel_path() for a more convenient variant of
 * this function.
 *
 * This function is basically a convenience wrapper that handles
 * calling gtk_widget_set_accel_path() with the appropriate accelerator
 * group for the menu item.
 *
 * Note that you do need to set an accelerator on the parent menu with
 * gtk_menu_set_accel_group() for this to work.
 *
 * Note that @accel_path string will be stored in a #GQuark.
 * Therefore, if you pass a static string, you can save some memory
 * by interning it first with g_intern_static_string().
 */
void
gtk_menu_item_set_accel_path (GtkMenuItem *menu_item,
                              const gchar *accel_path)
{
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *parent;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (accel_path == NULL ||
                    (accel_path[0] == '<' && strchr (accel_path, '/')));

  widget = GTK_WIDGET (menu_item);

  /* store new path */
  priv->accel_path = g_intern_string (accel_path);

  /* forget accelerators associated with old path */
  gtk_widget_set_accel_path (widget, NULL, NULL);

  /* install accelerators associated with new path */
  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU (parent))
    {
      GtkMenu *menu = GTK_MENU (parent);

      if (menu->priv->accel_group)
        _gtk_menu_item_refresh_accel_path (GTK_MENU_ITEM (widget),
                                           NULL,
                                           menu->priv->accel_group,
                                           FALSE);
    }
}

/**
 * gtk_menu_item_get_accel_path:
 * @menu_item:  a valid #GtkMenuItem
 *
 * Retrieve the accelerator path that was previously set on @menu_item.
 *
 * See gtk_menu_item_set_accel_path() for details.
 *
 * Returns: (nullable) (transfer none): the accelerator path corresponding to
 *     this menu item’s functionality, or %NULL if not set
 *
 * Since: 2.14
 */
const gchar *
gtk_menu_item_get_accel_path (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), NULL);

  return menu_item->priv->accel_path;
}

static void
gtk_menu_item_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_MENU_ITEM (container));
  g_return_if_fail (callback != NULL);

  child = gtk_bin_get_child (GTK_BIN (container));
  if (child)
    callback (child, callback_data);
}

gboolean
_gtk_menu_item_is_selectable (GtkWidget *menu_item)
{
  if ((!gtk_bin_get_child (GTK_BIN (menu_item)) &&
       G_OBJECT_TYPE (menu_item) == GTK_TYPE_MENU_ITEM) ||
      GTK_IS_SEPARATOR_MENU_ITEM (menu_item) ||
      !gtk_widget_is_sensitive (menu_item) ||
      !gtk_widget_get_visible (menu_item))
    return FALSE;

  return TRUE;
}

static void
gtk_menu_item_ensure_label (GtkMenuItem *menu_item)
{
  GtkWidget *accel_label;

  if (!gtk_bin_get_child (GTK_BIN (menu_item)))
    {
      accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, "xalign", 0.0, NULL);
      gtk_widget_set_halign (accel_label, GTK_ALIGN_FILL);
      gtk_widget_set_valign (accel_label, GTK_ALIGN_CENTER);

      gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
      gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
                                        GTK_WIDGET (menu_item));
      gtk_widget_show (accel_label);
    }
}

/**
 * gtk_menu_item_set_label:
 * @menu_item: a #GtkMenuItem
 * @label: the text you want to set
 *
 * Sets @text on the @menu_item label
 *
 * Since: 2.16
 */
void
gtk_menu_item_set_label (GtkMenuItem *menu_item,
                         const gchar *label)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  GTK_MENU_ITEM_GET_CLASS (menu_item)->set_label (menu_item, label);
}

/**
 * gtk_menu_item_get_label:
 * @menu_item: a #GtkMenuItem
 *
 * Sets @text on the @menu_item label
 *
 * Returns: The text in the @menu_item label. This is the internal
 *   string used by the label, and must not be modified.
 *
 * Since: 2.16
 */
const gchar *
gtk_menu_item_get_label (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), NULL);

  return GTK_MENU_ITEM_GET_CLASS (menu_item)->get_label (menu_item);
}

/**
 * gtk_menu_item_set_use_underline:
 * @menu_item: a #GtkMenuItem
 * @setting: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text indicates the next character
 * should be used for the mnemonic accelerator key.
 *
 * Since: 2.16
 */
void
gtk_menu_item_set_use_underline (GtkMenuItem *menu_item,
                                 gboolean     setting)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_menu_item_ensure_label (menu_item);

  child = gtk_bin_get_child (GTK_BIN (menu_item));
  if (GTK_IS_LABEL (child) &&
      gtk_label_get_use_underline (GTK_LABEL (child)) != setting)
    {
      gtk_label_set_use_underline (GTK_LABEL (child), setting);
      g_object_notify_by_pspec (G_OBJECT (menu_item), menu_item_props[PROP_USE_UNDERLINE]);
    }
}

/**
 * gtk_menu_item_get_use_underline:
 * @menu_item: a #GtkMenuItem
 *
 * Checks if an underline in the text indicates the next character
 * should be used for the mnemonic accelerator key.
 *
 * Returns: %TRUE if an embedded underline in the label
 *     indicates the mnemonic accelerator key.
 *
 * Since: 2.16
 */
gboolean
gtk_menu_item_get_use_underline (GtkMenuItem *menu_item)
{
  GtkWidget *child;

  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), FALSE);

  gtk_menu_item_ensure_label (menu_item);

  child = gtk_bin_get_child (GTK_BIN (menu_item));
  if (GTK_IS_LABEL (child))
    return gtk_label_get_use_underline (GTK_LABEL (child));

  return FALSE;
}

/**
 * gtk_menu_item_set_reserve_indicator:
 * @menu_item: a #GtkMenuItem
 * @reserve: the new value
 *
 * Sets whether the @menu_item should reserve space for
 * the submenu indicator, regardless if it actually has
 * a submenu or not.
 *
 * There should be little need for applications to call
 * this functions.
 *
 * Since: 3.0
 */
void
gtk_menu_item_set_reserve_indicator (GtkMenuItem *menu_item,
                                     gboolean     reserve)
{
  GtkMenuItemPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  priv = menu_item->priv;

  if (priv->reserve_indicator != reserve)
    {
      priv->reserve_indicator = reserve;
      update_arrow_gadget (menu_item);
      gtk_widget_queue_resize (GTK_WIDGET (menu_item));
    }
}

/**
 * gtk_menu_item_get_reserve_indicator:
 * @menu_item: a #GtkMenuItem
 *
 * Returns whether the @menu_item reserves space for
 * the submenu indicator, regardless if it has a submenu
 * or not.
 *
 * Returns: %TRUE if @menu_item always reserves space for the
 *     submenu indicator
 *
 * Since: 3.0
 */
gboolean
gtk_menu_item_get_reserve_indicator (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), FALSE);

  return menu_item->priv->reserve_indicator;
}
