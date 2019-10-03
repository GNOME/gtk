/*
 * Copyright © 2014 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "gtkmodelbutton.h"

#include "gtkactionhelperprivate.h"
#include "gtkboxlayout.h"
#include "gtkgestureclick.h"
#include "gtkwidgetprivate.h"
#include "gtkmenutrackeritemprivate.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtkpopovermenuprivate.h"
#include "gtkintl.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkiconprivate.h"
#include "gtksizegroup.h"
#include "gtkaccellabelprivate.h"
#include "gtkactionable.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerkey.h"
#include "gtknative.h"

/**
 * SECTION:gtkmodelbutton
 * @Short_description: A button that uses a GAction as model
 * @Title: GtkModelButton
 *
 * GtkModelButton is a button class that can use a #GAction as its model.
 * In contrast to #GtkToggleButton or #GtkRadioButton, which can also
 * be backed by a #GAction via the #GtkActionable:action-name property,
 * GtkModelButton will adapt its appearance according to the kind of
 * action it is backed by, and appear either as a plain, check or
 * radio button.
 *
 * Model buttons are used when popovers from a menu model with
 * gtk_popover_new_from_model(); they can also be used manually in
 * a #GtkPopoverMenu.
 *
 * When the action is specified via the #GtkActionable:action-name
 * and #GtkActionable:action-target properties, the role of the button
 * (i.e. whether it is a plain, check or radio button) is determined by
 * the type of the action and doesn't have to be explicitly specified
 * with the #GtkModelButton:role property.
 *
 * The content of the button is specified by the #GtkModelButton:text
 * and #GtkModelButton:icon properties.
 *
 * The appearance of model buttons can be influenced with the
 * #GtkModelButton:centered and #GtkModelButton:iconic properties.
 *
 * Model buttons have built-in support for submenus in #GtkPopoverMenu.
 * To make a GtkModelButton that opens a submenu when activated, set
 * the #GtkModelButton:menu-name property. To make a button that goes
 * back to the parent menu, you should set the #GtkModelButton:inverted
 * property to place the submenu indicator at the opposite side.
 *
 * # Example
 *
 * |[
 * <object class="GtkPopoverMenu">
 *   <child>
 *     <object class="GtkBox">
 *       <property name="visible">True</property>
 *       <property name="margin">10</property>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">view.cut</property>
 *           <property name="text" translatable="yes">Cut</property>
 *         </object>
 *       </child>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">view.copy</property>
 *           <property name="text" translatable="yes">Copy</property>
 *         </object>
 *       </child>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">view.paste</property>
 *           <property name="text" translatable="yes">Paste</property>
 *         </object>
 *       </child>
 *     </object>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * modelbutton
 * ├── <child>
 * ╰── check
 * ]|
 *
 * |[<!-- language="plain" -->
 * modelbutton
 * ├── <child>
 * ╰── radio
 * ]|
 *
 * |[<!-- language="plain" -->
 * modelbutton
 * ├── <child>
 * ╰── arrow
 * ]|
 *
 * GtkModelButton has a main CSS node with name modelbutton, and a subnode,
 * which will have the name check, radio or arrow, depending on the role
 * of the button and whether it has a menu name set.
 *
 * The subnode is positioned before or after the content nodes and gets the
 * .left or .right style class, depending on where it is located.
 *
 * |[<!-- language="plain" -->
 * button.model
 * ├── <child>
 * ╰── check
 * ]|
 *
 * Iconic model buttons (see #GtkModelButton:iconic) change the name of
 * their main node to button and add a .model style class to it. The indicator
 * subnode is invisible in this case.
 */

struct _GtkModelButton
{
  GtkWidget parent_instance;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *accel_label;
  GtkWidget *start_box;
  GtkWidget *start_indicator;
  GtkWidget *end_indicator;
  GtkWidget *popover;
  GtkActionHelper *action_helper;
  gchar *menu_name;
  GtkButtonRole role;
  GtkSizeGroup *indicators;
  char *accel;
  guint open_timeout;

  guint active : 1;
  guint centered : 1;
  guint iconic : 1;
};

typedef GtkWidgetClass GtkModelButtonClass;

static void gtk_model_button_actionable_iface_init (GtkActionableInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkModelButton, gtk_model_button, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_model_button_actionable_iface_init))

