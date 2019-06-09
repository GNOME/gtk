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
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtkpopover.h"
#include "gtkintl.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkiconprivate.h"
#include "gtksizegroup.h"
#include "gtkaccellabelprivate.h"
#include "gtkactionable.h"

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
  GtkWidget *accel_label;
  GtkWidget *start_box;
  GtkWidget *start_indicator;
  GtkWidget *end_box;
  GtkWidget *end_indicator;
  gboolean active;
  gboolean centered;
  gboolean iconic;
  gchar *menu_name;
  GtkButtonRole role;
  GtkSizeGroup *indicators;
  char *accel;
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
  PROP_ICONIC,
  PROP_ACCEL,
  PROP_INDICATOR_SIZE_GROUP,
  LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

static void
update_node_ordering (GtkModelButton *button)
{
  GtkStyleContext *start_indicator_context;
  GtkStyleContext *end_indicator_context;
  GtkWidget *child;

  start_indicator_context = gtk_widget_get_style_context (button->start_indicator);
  end_indicator_context = gtk_widget_get_style_context (button->end_indicator);

  if (gtk_widget_get_direction (GTK_WIDGET (button)) == GTK_TEXT_DIR_LTR)
    {
      gtk_style_context_add_class (start_indicator_context, GTK_STYLE_CLASS_LEFT);
      gtk_style_context_remove_class (start_indicator_context, GTK_STYLE_CLASS_RIGHT);
      gtk_style_context_add_class (end_indicator_context, GTK_STYLE_CLASS_RIGHT);
      gtk_style_context_remove_class (end_indicator_context, GTK_STYLE_CLASS_LEFT);

      child = gtk_widget_get_first_child (GTK_WIDGET (button));
      if (child != button->start_box)
        gtk_widget_insert_before (button->start_box, GTK_WIDGET (button), child);
      child = gtk_widget_get_last_child (GTK_WIDGET (button));
      if (child != button->end_box)
        gtk_widget_insert_after (button->end_box, GTK_WIDGET (button), child);
    }
  else
    {
      gtk_style_context_add_class (start_indicator_context, GTK_STYLE_CLASS_RIGHT);
      gtk_style_context_remove_class (start_indicator_context, GTK_STYLE_CLASS_LEFT);
      gtk_style_context_add_class (end_indicator_context, GTK_STYLE_CLASS_LEFT);
      gtk_style_context_remove_class (end_indicator_context, GTK_STYLE_CLASS_RIGHT);

      child = gtk_widget_get_first_child (GTK_WIDGET (button));
      if (child != button->end_box)
        gtk_widget_insert_before (button->end_box, GTK_WIDGET (button), child);
      child = gtk_widget_get_last_child (GTK_WIDGET (button));
      if (child != button->start_box)
        gtk_widget_insert_after (button->start_box, GTK_WIDGET (button), child);
    }
}

