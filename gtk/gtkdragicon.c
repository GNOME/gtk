/* GTK - The GIMP Toolkit
 * Copyright 2019 Matthias Clasen
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

#include "gtkdragiconprivate.h"

#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkwidgetprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtknativeprivate.h"
#include "gtkpicture.h"
#include "gtkcssnumbervalueprivate.h"


/**
 * SECTION:gtkdragicon
 * @Short_description: A toplevel to use as drag icon
 * @Title: GtkDragIcon
 *
 * GtkDragIcon is a #GtkNative implementation with the sole purpose
 * to serve as a drag icon during DND operations. A drag icon moves
 * with the pointer during a drag operation and is destroyed when
 * the drag ends.
 *
 * To set up a drag icon and associate it with an ongoing drag operation,
 * use gtk_drag_icon_set_from_paintable(). It is also possible to create
 * a GtkDragIcon with gtk_drag_icon_new_for_drag(() and populate it
 * with widgets yourself.
 */
struct _GtkDragIcon
{
  GtkWidget parent_instance;

  GdkSurface *surface;
  GskRenderer *renderer;
  GtkWidget *widget;
};

struct _GtkDragIconClass
{
  GtkWidgetClass parent_class;
};

enum {
  LAST_ARG = 1
};

static void gtk_drag_icon_root_init   (GtkRootInterface *iface);
static void gtk_drag_icon_native_init (GtkNativeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkDragIcon, gtk_drag_icon, GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_NATIVE,
                                                gtk_drag_icon_native_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ROOT,
                                                gtk_drag_icon_root_init))

static GdkDisplay *
gtk_drag_icon_root_get_display (GtkRoot *self)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (self);

  if (icon->surface)
    return gdk_surface_get_display (icon->surface);

  return gdk_display_get_default ();
}

static void
gtk_drag_icon_root_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_drag_icon_root_get_display;
}

static GdkSurface *
gtk_drag_icon_native_get_surface (GtkNative *native)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (native);

  return icon->surface;
}

static GskRenderer *
gtk_drag_icon_native_get_renderer (GtkNative *native)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (native);

  return icon->renderer;
}

static void
gtk_drag_icon_native_get_surface_transform (GtkNative *native,
                                            int       *x,
                                            int       *y)
{
  GtkCssStyle *style;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (GTK_WIDGET (native)));
  *x  = _gtk_css_number_value_get (style->size->margin_left, 100) +
        _gtk_css_number_value_get (style->border->border_left_width, 100) +
        _gtk_css_number_value_get (style->size->padding_left, 100);
  *y  = _gtk_css_number_value_get (style->size->margin_top, 100) +
        _gtk_css_number_value_get (style->border->border_top_width, 100) +
        _gtk_css_number_value_get (style->size->padding_top, 100);
}

static void
gtk_drag_icon_move_resize (GtkDragIcon *icon)
{
  GtkRequisition req;

  if (icon->surface)
    {
      gtk_widget_get_preferred_size (GTK_WIDGET (icon), NULL, &req);
      gdk_surface_resize (icon->surface, req.width, req.height);
    }
}

static void
gtk_drag_icon_native_check_resize (GtkNative *native)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (native);
  GtkWidget *widget = GTK_WIDGET (native);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    {
      gtk_drag_icon_move_resize (icon);
      if (icon->surface)
        gtk_widget_allocate (widget,
                             gdk_surface_get_width (icon->surface),
                             gdk_surface_get_height (icon->surface),
                             -1, NULL);
    }
}

static void
gtk_drag_icon_native_init (GtkNativeInterface *iface)
{
  iface->get_surface = gtk_drag_icon_native_get_surface;
  iface->get_renderer = gtk_drag_icon_native_get_renderer;
  iface->get_surface_transform = gtk_drag_icon_native_get_surface_transform;
  iface->check_resize = gtk_drag_icon_native_check_resize;
}

static gboolean
surface_render (GdkSurface     *surface,
                cairo_region_t *region,
                GtkWidget      *widget)
{
  gtk_widget_render (widget, surface, region);
  return TRUE;
}

static void
gtk_drag_icon_realize (GtkWidget *widget)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  g_warn_if_fail (icon->surface != NULL);

  gdk_surface_set_widget (icon->surface, widget);

  g_signal_connect (icon->surface, "render", G_CALLBACK (surface_render), widget);

  GTK_WIDGET_CLASS (gtk_drag_icon_parent_class)->realize (widget);

  icon->renderer = gsk_renderer_new_for_surface (icon->surface);
}

static void
gtk_drag_icon_unrealize (GtkWidget *widget)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  GTK_WIDGET_CLASS (gtk_drag_icon_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (icon->renderer);
  g_clear_object (&icon->renderer);

  if (icon->surface)
    {
      g_signal_handlers_disconnect_by_func (icon->surface, surface_render, widget);
      gdk_surface_set_widget (icon->surface, NULL);
    }
}

static void
gtk_drag_icon_map (GtkWidget *widget)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  gdk_surface_show (icon->surface);

  GTK_WIDGET_CLASS (gtk_drag_icon_parent_class)->map (widget);

  if (icon->widget && gtk_widget_get_visible (icon->widget))
    gtk_widget_map (icon->widget);
}

static void
gtk_drag_icon_unmap (GtkWidget *widget)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  g_warn_if_fail (icon->surface != NULL);
  GTK_WIDGET_CLASS (gtk_drag_icon_parent_class)->unmap (widget);
  if (icon->surface)
    gdk_surface_hide (icon->surface);

  if (icon->widget)
    gtk_widget_unmap (icon->widget);
}