enum
{
  PROP_0,
  PROP_ROLE,
  PROP_ICON,
  PROP_TEXT,
  PROP_USE_MARKUP,
  PROP_ACTIVE,
  PROP_MENU_NAME,
  PROP_POPOVER,
  PROP_ICONIC,
  PROP_ACCEL,
  PROP_INDICATOR_SIZE_GROUP,

  /* actionable properties */
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  LAST_PROP = PROP_ACTION_NAME
};

enum
{
  SIGNAL_CLICKED,
  LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_model_button_set_action_name (GtkActionable *actionable,
                                  const char    *action_name)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  if (!self->action_helper)
    self->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_name (self->action_helper, action_name);
}

static void
gtk_model_button_set_action_target_value (GtkActionable *actionable,
                                          GVariant      *action_target)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  if (!self->action_helper)
    self->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (self->action_helper, action_target);
}

static const char *
gtk_model_button_get_action_name (GtkActionable *actionable)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  return gtk_action_helper_get_action_name (self->action_helper);
}

static GVariant *
gtk_model_button_get_action_target_value (GtkActionable *actionable)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  return gtk_action_helper_get_action_target_value (self->action_helper);
}

static void
gtk_model_button_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_model_button_get_action_name;
  iface->set_action_name = gtk_model_button_set_action_name;
  iface->get_action_target_value = gtk_model_button_get_action_target_value;
  iface->set_action_target_value = gtk_model_button_set_action_target_value;
}

static void
update_node_ordering (GtkModelButton *button)
{
  GtkStyleContext *start_indicator_context = NULL;
  GtkStyleContext *end_indicator_context = NULL;
  GtkWidget *child;

  if (button->start_indicator)
    start_indicator_context = gtk_widget_get_style_context (button->start_indicator);

  if (button->end_indicator)
    end_indicator_context = gtk_widget_get_style_context (button->end_indicator);

  if (gtk_widget_get_direction (GTK_WIDGET (button)) == GTK_TEXT_DIR_LTR)
    {
      if (start_indicator_context)
        {
          gtk_style_context_add_class (start_indicator_context, GTK_STYLE_CLASS_LEFT);
          gtk_style_context_remove_class (start_indicator_context, GTK_STYLE_CLASS_RIGHT);
        }

      if (end_indicator_context)
        {
          gtk_style_context_add_class (end_indicator_context, GTK_STYLE_CLASS_RIGHT);
          gtk_style_context_remove_class (end_indicator_context, GTK_STYLE_CLASS_LEFT);
        }

      child = gtk_widget_get_first_child (GTK_WIDGET (button));
     if (button->start_indicator && child != button->start_box)
        gtk_widget_insert_before (button->start_box, GTK_WIDGET (button), child);

      child = gtk_widget_get_last_child (GTK_WIDGET (button));
      if (button->end_indicator && child != button->end_indicator)
        gtk_widget_insert_after (button->end_indicator, GTK_WIDGET (button), child);
    }
  else
    {
      if (start_indicator_context)
        {
          gtk_style_context_add_class (start_indicator_context, GTK_STYLE_CLASS_RIGHT);
          gtk_style_context_remove_class (start_indicator_context, GTK_STYLE_CLASS_LEFT);
        }

      if (end_indicator_context)
        {
          gtk_style_context_add_class (end_indicator_context, GTK_STYLE_CLASS_LEFT);
          gtk_style_context_remove_class (end_indicator_context, GTK_STYLE_CLASS_RIGHT);
        }

      child = gtk_widget_get_first_child (GTK_WIDGET (button));
      if (button->end_indicator && child != button->end_indicator)
        gtk_widget_insert_before (button->end_indicator, GTK_WIDGET (button), child);

      child = gtk_widget_get_last_child (GTK_WIDGET (button));
      if (child != button->start_box)
        gtk_widget_insert_after (button->start_box, GTK_WIDGET (button), child);
    }
}

static void
update_end_indicator (GtkModelButton *self)
{
  const gboolean is_ltr = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR;
  GtkStyleContext *context;

  if (!self->end_indicator)
    return;

  if (self->role == GTK_BUTTON_ROLE_NORMAL &&
      (self->menu_name != NULL || self->popover != NULL))
    {
      gtk_icon_set_image (GTK_ICON (self->end_indicator), GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT);
    }

  context = gtk_widget_get_style_context (self->end_indicator);

  if (is_ltr)
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_LEFT);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);
    }
  else
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_RIGHT);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);
    }
}

