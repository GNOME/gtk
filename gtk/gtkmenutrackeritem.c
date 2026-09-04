/*
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmenutrackeritemprivate.h"
#include "gtkactiontreeprivate.h"
#include "gtkdebug.h"
#include "gtkprivate.h"

#include <string.h>

/*< private >
 * GtkMenuTrackerItem:
 *
 * A GtkMenuTrackerItem is a small helper class used by GtkMenuTracker to
 * represent menu items. It has one of three classes: normal item, separator,
 * or submenu.
 *
 * If an item is one of the non-normal classes (submenu, separator), only the
 * label of the item needs to be respected. Otherwise, all the properties
 * of the item contribute to the item’s appearance and state.
 *
 * Implementing the appearance of the menu item is up to toolkits, and certain
 * toolkits may choose to ignore certain properties, like icon or accel. The
 * role of the item determines its accessibility role, along with its
 * decoration if the GtkMenuTrackerItem::toggled property is true. As an
 * example, if the item has the role %GTK_MENU_TRACKER_ITEM_ROLE_CHECK and
 * GtkMenuTrackerItem::toggled is %FALSE, its accessible role should be that of
 * a check menu item, and no decoration should be drawn. But if
 * GtkMenuTrackerItem::toggled is %TRUE, a checkmark should be drawn.
 *
 * All properties except for the two class-determining properties,
 * GtkMenuTrackerItem::is-separator and GtkMenuTrackerItem::has-submenu are
 * allowed to change, so listen to the notify signals to update your item's
 * appearance. When using a GObject library, this can conveniently be done
 * with g_object_bind_property() and GBinding, and this is how this is
 * implemented in GTK; the appearance side is implemented in GtkModelMenuItem.
 *
 * When an item is clicked, simply call gtk_menu_tracker_item_activated() in
 * response. The GtkMenuTrackerItem will take care of everything related to
 * activating the item and will itself update the state of all items in
 * response.
 *
 * Submenus are a special case of menu item. When an item is a submenu, you
 * should create a submenu for it with gtk_menu_tracker_new_item_for_submenu(),
 * and apply the same menu tracking logic you would for a toplevel menu.
 * Applications using submenus may want to lazily build their submenus in
 * response to the user clicking on it, as building a submenu may be expensive.
 *
 * Thus, the submenu has two special controls -- the submenu’s visibility
 * should be controlled by the GtkMenuTrackerItem::submenu-shown property,
 * and if a user clicks on the submenu, do not immediately show the menu,
 * but call gtk_menu_tracker_item_request_submenu_shown() and wait for the
 * GtkMenuTrackerItem::submenu-shown property to update. If the user navigates,
 * the application may want to be notified so it can cancel the expensive
 * operation that it was using to build the submenu. Thus,
 * gtk_menu_tracker_item_request_submenu_shown() takes a boolean parameter.
 * Use %TRUE when the user wants to open the submenu, and %FALSE when the
 * user wants to close the submenu.
 */

typedef GObjectClass GtkMenuTrackerItemClass;

struct _GtkMenuTrackerItem
{
  GObject parent_instance;

  GtkActionNode         *action_node;
  GtkActionSubscription *action_subscription;
  GtkActionKey          *action_key;
  GVariant              *action_target;
  char                  *action_namespace;
  GMenuItem             *item;
  guint role : 4; /* GtkMenuTrackerItemRole */
  guint is_separator : 1;
  guint can_activate : 1;
  guint sensitive : 1;
  guint toggled : 1;
  guint submenu_shown : 1;
  guint submenu_requested : 1;
  guint hidden_when : 2;
  guint is_visible : 1;
};

#define HIDDEN_NEVER         0
#define HIDDEN_WHEN_MISSING  1
#define HIDDEN_WHEN_DISABLED 2
#define HIDDEN_WHEN_ALWAYS   3

enum {
  PROP_0,
  PROP_IS_SEPARATOR,
  PROP_LABEL,
  PROP_USE_MARKUP,
  PROP_ICON,
  PROP_VERB_ICON,
  PROP_SENSITIVE,
  PROP_ROLE,
  PROP_TOGGLED,
  PROP_ACCEL,
  PROP_SUBMENU_SHOWN,
  PROP_IS_VISIBLE,
  N_PROPS
};

