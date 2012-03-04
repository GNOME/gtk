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

#include "gtkcolorplaneprivate.h"

#include "gtkaccessible.h"
#include "gtkadjustment.h"
#include "gtkcolorutils.h"
#include "gtkpressandholdprivate.h"
#include "gtkintl.h"

struct _GtkColorPlanePrivate
{
  GtkAdjustment *h_adj;
  GtkAdjustment *s_adj;
  GtkAdjustment *v_adj;

  cairo_surface_t *surface;
  gboolean in_drag;

  GtkPressAndHold *press_and_hold;
};

G_DEFINE_TYPE (GtkColorPlane, gtk_color_plane, GTK_TYPE_DRAWING_AREA)

static void
sv_to_xy (GtkColorPlane *plane,
          gint          *x,
          gint          *y)
{
  gdouble s, v;
  gint width, height;

  width = gtk_widget_get_allocated_width (GTK_WIDGET (plane));
  height = gtk_widget_get_allocated_height (GTK_WIDGET (plane));

  s = gtk_adjustment_get_value (plane->priv->s_adj);
  v = gtk_adjustment_get_value (plane->priv->v_adj);

  *x = CLAMP (width * v, 0, width - 1);
  *y = CLAMP (height * (1 - s), 0, height - 1);
}

static gboolean
plane_draw (GtkWidget *widget,
            cairo_t   *cr)
{
  GtkColorPlane *plane = GTK_COLOR_PLANE (widget);
  gint x, y;
  gint width, height;

  cairo_set_source_surface (cr, plane->priv->surface, 0, 0);
  cairo_paint (cr);

  sv_to_xy (plane, &x, &y);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  cairo_move_to (cr, 0,     y + 0.5);
  cairo_line_to (cr, width, y + 0.5);

  cairo_move_to (cr, x + 0.5, 0);
  cairo_line_to (cr, x + 0.5, height);

  if (gtk_widget_has_visible_focus (widget))
    {
      cairo_set_line_width (cr, 3.0);
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.6);
      cairo_stroke_preserve (cr);

      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.8);
      cairo_stroke (cr);
    }
  else
    {
      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 0.8);
      cairo_stroke (cr);
    }

  return FALSE;
}

static void
create_surface (GtkColorPlane *plane)
{
  GtkWidget *widget = GTK_WIDGET (plane);
  cairo_t *cr;
  cairo_surface_t *surface;
  gint width, height, stride;
  cairo_surface_t *tmp;
  guint red, green, blue;
  guint32 *data, *p;
  gdouble h, s, v;
  gdouble r, g, b;
  gdouble sf, vf;
  gint x, y;

  if (!gtk_widget_get_realized (widget))
    return;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               width, height);

  if (plane->priv->surface)
    cairo_surface_destroy (plane->priv->surface);
  plane->priv->surface = surface;

  if (width == 1 || height == 1)
    return;

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width);

  data = g_malloc (height * stride);

  h = gtk_adjustment_get_value (plane->priv->h_adj);
  sf = 1.0 / (height - 1);
  vf = 1.0 / (width - 1);
  for (y = 0; y < height; y++)
    {
      s = CLAMP (1.0 - y * sf, 0.0, 1.0);
      p = data + y * (stride / 4);
      for (x = 0; x < width; x++)
        {
          v = x * vf;
          gtk_hsv_to_rgb (h, s, v, &r, &g, &b);
          red = CLAMP (r * 255, 0, 255);
          green = CLAMP (g * 255, 0, 255);
          blue = CLAMP (b * 255, 0, 255);
          p[x] = (red << 16) | (green << 8) | blue;
        }
    }

  tmp = cairo_image_surface_create_for_data ((guchar *)data, CAIRO_FORMAT_RGB24,
                                             width, height, stride);
  cr = cairo_create (surface);

  cairo_set_source_surface (cr, tmp, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (tmp);
  g_free (data);
}

static gboolean
plane_configure (GtkWidget         *widget,
                 GdkEventConfigure *event)
{
  create_surface (GTK_COLOR_PLANE (widget));
  return TRUE;
}

static void
set_cross_grab (GtkWidget *widget,
                GdkDevice *device,
                guint32    time)
{
  GdkCursor *cursor;

  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (widget)),
                                       GDK_CROSSHAIR);
  gdk_device_grab (device,
                   gtk_widget_get_window (widget),
                   GDK_OWNERSHIP_NONE,
                   FALSE,
                   GDK_POINTER_MOTION_MASK
                    | GDK_POINTER_MOTION_HINT_MASK
                    | GDK_BUTTON_RELEASE_MASK,
                   cursor,
                   time);
  g_object_unref (cursor);
}

