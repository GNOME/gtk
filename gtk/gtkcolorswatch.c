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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkcolorswatch.h"

#include "gtkroundedboxprivate.h"
#include "gtkdnd.h"
#include "gtkicontheme.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkmenushell.h"
#include "gtkprivate.h"
#include "gtkintl.h"


struct _GtkColorSwatchPrivate
{
  GdkRGBA color;
  gdouble radius[4];
  gchar *icon;
  guint    selected         : 1;
  guint    has_color        : 1;
  guint    can_drop         : 1;
  guint    contains_pointer : 1;
  guint    use_alpha        : 1;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_SELECTED
};

enum
{
  ACTIVATE,
  CUSTOMIZE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkColorSwatch, gtk_color_swatch, GTK_TYPE_DRAWING_AREA)

static void
gtk_color_swatch_init (GtkColorSwatch *swatch)
{
  swatch->priv = G_TYPE_INSTANCE_GET_PRIVATE (swatch,
                                              GTK_TYPE_COLOR_SWATCH,
                                              GtkColorSwatchPrivate);

  gtk_widget_set_can_focus (GTK_WIDGET (swatch), TRUE);
  gtk_widget_set_events (GTK_WIDGET (swatch), GDK_BUTTON_PRESS_MASK
                                              | GDK_BUTTON_RELEASE_MASK
                                              | GDK_EXPOSURE_MASK
                                              | GDK_ENTER_NOTIFY_MASK
                                              | GDK_LEAVE_NOTIFY_MASK);
  swatch->priv->use_alpha = TRUE;
}

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

static cairo_pattern_t *
get_checkered_pattern (void)
{
  /* need to respect pixman's stride being a multiple of 4 */
  static unsigned char data[8] = { 0xFF, 0x00, 0x00, 0x00,
                                   0x00, 0xFF, 0x00, 0x00 };
  static cairo_surface_t *checkered = NULL;
  cairo_pattern_t *pattern;

  if (checkered == NULL)
    checkered = cairo_image_surface_create_for_data (data,
                                                     CAIRO_FORMAT_A8,
                                                     2, 2, 4);

  pattern = cairo_pattern_create_for_surface (checkered);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);

  return pattern;
}

static gboolean
swatch_draw (GtkWidget *widget,
             cairo_t   *cr)
{
  GtkColorSwatch *swatch = (GtkColorSwatch*)widget;
  GtkRoundedBox box;
  gint i;
  gdouble width, height;
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA bg;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_style_context_save (context);
  cairo_save (cr);

  gtk_style_context_set_state (context, state);

  _gtk_rounded_box_init_rect (&box, 0, 0, width, height);
  for (i = 0; i < 4; i++)
    box.corner[i].horizontal = box.corner[i].vertical = swatch->priv->radius[i];

  _gtk_rounded_box_path (&box, cr);

  cairo_clip_preserve (cr);

  if (swatch->priv->has_color)
    {
      cairo_pattern_t *pattern;
      cairo_matrix_t matrix;

      if (swatch->priv->use_alpha)
        {
          cairo_set_source_rgb (cr, 0.33, 0.33, 0.33);
          cairo_fill_preserve (cr);
          cairo_set_source_rgb (cr, 0.66, 0.66, 0.66);

          pattern = get_checkered_pattern ();
          cairo_matrix_init_scale (&matrix, 0.125, 0.125);
          cairo_pattern_set_matrix (pattern, &matrix);
          cairo_mask (cr, pattern);
          cairo_pattern_destroy (pattern);

          gdk_cairo_set_source_rgba (cr, &swatch->priv->color);
        }
      else
        {
          cairo_set_source_rgb (cr,
                                swatch->priv->color.red,
                                swatch->priv->color.green,
                                swatch->priv->color.blue);
        }
      cairo_fill_preserve (cr);
    }

  cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);

  if (gtk_widget_has_visible_focus (widget))
    {
      cairo_set_line_width (cr, 2);
      if (swatch->priv->has_color && INTENSITY (swatch->priv->color.red, swatch->priv->color.green, swatch->priv->color.blue) < 0.5)
        cairo_set_source_rgba (cr, 1., 1., 1., 0.4);
      else
        cairo_set_source_rgba (cr, 0., 0., 0., 0.4);
      _gtk_rounded_box_shrink (&box, 3, 3, 3, 3);
      _gtk_rounded_box_path (&box, cr);
      cairo_stroke (cr);
    }

  if (swatch->priv->icon)
    {
      GdkPixbuf *pixbuf;
      GtkIconTheme *theme;
      theme = gtk_icon_theme_get_default ();
      pixbuf = gtk_icon_theme_load_icon (theme, "list-add-symbolic", 16,
                                         GTK_ICON_LOOKUP_GENERIC_FALLBACK
                                         | GTK_ICON_LOOKUP_USE_BUILTIN,
                                         NULL);

      gtk_render_icon (context, cr, pixbuf,
                       (width - gdk_pixbuf_get_width (pixbuf)) / 2,
                       (height - gdk_pixbuf_get_height (pixbuf)) / 2);
      g_object_unref (pixbuf);
    }
  else  if (swatch->priv->selected)
    {
      gtk_style_context_get_background_color (context, state, &bg);
      cairo_new_sub_path (cr);
      cairo_arc (cr, width / 2, height / 2, 10, 0, 2 * G_PI);
      cairo_close_path (cr);
      gdk_cairo_set_source_rgba (cr, &bg);
      cairo_fill_preserve (cr);
      if (INTENSITY (swatch->priv->color.red, swatch->priv->color.green, swatch->priv->color.blue) > 0.5)
        cairo_set_source_rgba (cr, 0., 0., 0., 0.4);
      else
        cairo_set_source_rgba (cr, 1., 1., 1., 0.4);
      cairo_set_line_width (cr, 2);
      cairo_stroke (cr);
      gtk_style_context_set_state (context, state | GTK_STATE_FLAG_ACTIVE);
      gtk_render_check (context, cr, width / 2 - 5, height / 2 - 5, 10, 10);
    }

  cairo_restore (cr);
  gtk_style_context_restore (context);

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
  *min = *nat = 48;
}

