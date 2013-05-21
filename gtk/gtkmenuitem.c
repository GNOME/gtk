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
#include "gtkactivatable.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"
#include "a11y/gtkmenuitemaccessible.h"
#include "deprecated/gtktearoffmenuitem.h"

/**
 * SECTION:gtkmenuitem
 * @Short_description: The widget used for item in menus
 * @Title: GtkMenuItem
 * @See_also: #GtkBin, #GtkMenuShell
 *
 * The #GtkMenuItem widget and the derived widgets are the only valid
 * childs for menus. Their function is to correctly handle highlighting,
 * alignment, events and submenus.
 *
 * As it derives from #GtkBin it can hold any valid child widget, altough
 * only a few are really useful.
 *
 * <refsect2 id="GtkMenuItem-BUILDER-UI">
 * <title>GtkMenuItem as GtkBuildable</title>
 * The GtkMenuItem implementation of the GtkBuildable interface
 * supports adding a submenu by specifying "submenu" as the "type"
 * attribute of a &lt;child&gt; element.
 * <example>
 * <title>A UI definition fragment with submenus</title>
 * <programlisting><![CDATA[
 * <object class="GtkMenuItem">
 *   <child type="submenu">
 *     <object class="GtkMenu"/>
 *   </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * </refsect2>
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

  /* activatable properties */
  PROP_ACTIVATABLE_RELATED_ACTION,
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
static void gtk_menu_item_size_allocate  (GtkWidget        *widget,
                                          GtkAllocation    *allocation);
static void gtk_menu_item_realize        (GtkWidget        *widget);
static void gtk_menu_item_unrealize      (GtkWidget        *widget);
static void gtk_menu_item_map            (GtkWidget        *widget);
static void gtk_menu_item_unmap          (GtkWidget        *widget);
static gboolean gtk_menu_item_enter      (GtkWidget        *widget,
                                          GdkEventCrossing *event);
static gboolean gtk_menu_item_leave      (GtkWidget        *widget,
                                          GdkEventCrossing *event);
static gboolean gtk_menu_item_draw       (GtkWidget        *widget,
                                          cairo_t          *cr);
static void gtk_menu_item_parent_set     (GtkWidget        *widget,
                                          GtkWidget        *previous_parent);


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
static void gtk_menu_item_position_menu  (GtkMenu          *menu,
                                          gint             *x,
                                          gint             *y,
                                          gboolean         *push_in,
                                          gpointer          user_data);
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

static void gtk_menu_item_get_preferred_width            (GtkWidget           *widget,
                                                          gint                *minimum_size,
                                                          gint                *natural_size);
static void gtk_menu_item_get_preferred_height           (GtkWidget           *widget,
                                                          gint                *minimum_size,
                                                          gint                *natural_size);
static void gtk_menu_item_get_preferred_height_for_width (GtkWidget           *widget,
                                                          gint                 for_size,
                                                          gint                *minimum_size,
                                                          gint                *natural_size);

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

static GtkBuildableIface *parent_buildable_iface;