static gboolean
plane_grab_broken (GtkWidget          *widget,
                   GdkEventGrabBroken *event)
{
  GTK_COLOR_PLANE (widget)->priv->in_drag = FALSE;
  return TRUE;
}

static void
h_changed (GtkColorPlane *plane)
{
  create_surface (plane);
  gtk_widget_queue_draw (GTK_WIDGET (plane));
}

static void
sv_changed (GtkColorPlane *plane)
{
  gtk_widget_queue_draw (GTK_WIDGET (plane));
}

static void
update_color (GtkColorPlane *plane,
              gint           x,
              gint           y)
{
  GtkWidget *widget = GTK_WIDGET (plane);
  gdouble s, v;

  s = CLAMP (1 - y * (1.0 / gtk_widget_get_allocated_height (widget)), 0, 1);
  v = CLAMP (x * (1.0 / gtk_widget_get_allocated_width (widget)), 0, 1);
  gtk_adjustment_set_value (plane->priv->s_adj, s);
  gtk_adjustment_set_value (plane->priv->v_adj, v);

  gtk_widget_queue_draw (widget);
}

static gboolean
plane_button_press (GtkWidget      *widget,
                    GdkEventButton *event)
{
  GtkColorPlane *plane = GTK_COLOR_PLANE (widget);

  if (event->button == GDK_BUTTON_SECONDARY)
    {
      gboolean handled;

      g_signal_emit_by_name (widget, "popup-menu", &handled);

      return TRUE;
    }

  if (plane->priv->in_drag || event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  plane->priv->in_drag = TRUE;
  set_cross_grab (widget, gdk_event_get_device ((GdkEvent*)event), event->time);
  update_color (plane, event->x, event->y);
  gtk_widget_grab_focus (widget);

  return TRUE;
}

static gboolean
plane_button_release (GtkWidget      *widget,
                      GdkEventButton *event)
{
  GtkColorPlane *plane = GTK_COLOR_PLANE (widget);

  if (!plane->priv->in_drag || event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  plane->priv->in_drag = FALSE;

  update_color (plane, event->x, event->y);
  gdk_device_ungrab (gdk_event_get_device ((GdkEvent *) event), event->time);

  return TRUE;
}

static gboolean
plane_motion_notify (GtkWidget      *widget,
                     GdkEventMotion *event)
{
  GtkColorPlane *plane = GTK_COLOR_PLANE (widget);

  if (!plane->priv->in_drag)
    return FALSE;

  gdk_event_request_motions (event);
  update_color (plane, event->x, event->y);

  return TRUE;
}

static void
hold_action (GtkPressAndHold *pah,
             gint             x,
             gint             y,
             GtkColorPlane   *plane)
{
  gboolean handled;

  g_signal_emit_by_name (plane, "popup-menu", &handled);
}

static gboolean
plane_touch (GtkWidget     *widget,
             GdkEventTouch *event)
{
  GtkColorPlane *plane = GTK_COLOR_PLANE (widget);

  if (!plane->priv->press_and_hold)
    {
      gint drag_threshold;

      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-dnd-drag-threshold", &drag_threshold,
                    NULL);

      plane->priv->press_and_hold = gtk_press_and_hold_new ();

      g_object_set (plane->priv->press_and_hold,
                    "drag-threshold", drag_threshold,
                    "hold-time", 1000,
                    NULL);

      g_signal_connect (plane->priv->press_and_hold, "hold",
                        G_CALLBACK (hold_action), plane);
    }

  gtk_press_and_hold_process_event (plane->priv->press_and_hold, (GdkEvent *)event);
  update_color (plane, event->x, event->y);

  return TRUE;
}

static void
sv_move (GtkColorPlane *plane,
         gdouble        ds,
         gdouble        dv)
{
  gdouble s, v;

  s = gtk_adjustment_get_value (plane->priv->s_adj);
  v = gtk_adjustment_get_value (plane->priv->v_adj);

  if (s + ds > 1)
    {
      if (s < 1)
        s = 1;
      else
        goto error;
    }
  else if (s + ds < 0)
    {
      if (s > 0)
        s = 0;
      else
        goto error;
    }
  else
    {
      s += ds;
    }

  if (v + dv > 1)
    {
      if (v < 1)
        v = 1;
      else
        goto error;
    }
  else if (v + dv < 0)
    {
      if (v > 0)
        v = 0;
      else
        goto error;
    }
  else
    {
      v += dv;
    }

  gtk_adjustment_set_value (plane->priv->s_adj, s);
  gtk_adjustment_set_value (plane->priv->v_adj, v);
  return;

error:
  gtk_widget_error_bell (GTK_WIDGET (plane));
}

static gboolean
plane_key_press (GtkWidget   *widget,
                 GdkEventKey *event)
{
  GtkColorPlane *plane = GTK_COLOR_PLANE (widget);
  gdouble step;

  if ((event->state & GDK_MOD1_MASK) != 0)
    step = 0.1;
  else
    step = 0.01;

  if (event->keyval == GDK_KEY_Up ||
      event->keyval == GDK_KEY_KP_Up)
    sv_move (plane, step, 0);
  else if (event->keyval == GDK_KEY_Down ||
           event->keyval == GDK_KEY_KP_Down)
    sv_move (plane, -step, 0);
  else if (event->keyval == GDK_KEY_Left ||
           event->keyval == GDK_KEY_KP_Left)
    sv_move (plane, 0, -step);
  else if (event->keyval == GDK_KEY_Right ||
           event->keyval == GDK_KEY_KP_Right)
    sv_move (plane, 0, step);
  else
    return GTK_WIDGET_CLASS (gtk_color_plane_parent_class)->key_press_event (widget, event);

  return TRUE;
}

static void
gtk_color_plane_init (GtkColorPlane *plane)
{
  AtkObject *atk_obj;

  plane->priv = G_TYPE_INSTANCE_GET_PRIVATE (plane,
                                             GTK_TYPE_COLOR_PLANE,
                                             GtkColorPlanePrivate);

  gtk_widget_set_can_focus (GTK_WIDGET (plane), TRUE);
  gtk_widget_set_events (GTK_WIDGET (plane), GDK_KEY_PRESS_MASK
                                             | GDK_TOUCH_MASK
                                             | GDK_BUTTON_PRESS_MASK
                                             | GDK_BUTTON_RELEASE_MASK
                                             | GDK_POINTER_MOTION_MASK);

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (plane));
  if (GTK_IS_ACCESSIBLE (atk_obj))
    {
      atk_object_set_name (atk_obj, _("Color Plane"));
      atk_object_set_role (atk_obj, ATK_ROLE_COLOR_CHOOSER);
    }
}