static GParamSpec *gtk_menu_tracker_item_pspecs[N_PROPS];

G_DEFINE_TYPE (GtkMenuTrackerItem, gtk_menu_tracker_item, G_TYPE_OBJECT)

GType
gtk_menu_tracker_item_role_get_type (void)
{
  static gsize gtk_menu_tracker_item_role_type;

  if (g_once_init_enter (&gtk_menu_tracker_item_role_type))
    {
      static const GEnumValue values[] = {
        { GTK_MENU_TRACKER_ITEM_ROLE_NORMAL, "GTK_MENU_TRACKER_ITEM_ROLE_NORMAL", "normal" },
        { GTK_MENU_TRACKER_ITEM_ROLE_CHECK, "GTK_MENU_TRACKER_ITEM_ROLE_CHECK", "check" },
        { GTK_MENU_TRACKER_ITEM_ROLE_RADIO, "GTK_MENU_TRACKER_ITEM_ROLE_RADIO", "radio" },
        { 0, NULL, NULL }
      };
      GType type;

      type = g_enum_register_static (I_("GtkMenuTrackerItemRole"), values);

      g_once_init_leave (&gtk_menu_tracker_item_role_type, type);
    }

  return gtk_menu_tracker_item_role_type;
}

static void
gtk_menu_tracker_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (object);

  switch (prop_id)
    {
    case PROP_IS_SEPARATOR:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_is_separator (self));
      break;
    case PROP_LABEL:
      g_value_set_string (value, gtk_menu_tracker_item_get_label (self));
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_use_markup (self));
      break;
    case PROP_ICON:
      g_value_take_object (value, gtk_menu_tracker_item_get_icon (self));
      break;
    case PROP_VERB_ICON:
      g_value_take_object (value, gtk_menu_tracker_item_get_verb_icon (self));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_sensitive (self));
      break;
    case PROP_ROLE:
      g_value_set_enum (value, gtk_menu_tracker_item_get_role (self));
      break;
    case PROP_TOGGLED:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_toggled (self));
      break;
    case PROP_ACCEL:
      g_value_set_string (value, gtk_menu_tracker_item_get_accel (self));
      break;
    case PROP_SUBMENU_SHOWN:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_submenu_shown (self));
      break;
    case PROP_IS_VISIBLE:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_is_visible (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtk_menu_tracker_item_finalize (GObject *object)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (object);

  g_clear_pointer (&self->action_subscription, gtk_action_subscription_cancel);
  g_clear_pointer (&self->action_key, gtk_action_key_unref);
  g_clear_pointer (&self->action_target, g_variant_unref);
  g_clear_pointer (&self->action_namespace, g_free);
  self->action_node = NULL;
  g_clear_object (&self->item);

  G_OBJECT_CLASS (gtk_menu_tracker_item_parent_class)->finalize (object);
}

static void
gtk_menu_tracker_item_init (GtkMenuTrackerItem * self)
{
}

