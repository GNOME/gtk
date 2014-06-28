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

struct _GtkModelButton
{
  GtkButton parent_instance;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  gboolean toggled;
  gboolean has_submenu;
  gboolean centered;
  gboolean inverted;
  gboolean iconic;
  GtkMenuTrackerItemRole role;
};

typedef GtkButtonClass GtkModelButtonClass;

G_DEFINE_TYPE (GtkModelButton, gtk_model_button, GTK_TYPE_BUTTON)

enum
{
  PROP_0,
  PROP_ACTION_ROLE,
  PROP_ICON,
  PROP_TEXT,
  PROP_TOGGLED,
  PROP_ACCEL,
  PROP_HAS_SUBMENU,
  PROP_INVERTED,
  PROP_CENTERED,
  PROP_ICONIC
};

static void
gtk_model_button_set_action_role (GtkModelButton         *button,
                                  GtkMenuTrackerItemRole  role)
{
  AtkObject *accessible;
  AtkRole a11y_role;

  if (role == button->role)
    return;

  button->role = role;
  gtk_widget_queue_draw (GTK_WIDGET (button));

  accessible = gtk_widget_get_accessible (GTK_WIDGET (button));
  switch (role)
    {
    case GTK_MENU_TRACKER_ITEM_ROLE_NORMAL:
      a11y_role = ATK_ROLE_PUSH_BUTTON;
      break;

    case GTK_MENU_TRACKER_ITEM_ROLE_CHECK:
      a11y_role = ATK_ROLE_CHECK_BOX;
      break;

    case GTK_MENU_TRACKER_ITEM_ROLE_RADIO:
      a11y_role = ATK_ROLE_RADIO_BUTTON;
      break;

    default:
      g_assert_not_reached ();
    }

  atk_object_set_role (accessible, a11y_role);
}

static void
update_visibility (GtkModelButton *button)
{
  gboolean has_icon;
  gboolean has_text;

  has_icon = gtk_image_get_storage_type (GTK_IMAGE (button->image)) != GTK_IMAGE_EMPTY;
  has_text = gtk_label_get_text (GTK_LABEL (button->label))[0] != '\0';

  gtk_widget_set_visible (button->image, has_icon);
  gtk_widget_set_visible (button->label, has_text && (!button->iconic || !has_icon));
}

static void
gtk_model_button_set_icon (GtkModelButton *button,
                           GIcon          *icon)
{
  gtk_image_set_from_gicon (GTK_IMAGE (button->image), icon, GTK_ICON_SIZE_MENU);
  update_visibility (button);
}

static void
gtk_model_button_set_text (GtkModelButton *button,
                           const gchar    *text)
{
  gtk_label_set_text_with_mnemonic (GTK_LABEL (button->label), text);
  update_visibility (button);
}

static void
gtk_model_button_set_accel (GtkModelButton *button,
                            const gchar    *accel)
{
  /* ignore */
}