static GtkStateFlags
get_start_indicator_state (GtkModelButton *self)
{
  GtkStateFlags state = gtk_widget_get_state_flags (GTK_WIDGET (self));

  if (self->role == GTK_BUTTON_ROLE_CHECK ||
      self->role == GTK_BUTTON_ROLE_RADIO)
    {
      if (self->active)
        state |= GTK_STATE_FLAG_CHECKED;
      else
        state &= ~GTK_STATE_FLAG_CHECKED;
    }

  return state;
}

static void
update_start_indicator (GtkModelButton *self)
{
  const gboolean is_ltr = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR;
  GtkStyleContext *context;

  if (!self->start_indicator)
    return;

  if (self->role == GTK_BUTTON_ROLE_CHECK)
      gtk_icon_set_image (GTK_ICON (self->start_indicator), GTK_CSS_IMAGE_BUILTIN_CHECK);
  else if (self->role == GTK_BUTTON_ROLE_RADIO)
      gtk_icon_set_image (GTK_ICON (self->start_indicator), GTK_CSS_IMAGE_BUILTIN_OPTION);
  else if (self->role == GTK_BUTTON_ROLE_TITLE)
    gtk_icon_set_image (GTK_ICON (self->start_indicator), GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT);
  else
    gtk_icon_set_image (GTK_ICON (self->start_indicator), GTK_CSS_IMAGE_BUILTIN_NONE);

  gtk_widget_set_state_flags (self->start_indicator, get_start_indicator_state (self), TRUE);

  context = gtk_widget_get_style_context (self->start_indicator);

  if (is_ltr)
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_RIGHT);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);
    }
  else
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_LEFT);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);
    }
}

static void
gtk_model_button_update_state (GtkModelButton *self)
{
  update_start_indicator (self);
  update_end_indicator (self);

  if (self->iconic)
    gtk_widget_set_state_flags (GTK_WIDGET (self),
                                get_start_indicator_state (self),
                                TRUE);
}

static void
gtk_model_button_state_flags_changed (GtkWidget     *widget,
                                      GtkStateFlags  previous_flags)
{
  gtk_model_button_update_state (GTK_MODEL_BUTTON (widget));

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->state_flags_changed (widget, previous_flags);
}

static void
gtk_model_button_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_dir)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);

  gtk_model_button_update_state (button);
  update_node_ordering (button);

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->direction_changed (widget, previous_dir);
}

static void
update_node_name (GtkModelButton *self)
{
  AtkObject *accessible;
  AtkRole a11y_role;
  const gchar *start_name;
  const gchar *end_name;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (self));
  switch (self->role)
    {
    case GTK_BUTTON_ROLE_TITLE:
      a11y_role = ATK_ROLE_PUSH_BUTTON;
      start_name = I_("arrow");
      end_name = I_("none");
      break;
    case GTK_BUTTON_ROLE_NORMAL:
      a11y_role = ATK_ROLE_PUSH_BUTTON;
      start_name = I_("none");
      if (self->menu_name || self->popover)
        end_name = I_("arrow");
      else
        end_name = I_("none");
      break;

    case GTK_BUTTON_ROLE_CHECK:
      a11y_role = ATK_ROLE_CHECK_BOX;
      start_name = I_("check");
      end_name = I_("none");
      break;

    case GTK_BUTTON_ROLE_RADIO:
      a11y_role = ATK_ROLE_RADIO_BUTTON;
      start_name = I_("radio");
      end_name = I_("none");
      break;

    default:
      g_assert_not_reached ();
    }

  if (self->iconic)
    {
      start_name = I_("none");
      end_name = I_("none");
    }

  atk_object_set_role (accessible, a11y_role);

  if (start_name != I_("none") && !self->start_indicator)
    {
      self->start_indicator = gtk_icon_new (start_name);
      gtk_widget_set_halign (self->start_indicator, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (self->start_indicator, GTK_ALIGN_CENTER);
      update_start_indicator (self);

      gtk_container_add (GTK_CONTAINER (self->start_box), self->start_indicator);
    }
  else if (self->start_indicator)
    {
      gtk_container_remove (GTK_CONTAINER (self->start_box), self->start_indicator);
      self->start_indicator = NULL;
    }

  if (end_name != I_("none") && !self->end_indicator)
    {
      self->end_indicator = gtk_icon_new (end_name);
      gtk_widget_set_halign (self->end_indicator, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (self->end_indicator, GTK_ALIGN_CENTER);
      gtk_widget_set_parent (self->end_indicator, GTK_WIDGET (self));
      update_end_indicator (self);
    }
  else
    {
      g_clear_pointer (&self->end_indicator, gtk_widget_unparent);
    }
}

static void
gtk_model_button_set_role (GtkModelButton *self,
                           GtkButtonRole   role)
{
  if (role == self->role)
    return;

  self->role = role;

  if (role == GTK_BUTTON_ROLE_TITLE)
    {
      gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "title");
      gtk_widget_set_halign (self->label, GTK_ALIGN_CENTER);
    }
  else
    {
      gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "title");
      gtk_widget_set_halign (self->label, GTK_ALIGN_START);
    }

  update_node_name (self);
  gtk_model_button_update_state (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ROLE]);
}