static void
gtk_menu_tracker_item_class_init (GtkMenuTrackerItemClass *class)
{
  class->get_property = gtk_menu_tracker_item_get_property;
  class->finalize = gtk_menu_tracker_item_finalize;

  gtk_menu_tracker_item_pspecs[PROP_IS_SEPARATOR] =
    g_param_spec_boolean ("is-separator", NULL, NULL, FALSE, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL, NULL, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup", NULL, NULL, FALSE, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL, G_TYPE_ICON, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_VERB_ICON] =
    g_param_spec_object ("verb-icon", NULL, NULL, G_TYPE_ICON, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_SENSITIVE] =
    g_param_spec_boolean ("sensitive", NULL, NULL, FALSE, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ROLE] =
    g_param_spec_enum ("role", NULL, NULL,
                       GTK_TYPE_MENU_TRACKER_ITEM_ROLE, GTK_MENU_TRACKER_ITEM_ROLE_NORMAL,
                       G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_TOGGLED] =
    g_param_spec_boolean ("toggled", NULL, NULL, FALSE, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ACCEL] =
    g_param_spec_string ("accel", NULL, NULL, NULL, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_SUBMENU_SHOWN] =
    g_param_spec_boolean ("submenu-shown", NULL, NULL, FALSE, G_PARAM_STATIC_NAME | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_IS_VISIBLE] =
    g_param_spec_boolean ("is-visible", NULL, NULL, FALSE, G_PARAM_STATIC_NAME | G_PARAM_READABLE);

  g_object_class_install_properties (class, N_PROPS, gtk_menu_tracker_item_pspecs);
}

/* This syncs up the visibility for the hidden-when='' case. We call it
 * when the action subscription's coherent snapshot changes and during
 * subscription initialization.
 */
static void
gtk_menu_tracker_item_update_visibility (GtkMenuTrackerItem *self)
{
  gboolean visible;

  switch (self->hidden_when)
    {
    case HIDDEN_NEVER:
      visible = TRUE;
      break;

    case HIDDEN_WHEN_MISSING:
      visible = self->can_activate;
      break;

    case HIDDEN_WHEN_DISABLED:
      visible = self->sensitive;
      break;

    case HIDDEN_WHEN_ALWAYS:
      visible = FALSE;
      break;

    default:
      g_assert_not_reached ();
    }

  if (visible != self->is_visible)
    {
      self->is_visible = visible;
      g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_IS_VISIBLE]);
    }
}

static void
gtk_menu_tracker_item_action_changed (GtkActionSubscription   *subscription,
                                      GtkActionChange          changed,
                                      const GtkActionSnapshot *snapshot,
                                      gpointer                 user_data)
{
  GtkMenuTrackerItem *self = user_data;
  gboolean old_can_activate;
  gboolean old_sensitive;
  gboolean old_toggled;
  GtkMenuTrackerItemRole old_role;
  guint n_changed;

  old_can_activate = self->can_activate;
  old_sensitive = self->sensitive;
  old_toggled = self->toggled;
  old_role = self->role;

  self->can_activate = snapshot->present &&
                       ((self->action_target == NULL && snapshot->parameter_type == NULL) ||
                        (self->action_target != NULL && snapshot->parameter_type != NULL &&
                         g_variant_is_of_type (self->action_target, snapshot->parameter_type)));

  if (snapshot->present && !self->can_activate)
    {
      GTK_DEBUG (ACTIONS, "menutracker: action %s can't be activated due to parameter type mismatch "
                          "(parameter type %s, target type %s)",
                          gtk_action_key_get_full_name (self->action_key),
                          snapshot->parameter_type != NULL
                            ? g_variant_type_peek_string (snapshot->parameter_type) : "NULL",
                          self->action_target != NULL
                            ? g_variant_get_type_string (self->action_target) : "NULL");
    }

  self->sensitive = self->can_activate && snapshot->enabled;
  self->toggled = FALSE;
  self->role = GTK_MENU_TRACKER_ITEM_ROLE_NORMAL;

  if (self->can_activate && self->action_target != NULL && snapshot->state != NULL)
    {
      self->toggled = g_variant_equal (snapshot->state, self->action_target);
      self->role = GTK_MENU_TRACKER_ITEM_ROLE_RADIO;
    }
  else if (self->can_activate &&
           snapshot->state != NULL &&
           g_variant_is_of_type (snapshot->state, G_VARIANT_TYPE_BOOLEAN))
    {
      self->toggled = g_variant_get_boolean (snapshot->state);
      self->role = GTK_MENU_TRACKER_ITEM_ROLE_CHECK;
    }

  if (old_can_activate && !self->can_activate)
    gtk_menu_tracker_item_update_visibility (self);

  n_changed = (old_role != self->role)
            + (old_toggled != self->toggled)
            + (old_sensitive != self->sensitive)
            + ((changed & GTK_ACTION_CHANGE_ACCEL) != 0);

  if (n_changed > 1)
    g_object_freeze_notify (G_OBJECT (self));

  if (self->sensitive != old_sensitive)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_SENSITIVE]);

  if (self->toggled != old_toggled)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_TOGGLED]);

  if (self->role != old_role)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_ROLE]);

  if ((changed & GTK_ACTION_CHANGE_ACCEL) != 0)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_ACCEL]);

  if (n_changed > 1)
    g_object_thaw_notify (G_OBJECT (self));

  if (!old_can_activate || self->can_activate)
    gtk_menu_tracker_item_update_visibility (self);
}