G_DEFINE_TYPE_WITH_CODE (GtkMenuItem, gtk_menu_item, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_menu_item_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                gtk_menu_item_activatable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE,
                                                gtk_menu_item_actionable_interface_init))

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

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_MENU_ITEM_ACCESSIBLE);

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
                  _gtk_marshal_VOID__VOID,
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
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  menu_item_signals[TOGGLE_SIZE_REQUEST] =
    g_signal_new (I_("toggle-size-request"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, toggle_size_request),
                  NULL, NULL,
                  _gtk_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  menu_item_signals[TOGGLE_SIZE_ALLOCATE] =
    g_signal_new (I_("toggle-size-allocate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, toggle_size_allocate),
                  NULL, NULL,
                  _gtk_marshal_VOID__INT,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);

  menu_item_signals[SELECT] =
    g_signal_new (I_("select"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, select),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  menu_item_signals[DESELECT] =
    g_signal_new (I_("deselect"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuItemClass, deselect),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkMenuItem:right-justified:
   *
   * Sets whether the menu item appears justified
   * at the right side of a menu bar.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RIGHT_JUSTIFIED,
                                   g_param_spec_boolean ("right-justified",
                                                         P_("Right Justified"),
                                                         P_("Sets whether the menu item appears justified at the right side of a menu bar"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));

  /**
   * GtkMenuItem:submenu:
   *
   * The submenu attached to the menu item, or %NULL if it has none.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SUBMENU,
                                   g_param_spec_object ("submenu",
                                                        P_("Submenu"),
                                                        P_("The submenu attached to the menu item, or NULL if it has none"),
                                                        GTK_TYPE_MENU,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenuItem:accel-path:
   *
   * Sets the accelerator path of the menu item, through which runtime
   * changes of the menu item's accelerator caused by the user can be
   * identified and saved to persistant storage.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_PATH,
                                   g_param_spec_string ("accel-path",
                                                        P_("Accel Path"),
                                                        P_("Sets the accelerator path of the menu item"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenuItem:label:
   *
   * The text for the child label.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("The text for the child label"),
                                                        "",
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenuItem:use-underline:
   *
   * %TRUE if underlines in the text indicate mnemonics.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline",
                                                         P_("Use underline"),
                                                         P_("If set, an underline in the text indicates "
                                                            "the next character should be used for the "
                                                            "mnemonic accelerator key"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  gtk_widget_class_install_style_property_parser (widget_class,
                                                  g_param_spec_enum ("selected-shadow-type",
                                                                     "Selected Shadow Type",
                                                                     "Shadow type when item is selected",
                                                                     GTK_TYPE_SHADOW_TYPE,
                                                                     GTK_SHADOW_NONE,
                                                                     GTK_PARAM_READABLE),
                                                  gtk_rc_property_parse_enum);

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

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("toggle-spacing",
                                                             "Icon Spacing",
                                                             "Space between icon and label",
                                                             0,
                                                             G_MAXINT,
                                                             5,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("arrow-spacing",
                                                             "Arrow Spacing",
                                                             "Space between label and arrow",
                                                             0,
                                                             G_MAXINT,
                                                             10,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("Amount of space used up by arrow, relative to the menu item's font size"),
                                                               0.0, 2.0, 0.8,
                                                               GTK_PARAM_READABLE));

  /**
   * GtkMenuItem:width-chars:
   *
   * The minimum desired width of the menu item in characters.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("width-chars",
                                                             P_("Width in Characters"),
                                                             P_("The minimum desired width of the menu item in characters"),
                                                             0, G_MAXINT, 12,
                                                             GTK_PARAM_READABLE));

  g_type_class_add_private (klass, sizeof (GtkMenuItemPrivate));
}

static void
gtk_menu_item_init (GtkMenuItem *menu_item)
{
  GtkStyleContext *context;
  GtkMenuItemPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (menu_item,
                                      GTK_TYPE_MENU_ITEM,
                                      GtkMenuItemPrivate);
  menu_item->priv = priv;

  gtk_widget_set_has_window (GTK_WIDGET (menu_item), FALSE);

  priv->action = NULL;
  priv->use_action_appearance = TRUE;
  
  menu_item->priv->submenu = NULL;
  menu_item->priv->toggle_size = 0;
  menu_item->priv->accelerator_width = 0;
  if (gtk_widget_get_direction (GTK_WIDGET (menu_item)) == GTK_TEXT_DIR_RTL)
    priv->submenu_direction = GTK_DIRECTION_LEFT;
  else
    priv->submenu_direction = GTK_DIRECTION_RIGHT;
  priv->submenu_placement = GTK_TOP_BOTTOM;
  priv->right_justify = FALSE;
  priv->use_action_appearance = TRUE;
  priv->timer = 0;
  priv->action = NULL;

  context = gtk_widget_get_style_context (GTK_WIDGET (menu_item));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUITEM);
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
      gtk_action_disconnect_accelerator (priv->action);
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (menu_item), NULL);
      priv->action = NULL;
    }
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
      g_value_set_boolean (value, menu_item->priv->right_justify);
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
}

static void
get_arrow_size (GtkWidget *widget,
                GtkWidget *child,
                gint      *size,
                gint      *spacing)
{
  PangoContext     *context;
  PangoFontMetrics *metrics;
  gfloat            arrow_scaling;
  gint              arrow_spacing;

  g_assert (size);

  gtk_widget_style_get (widget,
                        "arrow-scaling", &arrow_scaling,
                        "arrow-spacing", &arrow_spacing,
                        NULL);

  if (spacing != NULL)
    *spacing = arrow_spacing;

  context = gtk_widget_get_pango_context (child);

  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));

  *size = (PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                         pango_font_metrics_get_descent (metrics)));

  pango_font_metrics_unref (metrics);

  *size = *size * arrow_scaling;
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

static gint
get_minimum_width (GtkWidget *widget)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint width;
  gint width_chars;

  context = gtk_widget_get_pango_context (widget);

  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));

  width = pango_font_metrics_get_approximate_char_width (metrics);

  pango_font_metrics_unref (metrics);

  gtk_widget_style_get (widget, "width-chars", &width_chars, NULL);

  return PANGO_PIXELS (width_chars * width);
}

static void
gtk_menu_item_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkBin *bin;
  GtkWidget *child;
  GtkWidget *parent;
  guint accel_width;
  guint border_width;
  gint  min_width, nat_width;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;

  min_width = nat_width = 0;
  bin = GTK_BIN (widget);
  parent = gtk_widget_get_parent (widget);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  min_width = (border_width * 2) + padding.left + padding.right;
  nat_width = min_width;

  child = gtk_bin_get_child (bin);

  if (child != NULL && gtk_widget_get_visible (child))
    {
      GtkMenuItemPrivate *priv = menu_item->priv;
      gint child_min, child_nat;

      gtk_widget_get_preferred_width (child, &child_min, &child_nat);

      if ((menu_item->priv->submenu && !GTK_IS_MENU_BAR (parent)) || priv->reserve_indicator)
	{
	  gint arrow_spacing, arrow_size;

	  get_arrow_size (widget, child, &arrow_size, &arrow_spacing);

          min_width += arrow_size;
          min_width += arrow_spacing;

          min_width = MAX (min_width, get_minimum_width (widget));
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

  if (minimum_size)
    *minimum_size = min_width;

  if (natural_size)
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
  GtkBin *bin;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkWidget *child;
  GtkWidget *parent;
  guint accel_width;
  guint border_width;
  gint min_height, nat_height;
  gint avail_size = 0;

  min_height = nat_height = 0;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  bin = GTK_BIN (widget);
  parent = gtk_widget_get_parent (widget);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  min_height   = (border_width * 2) + padding.top + padding.bottom;

  if (for_size != -1)
    {
      avail_size = for_size;
      avail_size -= (border_width * 2) + padding.left + padding.right;
    }

  nat_height = min_height;

  child = gtk_bin_get_child (bin);

  if (child != NULL && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;
      gint arrow_size = 0, arrow_spacing = 0;

      if ((priv->submenu && !GTK_IS_MENU_BAR (parent)) || priv->reserve_indicator)
        get_arrow_size (widget, child, &arrow_size, &arrow_spacing);

      if (for_size != -1)
        {
          avail_size -= (arrow_size + arrow_spacing);
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
  else /* separator item */
    {
      gboolean wide_separators;
      gint     separator_height;

      gtk_widget_style_get (widget,
                            "wide-separators",  &wide_separators,
                            "separator-height", &separator_height,
                            NULL);

      if (wide_separators)
        {
          min_height += separator_height;
          nat_height += separator_height;
        }
      else
        {
          /* force odd, so that we can have the same space above and
           * below the line.
           */
          if (min_height % 2 == 0)
            min_height += 1;
          if (nat_height % 2 == 0)
            nat_height += 1;
        }
    }

  accel_width = 0;
  gtk_container_foreach (GTK_CONTAINER (menu_item),
                         gtk_menu_item_accel_width_foreach,
                         &accel_width);
  priv->accelerator_width = accel_width;

  if (minimum_size)
    *minimum_size = min_height;

  if (natural_size)
    *natural_size = nat_height;
}

static void
gtk_menu_item_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  gtk_menu_item_real_get_height (widget, -1, minimum_size, natural_size);
}

static void
gtk_menu_item_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       for_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  gtk_menu_item_real_get_height (widget, for_size, minimum_size, natural_size);
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

      label = gtk_action_get_label (action);
      gtk_menu_item_set_label (menu_item, label);
    }
}

