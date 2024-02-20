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

#include "gtkbutton.h"
#include "gtkbuttonprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkmenutrackeritem.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkrender.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtkpopover.h"
#include "gtkintl.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcontainerprivate.h"

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
  GtkButton parent_instance;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkCssGadget *gadget;
  GtkCssGadget *indicator_gadget;
  gboolean active;
  gboolean centered;
  gboolean inverted;
  gboolean iconic;
  gchar *menu_name;
  GtkButtonRole role;
};

typedef GtkButtonClass GtkModelButtonClass;

G_DEFINE_TYPE (GtkModelButton, gtk_model_button, GTK_TYPE_BUTTON)

enum
{
  PROP_0,
  PROP_ROLE,
  PROP_ICON,
  PROP_TEXT,
  PROP_USE_MARKUP,
  PROP_ACTIVE,
  PROP_MENU_NAME,
  PROP_INVERTED,
  PROP_CENTERED,
  PROP_ICONIC,
  LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

static gboolean
indicator_is_left (GtkWidget *widget)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);

  return ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL && !button->inverted) ||
          (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR && button->inverted));
}

static void
gtk_model_button_update_state (GtkModelButton *button,
                               GtkStateFlags   previous_flags)
{
  GtkStateFlags state;
  GtkStateFlags indicator_state;
  GtkCssImageBuiltinType image_type = GTK_CSS_IMAGE_BUILTIN_NONE;

  state = gtk_widget_get_state_flags (GTK_WIDGET (button));
  indicator_state = state;

  gtk_css_gadget_set_state (button->gadget, state);

  if (button->role == GTK_BUTTON_ROLE_CHECK)
    {
      if (button->active && !button->menu_name)
        {
          indicator_state |= GTK_STATE_FLAG_CHECKED;
          image_type = GTK_CSS_IMAGE_BUILTIN_CHECK;
        }
      else
        {
          indicator_state &= ~GTK_STATE_FLAG_CHECKED;
        }
    }
  if (button->role == GTK_BUTTON_ROLE_RADIO)
    {
      if (button->active && !button->menu_name)
        {
          indicator_state |= GTK_STATE_FLAG_CHECKED;
          image_type = GTK_CSS_IMAGE_BUILTIN_OPTION;
        }
      else
        {
          indicator_state &= ~GTK_STATE_FLAG_CHECKED;
        }
    }

  if (button->menu_name)
    {
      if (indicator_is_left (GTK_WIDGET (button)))
        image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT;
      else
        image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT;
    }

  gtk_builtin_icon_set_image (GTK_BUILTIN_ICON (button->indicator_gadget), image_type);

  if (button->iconic)
    gtk_css_gadget_set_state (button->gadget, indicator_state);
  else
    gtk_css_gadget_set_state (button->gadget, state);

  gtk_css_gadget_set_state (button->indicator_gadget, indicator_state);

  if (button->role == GTK_BUTTON_ROLE_CHECK ||
      button->role == GTK_BUTTON_ROLE_RADIO)
    {
      AtkObject *object = _gtk_widget_peek_accessible (GTK_WIDGET (button));
      gboolean was_checked = (previous_flags & GTK_STATE_FLAG_CHECKED) != 0;
      gboolean is_checked = (indicator_state & GTK_STATE_FLAG_CHECKED) != 0;
      if (object && (was_checked != is_checked))
        atk_object_notify_state_change (object, ATK_STATE_CHECKED, is_checked);
    }
}

static void
update_node_ordering (GtkModelButton *button)
{
  GtkCssNode *widget_node, *indicator_node, *node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (button));
  indicator_node = gtk_css_gadget_get_node (button->indicator_gadget);

  if (indicator_is_left (GTK_WIDGET (button)))
    {
      gtk_css_gadget_add_class (button->indicator_gadget, GTK_STYLE_CLASS_LEFT);
      gtk_css_gadget_remove_class (button->indicator_gadget, GTK_STYLE_CLASS_RIGHT);

      node = gtk_css_node_get_first_child (widget_node);
      if (node != indicator_node)
        gtk_css_node_insert_before (widget_node, indicator_node, node);
    }
  else
    {
      gtk_css_gadget_remove_class (button->indicator_gadget, GTK_STYLE_CLASS_LEFT);
      gtk_css_gadget_add_class (button->indicator_gadget, GTK_STYLE_CLASS_RIGHT);

      node = gtk_css_node_get_last_child (widget_node);
      if (node != indicator_node)
        gtk_css_node_insert_after (widget_node, indicator_node, node);
    }
}