static void
update_visibility (GtkModelButton *self)
{
  gboolean has_icon;
  gboolean has_text;

  has_icon = self->image && gtk_image_get_storage_type (GTK_IMAGE (self->image)) != GTK_IMAGE_EMPTY;
  has_text = gtk_label_get_text (GTK_LABEL (self->label))[0] != '\0';

  gtk_widget_set_visible (self->label, has_text && (!self->iconic || !has_icon));
  gtk_widget_set_hexpand (self->label,
                          gtk_widget_get_visible (self->label) && !has_icon);

  if (self->image)
    {
      gtk_widget_set_visible (self->image, has_icon && (self->iconic || !has_text));
      gtk_widget_set_hexpand (self->image,
                              has_icon && (!has_text || !gtk_widget_get_visible (self->label)));
    }
}

static void
gtk_model_button_set_icon (GtkModelButton *self,
                           GIcon          *icon)
{
  if (!self->image && icon)
    {
      self->image = gtk_image_new_from_gicon (icon);
      gtk_widget_insert_before (self->image, GTK_WIDGET (self), self->label);
    }
  else if (self->image && !icon)
    {
      g_clear_pointer (&self->image, gtk_widget_unparent);
    }
  else if (icon)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (self->image), icon);
    }

  update_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
}

static void
gtk_model_button_set_text (GtkModelButton *button,
                           const gchar    *text)
{
  gtk_label_set_text_with_mnemonic (GTK_LABEL (button->label),
                                    text ? text : "");
  update_visibility (button);
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_TEXT]);
}

static void
gtk_model_button_set_use_markup (GtkModelButton *button,
                                 gboolean        use_markup)
{
  use_markup = !!use_markup;
  if (gtk_label_get_use_markup (GTK_LABEL (button->label)) == use_markup)
    return;

  gtk_label_set_use_markup (GTK_LABEL (button->label), use_markup);
  update_visibility (button);
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_USE_MARKUP]);
}

static void
gtk_model_button_set_active (GtkModelButton *button,
                             gboolean        active)
{
  active = !!active;
  if (button->active == active)
    return;

  button->active = active;
  gtk_model_button_update_state (button);
  gtk_widget_queue_draw (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ACTIVE]);
}

static void
gtk_model_button_set_menu_name (GtkModelButton *button,
                                const gchar    *menu_name)
{
  g_free (button->menu_name);
  button->menu_name = g_strdup (menu_name);

  update_node_name (button);
  gtk_model_button_update_state (button);

  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_MENU_NAME]);
}

static void
gtk_model_button_set_iconic (GtkModelButton *self,
                             gboolean        iconic)
{
  GtkCssNode *widget_node;
  GtkStyleContext *context;

  iconic = !!iconic;
  if (self->iconic == iconic)
    return;

  self->iconic = iconic;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  if (iconic)
    {
      gtk_widget_hide (self->start_box);
      gtk_css_node_set_name (widget_node, I_("button"));
      gtk_style_context_add_class (context, "model");
      gtk_style_context_add_class (context, "image-button");
      gtk_style_context_remove_class (context, "flat");
    }
  else
    {
      gtk_widget_show (self->start_box);
      gtk_css_node_set_name (widget_node, I_("modelbutton"));
      gtk_style_context_remove_class (context, "model");
      gtk_style_context_remove_class (context, "image-button");
      gtk_style_context_add_class (context, "flat");
    }

  self->centered = iconic;

  if (!iconic)
    {
      gtk_container_remove (GTK_CONTAINER (self->start_box), self->start_indicator);
      self->start_indicator = NULL;
      g_clear_pointer (&self->end_indicator, gtk_widget_unparent);
    }

  update_node_name (self);
  update_visibility (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICONIC]);
}

