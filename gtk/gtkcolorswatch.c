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

#include "gtkcolorchooserprivate.h"
#include "gtkroundedboxprivate.h"
#include "gtkthemingbackgroundprivate.h"
#include "gtkdnd.h"
#include "gtkicontheme.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkmenushell.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkwidgetprivate.h"
#include "a11y/gtkcolorswatchaccessibleprivate.h"


struct _GtkColorSwatchPrivate
{
  GdkRGBA color;
  gdouble radius[4];
  gchar *icon;
  guint    has_color        : 1;
  guint    use_alpha        : 1;
  guint    selectable       : 1;

  GdkWindow *event_window;

  GtkGesture *long_press_gesture;
  GtkGesture *multipress_gesture;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_SELECTABLE
};

enum
{
  ACTIVATE,
  CUSTOMIZE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void hold_action (GtkGestureLongPress  *gesture,
                         gdouble               x,
                         gdouble               y,
                         GtkColorSwatch       *swatch);
static void tap_action  (GtkGestureMultiPress *gesture,
                         gint                  n_press,
                         gdouble               x,
                         gdouble               y,
                         GtkColorSwatch       *swatch);

G_DEFINE_TYPE_WITH_PRIVATE (GtkColorSwatch, gtk_color_swatch, GTK_TYPE_WIDGET)

static void
gtk_color_swatch_init (GtkColorSwatch *swatch)
{
  swatch->priv = gtk_color_swatch_get_instance_private (swatch);
  swatch->priv->use_alpha = TRUE;
  swatch->priv->selectable = TRUE;

  gtk_widget_set_can_focus (GTK_WIDGET (swatch), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (swatch), FALSE);

  swatch->priv->long_press_gesture = gtk_gesture_long_press_new (GTK_WIDGET (swatch));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (swatch->priv->long_press_gesture),
                                     TRUE);
  g_signal_connect (swatch->priv->long_press_gesture, "pressed",
                    G_CALLBACK (hold_action), swatch);

  swatch->priv->multipress_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (swatch));
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (swatch->priv->multipress_gesture), 0);
  g_signal_connect (swatch->priv->multipress_gesture, "pressed",
                    G_CALLBACK (tap_action), swatch);
}

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)
#define PIXBUF_SIZE 16