static void
gtk_model_button_state_flags_changed (GtkWidget     *widget,
                                      GtkStateFlags  previous_flags)
{
  gtk_model_button_update_state (GTK_MODEL_BUTTON (widget), previous_flags);

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->state_flags_changed (widget, previous_flags);
}

static void
gtk_model_button_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_dir)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);

  gtk_model_button_update_state (button, GTK_STATE_FLAG_NORMAL);
  update_node_ordering (button);

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->direction_changed (widget, previous_dir);
}

static void
update_node_name (GtkModelButton *button)
{
  AtkObject *accessible;
  AtkRole a11y_role;
  const gchar *indicator_name;
  gboolean indicator_visible;
  GtkCssNode *indicator_node;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (button));
  switch (button->role)
    {
    case GTK_BUTTON_ROLE_NORMAL:
      a11y_role = ATK_ROLE_PUSH_BUTTON;
      if (button->menu_name)
        {
          indicator_name = I_("arrow");
          indicator_visible = TRUE;
        }
      else
        {
          indicator_name = I_("check");
          indicator_visible = FALSE;
        }
      break;

    case GTK_BUTTON_ROLE_CHECK:
      a11y_role = ATK_ROLE_CHECK_BOX;
      indicator_name = I_("check");
      indicator_visible = TRUE;
      break;

    case GTK_BUTTON_ROLE_RADIO:
      a11y_role = ATK_ROLE_RADIO_BUTTON;
      indicator_name = I_("radio");
      indicator_visible = TRUE;
      break;

    default:
      g_assert_not_reached ();
    }

  if (button->iconic)
    indicator_visible = FALSE;

  atk_object_set_role (accessible, a11y_role);

  indicator_node = gtk_css_gadget_get_node (button->indicator_gadget);
  gtk_css_node_set_name (indicator_node, indicator_name);
  gtk_css_node_set_visible (indicator_node, indicator_visible);
}

static void
gtk_model_button_set_role (GtkModelButton *button,
                           GtkButtonRole   role)
{
  if (role == button->role)
    return;

  button->role = role;

  update_node_name (button);

  gtk_model_button_update_state (button, GTK_STATE_FLAG_NORMAL);
  gtk_widget_queue_draw (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ROLE]);
}

static void
update_visibility (GtkModelButton *button)
{
  gboolean has_icon;
  gboolean has_text;

  has_icon = gtk_image_get_storage_type (GTK_IMAGE (button->image)) != GTK_IMAGE_EMPTY;
  has_text = gtk_label_get_text (GTK_LABEL (button->label))[0] != '\0';

  gtk_widget_set_visible (button->image, has_icon && (button->iconic || !has_text));
  gtk_widget_set_visible (button->label, has_text && (!button->iconic || !has_icon));
}

static void
gtk_model_button_set_icon (GtkModelButton *button,
                           GIcon          *icon)
{
  gtk_image_set_from_gicon (GTK_IMAGE (button->image), icon, GTK_ICON_SIZE_MENU);
  update_visibility (button);
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ICON]);
}

static void
gtk_model_button_set_text (GtkModelButton *button,
                           const gchar    *text)
{
  gtk_label_set_text_with_mnemonic (GTK_LABEL (button->label), text);
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
  gtk_model_button_update_state (button, GTK_STATE_FLAG_NORMAL);
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
  gtk_model_button_update_state (button, GTK_STATE_FLAG_NORMAL);

  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_MENU_NAME]);
}

static void
gtk_model_button_set_inverted (GtkModelButton *button,
                               gboolean        inverted)
{
  inverted = !!inverted;
  if (button->inverted == inverted)
    return;

  button->inverted = inverted;
  gtk_model_button_update_state (button, GTK_STATE_FLAG_NORMAL);
  update_node_ordering (button);
  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_INVERTED]);
}

static void
gtk_model_button_set_centered (GtkModelButton *button,
                               gboolean        centered)
{
  centered = !!centered;
  if (button->centered == centered)
    return;

  button->centered = centered;
  gtk_widget_set_halign (button->box, button->centered ? GTK_ALIGN_CENTER : GTK_ALIGN_FILL);
  gtk_widget_queue_draw (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_CENTERED]);
}