GtkMenuTrackerItem *
_gtk_menu_tracker_item_new (GtkActionNode       *action_node,
                            GMenuModel          *model,
                            int                  item_index,
                            gboolean             mac_os_mode,
                            const char          *action_namespace,
                            gboolean             is_separator)
{
  GtkMenuTrackerItem *self;
  const char *action_name;
  const char *hidden_when;

  g_return_val_if_fail (action_node != NULL, NULL);
  g_return_val_if_fail (G_IS_MENU_MODEL (model), NULL);

  self = g_object_new (GTK_TYPE_MENU_TRACKER_ITEM, NULL);
  self->item = g_menu_item_new_from_model (model, item_index);
  self->action_namespace = g_strdup (action_namespace);
  self->action_node = action_node;
  self->is_separator = is_separator;

  if (!is_separator && g_menu_item_get_attribute (self->item, "hidden-when", "&s", &hidden_when))
    {
      if (g_str_equal (hidden_when, "action-disabled"))
        self->hidden_when = HIDDEN_WHEN_DISABLED;
      else if (g_str_equal (hidden_when, "action-missing"))
        self->hidden_when = HIDDEN_WHEN_MISSING;
      else if (mac_os_mode && g_str_equal (hidden_when, "macos-menubar"))
        self->hidden_when = HIDDEN_WHEN_ALWAYS;

      /* Ignore other values -- this code may be running in context of a
       * desktop shell or the like and should not spew criticals due to
       * application bugs...
       *
       * Note: if we just set a hidden-when state, but don't get the
       * action_name below then our visibility will be FALSE forever.
       * That's to be expected since the action is missing...
       */
    }

  if (!is_separator && g_menu_item_get_attribute (self->item, "action", "&s", &action_name))
    {
      char *full_action_name = NULL;
      GVariant *target;

      target = g_menu_item_get_attribute_value (self->item, "target", NULL);
      full_action_name = action_namespace != NULL
                       ? g_strconcat (action_namespace, ".", action_name, NULL)
                       : g_strdup (action_name);
      self->action_key = gtk_action_key_new (full_action_name);
      self->action_target = target != NULL ? g_variant_ref (target) : NULL;
      if (self->action_key != NULL)
        {
          self->action_subscription = gtk_action_node_subscribe (self->action_node,
                                     self->action_key,
                                     self->action_target != NULL
                                       ? g_variant_ref (self->action_target) : NULL,
                                     (GTK_ACTION_INTEREST_PRESENT |
                                      GTK_ACTION_INTEREST_ENABLED |
                                      GTK_ACTION_INTEREST_RAW_STATE |
                                      GTK_ACTION_INTEREST_ACCEL),
                                     gtk_menu_tracker_item_action_changed,
                                     self,
                                     NULL);
          if (self->action_subscription != NULL)
            gtk_action_subscription_set_owner_location (self->action_subscription,
                                                        &self->action_subscription);
        }

      g_clear_pointer (&target, g_variant_unref);

      if (GTK_DEBUG_CHECK (ACTIONS))
        {
          if (!strchr (full_action_name, '.'))
            gdk_debug_message ("menutracker: action name %s doesn't look like 'app.' or 'win.'; "
                               "it is unlikely to work", full_action_name);
        }

      if (self->action_subscription == NULL)
        gtk_menu_tracker_item_update_visibility (self);

      g_clear_pointer (&full_action_name, g_free);
    }
  else
    {
      gtk_menu_tracker_item_update_visibility (self);
      self->sensitive = TRUE;
    }

  return self;
}

GtkActionNode *
_gtk_menu_tracker_item_get_action_node (GtkMenuTrackerItem *self)
{
  g_return_val_if_fail (GTK_IS_MENU_TRACKER_ITEM (self), NULL);

  return self->action_node;
}

/*< private >
 * gtk_menu_tracker_item_get_is_separator:
 * @self: A GtkMenuTrackerItem instance
 *
 * Returns: whether the menu item is a separator. If so, only
 * certain properties may need to be obeyed. See the documentation
 * for GtkMenuTrackerItem.
 */