static void
gtk_model_button_set_popover (GtkModelButton *button,
                              GtkWidget      *popover)
{
  if (button->popover)
    gtk_popover_set_relative_to (GTK_POPOVER (button->popover), NULL);

  button->popover = popover;

  if (button->popover)
    {
      gtk_popover_set_relative_to (GTK_POPOVER (button->popover), GTK_WIDGET (button));
      gtk_popover_set_position (GTK_POPOVER (button->popover), GTK_POS_RIGHT);
    }

  update_node_name (button);
  gtk_model_button_update_state (button);

  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_POPOVER]);
}

static void
update_accel (GtkModelButton *self,
              const char     *accel)
{
  if (accel)
    {
      guint key;
      GdkModifierType mods;
      GtkAccelLabelClass *accel_class;
      char *str;

      if (!self->accel_label)
        {
          self->accel_label = g_object_new (GTK_TYPE_LABEL,
                                            "css-name", "accelerator",
                                            NULL);
          gtk_widget_insert_before (self->accel_label, GTK_WIDGET (self), NULL);
        }

      gtk_accelerator_parse (accel, &key, &mods);

      accel_class = g_type_class_ref (GTK_TYPE_ACCEL_LABEL);
      str = _gtk_accel_label_class_get_accelerator_label (accel_class, key, mods);
      gtk_label_set_label (GTK_LABEL (self->accel_label), str);
      g_free (str);
      g_type_class_unref (accel_class);
    }
  else
    {
      g_clear_pointer (&self->accel_label, gtk_widget_unparent);
    }
}

static void
gtk_model_button_set_accel (GtkModelButton *button,
                            const char     *accel)
{
  g_free (button->accel);
  button->accel = g_strdup (accel);
  update_accel (button, button->accel);

  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ACCEL]);
}

static void
gtk_model_button_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ROLE:
      g_value_set_enum (value, self->role);
      break;

    case PROP_ICON:
      g_value_set_object (value, self->image ? gtk_image_get_gicon (GTK_IMAGE (self->image)) : NULL);
      break;

    case PROP_TEXT:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (self->label)));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, gtk_label_get_use_markup (GTK_LABEL (self->label)));
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;

    case PROP_MENU_NAME:
      g_value_set_string (value, self->menu_name);
      break;

    case PROP_POPOVER:
      g_value_set_object (value, self->popover);
      break;

    case PROP_ICONIC:
      g_value_set_boolean (value, self->iconic);
      break;

    case PROP_ACCEL:
      g_value_set_string (value, self->accel);
      break;

    case PROP_INDICATOR_SIZE_GROUP:
      g_value_set_object (value, self->indicators);
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (self->action_helper));
      break;

    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (self->action_helper));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_model_button_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ROLE:
      gtk_model_button_set_role (button, g_value_get_enum (value));
      break;

    case PROP_ICON:
      gtk_model_button_set_icon (button, g_value_get_object (value));
      break;

    case PROP_TEXT:
      gtk_model_button_set_text (button, g_value_get_string (value));
      break;

    case PROP_USE_MARKUP:
      gtk_model_button_set_use_markup (button, g_value_get_boolean (value));
      break;

    case PROP_ACTIVE:
      gtk_model_button_set_active (button, g_value_get_boolean (value));
      break;

    case PROP_MENU_NAME:
      gtk_model_button_set_menu_name (button, g_value_get_string (value));
      break;

    case PROP_POPOVER:
      gtk_model_button_set_popover (button, (GtkWidget *)g_value_get_object (value));
      break;

    case PROP_ICONIC:
      gtk_model_button_set_iconic (button, g_value_get_boolean (value));
      break;

    case PROP_ACCEL:
      gtk_model_button_set_accel (button, g_value_get_string (value));
      break;

    case PROP_INDICATOR_SIZE_GROUP:
      if (button->indicators)
        gtk_size_group_remove_widget (button->indicators, button->start_box);
      button->indicators = GTK_SIZE_GROUP (g_value_get_object (value));
      if (button->indicators)
        gtk_size_group_add_widget (button->indicators, button->start_box);
      break;

    case PROP_ACTION_NAME:
      gtk_model_button_set_action_name (GTK_ACTIONABLE (button), g_value_get_string (value));
      break;

    case PROP_ACTION_TARGET:
      gtk_model_button_set_action_target_value (GTK_ACTIONABLE (button), g_value_get_variant (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_model_button_dispose (GObject *object)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (object);

  g_clear_pointer (&model_button->menu_name, g_free);

  G_OBJECT_CLASS (gtk_model_button_parent_class)->dispose (object);
}

static void
switch_menu (GtkModelButton *button)
{
  GtkWidget *stack;

  stack = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_STACK);
  if (stack != NULL)
    gtk_stack_set_visible_child_name (GTK_STACK (stack), button->menu_name);
}