static void
gtk_drag_icon_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  if (icon->widget)
    gtk_widget_measure (icon->widget,
                        orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);

  *minimum = MAX (*minimum, 1);
  *natural = MAX (*natural, 1);
}

static void
gtk_drag_icon_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  gtk_drag_icon_move_resize (icon);

  if (icon->widget)
    gtk_widget_allocate (icon->widget, width, height, baseline, NULL);
}

static void
gtk_drag_icon_show (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_css_node_validate (gtk_widget_get_css_node (widget));
  gtk_widget_realize (widget);
  gtk_drag_icon_native_check_resize (GTK_NATIVE (widget));
  gtk_widget_map (widget);
}

static void
gtk_drag_icon_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
}

static void
gtk_drag_icon_dispose (GObject *object)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (object);

  g_clear_pointer (&icon->widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_drag_icon_parent_class)->dispose (object);

  g_clear_object (&icon->surface);
}

static void
gtk_drag_icon_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
  switch (prop_id)
    {
    case LAST_ARG + GTK_ROOT_PROP_FOCUS_WIDGET:
      g_value_set_object (value, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_drag_icon_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec    *pspec)
{
  switch (prop_id)
    {
    case LAST_ARG + GTK_ROOT_PROP_FOCUS_WIDGET:
      // do nothing
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_drag_icon_add (GtkContainer *self,
                   GtkWidget    *widget)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (self);

  if (icon->widget)
    {
      g_warning ("GtkDragIcon already has a child");
      return;
    }

  gtk_widget_set_parent (widget, GTK_WIDGET (icon));
  icon->widget = widget;
}

static void
gtk_drag_icon_remove (GtkContainer *self,
                      GtkWidget    *widget)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (self);

  if (icon->widget == widget)
    {
      gtk_widget_unparent (widget);
      icon->widget = NULL;
    }
}

static void
gtk_drag_icon_class_init (GtkDragIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->dispose = gtk_drag_icon_dispose;
  object_class->get_property = gtk_drag_icon_get_property;
  object_class->set_property = gtk_drag_icon_set_property;

  widget_class->realize = gtk_drag_icon_realize;
  widget_class->unrealize = gtk_drag_icon_unrealize;
  widget_class->map = gtk_drag_icon_map;
  widget_class->unmap = gtk_drag_icon_unmap;
  widget_class->measure = gtk_drag_icon_measure;
  widget_class->size_allocate = gtk_drag_icon_size_allocate;
  widget_class->show = gtk_drag_icon_show;
  widget_class->hide = gtk_drag_icon_hide;

  container_class->add = gtk_drag_icon_add;
  container_class->remove = gtk_drag_icon_remove;

  gtk_root_install_properties (object_class, LAST_ARG);

  gtk_widget_class_set_css_name (widget_class, "dnd");
}

static void
gtk_drag_icon_init (GtkDragIcon *self)
{
}

GtkWidget *
gtk_drag_icon_new (void)
{
  return g_object_new (GTK_TYPE_DRAG_ICON, NULL);
}

/**
 * gtk_drag_icon_new_for_drag:
 * @drag: a #GtkDrag
 *
 * Creates a #GtkDragIcon and associates it with the drag operation.
 *
 * Returns: the new #GtkDragIcon
 */
GtkWidget *
gtk_drag_icon_new_for_drag (GdkDrag *drag)
{
  GtkWidget *icon;

  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);

  icon = g_object_new (GTK_TYPE_DRAG_ICON, NULL);

  gtk_drag_icon_set_surface (GTK_DRAG_ICON (icon), gdk_drag_get_drag_surface (drag));

  return icon;
}

/**
 * gtk_drag_icon_set_from_paintable:
 * @drag: a #GdkDrag
 * @paintable: a #GdkPaintable to display
 * @hot_x: X coordinate of the hotspot
 * @hot_y: Y coordinate of the hotspot
 *
 * Creates a #GtkDragIcon that shows @paintable, and associates
 * it with the drag operation. The hotspot position on the paintable
 * is aligned with the hotspot of the cursor.
 */
void
gtk_drag_icon_set_from_paintable (GdkDrag      *drag,
                                  GdkPaintable *paintable,
                                  int           hot_x,
                                  int           hot_y)
{
  GtkWidget *icon;
  GtkWidget *picture;

  gdk_drag_set_hotspot (drag, hot_x, hot_y);

  icon = gtk_drag_icon_new_for_drag (drag);

  picture = gtk_picture_new_for_paintable (paintable);
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
  gtk_container_add (GTK_CONTAINER (icon), picture);

  g_object_set_data_full (G_OBJECT (drag),
                          "icon",
                          g_object_ref_sink (icon),
                          (GDestroyNotify)gtk_widget_destroy);
  gtk_widget_show (icon);
}

void
gtk_drag_icon_set_surface (GtkDragIcon *icon,
                           GdkSurface  *surface)
{
  g_set_object (&icon->surface, surface);
}

void
gtk_drag_icon_set_widget (GtkDragIcon *icon,
                          GtkWidget   *widget)
{
  if (icon->widget == widget)
    return;

  if (icon->widget)
    gtk_widget_unparent (icon->widget);

  icon->widget = widget;

  if (icon->widget)
    gtk_widget_set_parent (icon->widget, GTK_WIDGET (icon));
}