gboolean
gtk_menu_tracker_item_get_is_separator (GtkMenuTrackerItem *self)
{
  return self->is_separator;
}

/*< private >
 * gtk_menu_tracker_item_get_has_submenu:
 * @self: A GtkMenuTrackerItem instance
 *
 * Returns: whether the menu item has a submenu. If so, only
 * certain properties may need to be obeyed. See the documentation
 * for GtkMenuTrackerItem.
 */
gboolean
gtk_menu_tracker_item_get_has_link (GtkMenuTrackerItem *self,
                                    const char         *link_name)
{
  GMenuModel *link;

  link = g_menu_item_get_link (self->item, link_name);

  if (link)
    {
      g_object_unref (link);
      return TRUE;
    }
  else
    return FALSE;
}

const char *
gtk_menu_tracker_item_get_label (GtkMenuTrackerItem *self)
{
  const char *label = NULL;

  g_menu_item_get_attribute (self->item, G_MENU_ATTRIBUTE_LABEL, "&s", &label);

  return label;
}

gboolean
gtk_menu_tracker_item_get_use_markup (GtkMenuTrackerItem *self)
{
  return g_menu_item_get_attribute (self->item, "use-markup", "&s", NULL);
}

/*< private >
 * gtk_menu_tracker_item_get_icon:
 *
 * Returns: (transfer full):
 */
GIcon *
gtk_menu_tracker_item_get_icon (GtkMenuTrackerItem *self)
{
  GVariant *icon_data;
  GIcon *icon;

  icon_data = g_menu_item_get_attribute_value (self->item, "icon", NULL);

  if (icon_data == NULL)
    return NULL;

  icon = g_icon_deserialize (icon_data);
  g_variant_unref (icon_data);

  return icon;
}

/*< private >
 * gtk_menu_tracker_item_get_verb_icon:
 *
 * Returns: (transfer full):
 */
GIcon *
gtk_menu_tracker_item_get_verb_icon (GtkMenuTrackerItem *self)
{
  GVariant *icon_data;
  GIcon *icon;

  icon_data = g_menu_item_get_attribute_value (self->item, "verb-icon", NULL);

  if (icon_data == NULL)
    return NULL;

  icon = g_icon_deserialize (icon_data);
  g_variant_unref (icon_data);

  return icon;
}

gboolean
gtk_menu_tracker_item_get_sensitive (GtkMenuTrackerItem *self)
{
  return self->sensitive;
}

GtkMenuTrackerItemRole
gtk_menu_tracker_item_get_role (GtkMenuTrackerItem *self)
{
  return self->role;
}

gboolean
gtk_menu_tracker_item_get_toggled (GtkMenuTrackerItem *self)
{
  return self->toggled;
}

const char *
gtk_menu_tracker_item_get_accel (GtkMenuTrackerItem *self)
{
  const char *accel;

  if (self->action_key == NULL)
    return NULL;

  if (g_menu_item_get_attribute (self->item, "accel", "&s", &accel))
    return accel;

  if (self->action_subscription == NULL)
    return NULL;

  return gtk_action_subscription_get_snapshot (self->action_subscription)->primary_accel;
}

const char *
gtk_menu_tracker_item_get_action_name (GtkMenuTrackerItem *self)
{

  if (self->action_key == NULL)
    return NULL;

  return gtk_action_key_get_full_name (self->action_key);
}

GVariant *
gtk_menu_tracker_item_get_action_target (GtkMenuTrackerItem *self)
{
  return g_menu_item_get_attribute_value (self->item, G_MENU_ATTRIBUTE_TARGET, NULL);
}

const char *
gtk_menu_tracker_item_get_special (GtkMenuTrackerItem *self)
{
  const char *special = NULL;

  g_menu_item_get_attribute (self->item, "gtk-macos-special", "&s", &special);

  return special;
}

const char *
gtk_menu_tracker_item_get_custom (GtkMenuTrackerItem *self)
{
  const char *custom = NULL;

  g_menu_item_get_attribute (self->item, "custom", "&s", &custom);

  return custom;
}