static void
close_menu (GtkModelButton *self)
{
  GtkWidget *popover;

  popover = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_POPOVER);
  while (popover != NULL)
    {
      gtk_popover_popdown (GTK_POPOVER (popover));
      if (GTK_IS_POPOVER_MENU (popover))
        popover = gtk_popover_menu_get_parent_menu (GTK_POPOVER_MENU (popover));
      else
        popover = NULL;
    }
}

static void
gtk_model_button_clicked (GtkGestureClick *gesture,
                          guint            n_press,
                          double          x,
                          double          y,
                          GtkModelButton *self)
{
  if (self->menu_name != NULL)
    {
      switch_menu (self);
    }
  else if (self->popover != NULL)
    {
      GtkPopoverMenu *menu;
      GtkWidget *submenu;

      menu = (GtkPopoverMenu *)gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_POPOVER_MENU);
      submenu = self->popover;
      gtk_popover_popup (GTK_POPOVER (submenu));
      gtk_popover_menu_set_open_submenu (menu, submenu);
      gtk_popover_menu_set_parent_menu (GTK_POPOVER_MENU (submenu), GTK_WIDGET (menu));
    }
  else if (self->role == GTK_BUTTON_ROLE_NORMAL)
    {
      close_menu (self);
    }

  g_signal_emit (self, signals[SIGNAL_CLICKED], 0);
}

static void
gtk_model_button_finalize (GObject *object)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  g_clear_object (&button->action_helper);
  g_clear_pointer (&button->image, gtk_widget_unparent);
  g_clear_pointer (&button->label, gtk_widget_unparent);
  g_clear_pointer (&button->start_box, gtk_widget_unparent);
  g_clear_pointer (&button->accel_label, gtk_widget_unparent);
  g_clear_pointer (&button->end_indicator, gtk_widget_unparent);
  g_free (button->accel);
  g_clear_pointer (&button->popover, gtk_widget_unparent);

  if (button->open_timeout)
    g_source_remove (button->open_timeout);

  G_OBJECT_CLASS (gtk_model_button_parent_class)->finalize (object);
}

static void
gtk_model_button_root (GtkWidget *widget)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (widget);
  GtkRoot *root;
  GtkApplication *app;
  const char *action_name;
  GVariant *action_target;

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->root (widget);

  if (!self->accel)
    return;

  root = gtk_widget_get_root (widget);

  if (!GTK_IS_WINDOW (root))
    return;

  app = gtk_window_get_application (GTK_WINDOW (root));

  if (!app)
    return;

  action_name = gtk_actionable_get_action_name (GTK_ACTIONABLE (widget));
  action_target = gtk_actionable_get_action_target_value (GTK_ACTIONABLE (widget));

  if (action_name)
    {
      char *detailed;
      char **accels;

      detailed = g_action_print_detailed_name (action_name, action_target);
      accels = gtk_application_get_accels_for_action (app, detailed);

      update_accel (self, accels[0]);

      g_strfreev (accels);
      g_free (detailed);
    }
}

