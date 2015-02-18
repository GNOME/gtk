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
 */

struct _GtkModelButton
{
  GtkButton parent_instance;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
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
  PROP_ACTIVE,
  PROP_MENU_NAME,
  PROP_INVERTED,
  PROP_CENTERED,
  PROP_ICONIC,
  LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

static void
gtk_model_button_update_state (GtkModelButton *button)
{
  GtkStateFlags state;

  if (button->role == GTK_BUTTON_ROLE_NORMAL)
    return;

  state = gtk_widget_get_state_flags (GTK_WIDGET (button));

  state &= ~GTK_STATE_FLAG_CHECKED;

  if (button->active && !button->menu_name)
    state |= GTK_STATE_FLAG_CHECKED;

  gtk_widget_set_state_flags (GTK_WIDGET (button), state, TRUE);
}


static void
gtk_model_button_set_role (GtkModelButton *button,
                           GtkButtonRole   role)
{
  AtkObject *accessible;
  AtkRole a11y_role;

  if (role == button->role)
    return;

  button->role = role;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (button));
  switch (role)
    {
    case GTK_BUTTON_ROLE_NORMAL:
      a11y_role = ATK_ROLE_PUSH_BUTTON;
      break;

    case GTK_BUTTON_ROLE_CHECK:
      a11y_role = ATK_ROLE_CHECK_BOX;
      break;

    case GTK_BUTTON_ROLE_RADIO:
      a11y_role = ATK_ROLE_RADIO_BUTTON;
      break;

    default:
      g_assert_not_reached ();
    }

  atk_object_set_role (accessible, a11y_role);

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
gtk_model_button_set_active (GtkModelButton *button,
                             gboolean        active)
{
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
  gtk_model_button_update_state (button);
  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_MENU_NAME]);
}

static void
gtk_model_button_set_inverted (GtkModelButton *button,
                               gboolean        inverted)
{
  if (button->inverted == inverted)
    return;

  button->inverted = inverted;
  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_INVERTED]);
}

static void
gtk_model_button_set_centered (GtkModelButton *button,
                               gboolean        centered)
{
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
  GtkStyleContext *context;

  if (button->iconic == iconic)
    return;

  button->iconic = iconic;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  if (iconic)
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_MENUITEM);
      gtk_style_context_add_class (context, "image-button");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
    }
  else
    {
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUITEM);
      gtk_style_context_remove_class (context, "image-button");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
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
      g_assert_not_reached ();
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
      g_assert_not_reached ();
    }
}

static void
gtk_model_button_get_full_border (GtkModelButton *button,
                                  GtkBorder      *border,
                                  gint           *indicator)
{
  gint indicator_size, indicator_spacing;
  gint border_width;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (button));

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &indicator_size, NULL);
  indicator_spacing = indicator_size / 8;

  border->left = border_width;
  border->right = border_width;
  border->top = border_width;
  border->bottom = border_width;

  if (button->iconic)
    *indicator = 0;
  else
    *indicator = indicator_size + 2 * indicator_spacing;
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
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);
  GtkWidget *child;
  GtkBorder border;
  gint indicator;


  gtk_model_button_get_full_border (button, &border, &indicator);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      if (height > -1)
        height -= border.top + border.bottom;

      _gtk_widget_get_preferred_size_for_size (child,
                                               GTK_ORIENTATION_HORIZONTAL,
                                               height,
                                               minimum, natural,
                                               NULL, NULL);
    }
  else
    {
      *minimum = 0;
      *natural = 0;
    }

  *minimum += border.left + border.right;
  *natural += border.left + border.right;

  if (button->centered)
    {
      *minimum += 2 * indicator;
      *natural += 2 * indicator;
    }
  else if (needs_indicator (button))
    {
      *minimum += indicator;
      *natural += indicator;
    }
}

static void
gtk_model_button_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  gtk_model_button_get_preferred_width_for_height (widget, -1, minimum, natural);
}

static void
gtk_model_button_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
                                                              gint       width,
                                                              gint      *minimum,
                                                              gint      *natural,
                                                              gint      *minimum_baseline,
                                                              gint      *natural_baseline)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);
  GtkWidget *child;
  GtkBorder border;
  gint indicator;

  gtk_model_button_get_full_border (button, &border, &indicator);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;
      gint child_min_baseline = -1, child_nat_baseline = -1;

      if (width > -1)
        {
          width -= border.left + border.right;
          if (button->centered)
            width -= 2 * indicator;
          else if (needs_indicator (button))
            width -= indicator;
        }

      gtk_widget_get_preferred_height_and_baseline_for_width (child, width,
                                                              &child_min, &child_nat,
                                                              &child_min_baseline, &child_nat_baseline);

      if (button->centered)
        {
          *minimum = MAX (2 * indicator, child_min);
          *natural = MAX (2 * indicator, child_nat);
        }
      else if (needs_indicator (button))
        {
          *minimum = MAX (indicator, child_min);
          *natural = MAX (indicator, child_nat);
        }
      else
        {
          *minimum = child_min;
          *natural = child_nat;
        }

      if (minimum_baseline && child_min_baseline >= 0)
        *minimum_baseline = child_min_baseline + border.top + (*minimum - child_min) / 2;
      if (natural_baseline && child_nat_baseline >= 0)
        *natural_baseline = child_nat_baseline + border.top + (*natural - child_nat) / 2;
    }
  else
    {
      if (button->centered)
        {
          *minimum = 2 * indicator;
          *natural = 2 * indicator;
        }
      else if (needs_indicator (button))
        {
          *minimum = indicator;
          *natural = indicator;
        }
      else
        {
          *minimum = 0;
          *natural = 0;
        }
    }

  *minimum += border.top + border.bottom;
  *natural += border.top + border.bottom;
}