static void
gtk_model_button_update_state (GtkModelButton *button)
{
  GtkStateFlags state;
  GtkStateFlags indicator_state;
  GtkCssImageBuiltinType start_type;
  GtkCssImageBuiltinType end_type;
  gboolean centered;

  state = gtk_widget_get_state_flags (GTK_WIDGET (button));
  indicator_state = state;
  start_type = GTK_CSS_IMAGE_BUILTIN_NONE;
  end_type = GTK_CSS_IMAGE_BUILTIN_NONE;
  centered = FALSE;

  switch (button->role)
    {
    case GTK_BUTTON_ROLE_CHECK:
      start_type = GTK_CSS_IMAGE_BUILTIN_CHECK;
      end_type = GTK_CSS_IMAGE_BUILTIN_NONE;
      if (button->active)
        indicator_state |= GTK_STATE_FLAG_CHECKED;
      else
        indicator_state &= ~GTK_STATE_FLAG_CHECKED;
      break;

    case GTK_BUTTON_ROLE_RADIO:
      start_type = GTK_CSS_IMAGE_BUILTIN_OPTION;
      end_type = GTK_CSS_IMAGE_BUILTIN_NONE;
      if (button->active)
        indicator_state |= GTK_STATE_FLAG_CHECKED;
      else
        indicator_state &= ~GTK_STATE_FLAG_CHECKED;
      break;

    case GTK_BUTTON_ROLE_TITLE:
      centered = TRUE;
      start_type = GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT;
      end_type = GTK_CSS_IMAGE_BUILTIN_NONE;
      break;

    case GTK_BUTTON_ROLE_NORMAL:
      start_type = GTK_CSS_IMAGE_BUILTIN_NONE;
      if (button->menu_name != NULL)
        end_type = GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT;
      else
        end_type = GTK_CSS_IMAGE_BUILTIN_NONE;
      centered = button->iconic;
      break;

    default:
      g_assert_not_reached ();
    }

  if (button->centered != centered)
    {
      button->centered = centered;
      gtk_widget_set_halign (button->box, button->centered ? GTK_ALIGN_CENTER : GTK_ALIGN_FILL);
      gtk_widget_queue_resize (GTK_WIDGET (button));
    }

  gtk_icon_set_image (GTK_ICON (button->start_indicator), start_type);
  gtk_icon_set_image (GTK_ICON (button->end_indicator), end_type);

  if (button->iconic)
    gtk_widget_set_state_flags (GTK_WIDGET (button), indicator_state, TRUE);
  else
    gtk_widget_set_state_flags (GTK_WIDGET (button), state, TRUE);

  gtk_widget_set_state_flags (button->start_indicator, indicator_state, TRUE);
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
update_node_name (GtkModelButton *button)
{
  AtkObject *accessible;
  AtkRole a11y_role;
  const gchar *start_name;
  const gchar *end_name;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (button));
  switch (button->role)
    {
    case GTK_BUTTON_ROLE_TITLE:
      a11y_role = ATK_ROLE_PUSH_BUTTON;
      start_name = I_("arrow");
      end_name = I_("none");
      break;
    case GTK_BUTTON_ROLE_NORMAL:
      a11y_role = ATK_ROLE_PUSH_BUTTON;
      start_name = I_("none");
      if (button->menu_name)
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

  if (button->iconic)
    {
      start_name = I_("none");
      end_name = I_("none");
    }

  atk_object_set_role (accessible, a11y_role);

  gtk_icon_set_css_name (GTK_ICON (button->start_indicator), start_name);
  gtk_icon_set_css_name (GTK_ICON (button->end_indicator), end_name);
  gtk_widget_set_visible (button->start_indicator, start_name != I_("none"));
  gtk_widget_set_visible (button->end_indicator, end_name != I_("none"));
}

static void
gtk_model_button_set_role (GtkModelButton *button,
                           GtkButtonRole   role)
{
  if (role == button->role)
    return;

  button->role = role;

  if (role == GTK_BUTTON_ROLE_TITLE)
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (button)), "title");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (button)), "title");

  update_node_name (button);
  gtk_model_button_update_state (button);

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
  gtk_widget_set_hexpand (button->image, has_icon && !has_text);
  gtk_widget_set_hexpand (button->box, FALSE);
}

static void
gtk_model_button_set_icon (GtkModelButton *button,
                           GIcon          *icon)
{
  gtk_image_set_from_gicon (GTK_IMAGE (button->image), icon);
  update_visibility (button);
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ICON]);
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
gtk_model_button_set_iconic (GtkModelButton *button,
                             gboolean        iconic)
{
  GtkCssNode *widget_node;
  GtkStyleContext *context;

  iconic = !!iconic;
  if (button->iconic == iconic)
    return;

  button->iconic = iconic;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (button));
  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  if (iconic)
    {
      gtk_css_node_set_name (widget_node, I_("button"));
      gtk_style_context_add_class (context, "model");
      gtk_style_context_add_class (context, "image-button");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
    }
  else
    {
      gtk_css_node_set_name (widget_node, I_("modelbutton"));
      gtk_style_context_remove_class (context, "model");
      gtk_style_context_remove_class (context, "image-button");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    }

  button->centered = iconic;

  gtk_widget_set_visible (button->start_box, !iconic);
  gtk_widget_set_visible (button->end_box, !iconic);

  gtk_widget_set_halign (button->box, button->centered ? GTK_ALIGN_CENTER : GTK_ALIGN_FILL);

  update_node_name (button);
  update_visibility (button);
  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ICONIC]);
}