/*
 * gtk_menu_is_empty:
 * @menu: (allow-none): a #GtkMenu or %NULL
 * 
 * Determines whether @menu is empty. A menu is considered empty if it
 * the only visible children are tearoff menu items or "filler" menu 
 * items which were inserted to mark the menu as empty.
 * 
 * This function is used by #GtkAction.
 *
 * Return value: whether @menu is empty.
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
	  if (!GTK_IS_TEAROFF_MENU_ITEM (cur->data) &&
	      !g_object_get_data (cur->data, "gtk-empty-menu-item"))
            {
	      result = FALSE;
              break;
            }
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
    _gtk_action_sync_menu_visible (action, GTK_WIDGET (menu_item),
                                   gtk_menu_is_empty (gtk_menu_item_get_submenu (menu_item)));
  else if (strcmp (property_name, "sensitive") == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (menu_item), gtk_action_is_sensitive (action));
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

  gtk_widget_set_sensitive (GTK_WIDGET (menu_item), gtk_action_is_sensitive (action));

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

      if (GTK_IS_ACCEL_LABEL (label) && gtk_action_get_accel_path (action))
        {
          gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), NULL);
          gtk_accel_label_set_accel_closure (GTK_ACCEL_LABEL (label),
                                             gtk_action_get_accel_closure (action));
        }

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

        gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (menu_item), priv->action);
      }
}


/**
 * gtk_menu_item_set_submenu:
 * @menu_item: a #GtkMenuItem
 * @submenu: (allow-none): the submenu, or %NULL
 *
 * Sets or replaces the menu item's submenu, or removes it when a %NULL
 * submenu is passed.
 */