static void
gtk_model_button_set_iconic (GtkModelButton *button,
                             gboolean        iconic)
{
  GtkCssNode *widget_node;
  GtkCssNode *indicator_node;

  iconic = !!iconic;
  if (button->iconic == iconic)
    return;

  button->iconic = iconic;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (button));
  indicator_node = gtk_css_gadget_get_node (button->indicator_gadget);
  if (iconic)
    {
      gtk_css_node_set_name (widget_node, I_("button"));
      gtk_css_gadget_add_class (button->gadget, "model");
      gtk_css_gadget_add_class (button->gadget, "image-button");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
      gtk_css_node_set_visible (indicator_node, FALSE);
    }
  else
    {
      gtk_css_node_set_name (widget_node, I_("modelbutton"));
      gtk_css_gadget_remove_class (button->gadget, "model");
      gtk_css_gadget_remove_class (button->gadget, "image-button");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      gtk_css_node_set_visible (indicator_node,
                                button->role != GTK_BUTTON_ROLE_NORMAL ||
                                button->menu_name == NULL);
    }

  update_visibility (button);
  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ICONIC]);
}

static void
gtk_model_button_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ROLE:
      g_value_set_enum (value, button->role);
      break;

    case PROP_ICON:
      {
        GIcon *icon;
        gtk_image_get_gicon (GTK_IMAGE (button->image), &icon, NULL);
        g_value_set_object (value, icon);
      }
      break;

    case PROP_TEXT:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (button->label)));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, gtk_label_get_use_markup (GTK_LABEL (button->label)));
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, button->active);
      break;

    case PROP_MENU_NAME:
      g_value_set_string (value, button->menu_name);
      break;

    case PROP_INVERTED:
      g_value_set_boolean (value, button->inverted);
      break;

    case PROP_CENTERED:
      g_value_set_boolean (value, button->centered);
      break;

    case PROP_ICONIC:
      g_value_set_boolean (value, button->iconic);
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

    case PROP_INVERTED:
      gtk_model_button_set_inverted (button, g_value_get_boolean (value));
      break;

    case PROP_CENTERED:
      gtk_model_button_set_centered (button, g_value_get_boolean (value));
      break;

    case PROP_ICONIC:
      gtk_model_button_set_iconic (button, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
has_sibling_with_indicator (GtkWidget *button)
{
  GtkWidget *parent;
  gboolean has_indicator;
  GList *children, *l;
  GtkModelButton *sibling;

  has_indicator = FALSE;

  parent = gtk_widget_get_parent (button);
  children = gtk_container_get_children (GTK_CONTAINER (parent));

  for (l = children; l; l = l->next)
    {
      sibling = l->data;

      if (!GTK_IS_MODEL_BUTTON (sibling))
        continue;

      if (!gtk_widget_is_visible (GTK_WIDGET (sibling)))
        continue;

      if (!sibling->centered &&
          (sibling->menu_name || sibling->role != GTK_BUTTON_ROLE_NORMAL))
        {
          has_indicator = TRUE;
          break;
        }
    }

  g_list_free (children);

  return has_indicator;
}

static gboolean
needs_indicator (GtkModelButton *button)
{
  if (button->role != GTK_BUTTON_ROLE_NORMAL)
    return TRUE;

  return has_sibling_with_indicator (GTK_WIDGET (button));
}

static void
gtk_model_button_get_preferred_width_for_height (GtkWidget *widget,
                                                 gint       height,
                                                 gint      *minimum,
                                                 gint      *natural)
{
  GtkCssGadget *gadget;

  if (GTK_MODEL_BUTTON (widget)->iconic)
    gadget = GTK_BUTTON (widget)->priv->gadget;
  else
    gadget = GTK_MODEL_BUTTON (widget)->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     height,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_model_button_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  GtkCssGadget *gadget;

  if (GTK_MODEL_BUTTON (widget)->iconic)
    gadget = GTK_BUTTON (widget)->priv->gadget;
  else
    gadget = GTK_MODEL_BUTTON (widget)->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_model_button_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
                                                              gint       width,
                                                              gint      *minimum,
                                                              gint      *natural,
                                                              gint      *minimum_baseline,
                                                              gint      *natural_baseline)
{
  GtkCssGadget *gadget;

  if (GTK_MODEL_BUTTON (widget)->iconic)
    gadget = GTK_BUTTON (widget)->priv->gadget;
  else
    gadget = GTK_MODEL_BUTTON (widget)->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

static void
gtk_model_button_get_preferred_height_for_width (GtkWidget *widget,
                                                 gint       width,
                                                 gint      *minimum,
                                                 gint      *natural)
{
  GtkCssGadget *gadget;

  if (GTK_MODEL_BUTTON (widget)->iconic)
    gadget = GTK_BUTTON (widget)->priv->gadget;
  else
    gadget = GTK_MODEL_BUTTON (widget)->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_model_button_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  GtkCssGadget *gadget;

  if (GTK_MODEL_BUTTON (widget)->iconic)
    gadget = GTK_BUTTON (widget)->priv->gadget;
  else
    gadget = GTK_MODEL_BUTTON (widget)->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_model_button_measure (GtkCssGadget   *gadget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline,
                          gpointer        data)
{
  GtkWidget *widget;
  GtkModelButton *button;
  GtkWidget *child;

  widget = gtk_css_gadget_get_owner (gadget);
  button = GTK_MODEL_BUTTON (widget);
  child = gtk_bin_get_child (GTK_BIN (widget));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint check_min, check_nat;

      gtk_css_gadget_get_preferred_size (button->indicator_gadget,
                                         GTK_ORIENTATION_HORIZONTAL,
                                         -1,
                                         &check_min, &check_nat,
                                         NULL, NULL);

      if (child && gtk_widget_get_visible (child))
        {
          _gtk_widget_get_preferred_size_for_size (child,
                                                   GTK_ORIENTATION_HORIZONTAL,
                                                   for_size,
                                                   minimum, natural,
                                                   minimum_baseline, natural_baseline);
        }
      else
        {
          *minimum = 0;
          *natural = 0;
        }

      if (button->centered)
        {
          *minimum += 2 * check_min;
          *natural += 2 * check_nat;
        }
      else if (needs_indicator (button))
        {
          *minimum += check_min;
          *natural += check_nat;
        }
    }
  else
    {
      gint check_min, check_nat;

      gtk_css_gadget_get_preferred_size (button->indicator_gadget,
                                         GTK_ORIENTATION_VERTICAL,
                                         -1,
                                         &check_min, &check_nat,
                                         NULL, NULL);

      if (child && gtk_widget_get_visible (child))
        {
          gint child_min, child_nat;
          gint child_min_baseline = -1, child_nat_baseline = -1;

          if (for_size > -1)
            {
              if (button->centered)
                for_size -= 2 * check_nat;
              else if (needs_indicator (button))
                for_size -= check_nat;
            }

          gtk_widget_get_preferred_height_and_baseline_for_width (child, for_size,
                                                                  &child_min, &child_nat,
                                                                  &child_min_baseline, &child_nat_baseline);

          if (button->centered)
            {
              *minimum = MAX (2 * check_min, child_min);
              *natural = MAX (2 * check_nat, child_nat);
            }
          else if (needs_indicator (button))
            {
              *minimum = MAX (check_min, child_min);
              *natural = MAX (check_nat, child_nat);
            }
          else
            {
              *minimum = child_min;
              *natural = child_nat;
            }

          if (minimum_baseline && child_min_baseline >= 0)
            *minimum_baseline = child_min_baseline + (*minimum - child_min) / 2;
          if (natural_baseline && child_nat_baseline >= 0)
            *natural_baseline = child_nat_baseline + (*natural - child_nat) / 2;
        }
      else
        {
          if (button->centered)
            {
              *minimum = 2 * check_min;
              *natural = 2 * check_nat;
            }
          else if (needs_indicator (button))
            {
              *minimum = check_min;
              *natural = check_nat;
            }
          else
            {
              *minimum = 0;
              *natural = 0;
            }
        }
    }
}

static void
gtk_model_button_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  GtkCssGadget *gadget;
  GdkRectangle clip;

  if (GTK_MODEL_BUTTON (widget)->iconic)
    gadget = GTK_BUTTON (widget)->priv->gadget;
  else
    gadget = GTK_MODEL_BUTTON (widget)->gadget;

  gtk_widget_set_allocation (widget, allocation);
  gtk_css_gadget_allocate (gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_model_button_allocate (GtkCssGadget        *gadget,
                           const GtkAllocation *allocation,
                           int                  baseline,
                           GtkAllocation       *out_clip,
                           gpointer             unused)
{
  GtkWidget *widget;
  GtkModelButton *button;
  PangoContext *pango_context;
  PangoFontMetrics *metrics;
  GtkAllocation child_allocation;
  GtkWidget *child;
  gint check_min_width, check_nat_width;
  gint check_min_height, check_nat_height;
  GdkRectangle check_clip;

  widget = gtk_css_gadget_get_owner (gadget);
  button = GTK_MODEL_BUTTON (widget);
  child = gtk_bin_get_child (GTK_BIN (widget));



  gtk_css_gadget_get_preferred_size (button->indicator_gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     &check_min_width, &check_nat_width,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (button->indicator_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &check_min_height, &check_nat_height,
                                     NULL, NULL);

  if (indicator_is_left (widget))
    child_allocation.x = allocation->x;
  else
    child_allocation.x = allocation->x + allocation->width - check_nat_width;
  child_allocation.y = allocation->y + (allocation->height - check_nat_height) / 2;
  child_allocation.width = check_nat_width;
  child_allocation.height = check_nat_height;

  gtk_css_gadget_allocate (button->indicator_gadget,
                           &child_allocation,
                           baseline,
                           &check_clip);

  if (child && gtk_widget_get_visible (child))
    {
      GtkBorder border = { 0, };

      if (button->centered)
        {
          border.left = check_nat_width;
          border.right = check_nat_width;
        }
      else if (needs_indicator (button))
        {
          if (indicator_is_left (widget))
            border.left += check_nat_width;
          else
            border.right += check_nat_width;
        }

      child_allocation.x = allocation->x + border.left;
      child_allocation.y = allocation->y + border.top;
      child_allocation.width = allocation->width - border.left - border.right;
      child_allocation.height = allocation->height - border.top - border.bottom;

      baseline = gtk_widget_get_allocated_baseline (widget);
      if (baseline != -1)
        baseline -= border.top;

      gtk_widget_size_allocate_with_baseline (child, &child_allocation, baseline);
    }

  pango_context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (pango_context,
                                       pango_context_get_font_description (pango_context),
                                       pango_context_get_language (pango_context));
  GTK_BUTTON (button)->priv->baseline_align =
    (double)pango_font_metrics_get_ascent (metrics) /
    (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));
  pango_font_metrics_unref (metrics);


  if (gtk_widget_get_realized (widget))
    {
      GtkAllocation border_allocation;
      gtk_css_gadget_get_border_allocation (gadget, &border_allocation, NULL);

      gdk_window_move_resize (gtk_button_get_event_window (GTK_BUTTON (widget)),
                              border_allocation.x,
                              border_allocation.y,
                              border_allocation.width,
                              border_allocation.height);
    }

  gtk_container_get_children_clip (GTK_CONTAINER (widget), out_clip);
  gdk_rectangle_union (out_clip, &check_clip, out_clip);
}

static gint
gtk_model_button_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  GtkCssGadget *gadget;

  if (GTK_MODEL_BUTTON (widget)->iconic)
    gadget = GTK_BUTTON (widget)->priv->gadget;
  else
    gadget = GTK_MODEL_BUTTON (widget)->gadget;

  gtk_css_gadget_draw (gadget, cr);

  return FALSE;
}

static gboolean
gtk_model_button_render (GtkCssGadget *gadget,
                         cairo_t      *cr,
                         int           x,
                         int           y,
                         int           width,
                         int           height,
                         gpointer      data)
{
  GtkWidget *widget;
  GtkModelButton *button;
  GtkWidget *child;

  widget = gtk_css_gadget_get_owner (gadget);
  button = GTK_MODEL_BUTTON (widget);

  if (gtk_css_node_get_visible (gtk_css_gadget_get_node (button->indicator_gadget)))
    gtk_css_gadget_draw (button->indicator_gadget, cr);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);

  return gtk_widget_has_visible_focus (widget);
}

static void
gtk_model_button_destroy (GtkWidget *widget)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (widget);

  g_clear_pointer (&model_button->menu_name, g_free);

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->destroy (widget);
}