static void
gtk_model_button_get_preferred_height_for_width (GtkWidget *widget,
                                                 gint       width,
                                                 gint      *minimum,
                                                 gint      *natural)
{
  gtk_model_button_get_preferred_height_and_baseline_for_width (widget, width,
                                                                minimum, natural,
                                                                NULL, NULL);
}

static void
gtk_model_button_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  gtk_model_button_get_preferred_height_and_baseline_for_width (widget, -1,
                                                                minimum, natural,
                                                                NULL, NULL);
}

static gboolean
indicator_is_left (GtkWidget *widget)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);

  return ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL && !button->inverted) ||
          (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR && button->inverted));
   
}

static void
gtk_model_button_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);
  PangoContext *pango_context;
  PangoFontMetrics *metrics;
  GtkAllocation child_allocation;
  gint baseline;
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_button_get_event_window (GTK_BUTTON (widget)),
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      GtkBorder border;
      gint indicator;
          
      gtk_model_button_get_full_border (button, &border, &indicator);

      if (button->centered)
        {
          border.left += indicator;
          border.right += indicator;
        }
      else if (needs_indicator (button))
        {
          if (indicator_is_left (widget))
            border.left += indicator;
          else
            border.right += indicator;
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
}

static gint
gtk_model_button_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (widget);
  GtkButton *button = GTK_BUTTON (widget);
  GtkWidget *child;
  GtkStyleContext *context;
  gint border_width;
  gint x, y;
  gint width, height;
  gint indicator_size, indicator_spacing;
  gint baseline;

  if (model_button->iconic)
    return GTK_WIDGET_CLASS (gtk_model_button_parent_class)->draw (widget, cr);

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  baseline = gtk_widget_get_allocated_baseline (widget);

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &indicator_size, NULL);
  indicator_spacing = indicator_size / 8;

  x = width - border_width - indicator_spacing - indicator_size;

  if (indicator_is_left (widget))
    x = width - (indicator_size + x);

  if (baseline == -1)
    y = (height - indicator_size) / 2;
  else
    y = CLAMP (baseline - indicator_size * button->priv->baseline_align,
               0, height - indicator_size);

  gtk_render_background (context, cr,
                         border_width, border_width,
                         width - 2 * border_width,
                         height - 2 * border_width);
  gtk_render_frame (context, cr,
                    border_width, border_width,
                    width - 2 * border_width,
                    height - 2 * border_width);

  if (model_button->menu_name)
    {
      GtkStateFlags state;

      gtk_style_context_save (context);
      state = gtk_style_context_get_state (context);
      state = state & ~(GTK_STATE_FLAG_DIR_LTR|GTK_STATE_FLAG_DIR_RTL);
      if (indicator_is_left (widget))
        state = state | GTK_STATE_FLAG_DIR_RTL;
      else
        state = state | GTK_STATE_FLAG_DIR_LTR;

      gtk_style_context_set_state (context, state);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_EXPANDER);
      gtk_render_expander (context, cr, x, y, indicator_size, indicator_size);
      gtk_style_context_restore (context);
    }
  else if (model_button->role == GTK_BUTTON_ROLE_CHECK)
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_CHECK);
      gtk_render_check (context, cr, x, y, indicator_size, indicator_size);
      gtk_style_context_restore (context);
    }
  else if (model_button->role == GTK_BUTTON_ROLE_RADIO)
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RADIO);
      gtk_render_option (context, cr, x, y, indicator_size, indicator_size);
      gtk_style_context_restore (context);
    }

  if (gtk_widget_has_visible_focus (widget))
    {
      GtkBorder border;
 
      gtk_style_context_get_border (context, gtk_style_context_get_state (context), &border);

      gtk_render_focus (context, cr,
                        border_width + border.left,
                        border_width + border.top,
                        width - 2 * border_width - border.left - border.right,
                        height - 2 * border_width - border.top - border.bottom);
    }

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);

  return FALSE;
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
        gtk_widget_hide (popover);
    }
}

static void
gtk_model_button_class_init (GtkModelButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);

  object_class->get_property = gtk_model_button_get_property;
  object_class->set_property = gtk_model_button_set_property;

  widget_class->get_preferred_width = gtk_model_button_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_model_button_get_preferred_width_for_height;
  widget_class->get_preferred_height = gtk_model_button_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_model_button_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_model_button_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_model_button_size_allocate;
  widget_class->draw = gtk_model_button_draw;

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
   * Wether to render the button contents centered instead of left-aligned.
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
}

static void
gtk_model_button_init (GtkModelButton *button)
{
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  button->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start (button->box, 12);
  gtk_widget_set_margin_end (button->box, 12);
  gtk_widget_set_margin_top (button->box, 3);
  gtk_widget_set_margin_bottom (button->box, 3);
  gtk_widget_set_halign (button->box, GTK_ALIGN_FILL);
  gtk_widget_show (button->box);
  button->image = gtk_image_new ();
  gtk_widget_set_no_show_all (button->image, TRUE);
  g_object_set (button->image, "margin", 4, NULL);
  button->label = gtk_label_new ("");
  gtk_widget_set_no_show_all (button->label, TRUE);
  gtk_container_add (GTK_CONTAINER (button->box), button->image);
  gtk_container_add (GTK_CONTAINER (button->box), button->label);
  gtk_container_add (GTK_CONTAINER (button), button->box);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (button)),
                               GTK_STYLE_CLASS_MENUITEM);
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