void
gtk_menu_item_set_submenu (GtkMenuItem *menu_item,
                           GtkWidget   *submenu)
{
  GtkMenuItemPrivate *priv = menu_item->priv;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (submenu == NULL || GTK_IS_MENU (submenu));

  if (priv->submenu != submenu)
    {
      if (priv->submenu)
        gtk_menu_detach (GTK_MENU (priv->submenu));

      if (submenu)
        {
          priv->submenu = submenu;
          gtk_menu_attach_to_widget (GTK_MENU (submenu),
                                     GTK_WIDGET (menu_item),
                                     gtk_menu_item_detacher);
        }

      if (gtk_widget_get_parent (GTK_WIDGET (menu_item)))
        gtk_widget_queue_resize (GTK_WIDGET (menu_item));

      g_object_notify (G_OBJECT (menu_item), "submenu");
    }
}

/**
 * gtk_menu_item_get_submenu:
 * @menu_item: a #GtkMenuItem
 *
 * Gets the submenu underneath this menu item, if any.
 * See gtk_menu_item_set_submenu().
 *
 * Return value: (transfer none): submenu for this menu item, or %NULL if none
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

void
gtk_menu_item_select (GtkMenuItem *menu_item)
{
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[SELECT], 0);

  /* Enable themeing of the parent menu item depending on whether
   * something is selected in its submenu
   */
  parent = gtk_widget_get_parent (GTK_WIDGET (menu_item));
  if (GTK_IS_MENU (parent))
    {
      GtkMenu *menu = GTK_MENU (parent);

      if (menu->priv->parent_menu_item)
        gtk_widget_queue_draw (GTK_WIDGET (menu->priv->parent_menu_item));
    }
}

/**
 * gtk_menu_item_deselect:
 * @menu_item: the menu item
 *
 * Emits the #GtkMenuItem::deselect signal on the given item. Behaves
 * exactly like #gtk_item_deselect.
 */