const char *
gtk_menu_tracker_item_get_display_hint (GtkMenuTrackerItem *self)
{
  const char *display_hint = NULL;

  g_menu_item_get_attribute (self->item, "display-hint", "&s", &display_hint);

  return display_hint;
}

const char *
gtk_menu_tracker_item_get_text_direction (GtkMenuTrackerItem *self)
{
  const char *text_direction = NULL;

  g_menu_item_get_attribute (self->item, "text-direction", "&s", &text_direction);

  return text_direction;
}

GMenuModel *
_gtk_menu_tracker_item_get_link (GtkMenuTrackerItem *self,
                                 const char         *link_name)
{
  return g_menu_item_get_link (self->item, link_name);
}

char *
_gtk_menu_tracker_item_get_link_namespace (GtkMenuTrackerItem *self)
{
  const char *namespace;

  if (g_menu_item_get_attribute (self->item, "action-namespace", "&s", &namespace))
    {
      if (self->action_namespace)
        return g_strjoin (".", self->action_namespace, namespace, NULL);
      else
        return g_strdup (namespace);
    }
  else
    return g_strdup (self->action_namespace);
}

gboolean
gtk_menu_tracker_item_get_should_request_show (GtkMenuTrackerItem *self)
{
  return g_menu_item_get_attribute (self->item, "submenu-action", "&s", NULL);
}

gboolean
gtk_menu_tracker_item_get_submenu_shown (GtkMenuTrackerItem *self)
{
  return self->submenu_shown;
}

static void
gtk_menu_tracker_item_set_submenu_shown (GtkMenuTrackerItem *self,
                                         gboolean            submenu_shown)
{
  if (submenu_shown == self->submenu_shown)
    return;

  self->submenu_shown = submenu_shown;
  g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_SUBMENU_SHOWN]);
}

void
gtk_menu_tracker_item_activated (GtkMenuTrackerItem *self)
{
  g_return_if_fail (GTK_IS_MENU_TRACKER_ITEM (self));

  if (!self->can_activate)
    return;

  gtk_action_node_activate (self->action_node, self->action_key, self->action_target);
}

typedef struct {
  GObject parent;

  GtkMenuTrackerItem    *item;
  GtkActionSubscription *subscription;
  GtkActionKey          *action_key;
  gboolean               first_time;
} GtkMenuTrackerOpener;

typedef struct {
  GObjectClass parent_class;
} GtkMenuTrackerOpenerClass;

GType gtk_menu_tracker_opener_get_type (void);

G_DEFINE_TYPE (GtkMenuTrackerOpener, gtk_menu_tracker_opener, G_TYPE_OBJECT)

static void
gtk_menu_tracker_opener_init (GtkMenuTrackerOpener *self)
{
}

static void
gtk_menu_tracker_opener_finalize (GObject *object)
{
  GtkMenuTrackerOpener *opener = (GtkMenuTrackerOpener *)object;

  if (opener->item != NULL)
    {
      GtkMenuTrackerItem *item = g_object_ref (opener->item);

      g_clear_weak_pointer (&opener->item);

      if (opener->subscription != NULL)
        gtk_action_subscription_cancel (opener->subscription);
      opener->subscription = NULL;

      gtk_action_node_change_state (item->action_node,
                                    opener->action_key,
                                    g_variant_new_boolean (FALSE));

      gtk_menu_tracker_item_set_submenu_shown (item, FALSE);

      g_object_unref (item);
    }

  g_clear_pointer (&opener->action_key, gtk_action_key_unref);

  G_OBJECT_CLASS (gtk_menu_tracker_opener_parent_class)->finalize (object);
}

static void
gtk_menu_tracker_opener_class_init (GtkMenuTrackerOpenerClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gtk_menu_tracker_opener_finalize;
}