static void
update_accel (GtkModelButton *button,
              const char     *accel)
{
  if (accel)
    {
      guint key;
      GdkModifierType mods;
      GtkAccelLabelClass *accel_class;
      char *str;

      gtk_accelerator_parse (accel, &key, &mods);

      accel_class = g_type_class_ref (GTK_TYPE_ACCEL_LABEL);
      str = _gtk_accel_label_class_get_accelerator_label (accel_class, key, mods);
      gtk_label_set_label (GTK_LABEL (button->accel_label), str);
      g_free (str);
      g_type_class_unref (accel_class);

      gtk_widget_show (button->accel_label);
    }
  else
    {
      gtk_widget_hide (button->accel_label);
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
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ROLE:
      g_value_set_enum (value, button->role);
      break;

    case PROP_ICON:
      g_value_set_object (value, gtk_image_get_gicon (GTK_IMAGE (button->image)));
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

    case PROP_ICONIC:
      g_value_set_boolean (value, button->iconic);
      break;

    case PROP_ACCEL:
      g_value_set_string (value, button->accel);
      break;

    case PROP_INDICATOR_SIZE_GROUP:
      g_value_set_object (value, button->indicators);
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_model_button_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  if (GTK_MODEL_BUTTON (widget)->iconic)
    {
      GTK_WIDGET_CLASS (gtk_model_button_parent_class)->measure (widget, orientation, for_size,
                                                                 minimum, natural,
                                                                 minimum_baseline, natural_baseline);
    }
  else
    {
      GtkModelButton *button;
      GtkWidget *child;

      button = GTK_MODEL_BUTTON (widget);
      child = gtk_bin_get_child (GTK_BIN (widget));

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          int start_min, start_nat;
          int end_min, end_nat;

          gtk_widget_measure (button->start_box,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &start_min, &start_nat,
                              NULL, NULL);

          gtk_widget_measure (button->end_box,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &end_min, &end_nat,
                              NULL, NULL);

          if (child)
            {
              gtk_widget_measure (child,
                                  orientation,
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
              *minimum += 2 * MAX (start_min, end_min);
              *natural += 2 * MAX (start_nat, end_nat);
            }
          else
            {
              *minimum += start_min + end_min;
              *natural += start_nat + end_nat;
            }
        }
      else
        {
          int start_min, start_nat;
          int end_min, end_nat;

          gtk_widget_measure (button->start_box,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &start_min, &start_nat,
                              NULL, NULL);

          gtk_widget_measure (button->end_box,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &end_min, &end_nat,
                              NULL, NULL);

          if (child)
            {
              gint child_min, child_nat;
              gint child_min_baseline = -1, child_nat_baseline = -1;

              if (for_size > -1)
                for_size -= start_nat + end_nat;

              gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL,
                                  for_size,
                                  &child_min, &child_nat,
                                  &child_min_baseline, &child_nat_baseline);

              *minimum = MAX (child_min, MAX (start_min, end_min));
              *natural = MAX (child_nat, MAX (start_nat, end_nat));

              if (minimum_baseline && child_min_baseline >= 0)
                *minimum_baseline = child_min_baseline + (*minimum - child_min) / 2;
              if (natural_baseline && child_nat_baseline >= 0)
                *natural_baseline = child_nat_baseline + (*natural - child_nat) / 2;
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
gtk_model_button_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  if (GTK_MODEL_BUTTON (widget)->iconic)
    {
      GTK_WIDGET_CLASS (gtk_model_button_parent_class)->size_allocate (widget,
                                                                       width,
                                                                       height,
                                                                       baseline);
    }
  else
    {
      GtkModelButton *button;
      GtkAllocation child_allocation;
      GtkWidget *child;
      int start_width, start_height;
      int end_width, end_height;
      int min;

      button = GTK_MODEL_BUTTON (widget);
      child = gtk_bin_get_child (GTK_BIN (widget));

      gtk_widget_measure (button->start_box,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          &min, &start_width,
                          NULL, NULL);
      gtk_widget_measure (button->start_box,
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &min, &start_height,
                          NULL, NULL);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        child_allocation.x = width - start_width;
      else
        child_allocation.x = 0;
      child_allocation.y = (height - start_height) / 2;
      child_allocation.width = start_width;
      child_allocation.height = start_height;

      gtk_widget_size_allocate (button->start_box, &child_allocation, baseline);

      gtk_widget_measure (button->end_box,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          &min, &end_width,
                          NULL, NULL);
      gtk_widget_measure (button->end_box,
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &min, &end_height,
                          NULL, NULL);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        child_allocation.x = 0;
      else
        child_allocation.x = width - end_width;
      child_allocation.y = (height - end_height) / 2;
      child_allocation.width = end_width;
      child_allocation.height = end_height;

      gtk_widget_size_allocate (button->end_box, &child_allocation, baseline);

      if (button->centered)
        end_width = start_width = MAX (start_width, end_width);

      if (child && gtk_widget_get_visible (child))
        {
          if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
            child_allocation.x = end_width;
          else
            child_allocation.x = start_width;
          child_allocation.y = 0;
          child_allocation.width = width - start_width - end_width;
          child_allocation.height = height;

          gtk_widget_size_allocate (child, &child_allocation, baseline);
        }
    }
}

static void
gtk_model_button_destroy (GtkWidget *widget)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (widget);

  g_clear_pointer (&model_button->menu_name, g_free);

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->destroy (widget);
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
close_menu (GtkModelButton *button)
{
  GtkWidget *popover;

  popover = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER);
  if (popover != NULL)
    gtk_popover_popdown (GTK_POPOVER (popover));
}

static void
gtk_model_button_clicked (GtkButton *button)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (button);

  if (model_button->menu_name != NULL)
    {
      switch_menu (model_button);
    }
  else if (model_button->role == GTK_BUTTON_ROLE_NORMAL)
    {
      close_menu (model_button);
    }
}

static void
gtk_model_button_finalize (GObject *object)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  gtk_widget_unparent (button->start_box);
  gtk_widget_unparent (button->end_box);
  g_free (button->accel);

  G_OBJECT_CLASS (gtk_model_button_parent_class)->finalize (object);
}