void
gtk_menu_item_deselect (GtkMenuItem *menu_item)
{
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[DESELECT], 0);

  /* Enable themeing of the parent menu item depending on whether
   * something is selected in its submenu
   */
  parent = gtk_widget_get_parent (GTK_WIDGET (menu_item));
  if (GTK_IS_MENU (parent))
    {
      GtkMenu *menu = GTK_MENU (parent);

      if (menu->priv->parent_menu_item)
        gtk_widget_queue_draw (GTK_WIDGET (menu->priv->parent_menu_item));
    }
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
 * @requisition: the requisition to use as signal data.
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
gtk_menu_item_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkTextDirection direction;
  GtkPackDirection child_pack_dir;
  GtkWidget *child;
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  bin = GTK_BIN (widget);

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

  gtk_widget_set_allocation (widget, allocation);

  child = gtk_bin_get_child (bin);
  if (child)
    {
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkBorder padding;
      guint border_width;

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_padding (context, state, &padding);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      child_allocation.x = border_width + padding.left;
      child_allocation.y = border_width + padding.top;

      child_allocation.width = allocation->width - (border_width * 2) -
        padding.left - padding.right;
      child_allocation.height = allocation->height - (border_width * 2) -
        padding.top - padding.bottom;

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

      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      if ((priv->submenu && !GTK_IS_MENU_BAR (parent)) || priv->reserve_indicator)
	{
	  gint arrow_spacing, arrow_size;

	  get_arrow_size (widget, child, &arrow_size, &arrow_spacing);

	  if (direction == GTK_TEXT_DIR_RTL)
	    child_allocation.x += arrow_size + arrow_spacing;
	  child_allocation.width -= arrow_size + arrow_spacing;
	}
      
      if (child_allocation.width < 1)
        child_allocation.width = 1;

      gtk_widget_size_allocate (child, &child_allocation);
    }

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  if (priv->submenu)
    gtk_menu_reposition (GTK_MENU (priv->submenu));
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

static gboolean
gtk_menu_item_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkStateFlags state;
  GtkStyleContext *context;
  GtkBorder padding;
  GtkWidget *child, *parent;
  gint x, y, w, h, width, height;
  guint border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  state = gtk_widget_get_state_flags (widget);
  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  x = border_width;
  y = border_width;
  w = width - border_width * 2;
  h = height - border_width * 2;

  child = gtk_bin_get_child (GTK_BIN (menu_item));
  parent = gtk_widget_get_parent (widget);

  gtk_style_context_get_padding (context, state, &padding);

  gtk_render_background (context, cr, x, y, w, h);
  gtk_render_frame (context, cr, x, y, w, h);

  if (priv->submenu && !GTK_IS_MENU_BAR (parent))
    {
      gint arrow_x, arrow_y;
      gint arrow_size;
      GtkTextDirection direction;
      gdouble angle;

      direction = gtk_widget_get_direction (widget);
      get_arrow_size (widget, child, &arrow_size, NULL);

      if (direction == GTK_TEXT_DIR_LTR)
        {
          arrow_x = x + w - arrow_size - padding.right;
          angle = G_PI / 2;
        }
      else
        {
          arrow_x = x + padding.left;
          angle = (3 * G_PI) / 2;
        }

      arrow_y = y + (h - arrow_size) / 2;

      gtk_render_arrow (context, cr, angle, arrow_x, arrow_y, arrow_size);
    }
  else if (!child)
    {
      gboolean wide_separators;
      gint     separator_height;

      gtk_widget_style_get (widget,
                            "wide-separators",    &wide_separators,
                            "separator-height",   &separator_height,
                            NULL);
      if (wide_separators)
        gtk_render_frame (context, cr,
                          x + padding.left,
                          y + padding.top,
                          w - padding.left - padding.right,
                          separator_height);
      else
        gtk_render_line (context, cr,
                         x + padding.left,
                         y + padding.top,
                         x + w - padding.right - 1,
                         y + padding.top);
    }

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->draw (widget, cr);

  return FALSE;
}

/**
 * gtk_menu_item_select:
 * @menu_item: the menu item
 *
 * Emits the #GtkMenuItem::select signal on the given item. Behaves
 * exactly like #gtk_item_select.
 */
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

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);

  if (priv->action)
    gtk_action_activate (priv->action);
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

      g_object_notify (G_OBJECT (menu_item), "label");
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
gtk_menu_item_real_popup_submenu (GtkWidget *widget,
                                  gboolean   remember_exact_time)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (gtk_widget_is_sensitive (priv->submenu) && parent)
    {
      gboolean take_focus;
      GtkMenuPositionFunc menu_position_func;

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

      /* gtk_menu_item_position_menu positions the submenu from the
       * menuitems position. If the menuitem doesn't have a window,
       * that doesn't work. In that case we use the default
       * positioning function instead which places the submenu at the
       * mouse cursor.
       */
      if (gtk_widget_get_window (widget))
        menu_position_func = gtk_menu_item_position_menu;
      else
        menu_position_func = NULL;

      gtk_menu_popup (GTK_MENU (priv->submenu),
                      parent,
                      widget,
                      menu_position_func,
                      menu_item,
                      GTK_MENU_SHELL (parent)->priv->button,
                      0);
    }

  /* Enable themeing of the parent menu item depending on whether
   * its submenu is shown or not.
   */
  gtk_widget_queue_draw (widget);
}