static void
gtk_menu_tracker_opener_update (GtkMenuTrackerOpener    *opener,
                                const GtkActionSnapshot *snapshot)
{
  gboolean is_open = TRUE;

  /* We consider the menu as being "open" if the action does not exist
   * or if there is another problem (no state, wrong state type, etc.).
   * If the action exists, with the correct state then we consider it
   * open if we have ever seen this state equal to TRUE.
   *
   * In the event that we see the state equal to FALSE, we force it back
   * to TRUE.  We do not signal that the menu was closed because this is
   * likely to create UI thrashing.
   *
   * The only way the menu can have a true-to-false submenu-shown
   * transition is if the user calls _request_submenu_shown (FALSE).
   * That is handled in _free() below.
   */

  if (snapshot->present &&
      snapshot->state != NULL &&
      g_variant_is_of_type (snapshot->state, G_VARIANT_TYPE_BOOLEAN))
    is_open = g_variant_get_boolean (snapshot->state);

  /* If it is already open, signal that.
   *
   * If it is not open, ask it to open.
   */
  if (is_open)
    gtk_menu_tracker_item_set_submenu_shown (opener->item, TRUE);

  if (!is_open || opener->first_time)
    {
      gtk_action_node_change_state (opener->item->action_node,
                                    opener->action_key,
                                    g_variant_new_boolean (TRUE));
      opener->first_time = FALSE;
    }
}

static void
gtk_menu_tracker_opener_changed (GtkActionSubscription   *subscription,
                                 GtkActionChange          changed,
                                 const GtkActionSnapshot *snapshot,
                                 gpointer                 user_data)
{
  gtk_menu_tracker_opener_update (user_data, snapshot);
}

static GtkMenuTrackerOpener *
gtk_menu_tracker_opener_new (GtkMenuTrackerItem *item,
                             const char         *submenu_action)
{
  GtkMenuTrackerOpener *opener;
  char *full_name;

  opener = g_object_new (gtk_menu_tracker_opener_get_type (), NULL);

  opener->first_time = TRUE;

  g_set_weak_pointer (&opener->item, item);

  if (item->action_namespace != NULL)
    full_name = g_strjoin (".", item->action_namespace, submenu_action, NULL);
  else
    full_name = g_strdup (submenu_action);

  opener->action_key = gtk_action_key_new (full_name);
  g_free (full_name);

  if (opener->action_key != NULL)
    {
      opener->subscription = gtk_action_node_subscribe (item->action_node,
                                 opener->action_key,
                                 NULL,
                                 (GTK_ACTION_INTEREST_PRESENT |
                                  GTK_ACTION_INTEREST_RAW_STATE),
                                 gtk_menu_tracker_opener_changed,
                                 opener,
                                 NULL);
      if (opener->subscription != NULL)
        gtk_action_subscription_set_owner_location (opener->subscription,
                                                    &opener->subscription);
    }

  return opener;
}

void
gtk_menu_tracker_item_request_submenu_shown (GtkMenuTrackerItem *self,
                                             gboolean            shown)
{
  const char *submenu_action;
  gboolean has_submenu_action;

  if (shown == self->submenu_requested)
    return;

  has_submenu_action = g_menu_item_get_attribute (self->item, "submenu-action", "&s", &submenu_action);

  self->submenu_requested = shown;

  /* If we have a submenu action, start a submenu opener and wait
   * for the reply from the client. Otherwise, simply open the
   * submenu immediately.
   */
  if (has_submenu_action)
    {
      if (shown)
        g_object_set_data_full (G_OBJECT (self), "submenu-opener",
                                gtk_menu_tracker_opener_new (self, submenu_action),
                                g_object_unref);
      else
        g_object_set_data (G_OBJECT (self), "submenu-opener", NULL);
    }
  else
    gtk_menu_tracker_item_set_submenu_shown (self, shown);
}

/*
 * gtk_menu_tracker_item_get_is_visible:
 * @self: A GtkMenuTrackerItem instance
 *
 * Don't use this unless you're tracking items for yourself -- normally
 * the tracker will emit add/remove automatically when this changes.
 *
 * Returns: if the item should currently be shown
 */
gboolean
gtk_menu_tracker_item_get_is_visible (GtkMenuTrackerItem *self)
{
  return self->is_visible;
}

/*
 * gtk_menu_tracker_item_may_disappear:
 * @self: A GtkMenuTrackerItem instance
 *
 * Returns: if the item may disappear (ie: is-visible property may change)
 */
gboolean
gtk_menu_tracker_item_may_disappear (GtkMenuTrackerItem *self)
{
  return self->hidden_when != HIDDEN_NEVER;
}