static gboolean
gtk_model_button_focus (GtkWidget        *widget,
                        GtkDirectionType  direction)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);

  if (gtk_widget_is_focus (widget))
    {
      if (direction == GTK_DIR_LEFT &&
          button->role == GTK_BUTTON_ROLE_TITLE &&
          button->menu_name != NULL)
        {
          switch_menu (button);
          return TRUE;
        }
      else if (direction == GTK_DIR_RIGHT &&
               button->role == GTK_BUTTON_ROLE_NORMAL &&
               button->menu_name != NULL)
        {
          switch_menu (button);
          return TRUE;
        }
      else if (direction == GTK_DIR_RIGHT &&
               button->role == GTK_BUTTON_ROLE_NORMAL &&
               button->popover != NULL)
        {
          GtkPopoverMenu *menu;
          GtkWidget *submenu;

          menu = GTK_POPOVER_MENU (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER_MENU));
          submenu = button->popover;
          gtk_popover_popup (GTK_POPOVER (submenu));
          gtk_popover_menu_set_open_submenu (menu, submenu);
          gtk_popover_menu_set_parent_menu (GTK_POPOVER_MENU (submenu), GTK_WIDGET (menu));
          return TRUE;
        }
    }
  else
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_model_button_class_init (GtkModelButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_model_button_dispose;
  object_class->finalize = gtk_model_button_finalize;
  object_class->get_property = gtk_model_button_get_property;
  object_class->set_property = gtk_model_button_set_property;

  widget_class->state_flags_changed = gtk_model_button_state_flags_changed;
  widget_class->direction_changed = gtk_model_button_direction_changed;
  widget_class->focus = gtk_model_button_focus;
  widget_class->root = gtk_model_button_root;

  /**
   * GtkModelButton:role:
   *
   * Specifies whether the button is a plain, check or radio button.
   * When #GtkActionable:action-name is set, the role will be determined
   * from the action and does not have to be set explicitly.
   */
  properties[PROP_ROLE] =
    g_param_spec_enum ("role",
                       P_("Role"),
                       P_("The role of this button"),
                       GTK_TYPE_BUTTON_ROLE,
                       GTK_BUTTON_ROLE_NORMAL,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:icon:
   *
   * A #GIcon that will be used if iconic appearance for the button is
   * desired.
   */
  properties[PROP_ICON] = 
    g_param_spec_object ("icon",
                         P_("Icon"),
                         P_("The icon"),
                         G_TYPE_ICON,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:text:
   *
   * The label for the button.
   */
  properties[PROP_TEXT] =
    g_param_spec_string ("text",
                         P_("Text"),
                         P_("The text"),
                         "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:use-markup:
   *
   * If %TRUE, XML tags in the text of the button are interpreted as by
   * pango_parse_markup() to format the enclosed spans of text. If %FALSE, the
   * text will be displayed verbatim.
   */
  properties[PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup",
                          P_("Use markup"),
                          P_("The text of the button includes XML markup. See pango_parse_markup()"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:active:
   *
   * The state of the button. This is reflecting the state of the associated
   * #GAction.
   */
  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          P_("Active"),
                          P_("Active"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:menu-name:
   *
   * The name of a submenu to open when the button is activated.  * If this is set, the button should not have an action associated with it.
   */
  properties[PROP_MENU_NAME] =
    g_param_spec_string ("menu-name",
                         P_("Menu name"),
                         P_("The name of the menu to open"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

 properties[PROP_POPOVER] =
   g_param_spec_object ("popover",
                        P_("Popover"),
                        P_("Popover to open"),
                        GTK_TYPE_POPOVER,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:iconic:
   *
   * If this property is set, the button will show an icon if one is set.
   * If no icon is set, the text will be used. This is typically used for
   * horizontal sections of linked buttons.
   */
  properties[PROP_ICONIC] =
    g_param_spec_boolean ("iconic",
                          P_("Iconic"),
                          P_("Whether to prefer the icon over text"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:indicator-size-group:
   *
   * Containers like #GtkPopoverMenu can provide a size group
   * in this property to align the checks and radios of all
   * the model buttons in a menu.
   */
  properties[PROP_INDICATOR_SIZE_GROUP] =
    g_param_spec_object ("indicator-size-group",
                          P_("Size group"),
                          P_("Size group for checks and radios"),
                          GTK_TYPE_SIZE_GROUP,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACCEL] =
    g_param_spec_string ("accel",
                         P_("Accel"),
                         P_("The accelerator"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, LAST_PROP, properties);

  g_object_class_override_property (object_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (object_class, PROP_ACTION_TARGET, "action-target");

  signals[SIGNAL_CLICKED] = g_signal_new (I_("clicked"),
                                          G_OBJECT_CLASS_TYPE (object_class),
                                          G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                          0,
                                          NULL, NULL,
                                          NULL,
                                          G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (class), ATK_ROLE_PUSH_BUTTON);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), I_("modelbutton"));
}

static void
close_submenus (GtkPopover *popover)
{
  GtkPopoverMenu *menu;

  if (GTK_IS_POPOVER_MENU (popover))
    {
      GtkWidget *submenu;

      menu = GTK_POPOVER_MENU (popover);
      submenu = gtk_popover_menu_get_open_submenu (menu);
      if (submenu)
        {
          close_submenus (GTK_POPOVER (submenu));
          gtk_popover_popdown (GTK_POPOVER (submenu));
          gtk_popover_menu_set_open_submenu (menu, NULL);
        }
    }
}

static gboolean
open_submenu (gpointer data)
{
  GtkModelButton *button = data;
  GtkPopover *popover;

  popover = (GtkPopover*)gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER);

  if (GTK_IS_POPOVER_MENU (popover))
    {
      gtk_popover_menu_set_active_item (GTK_POPOVER_MENU (popover), GTK_WIDGET (button));

      if (button->popover)
        {
          GtkWidget *submenu = button->popover;

          if (gtk_popover_menu_get_open_submenu (GTK_POPOVER_MENU (popover)) != submenu)
            close_submenus (popover);

          gtk_popover_popup (GTK_POPOVER (submenu));
          gtk_popover_menu_set_open_submenu (GTK_POPOVER_MENU (popover), submenu);
          gtk_popover_menu_set_parent_menu (GTK_POPOVER_MENU (submenu), GTK_WIDGET (popover));
        }
    }

  button->open_timeout = 0;

  return G_SOURCE_REMOVE;
}

#define OPEN_TIMEOUT 80

static void
start_open (GtkModelButton *button)
{
  if (button->open_timeout)
    g_source_remove (button->open_timeout);

  if (button->popover &&
      gtk_widget_get_visible (button->popover))
    return;

  button->open_timeout = g_timeout_add (OPEN_TIMEOUT, open_submenu, button);
  g_source_set_name_by_id (button->open_timeout, "[gtk] open_submenu");
}

static void
stop_open (GtkModelButton *button)
{
  if (button->open_timeout)
    {
      g_source_remove (button->open_timeout);
      button->open_timeout = 0;
    }
}

static void
enter_cb (GtkEventController *controller,
          double              x,
          double              y,
          GdkCrossingMode     mode,
          GdkNotifyType       type,
          gpointer            data)
{
  GtkWidget *target;
  GtkWidget *popover;
  gboolean is;
  gboolean contains;

  target = gtk_event_controller_get_widget (controller);
  popover = gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU);

  g_object_get (controller,
                "is-pointer-focus", &is,
                "contains-pointer-focus", &contains,
                NULL);

  if (popover && (is || contains))
    {
      if (gtk_popover_menu_get_open_submenu (GTK_POPOVER_MENU (popover)) != NULL)
        start_open (GTK_MODEL_BUTTON (target));
      else
        open_submenu (target);
    }
}

static void
motion_cb (GtkEventController *controller,
           double              x,
           double              y,
           gpointer            data)
{
  start_open (GTK_MODEL_BUTTON (data));
}

static void
leave_cb (GtkEventController *controller,
          GdkCrossingMode     mode,
          GdkNotifyType       type,
          gpointer            data)
{
  stop_open (GTK_MODEL_BUTTON (data));
}

static void
focus_in_cb (GtkEventController *controller,
             GdkCrossingMode     mode,
             GdkNotifyType       type,
             gpointer            data)
{
  GtkWidget *target;
  GtkWidget *popover;

  target = gtk_event_controller_get_widget (controller);
  popover = gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU);

  if (popover)
    gtk_popover_menu_set_active_item (GTK_POPOVER_MENU (popover), target);
}

static void
gtk_model_button_init (GtkModelButton *self)
{
  GtkEventController *controller;
  GtkGesture *gesture;

  self->role = GTK_BUTTON_ROLE_NORMAL;
  self->label = gtk_label_new ("");
  gtk_widget_set_halign (self->label, GTK_ALIGN_START);
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));

  self->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_insert_after (self->start_box, GTK_WIDGET (self), NULL);
  update_node_ordering (self);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "flat");

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (enter_cb), self);
  g_signal_connect (controller, "motion", G_CALLBACK (motion_cb), self);
  g_signal_connect (controller, "leave", G_CALLBACK (leave_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "focus-in", G_CALLBACK (focus_in_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "released", G_CALLBACK (gtk_model_button_clicked), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

/**
 * gtk_model_button_new:
 *
 * Creates a new GtkModelButton.
 *
 * Returns: the newly created #GtkModelButton widget
 */
GtkWidget *
gtk_model_button_new (void)
{
  return g_object_new (GTK_TYPE_MODEL_BUTTON, NULL);
}
