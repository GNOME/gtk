/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "config.h"

#include "gtkcolorswatchprivate.h"

#include "gtkbox.h"
#include "gtkcolorchooserprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkdragsource.h"
#include "gtkdroptarget.h"
#include "gtkgesturelongpress.h"
#include "gtkgestureclick.h"
#include "gtkgesturesingle.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmodelbuttonprivate.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtknative.h"
#include "gtkdragsource.h"

#include "a11y/gtkcolorswatchaccessibleprivate.h"

/*
 * GtkColorSwatch has two CSS nodes, the main one named colorswatch
 * and a subnode named overlay. The main node gets the .light or .dark
 * style classes added depending on the brightness of the color that
 * the swatch is showing.
 *
 * The color swatch has the .activatable style class by default. It can
 * be removed for non-activatable swatches.
 */

typedef struct
{
  GdkRGBA color;
  gdouble radius[4];
  gchar *icon;
  guint    has_color        : 1;
  guint    use_alpha        : 1;
  guint    selectable       : 1;
  guint    has_menu         : 1;

  GtkWidget *overlay_widget;

  GtkWidget *popover;
  GtkDropTarget *dest;
} GtkColorSwatchPrivate;

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_SELECTABLE,
  PROP_HAS_MENU,
  PROP_CAN_DROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkColorSwatch, gtk_color_swatch, GTK_TYPE_WIDGET)

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)
static void
swatch_snapshot (GtkWidget   *widget,
                 GtkSnapshot *snapshot)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  const int width = gtk_widget_get_width (widget);
  const int height = gtk_widget_get_height (widget);
  const GdkRGBA *color;

  color = &priv->color;
  if (priv->dest)
    {
      const GValue *value = gtk_drop_target_get_value (priv->dest);

      if (value)
        color = g_value_get_boxed (value);
    }

  if (priv->has_color)
    {
      if (priv->use_alpha && !gdk_rgba_is_opaque (color))
        {
          _gtk_color_chooser_snapshot_checkered_pattern (snapshot, width, height);

          gtk_snapshot_append_color (snapshot,
                                     color,
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
        }
      else
        {
          GdkRGBA opaque = *color;

          opaque.alpha = 1.0;

          gtk_snapshot_append_color (snapshot,
                                     &opaque,
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
        }
    }

  gtk_widget_snapshot_child (widget, priv->overlay_widget, snapshot);
}

static gboolean
swatch_drag_drop (GtkDropTarget  *dest,
                  const GValue   *value,
                  double          x,
                  double          y,
                  GtkColorSwatch *swatch)

{
  gtk_color_swatch_set_rgba (swatch, g_value_get_boxed (value));

  return TRUE;
}

static void
activate_color (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  double red, green, blue, alpha;

  red = priv->color.red;
  green = priv->color.green;
  blue = priv->color.blue;
  alpha = priv->color.alpha;

  gtk_widget_activate_action (GTK_WIDGET (swatch),
                              "color.select", "(dddd)", red, green, blue, alpha);
}

static void
customize_color (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  double red, green, blue, alpha;

  red = priv->color.red;
  green = priv->color.green;
  blue = priv->color.blue;
  alpha = priv->color.alpha;

  gtk_widget_activate_action (GTK_WIDGET (swatch),
                              "color.customize", "(dddd)", red, green, blue, alpha);
}

static gboolean
key_controller_key_pressed (GtkEventControllerKey *controller,
                            guint                  keyval,
                            guint                  keycode,
                            GdkModifierType        state,
                            GtkWidget             *widget)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  if (keyval == GDK_KEY_space ||
      keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_ISO_Enter||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_KP_Space)
    {
      if (priv->has_color &&
          priv->selectable &&
          (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_SELECTED) == 0)
        gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_SELECTED, FALSE);
      else
        customize_color (swatch);

      return TRUE;
    }

  return FALSE;
}

static GMenuModel *
gtk_color_swatch_get_menu_model (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  GMenu *menu, *section;
  GMenuItem *item;
  double red, green, blue, alpha;

  menu = g_menu_new ();

  red = priv->color.red;
  green = priv->color.green;
  blue = priv->color.blue;
  alpha = priv->color.alpha;

  section = g_menu_new ();
  item = g_menu_item_new (_("Customize"), NULL);
  g_menu_item_set_action_and_target_value (item, "color.customize",
                                           g_variant_new ("(dddd)", red, green, blue, alpha));

  g_menu_append_item (section, item);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (item);
  g_object_unref (section);

  return G_MENU_MODEL (menu);
}

static void
do_popup (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  GMenuModel *model;

  g_clear_pointer (&priv->popover, gtk_widget_unparent);

  model = gtk_color_swatch_get_menu_model (swatch);
  priv->popover = gtk_popover_menu_new_from_model (model);
  gtk_widget_set_parent (priv->popover, GTK_WIDGET (swatch));
  g_object_unref (model);

  gtk_popover_popup (GTK_POPOVER (priv->popover));
}