static gboolean
swatch_draw (GtkWidget *widget,
             cairo_t   *cr)
{
  GtkColorSwatch *swatch = (GtkColorSwatch*)widget;
  GtkThemingBackground background;
  gdouble width, height;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkIconTheme *theme;
  GtkBorder border, padding;
  GdkRectangle rect;
  GtkIconInfo *icon_info = NULL;

  theme = gtk_icon_theme_get_default ();
  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  cairo_save (cr);

  gtk_style_context_save (context);

  _gtk_theming_background_init (&background, context,
                                0, 0, width, height,
                                GTK_JUNCTION_NONE);

  if (swatch->priv->has_color)
    {
      cairo_pattern_t *pattern;
      cairo_matrix_t matrix;

      if (swatch->priv->use_alpha)
        {
          cairo_save (cr);

          _gtk_rounded_box_path (&background.padding_box, cr);
          cairo_clip_preserve (cr);

          cairo_set_source_rgb (cr, 0.33, 0.33, 0.33);
          cairo_fill_preserve (cr);

          pattern = _gtk_color_chooser_get_checkered_pattern ();
          cairo_matrix_init_scale (&matrix, 0.125, 0.125);
          cairo_pattern_set_matrix (pattern, &matrix);

          cairo_set_source_rgb (cr, 0.66, 0.66, 0.66);
          cairo_mask (cr, pattern);
          cairo_pattern_destroy (pattern);

          cairo_restore (cr);

          background.bg_color = swatch->priv->color;
        }
      else
        {
          background.bg_color = swatch->priv->color;
          background.bg_color.alpha = 1.0;
        }

      _gtk_theming_background_render (&background, cr);
    }
  else
    _gtk_theming_background_render (&background, cr);

  gtk_render_frame (context, cr, 0, 0, width, height);

  if (swatch->priv->icon)
    {
      icon_info = gtk_icon_theme_lookup_icon (theme, swatch->priv->icon, PIXBUF_SIZE,
                                              GTK_ICON_LOOKUP_GENERIC_FALLBACK
                                              | GTK_ICON_LOOKUP_USE_BUILTIN);
    }
  else if ((state & GTK_STATE_FLAG_SELECTED) != 0)
    {
      GIcon *gicon;

      gicon = g_themed_icon_new ("object-select-symbolic");
      /* fallback for themes that don't have object-select-symbolic */
      g_themed_icon_append_name (G_THEMED_ICON (gicon), "gtk-apply");

      icon_info = gtk_icon_theme_lookup_by_gicon (theme, gicon, PIXBUF_SIZE,
                                                  GTK_ICON_LOOKUP_GENERIC_FALLBACK
                                                  | GTK_ICON_LOOKUP_USE_BUILTIN);
      g_object_unref (gicon);
    }

  gtk_style_context_restore (context);

  /* now draw the overlay image */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_IMAGE);
  
  gtk_style_context_get_border (context, state, &border);
  gtk_style_context_get_padding (context, state, &padding);
  rect.width = PIXBUF_SIZE + border.left + border.right + padding.left + padding.right;
  rect.height = PIXBUF_SIZE + border.top + border.bottom + padding.top + padding.bottom;
  rect.x = (width - rect.width) / 2;
  rect.y = (height - rect.height) / 2;

  gtk_render_background (context, cr, rect.x, rect.y, rect.width, rect.height);
  gtk_render_frame (context, cr, rect.x, rect.y, rect.width, rect.height);

  if (icon_info != NULL)
    {
      GdkPixbuf *pixbuf;

      pixbuf = gtk_icon_info_load_symbolic_for_context (icon_info, context,
                                                        NULL, NULL);

      if (pixbuf != NULL)
        {
          gtk_render_icon (context, cr, pixbuf,
                           rect.x + border.left + padding.left,
                           rect.y + border.top + padding.top);
          g_object_unref (pixbuf);
        }

      g_object_unref (icon_info);
    }

  cairo_restore (cr);
  gtk_style_context_restore (context);

  if (gtk_widget_has_visible_focus (widget))
    {
      gtk_render_focus (context, cr, 0, 0, width, height);
    }

  return FALSE;
}

static void
drag_set_color_icon (GdkDragContext *context,
                     const GdkRGBA  *color)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 48, 32);
  cr = cairo_create (surface);
  gdk_cairo_set_source_rgba (cr, color);
  cairo_paint (cr);

  cairo_surface_set_device_offset (surface, -4, -4);
  gtk_drag_set_icon_surface (context, surface);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

static void
swatch_drag_begin (GtkWidget      *widget,
                   GdkDragContext *context)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  GdkRGBA color;

  gtk_color_swatch_get_rgba (swatch, &color);
  drag_set_color_icon (context, &color);
}

static void
swatch_drag_data_get (GtkWidget        *widget,
                      GdkDragContext   *context,
                      GtkSelectionData *selection_data,
                      guint             info,
                      guint             time)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  guint16 vals[4];
  GdkRGBA color;

  gtk_color_swatch_get_rgba (swatch, &color);

  vals[0] = color.red * 0xffff;
  vals[1] = color.green * 0xffff;
  vals[2] = color.blue * 0xffff;
  vals[3] = color.alpha * 0xffff;

  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("application/x-color"),
                          16, (guchar *)vals, 8);
}