static void
swatch_get_preferred_height (GtkWidget *widget,
                             gint      *min,
                             gint      *nat)
{
  *min = *nat = 32;
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
      if (swatch->priv->has_color && !swatch->priv->selected)
        gtk_color_swatch_set_selected (swatch, TRUE);
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
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  swatch->priv->contains_pointer = TRUE;
  return FALSE;
}

static gboolean
swatch_leave_notify (GtkWidget        *widget,
                     GdkEventCrossing *event)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  swatch->priv->contains_pointer = FALSE;
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
          GdkEventButton *event)
{
  GtkWidget *menu;
  GtkWidget *item;

  menu = gtk_menu_new ();
  item = gtk_menu_item_new_with_mnemonic (_("_Customize"));
  gtk_menu_attach_to_widget (GTK_MENU (menu), swatch, NULL);

  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (emit_customize), swatch);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_widget_show_all (item);

  if (event)
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    NULL, NULL, event->button, event->time);
  else
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    popup_position_func, swatch,
                    0, gtk_get_current_event_time ());
}

static gboolean
swatch_button_press (GtkWidget      *widget,
                     GdkEventButton *event)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  gtk_widget_grab_focus (widget);

  if (gdk_event_triggers_context_menu ((GdkEvent *) event) &&
      swatch->priv->has_color)
    {
      do_popup (widget, event);
      return TRUE;
    }
  else if (event->type == GDK_2BUTTON_PRESS &&
           event->button == GDK_BUTTON_PRIMARY)
    {
      g_signal_emit (swatch, signals[ACTIVATE], 0);
      return TRUE;
    }

  return FALSE;
}

static gboolean
swatch_button_release (GtkWidget      *widget,
                       GdkEventButton *event)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  if (event->button == GDK_BUTTON_PRIMARY &&
      swatch->priv->contains_pointer)
    {
      if (!swatch->priv->has_color)
        {
          g_signal_emit (swatch, signals[ACTIVATE], 0);
          return TRUE;
        }
      else if (!swatch->priv->selected)
        {
          gtk_color_swatch_set_selected (swatch, TRUE);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
swatch_popup_menu (GtkWidget *swatch)
{
  do_popup (swatch, NULL);
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
    case PROP_SELECTED:
      g_value_set_boolean (value, swatch->priv->selected);
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
    case PROP_SELECTED:
      gtk_color_swatch_set_selected (swatch, g_value_get_boolean (value));
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
  widget_class->button_press_event = swatch_button_press;
  widget_class->button_release_event = swatch_button_release;
  widget_class->enter_notify_event = swatch_enter_notify;
  widget_class->leave_notify_event = swatch_leave_notify;

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

  g_object_class_install_property (object_class, PROP_SELECTED,
      g_param_spec_boolean ("selected", P_("Selected"), P_("Selected"),
                            FALSE, GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkColorSwatchPrivate));
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
  if (!swatch->priv->has_color)
    gtk_drag_source_set (GTK_WIDGET (swatch),
                         GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                         dnd_targets, G_N_ELEMENTS (dnd_targets),
                         GDK_ACTION_COPY | GDK_ACTION_MOVE);

  swatch->priv->has_color = TRUE;
  swatch->priv->color = *color;

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
gtk_color_swatch_set_corner_radii (GtkColorSwatch *swatch,
                                   gdouble         top_left,
                                   gdouble         top_right,
                                   gdouble         bottom_right,
                                   gdouble         bottom_left)
{
  swatch->priv->radius[0] = top_left;
  swatch->priv->radius[1] = top_right;
  swatch->priv->radius[2] = bottom_right;
  swatch->priv->radius[3] = bottom_left;

  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

void
gtk_color_swatch_set_selected (GtkColorSwatch *swatch,
                               gboolean        selected)
{
  if (swatch->priv->selected != selected)
    {
      swatch->priv->selected = selected;
      gtk_widget_queue_draw (GTK_WIDGET (swatch));
      g_object_notify (G_OBJECT (swatch), "selected");
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
  if (!swatch->priv->can_drop)
    gtk_drag_dest_set (GTK_WIDGET (swatch),
                       GTK_DEST_DEFAULT_HIGHLIGHT |
                       GTK_DEST_DEFAULT_MOTION |
                       GTK_DEST_DEFAULT_DROP,
                       dnd_targets, G_N_ELEMENTS (dnd_targets),
                       GDK_ACTION_COPY);

  swatch->priv->can_drop = can_drop;
}

void
gtk_color_swatch_set_use_alpha (GtkColorSwatch *swatch,
                                gboolean        use_alpha)
{
  swatch->priv->use_alpha = use_alpha;
  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

/* vim:set foldmethod=marker: */