static void
gtk_model_button_map (GtkWidget *widget)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);
  GtkWindow *window;
  GtkApplication *app;
  const char *action_name;
  GVariant *action_target;

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->map (widget);

  if (button->accel)
    return;

  window = GTK_WINDOW (gtk_widget_get_root (widget));
  app = gtk_window_get_application (window);

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

      update_accel (button, accels[0]);

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
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);

  object_class->finalize = gtk_model_button_finalize;
  object_class->get_property = gtk_model_button_get_property;
  object_class->set_property = gtk_model_button_set_property;

  widget_class->measure = gtk_model_button_measure;
  widget_class->size_allocate = gtk_model_button_size_allocate;
  widget_class->destroy = gtk_model_button_destroy;
  widget_class->map = gtk_model_button_map;
  widget_class->state_flags_changed = gtk_model_button_state_flags_changed;
  widget_class->direction_changed = gtk_model_button_direction_changed;
  widget_class->focus = gtk_model_button_focus;

  button_class->clicked = gtk_model_button_clicked;

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
   * The name of a submenu to open when the button is activated.
   * If this is set, the button should not have an action associated with it.
   */
  properties[PROP_MENU_NAME] =
    g_param_spec_string ("menu-name",
                         P_("Menu name"),
                         P_("The name of the menu to open"),
                         NULL,
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
  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);

  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (class), ATK_ROLE_PUSH_BUTTON);
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), I_("modelbutton"));
}

static void
gtk_model_button_init (GtkModelButton *button)
{
  button->role = GTK_BUTTON_ROLE_NORMAL;
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  button->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (button->box, GTK_ALIGN_FILL);
  button->image = gtk_image_new ();
  gtk_widget_hide (button->image);
  button->label = gtk_label_new ("");
  gtk_widget_hide (button->label);
  button->accel_label = g_object_new (GTK_TYPE_LABEL,
                                      "css-name", "accelerator",
                                      NULL);
  gtk_widget_set_hexpand (button->accel_label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (button->accel_label), 0.0f);
  gtk_widget_set_halign (button->accel_label, GTK_ALIGN_END);
  gtk_widget_hide (button->accel_label);
  gtk_container_add (GTK_CONTAINER (button->box), button->image);
  gtk_container_add (GTK_CONTAINER (button->box), button->label);
  gtk_container_add (GTK_CONTAINER (button->box), button->accel_label);
  gtk_container_add (GTK_CONTAINER (button), button->box);

  button->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button->start_indicator = gtk_icon_new ("none");
  button->end_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button->end_indicator = gtk_icon_new ("none");
  gtk_container_add (GTK_CONTAINER (button->start_box), button->start_indicator);
  gtk_container_add (GTK_CONTAINER (button->end_box), button->end_indicator);
  gtk_widget_set_parent (button->start_box, GTK_WIDGET (button));
  gtk_widget_set_parent (button->end_box, GTK_WIDGET (button));
  gtk_widget_hide (button->start_indicator);
  gtk_widget_hide (button->end_indicator);
  update_node_ordering (button);
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