static gint
gtk_menu_item_popup_timeout (gpointer data)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (data);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (menu_item));

  if ((GTK_IS_MENU_SHELL (parent) && GTK_MENU_SHELL (parent)->priv->active) ||
      (GTK_IS_MENU (parent) && GTK_MENU (parent)->priv->torn_off))
    {
      gtk_menu_item_real_popup_submenu (GTK_WIDGET (menu_item), TRUE);
      if (priv->timer_from_keypress && priv->submenu)
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
    {
      return _gtk_menu_shell_get_popup_delay (GTK_MENU_SHELL (parent));
    }
  else
    {
      gint popup_delay;

      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-menu-popup-delay", &popup_delay,
                    NULL);

      return popup_delay;
    }
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
          GdkEvent *event = gtk_get_current_event ();

          priv->timer = gdk_threads_add_timeout (popup_delay,
                                                 gtk_menu_item_popup_timeout,
                                                 menu_item);

          if (event &&
              event->type != GDK_BUTTON_PRESS &&
              event->type != GDK_ENTER_NOTIFY)
            priv->timer_from_keypress = TRUE;
          else
            priv->timer_from_keypress = FALSE;

          if (event)
            gdk_event_free (event);

          return;
        }
    }

  gtk_menu_item_real_popup_submenu (widget, FALSE);
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

static void
get_offsets (GtkMenu *menu,
             gint    *horizontal_offset,
             gint    *vertical_offset)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;

  gtk_widget_style_get (GTK_WIDGET (menu),
                        "horizontal-offset", horizontal_offset,
                        "vertical-offset", vertical_offset,
                        NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (menu));
  state = gtk_widget_get_state_flags (GTK_WIDGET (menu));
  gtk_style_context_get_padding (context, state, &padding);

  *vertical_offset -= padding.top;
  *horizontal_offset += padding.left;
}