static void
gtk_model_button_set_toggled (GtkModelButton *button,
                              gboolean        toggled)
{
  button->toggled = toggled;
  gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
gtk_model_button_set_has_submenu (GtkModelButton *button,
                                  gboolean        has_submenu)
{
  button->has_submenu = has_submenu;
  gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
gtk_model_button_set_inverted (GtkModelButton *button,
                               gboolean        inverted)
{
  button->inverted = inverted;
  gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
gtk_model_button_set_centered (GtkModelButton *button,
                               gboolean        centered)
{
  button->centered = centered;
  gtk_widget_set_halign (button->box, button->centered ? GTK_ALIGN_CENTER : GTK_ALIGN_FILL);
  gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
gtk_model_button_set_iconic (GtkModelButton *button,
                             gboolean        iconic)
{
  GtkStyleContext *context;

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
    case PROP_ACTION_ROLE:
      gtk_model_button_set_action_role (button, g_value_get_enum (value));
      break;

    case PROP_ICON:
      gtk_model_button_set_icon (button, g_value_get_object (value));
      break;

    case PROP_TEXT:
      gtk_model_button_set_text (button, g_value_get_string (value));
      break;

    case PROP_TOGGLED:
      gtk_model_button_set_toggled (button, g_value_get_boolean (value));
      break;

    case PROP_ACCEL:
      gtk_model_button_set_accel (button, g_value_get_string (value));
      break;

    case PROP_HAS_SUBMENU:
      gtk_model_button_set_has_submenu (button, g_value_get_boolean (value));
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
          (sibling->has_submenu || sibling->role != GTK_MENU_TRACKER_ITEM_ROLE_NORMAL))
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
  if (button->role != GTK_MENU_TRACKER_ITEM_ROLE_NORMAL)
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
gtk_model_button_get_preferred_height_and_baseline_for_width (GtkWidget          *widget,
                                                              gint                width,
                                                              gint               *minimum,
                                                              gint               *natural,
                                                              gint               *minimum_baseline,
                                                              gint               *natural_baseline)
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

static GtkStateFlags
get_button_state (GtkModelButton *model_button)
{
  GtkButton *button = GTK_BUTTON (model_button);
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (GTK_WIDGET (model_button));

  state &= ~(GTK_STATE_FLAG_INCONSISTENT |
             GTK_STATE_FLAG_ACTIVE |
             GTK_STATE_FLAG_SELECTED |
             GTK_STATE_FLAG_PRELIGHT);

  if (model_button->toggled ||
      (button->priv->button_down && button->priv->in_button))
    state |= GTK_STATE_FLAG_ACTIVE;

  if (button->priv->activate_timeout ||
      (button->priv->button_down && button->priv->in_button))
    state |= GTK_STATE_FLAG_SELECTED;

  if (button->priv->in_button)
    state |= GTK_STATE_FLAG_PRELIGHT;

  if (model_button->has_submenu)
    state &= ~GTK_STATE_FLAG_ACTIVE;

  return state;
}

static gint
gtk_model_button_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (widget);
  GtkButton *button = GTK_BUTTON (widget);
  GtkWidget *child;
  GtkStyleContext *context;
  GtkStateFlags state;
  gint border_width;
  gint x, y;
  gint width, height;
  gint indicator_size, indicator_spacing;
  gint baseline;

  state = get_button_state (model_button);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);

  if (model_button->iconic)
    {
      GTK_WIDGET_CLASS (gtk_model_button_parent_class)->draw (widget, cr);
      goto out;
    }

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

  if (model_button->has_submenu)
    {
      state = state & ~(GTK_STATE_FLAG_DIR_LTR|GTK_STATE_FLAG_DIR_RTL);
      if (indicator_is_left (widget))
        state = state | GTK_STATE_FLAG_DIR_RTL;
      else
        state = state | GTK_STATE_FLAG_DIR_LTR;

      gtk_style_context_set_state (context, state);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_EXPANDER);
      gtk_render_expander (context, cr, x, y, indicator_size, indicator_size);
    }
  else if (model_button->role == GTK_MENU_TRACKER_ITEM_ROLE_CHECK)
    {
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_CHECK);
      gtk_render_check (context, cr, x, y, indicator_size, indicator_size);
    }
  else if (model_button->role == GTK_MENU_TRACKER_ITEM_ROLE_RADIO)
    {
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RADIO);
      gtk_render_option (context, cr, x, y, indicator_size, indicator_size);
    }

  if (gtk_widget_has_visible_focus (widget))
    {
      GtkBorder border;
 
      gtk_style_context_get_border (context, state, &border);

      gtk_render_focus (context, cr,
                        border_width + border.left,
                        border_width + border.top,
                        width - 2 * border_width - border.left - border.right,
                        height - 2 * border_width - border.top - border.bottom);
    }

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);

out:
  gtk_style_context_restore (context);

  return FALSE;
}

static void
gtk_model_button_class_init (GtkModelButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_model_button_set_property;

  widget_class->get_preferred_width = gtk_model_button_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_model_button_get_preferred_width_for_height;
  widget_class->get_preferred_height = gtk_model_button_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_model_button_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_model_button_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_model_button_size_allocate;
  widget_class->draw = gtk_model_button_draw;

  g_object_class_install_property (object_class, PROP_ACTION_ROLE,
                                   g_param_spec_enum ("action-role", "", "",
                                                      GTK_TYPE_MENU_TRACKER_ITEM_ROLE,
                                                      GTK_MENU_TRACKER_ITEM_ROLE_NORMAL,
                                                      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ICON,
                                   g_param_spec_object ("icon", "", "", G_TYPE_ICON,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TEXT,
                                   g_param_spec_string ("text", "", "", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TOGGLED,
                                   g_param_spec_boolean ("toggled", "", "", FALSE,
                                                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACCEL,
                                   g_param_spec_string ("accel", "", "", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_HAS_SUBMENU,
                                   g_param_spec_boolean ("has-submenu", "", "", FALSE,
                                                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INVERTED,
                                   g_param_spec_boolean ("inverted", "", "", FALSE,

                                                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CENTERED,
                                   g_param_spec_boolean ("centered", "", "", FALSE,
                                                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ICONIC,
                                   g_param_spec_boolean ("iconic", "", "", TRUE,
                                                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (class), ATK_ROLE_PUSH_BUTTON);
}

static void
gtk_model_button_init (GtkModelButton *button)
{
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  button->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set (button->box,
                "margin-start", 12,
                "margin-end", 12,
                "margin-top", 3,
                "margin-bottom", 3,
                NULL);
  gtk_widget_set_halign (button->box, GTK_ALIGN_FILL);
  gtk_widget_show (button->box);
  button->image = gtk_image_new ();
  g_object_set (button->image, "margin", 4, NULL);
  button->label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (button->box), button->image);
  gtk_container_add (GTK_CONTAINER (button->box), button->label);
  gtk_container_add (GTK_CONTAINER (button), button->box);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (button)),
                               GTK_STYLE_CLASS_MENUITEM);
}

GtkWidget *
gtk_model_button_new (void)
{
  return g_object_new (GTK_TYPE_MODEL_BUTTON, NULL);
}