static gboolean
swatch_primary_action (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  GtkWidget *widget = (GtkWidget *)swatch;
  GtkStateFlags flags;

  flags = gtk_widget_get_state_flags (widget);
  if (!priv->has_color)
    {
      customize_color (swatch);
      return TRUE;
    }
  else if (priv->selectable &&
           (flags & GTK_STATE_FLAG_SELECTED) == 0)
    {
      gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_SELECTED, FALSE);
      return TRUE;
    }

  return FALSE;
}

static void
hold_action (GtkGestureLongPress *gesture,
             gdouble              x,
             gdouble              y,
             GtkColorSwatch      *swatch)
{
  do_popup (swatch);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
tap_action (GtkGestureClick *gesture,
            gint             n_press,
            gdouble          x,
            gdouble          y,
            GtkColorSwatch  *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      if (n_press == 1)
        swatch_primary_action (swatch);
      else if (n_press > 1)
        activate_color (swatch);
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      if (priv->has_color && priv->has_menu)
        do_popup (swatch);
    }
}

static void
swatch_size_allocate (GtkWidget *widget,
                      int        width,
                      int        height,
                      int        baseline)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  gtk_widget_size_allocate (priv->overlay_widget,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, -1);

  if (priv->popover)
    gtk_native_check_resize (GTK_NATIVE (priv->popover));
}

static void
gtk_color_swatch_measure (GtkWidget *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  gint w, h, min;

  gtk_widget_measure (priv->overlay_widget,
                      orientation,
                      -1,
                      minimum, natural,
                      NULL, NULL);

  gtk_widget_get_size_request (widget, &w, &h);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    min = w < 0 ? 48 : w;
  else
    min = h < 0 ? 32 : h;

  *minimum = MAX (*minimum, min);
  *natural = MAX (*natural, min);
}



static gboolean
swatch_popup_menu (GtkWidget *widget)
{
  do_popup (GTK_COLOR_SWATCH (widget));
  return TRUE;
}

static void
update_icon (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  GtkImage *image = GTK_IMAGE (priv->overlay_widget);

  if (priv->icon)
    gtk_image_set_from_icon_name (image, priv->icon);
  else if (gtk_widget_get_state_flags (GTK_WIDGET (swatch)) & GTK_STATE_FLAG_SELECTED)
    gtk_image_set_from_icon_name (image, "object-select-symbolic");
  else
    gtk_image_clear (image);
}

static void
swatch_state_flags_changed (GtkWidget     *widget,
                            GtkStateFlags  previous_state)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  update_icon (swatch);

  GTK_WIDGET_CLASS (gtk_color_swatch_parent_class)->state_flags_changed (widget, previous_state);
}

/* GObject implementation {{{1 */

static void
swatch_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  GdkRGBA color;

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_swatch_get_rgba (swatch, &color);
      g_value_set_boxed (value, &color);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_color_swatch_get_selectable (swatch));
      break;
    case PROP_HAS_MENU:
      g_value_set_boolean (value, priv->has_menu);
      break;
    case PROP_CAN_DROP:
      g_value_set_boolean (value, priv->dest != NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
swatch_set_property (GObject      *object,
                     guint         prop_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_swatch_set_rgba (swatch, g_value_get_boxed (value));
      break;
    case PROP_SELECTABLE:
      gtk_color_swatch_set_selectable (swatch, g_value_get_boolean (value));
      break;
    case PROP_HAS_MENU:
      priv->has_menu = g_value_get_boolean (value);
      break;
    case PROP_CAN_DROP:
      gtk_color_swatch_set_can_drop (swatch, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
swatch_finalize (GObject *object)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  g_free (priv->icon);
  gtk_widget_unparent (priv->overlay_widget);
 
  G_OBJECT_CLASS (gtk_color_swatch_parent_class)->finalize (object);
}

static void
swatch_dispose (GObject *object)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  g_clear_pointer (&priv->popover, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_color_swatch_parent_class)->dispose (object);
}

static void
gtk_color_swatch_class_init (GtkColorSwatchClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)class;
  GObjectClass *object_class = (GObjectClass *)class;

  object_class->get_property = swatch_get_property;
  object_class->set_property = swatch_set_property;
  object_class->finalize = swatch_finalize;
  object_class->dispose = swatch_dispose;

  widget_class->measure = gtk_color_swatch_measure;
  widget_class->snapshot = swatch_snapshot;
  widget_class->popup_menu = swatch_popup_menu;
  widget_class->size_allocate = swatch_size_allocate;
  widget_class->state_flags_changed = swatch_state_flags_changed;

  g_object_class_install_property (object_class, PROP_RGBA,
      g_param_spec_boxed ("rgba", P_("RGBA Color"), P_("Color as RGBA"),
                          GDK_TYPE_RGBA, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SELECTABLE,
      g_param_spec_boolean ("selectable", P_("Selectable"), P_("Whether the swatch is selectable"),
                            TRUE, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_HAS_MENU,
      g_param_spec_boolean ("has-menu", P_("Has Menu"), P_("Whether the swatch should offer customization"),
                            TRUE, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_CAN_DROP,
      g_param_spec_boolean ("can-drop", P_("Can Drop"), P_("Whether the swatch should accept drops"),
                            FALSE, GTK_PARAM_READWRITE));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_COLOR_SWATCH_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("colorswatch"));
}

static void
gtk_color_swatch_init (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);
  GtkEventController *controller;
  GtkGesture *gesture;

  priv->use_alpha = TRUE;
  priv->selectable = TRUE;
  priv->has_menu = TRUE;
  priv->color.red = 0.75;
  priv->color.green = 0.25;
  priv->color.blue = 0.25;
  priv->color.alpha = 1.0;

  gtk_widget_set_can_focus (GTK_WIDGET (swatch), TRUE);
  gtk_widget_set_overflow (GTK_WIDGET (swatch), GTK_OVERFLOW_HIDDEN);

  gesture = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     TRUE);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (hold_action), swatch);
  gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (tap_action), swatch);
  gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (key_controller_key_pressed), swatch);
  gtk_widget_add_controller (GTK_WIDGET (swatch), controller);

  gtk_widget_add_css_class (GTK_WIDGET (swatch), "activatable");

  priv->overlay_widget = g_object_new (GTK_TYPE_IMAGE,
                                               "css-name", "overlay",
                                               NULL);
  gtk_widget_set_parent (priv->overlay_widget, GTK_WIDGET (swatch));
}