static void
gtk_menu_item_position_menu (GtkMenu  *menu,
                             gint     *x,
                             gint     *y,
                             gboolean *push_in,
                             gpointer  user_data)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (user_data);
  GtkMenuItemPrivate *priv = menu_item->priv;
  GtkAllocation allocation;
  GtkWidget *widget;
  GtkMenuItem *parent_menu_item;
  GtkWidget *parent;
  GdkScreen *screen;
  gint twidth, theight;
  gint tx, ty;
  GtkTextDirection direction;
  GdkRectangle monitor;
  gint monitor_num;
  gint horizontal_offset;
  gint vertical_offset;
  gint available_left, available_right;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder parent_padding;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  widget = GTK_WIDGET (user_data);

  if (push_in)
    *push_in = FALSE;

  direction = gtk_widget_get_direction (widget);

  twidth = gtk_widget_get_allocated_width (GTK_WIDGET (menu));
  theight = gtk_widget_get_allocated_height (GTK_WIDGET (menu));

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, priv->event_window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  if (!gdk_window_get_origin (gtk_widget_get_window (widget), &tx, &ty))
    {
      g_warning ("Menu not on screen");
      return;
    }

  gtk_widget_get_allocation (widget, &allocation);

  tx += allocation.x;
  ty += allocation.y;

  get_offsets (menu, &horizontal_offset, &vertical_offset);

  available_left = tx - monitor.x;
  available_right = monitor.x + monitor.width - (tx + allocation.width);

  parent = gtk_widget_get_parent (widget);
  priv->from_menubar = GTK_IS_MENU_BAR (parent);

  switch (priv->submenu_placement)
    {
    case GTK_TOP_BOTTOM:
      if (direction == GTK_TEXT_DIR_LTR)
        priv->submenu_direction = GTK_DIRECTION_RIGHT;
      else
        {
          priv->submenu_direction = GTK_DIRECTION_LEFT;
          tx += allocation.width - twidth;
        }
      if ((ty + allocation.height + theight) <= monitor.y + monitor.height)
        ty += allocation.height;
      else if ((ty - theight) >= monitor.y)
        ty -= theight;
      else if (monitor.y + monitor.height - (ty + allocation.height) > ty)
        ty += allocation.height;
      else
        ty -= theight;
      break;

    case GTK_LEFT_RIGHT:
      if (GTK_IS_MENU (parent))
        parent_menu_item = GTK_MENU_ITEM (GTK_MENU (parent)->priv->parent_menu_item);
      else
        parent_menu_item = NULL;

      context = gtk_widget_get_style_context (parent);
      state = gtk_widget_get_state_flags (parent);
      gtk_style_context_get_padding (context, state, &parent_padding);

      if (parent_menu_item && !GTK_MENU (parent)->priv->torn_off)
        {
          priv->submenu_direction = parent_menu_item->priv->submenu_direction;
        }
      else
        {
          if (direction == GTK_TEXT_DIR_LTR)
            priv->submenu_direction = GTK_DIRECTION_RIGHT;
          else
            priv->submenu_direction = GTK_DIRECTION_LEFT;
        }

      switch (priv->submenu_direction)
        {
        case GTK_DIRECTION_LEFT:
          if (tx - twidth - parent_padding.left - horizontal_offset >= monitor.x ||
              available_left >= available_right)
            tx -= twidth + parent_padding.left + horizontal_offset;
          else
            {
              priv->submenu_direction = GTK_DIRECTION_RIGHT;
              tx += allocation.width + parent_padding.right + horizontal_offset;
            }
          break;

        case GTK_DIRECTION_RIGHT:
          if (tx + allocation.width + parent_padding.right + horizontal_offset + twidth <= monitor.x + monitor.width ||
              available_right >= available_left)
            tx += allocation.width + parent_padding.right + horizontal_offset;
          else
            {
              priv->submenu_direction = GTK_DIRECTION_LEFT;
              tx -= twidth + parent_padding.left + horizontal_offset;
            }
          break;
        }

      ty += vertical_offset;

      /* If the height of the menu doesn't fit we move it upward. */
      ty = CLAMP (ty, monitor.y, MAX (monitor.y, monitor.y + monitor.height - theight));
      break;
    }

  /* If we have negative, tx, here it is because we can't get
   * the menu all the way on screen. Favor the left portion.
   */
  *x = CLAMP (tx, monitor.x, MAX (monitor.x, monitor.x + monitor.width - twidth));
  *y = ty;

  gtk_menu_set_monitor (menu, monitor_num);

  if (!gtk_widget_get_visible (menu->priv->toplevel))
    {
      gtk_window_set_type_hint (GTK_WINDOW (menu->priv->toplevel), priv->from_menubar?
                                GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU : GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    }
}

/**
 * gtk_menu_item_set_right_justified:
 * @menu_item: a #GtkMenuItem.
 * @right_justified: if %TRUE the menu item will appear at the
 *   far right if added to a menu bar
 *
 * Sets whether the menu item appears justified at the right
 * side of a menu bar. This was traditionally done for "Help"
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
 * Return value: %TRUE if the menu item will appear at the
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

  if (GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->parent_set)
    GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->parent_set (widget, previous_parent);
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
              path = priv->accel_path = (char*)g_intern_string (new_path);
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
 *     item's functionality, or %NULL to unset the current path.
 *
 * Set the accelerator path on @menu_item, through which runtime
 * changes of the menu item's accelerator caused by the user can be
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
  priv->accel_path = (char*)g_intern_string (accel_path);

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
 * Returns: the accelerator path corresponding to this menu
 *     item's functionality, or %NULL if not set
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
      accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
      gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

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
  if (GTK_IS_LABEL (child))
    {
      gtk_label_set_use_underline (GTK_LABEL (child), setting);

      g_object_notify (G_OBJECT (menu_item), "use-underline");
    }
}

/**
 * gtk_menu_item_get_use_underline:
 * @menu_item: a #GtkMenuItem
 *
 * Checks if an underline in the text indicates the next character
 * should be used for the mnemonic accelerator key.
 *
 * Return value: %TRUE if an embedded underline in the label
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