static void
swatch_drag_data_received (GtkWidget        *widget,
                           GdkDragContext   *context,
                           gint              x,
                           gint              y,
                           GtkSelectionData *selection_data,
                           guint             info,
                           guint             time)
{
  gint length;
  guint16 *vals;
  GdkRGBA color;

  length = gtk_selection_data_get_length (selection_data);

  if (length < 0)
    return;

  /* We accept drops with the wrong format, since the KDE color
   * chooser incorrectly drops application/x-color with format 8.
   */
  if (length != 8)
    {
      g_warning ("Received invalid color data\n");
      return;
    }

  vals = (guint16 *) gtk_selection_data_get_data (selection_data);

  color.red   = (gdouble)vals[0] / 0xffff;
  color.green = (gdouble)vals[1] / 0xffff;
  color.blue  = (gdouble)vals[2] / 0xffff;
  color.alpha = (gdouble)vals[3] / 0xffff;

  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (widget), &color);
}

static void
swatch_get_preferred_width (GtkWidget *widget,
                            gint      *min,
                            gint      *nat)
{
  gint w, h;

  gtk_widget_get_size_request (widget, &w, &h);

  if (w < 0)
    w = 48;

  *min = *nat = w;
}

static void
swatch_get_preferred_height (GtkWidget *widget,
                             gint      *min,
                             gint      *nat)
{
  gint w, h;

  gtk_widget_get_size_request (widget, &w, &h);

  if (h < 0)
    h = 32;

  *min = *nat = h;
}

static gboolean
swatch_key_press (GtkWidget   *widget,
                  GdkEventKey *event)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  if (event->keyval == GDK_KEY_space ||
      event->keyval == GDK_KEY_Return ||
      event->keyval == GDK_KEY_ISO_Enter||
      event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_KP_Space)
    {
      if (swatch->priv->has_color && 
          swatch->priv->selectable &&
          (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_SELECTED) == 0)
        gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_SELECTED, FALSE);
      else
        g_signal_emit (swatch, signals[ACTIVATE], 0);
      return TRUE;
    }

  if (GTK_WIDGET_CLASS (gtk_color_swatch_parent_class)->key_press_event (widget, event))
    return TRUE;

  return FALSE;
}

static gboolean
swatch_enter_notify (GtkWidget        *widget,
                     GdkEventCrossing *event)
{
  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);

  return FALSE;
}

static gboolean
swatch_leave_notify (GtkWidget        *widget,
                     GdkEventCrossing *event)
{
  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);

  return FALSE;
}

static void
emit_customize (GtkColorSwatch *swatch)
{
  g_signal_emit (swatch, signals[CUSTOMIZE], 0);
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer   user_data)
{
  GtkWidget *widget;
  GtkRequisition req;
  gint root_x, root_y;
  GdkScreen *screen;
  GdkWindow *window;
  GdkRectangle monitor;
  gint monitor_num;

  widget = GTK_WIDGET (user_data);
  g_return_if_fail (gtk_widget_get_realized (widget));
  window = gtk_widget_get_window (widget);

  screen = gtk_widget_get_screen (widget);
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  if (monitor_num < 0)
    monitor_num = 0;
  gtk_menu_set_monitor (menu, monitor_num);

  gdk_window_get_origin (window, &root_x, &root_y);
  gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);

  /* Put corner of menu centered on swatch */
  *x = root_x + gtk_widget_get_allocated_width (widget) / 2;
  *y = root_y + gtk_widget_get_allocated_height (widget) / 2;

  /* Ensure sanity */
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);
  *x = CLAMP (*x, monitor.x, MAX (monitor.x, monitor.width - req.width));
  *y = CLAMP (*y, monitor.y, MAX (monitor.y, monitor.height - req.height));
}

static void
do_popup (GtkWidget      *swatch,
          gint            button,
          gint            time)
{
  GtkWidget *menu;
  GtkWidget *item;

  menu = gtk_menu_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (menu),
                               GTK_STYLE_CLASS_CONTEXT_MENU);

  item = gtk_menu_item_new_with_mnemonic (_("_Customize"));
  gtk_menu_attach_to_widget (GTK_MENU (menu), swatch, NULL);

  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (emit_customize), swatch);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_widget_show_all (item);

  if (button != 0)
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    NULL, NULL, button, time);
  else
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    popup_position_func, swatch,
                    button, time);
}