static void
plane_finalize (GObject *object)
{
  GtkColorPlane *plane = GTK_COLOR_PLANE (object);

  if (plane->priv->surface)
    cairo_surface_destroy (plane->priv->surface);

  g_clear_object (&plane->priv->h_adj);
  g_clear_object (&plane->priv->s_adj);
  g_clear_object (&plane->priv->v_adj);

  g_clear_object (&plane->priv->press_and_hold);

  G_OBJECT_CLASS (gtk_color_plane_parent_class)->finalize (object);
}

static void
gtk_color_plane_class_init (GtkColorPlaneClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = plane_finalize;

  widget_class->draw = plane_draw;
  widget_class->configure_event = plane_configure;
  widget_class->button_press_event = plane_button_press;
  widget_class->button_release_event = plane_button_release;
  widget_class->motion_notify_event = plane_motion_notify;
  widget_class->grab_broken_event = plane_grab_broken;
  widget_class->key_press_event = plane_key_press;
  widget_class->touch_event = plane_touch;

  g_type_class_add_private (class, sizeof (GtkColorPlanePrivate));
}

GtkWidget *
gtk_color_plane_new (GtkAdjustment *h_adj,
                     GtkAdjustment *s_adj,
                     GtkAdjustment *v_adj)
{
  GtkColorPlane *plane;

  plane = (GtkColorPlane *) g_object_new (GTK_TYPE_COLOR_PLANE, NULL);

  plane->priv->h_adj = g_object_ref_sink (h_adj);
  plane->priv->s_adj = g_object_ref_sink (s_adj);
  plane->priv->v_adj = g_object_ref_sink (v_adj);
  g_signal_connect_swapped (h_adj, "value-changed", G_CALLBACK (h_changed), plane);
  g_signal_connect_swapped (s_adj, "value-changed", G_CALLBACK (sv_changed), plane);
  g_signal_connect_swapped (v_adj, "value-changed", G_CALLBACK (sv_changed), plane);

  return (GtkWidget *)plane;
}