static void
gtk_model_button_clicked (GtkButton *button)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (button);

  if (model_button->menu_name != NULL)
    {
      GtkWidget *stack;

      stack = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_STACK);
      if (stack != NULL)
        gtk_stack_set_visible_child_name (GTK_STACK (stack), model_button->menu_name);
    }
  else if (model_button->role == GTK_BUTTON_ROLE_NORMAL)
    {
      GtkWidget *popover;

      popover = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER);
      if (popover != NULL)
        gtk_popover_popdown (GTK_POPOVER (popover));
    }
}

static void
gtk_model_button_finalize (GObject *object)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  g_clear_object (&button->indicator_gadget);
  g_clear_object (&button->gadget);

  G_OBJECT_CLASS (gtk_model_button_parent_class)->finalize (object);
}

static AtkObject *
gtk_model_button_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  object = GTK_WIDGET_CLASS (gtk_model_button_parent_class)->get_accessible (widget);

  gtk_model_button_update_state (GTK_MODEL_BUTTON (widget), GTK_STATE_FLAG_NORMAL);

  return object;
}

static void
gtk_model_button_class_init (GtkModelButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);

  object_class->finalize = gtk_model_button_finalize;
  object_class->get_property = gtk_model_button_get_property;
  object_class->set_property = gtk_model_button_set_property;

  widget_class->get_preferred_width = gtk_model_button_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_model_button_get_preferred_width_for_height;
  widget_class->get_preferred_height = gtk_model_button_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_model_button_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_model_button_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_model_button_size_allocate;
  widget_class->draw = gtk_model_button_draw;
  widget_class->destroy = gtk_model_button_destroy;
  widget_class->state_flags_changed = gtk_model_button_state_flags_changed;
  widget_class->direction_changed = gtk_model_button_direction_changed;
  widget_class->get_accessible = gtk_model_button_get_accessible;

  button_class->clicked = gtk_model_button_clicked;

  /**
   * GtkModelButton:role:
   *
   * Specifies whether the button is a plain, check or radio button.
   * When #GtkActionable:action-name is set, the role will be determined
   * from the action and does not have to be set explicitly.
   *
   * Since: 3.16
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
   *
   * Since: 3.16
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
   *
   * Since: 3.16
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
   *
   * Since: 3.24
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
   *
   * Since: 3.16
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
   * The name of a submenu to open when the button is activated.
   * If this is set, the button should not have an action associated with it.
   *
   * Since: 3.16
   */
  properties[PROP_MENU_NAME] =
    g_param_spec_string ("menu-name",
                         P_("Menu name"),
                         P_("The name of the menu to open"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:inverted:
   *
   * Whether to show the submenu indicator at the opposite side than normal.
   * This property should be set for model buttons that 'go back' to a parent
   * menu.
   *
   * Since: 3.16
   */
  properties[PROP_INVERTED] =
    g_param_spec_boolean ("inverted",
                          P_("Inverted"),
                          P_("Whether the menu is a parent"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:centered:
   *
   * Whether to render the button contents centered instead of left-aligned.
   * This property should be set for title-like items.
   *
   * Since: 3.16
   */
  properties[PROP_CENTERED] =
    g_param_spec_boolean ("centered",
                          P_("Centered"),
                          P_("Whether to center the contents"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:iconic:
   *
   * If this property is set, the button will show an icon if one is set.
   * If no icon is set, the text will be used. This is typically used for
   * horizontal sections of linked buttons.
   *
   * Since: 3.16
   */
  properties[PROP_ICONIC] =
    g_param_spec_boolean ("iconic",
                          P_("Iconic"),
                          P_("Whether to prefer the icon over text"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);

  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (class), ATK_ROLE_PUSH_BUTTON);
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "modelbutton");
}

static void
gtk_model_button_init (GtkModelButton *button)
{
  GtkCssNode *widget_node;

  button->role = GTK_BUTTON_ROLE_NORMAL;
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  button->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (button->box, GTK_ALIGN_FILL);
  gtk_widget_show (button->box);
  button->image = gtk_image_new ();
  gtk_widget_set_no_show_all (button->image, TRUE);
  button->label = gtk_label_new ("");
  gtk_widget_set_no_show_all (button->label, TRUE);
  gtk_container_add (GTK_CONTAINER (button->box), button->image);
  gtk_container_add (GTK_CONTAINER (button->box), button->label);
  gtk_container_add (GTK_CONTAINER (button), button->box);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (button));
  button->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                       GTK_WIDGET (button),
                                                       gtk_model_button_measure,
                                                       gtk_model_button_allocate,
                                                       gtk_model_button_render,
                                                       NULL,
                                                       NULL);
  button->indicator_gadget = gtk_builtin_icon_new ("check",
                                                   GTK_WIDGET (button),
                                                   button->gadget,
                                                   NULL);
  gtk_builtin_icon_set_default_size (GTK_BUILTIN_ICON (button->indicator_gadget), 16);
  update_node_ordering (button);
  gtk_css_node_set_visible (gtk_css_gadget_get_node (button->indicator_gadget), FALSE);
}

/**
 * gtk_model_button_new:
 *
 * Creates a new GtkModelButton.
 *
 * Returns: the newly created #GtkModelButton widget
 *
 * Since: 3.16
 */
GtkWidget *
gtk_model_button_new (void)
{
  return g_object_new (GTK_TYPE_MODEL_BUTTON, NULL);
}