static gboolean
swatch_primary_action (GtkColorSwatch *swatch)
{
  GtkWidget *widget = (GtkWidget *)swatch;
  GtkStateFlags flags;

  flags = gtk_widget_get_state_flags (widget);
  if (!swatch->priv->has_color)
    {
      g_signal_emit (swatch, signals[ACTIVATE], 0);
      return TRUE;
    }
  else if (swatch->priv->selectable &&
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
  emit_customize (swatch);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
tap_action (GtkGestureMultiPress *gesture,
            gint                  n_press,
            gdouble               x,
            gdouble               y,
            GtkColorSwatch       *swatch)
{
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      if (n_press == 1)
        swatch_primary_action (swatch);
      else if (n_press > 1)
        g_signal_emit (swatch, signals[ACTIVATE], 0);
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      if (swatch->priv->has_color)
        do_popup (GTK_WIDGET (swatch), button, gtk_get_current_event_time ());
    }
}

static void
swatch_map (GtkWidget *widget)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  GTK_WIDGET_CLASS (gtk_color_swatch_parent_class)->map (widget);

  if (swatch->priv->event_window)
    gdk_window_show (swatch->priv->event_window);
}

static void
swatch_unmap (GtkWidget *widget)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  if (swatch->priv->event_window)
    gdk_window_hide (swatch->priv->event_window);

  GTK_WIDGET_CLASS (gtk_color_swatch_parent_class)->unmap (widget);
}

static void
swatch_realize (GtkWidget *widget)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_ENTER_NOTIFY_MASK
                           | GDK_LEAVE_NOTIFY_MASK
                           | GDK_TOUCH_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  swatch->priv->event_window = 
    gdk_window_new (window,
                    &attributes, attributes_mask);
  gtk_widget_register_window (widget, swatch->priv->event_window);
}

static void
swatch_unrealize (GtkWidget *widget)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  if (swatch->priv->event_window)
    {
      gtk_widget_unregister_window (widget, swatch->priv->event_window);
      gdk_window_destroy (swatch->priv->event_window);
      swatch->priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_color_swatch_parent_class)->unrealize (widget);
}

static void
swatch_size_allocate (GtkWidget *widget,
                      GtkAllocation *allocation)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (swatch->priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  _gtk_widget_set_simple_clip (widget, NULL);
}

static gboolean
swatch_popup_menu (GtkWidget *swatch)
{
  do_popup (swatch, 0, gtk_get_current_event_time ());
  return TRUE;
}

/* GObject implementation {{{1 */

static void
swatch_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);
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

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_swatch_set_rgba (swatch, g_value_get_boxed (value));
      break;
    case PROP_SELECTABLE:
      gtk_color_swatch_set_selectable (swatch, g_value_get_boolean (value));
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

  g_free (swatch->priv->icon);

  g_object_unref (swatch->priv->long_press_gesture);
  g_object_unref (swatch->priv->multipress_gesture);

  G_OBJECT_CLASS (gtk_color_swatch_parent_class)->finalize (object);
}