/* Public API {{{1 */

GtkWidget *
gtk_color_swatch_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_SWATCH, NULL);
}

static GdkContentProvider *
gtk_color_swatch_drag_prepare (GtkDragSource  *source,
                               double          x,
                               double          y,
                               GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  return gdk_content_provider_new_typed (GDK_TYPE_RGBA, &priv->color);
}

void
gtk_color_swatch_set_rgba (GtkColorSwatch *swatch,
                           const GdkRGBA  *color)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  if (!priv->has_color)
    {
      GtkDragSource *source;

      source = gtk_drag_source_new ();
      g_signal_connect (source, "prepare", G_CALLBACK (gtk_color_swatch_drag_prepare), swatch);

      gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (source));
    }

  priv->has_color = TRUE;
  priv->color = *color;

  if (INTENSITY (priv->color.red, priv->color.green, priv->color.blue) > 0.5)
    {
      gtk_widget_add_css_class (GTK_WIDGET (swatch), "light");
      gtk_widget_remove_css_class (GTK_WIDGET (swatch), "dark");
    }
  else
    {
      gtk_widget_add_css_class (GTK_WIDGET (swatch), "dark");
      gtk_widget_remove_css_class (GTK_WIDGET (swatch), "light");
    }

  gtk_widget_queue_draw (GTK_WIDGET (swatch));
  g_object_notify (G_OBJECT (swatch), "rgba");
}

gboolean
gtk_color_swatch_get_rgba (GtkColorSwatch *swatch,
                           GdkRGBA        *color)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  if (priv->has_color)
    {
      color->red = priv->color.red;
      color->green = priv->color.green;
      color->blue = priv->color.blue;
      color->alpha = priv->color.alpha;
      return TRUE;
    }
  else
    {
      color->red = 1.0;
      color->green = 1.0;
      color->blue = 1.0;
      color->alpha = 1.0;
      return FALSE;
    }
}

void
gtk_color_swatch_set_icon (GtkColorSwatch *swatch,
                           const gchar    *icon)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  priv->icon = g_strdup (icon);
  update_icon (swatch);
  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

void
gtk_color_swatch_set_can_drop (GtkColorSwatch *swatch,
                               gboolean        can_drop)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  if (can_drop == (priv->dest != NULL))
    return;

  if (can_drop && !priv->dest)
    {
      priv->dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
      gtk_drop_target_set_preload (priv->dest, TRUE);
      g_signal_connect (priv->dest, "drop", G_CALLBACK (swatch_drag_drop), swatch);
      g_signal_connect_swapped (priv->dest, "notify::value", G_CALLBACK (gtk_widget_queue_draw), swatch);
      gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (priv->dest));
    }
  if (!can_drop && priv->dest)
    {
      gtk_widget_remove_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (priv->dest));
      priv->dest = NULL;
    }

  g_object_notify (G_OBJECT (swatch), "can-drop");
}

void
gtk_color_swatch_set_use_alpha (GtkColorSwatch *swatch,
                                gboolean        use_alpha)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  priv->use_alpha = use_alpha;
  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

void
gtk_color_swatch_set_selectable (GtkColorSwatch *swatch,
                                 gboolean selectable)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  if (selectable == priv->selectable)
    return;

  priv->selectable = selectable;
  g_object_notify (G_OBJECT (swatch), "selectable");
}

gboolean
gtk_color_swatch_get_selectable (GtkColorSwatch *swatch)
{
  GtkColorSwatchPrivate *priv = gtk_color_swatch_get_instance_private (swatch);

  return priv->selectable;
}

/* vim:set foldmethod=marker: */