static void
gtk_color_swatch_class_init (GtkColorSwatchClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)class;
  GObjectClass *object_class = (GObjectClass *)class;

  object_class->get_property = swatch_get_property;
  object_class->set_property = swatch_set_property;
  object_class->finalize = swatch_finalize;

  widget_class->get_preferred_width = swatch_get_preferred_width;
  widget_class->get_preferred_height = swatch_get_preferred_height;
  widget_class->draw = swatch_draw;
  widget_class->drag_begin = swatch_drag_begin;
  widget_class->drag_data_get = swatch_drag_data_get;
  widget_class->drag_data_received = swatch_drag_data_received;
  widget_class->key_press_event = swatch_key_press;
  widget_class->popup_menu = swatch_popup_menu;
  widget_class->enter_notify_event = swatch_enter_notify;
  widget_class->leave_notify_event = swatch_leave_notify;
  widget_class->realize = swatch_realize;
  widget_class->unrealize = swatch_unrealize;
  widget_class->map = swatch_map;
  widget_class->unmap = swatch_unmap;
  widget_class->size_allocate = swatch_size_allocate;

  signals[ACTIVATE] =
    g_signal_new ("activate",
                  GTK_TYPE_COLOR_SWATCH,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkColorSwatchClass, activate),
                  NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[CUSTOMIZE] =
    g_signal_new ("customize",
                  GTK_TYPE_COLOR_SWATCH,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkColorSwatchClass, customize),
                  NULL, NULL, NULL, G_TYPE_NONE, 0);

  g_object_class_install_property (object_class, PROP_RGBA,
      g_param_spec_boxed ("rgba", P_("RGBA Color"), P_("Color as RGBA"),
                          GDK_TYPE_RGBA, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SELECTABLE,
      g_param_spec_boolean ("selectable", P_("Selectable"), P_("Whether the swatch is selectable"),
                            TRUE, GTK_PARAM_READWRITE));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_COLOR_SWATCH_ACCESSIBLE);
}

/* Public API {{{1 */

GtkWidget *
gtk_color_swatch_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_SWATCH, NULL);
}

static const GtkTargetEntry dnd_targets[] = {
  { "application/x-color", 0 }
};

void
gtk_color_swatch_set_rgba (GtkColorSwatch *swatch,
                           const GdkRGBA  *color)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (swatch));

  if (!swatch->priv->has_color)
    {
      gtk_drag_source_set (GTK_WIDGET (swatch),
                           GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           dnd_targets, G_N_ELEMENTS (dnd_targets),
                           GDK_ACTION_COPY | GDK_ACTION_MOVE);
    }
  else
    {
      gtk_style_context_remove_class (context, "color-light");
      gtk_style_context_remove_class (context, "color-dark");
    }

  swatch->priv->has_color = TRUE;
  swatch->priv->color = *color;

  if (INTENSITY (swatch->priv->color.red, swatch->priv->color.green, swatch->priv->color.blue) > 0.5)
    gtk_style_context_add_class (context, "color-light");
  else
    gtk_style_context_add_class (context, "color-dark");

  gtk_widget_queue_draw (GTK_WIDGET (swatch));
  g_object_notify (G_OBJECT (swatch), "rgba");
}

gboolean
gtk_color_swatch_get_rgba (GtkColorSwatch *swatch,
                           GdkRGBA        *color)
{
  if (swatch->priv->has_color)
    {
      color->red = swatch->priv->color.red;
      color->green = swatch->priv->color.green;
      color->blue = swatch->priv->color.blue;
      color->alpha = swatch->priv->color.alpha;
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
  swatch->priv->icon = g_strdup (icon);
  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

void
gtk_color_swatch_set_can_drop (GtkColorSwatch *swatch,
                               gboolean        can_drop)
{
  if (can_drop)
    {
      gtk_drag_dest_set (GTK_WIDGET (swatch),
                         GTK_DEST_DEFAULT_HIGHLIGHT |
                         GTK_DEST_DEFAULT_MOTION |
                         GTK_DEST_DEFAULT_DROP,
                         dnd_targets, G_N_ELEMENTS (dnd_targets),
                         GDK_ACTION_COPY);
    }
  else
    {
      gtk_drag_dest_unset (GTK_WIDGET (swatch));
    }
}

void
gtk_color_swatch_set_use_alpha (GtkColorSwatch *swatch,
                                gboolean        use_alpha)
{
  swatch->priv->use_alpha = use_alpha;
  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

void
gtk_color_swatch_set_selectable (GtkColorSwatch *swatch,
                                 gboolean selectable)
{
  if (selectable == swatch->priv->selectable)
    return;

  swatch->priv->selectable = selectable;
  g_object_notify (G_OBJECT (swatch), "selectable");
}

gboolean
gtk_color_swatch_get_selectable (GtkColorSwatch *swatch)
{
  return swatch->priv->selectable;
}

/* vim:set foldmethod=marker: */
