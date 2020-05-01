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

#include "gtkdragicon.h"

#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkwidgetprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtknativeprivate.h"
#include "gtkpicture.h"
#include "gtkcssnumbervalueprivate.h"

/* for the drag icons */
#include "gtkcolorswatchprivate.h"
#include "gtklabel.h"
#include "gtktextutil.h"


/**
 * SECTION:gtkdragicon
 * @Short_description: A toplevel to use as drag icon
 * @Title: GtkDragIcon
 *
 * GtkDragIcon is a #GtkRoot implementation with the sole purpose
 * to serve as a drag icon during DND operations. A drag icon moves
 * with the pointer during a drag operation and is destroyed when
 * the drag ends.
 *
 * To set up a drag icon and associate it with an ongoing drag operation,
 * use gtk_drag_icon_get_for_drag() to get the icon for a drag. You can
 * then use it like any other widget and use gtk_drag_icon_set_child() to
 * set whatever widget should be used for the drag icon.
 *
 * Keep in mind that drag icons do not allow user input.
 */
struct _GtkDragIcon
{
  GtkWidget parent_instance;

  GdkSurface *surface;
  GskRenderer *renderer;
  GtkWidget *child;
};

struct _GtkDragIconClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_CHILD,

  LAST_ARG
};

static GParamSpec *properties[LAST_ARG] = { NULL, };

static void gtk_drag_icon_root_init   (GtkRootInterface *iface);
static void gtk_drag_icon_native_init (GtkNativeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkDragIcon, gtk_drag_icon, GTK_TYPE_WIDGET,
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
      gdk_drag_surface_present (GDK_DRAG_SURFACE (icon->surface),
                                MAX (1, req.width),
                                MAX (1, req.height));
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

  gtk_drag_icon_move_resize (icon);

  GTK_WIDGET_CLASS (gtk_drag_icon_parent_class)->map (widget);

  if (icon->child && gtk_widget_get_visible (icon->child))
    gtk_widget_map (icon->child);
}

static void
gtk_drag_icon_unmap (GtkWidget *widget)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  g_warn_if_fail (icon->surface != NULL);
  GTK_WIDGET_CLASS (gtk_drag_icon_parent_class)->unmap (widget);
  if (icon->surface)
    gdk_surface_hide (icon->surface);

  if (icon->child)
    gtk_widget_unmap (icon->child);
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

  if (icon->child)
    gtk_widget_measure (icon->child,
                        orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gtk_drag_icon_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkDragIcon *icon = GTK_DRAG_ICON (widget);

  if (icon->child)
    gtk_widget_allocate (icon->child, width, height, baseline, NULL);
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

  g_clear_pointer (&icon->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_drag_icon_parent_class)->dispose (object);

  g_clear_object (&icon->surface);
}

static void
gtk_drag_icon_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
  GtkDragIcon *self = GTK_DRAG_ICON (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
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
  GtkDragIcon *self = GTK_DRAG_ICON (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      gtk_drag_icon_set_child (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_drag_icon_class_init (GtkDragIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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
  widget_class->focus = gtk_widget_focus_none;
  widget_class->grab_focus = gtk_widget_grab_focus_none;

  /**
   * GtkDragIcon:child:
   *
   * The widget to display as drag icon.
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child",
                         P_("Child"),
                         P_("The widget to display as drag icon."),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_ARG, properties);

  gtk_widget_class_set_css_name (widget_class, "dnd");
}

static void
gtk_drag_icon_init (GtkDragIcon *self)
{
}

/**
 * gtk_drag_icon_get_for_drag:
 * @drag: a #GdkDrag
 *
 * Gets the #GtkDragIcon in use with @drag.
 *
 * If no drag icon exists yet, a new one will be created
 * and shown.
 *
 * Returns: (transfer none): the #GtkDragIcon
 */
GtkWidget *
gtk_drag_icon_get_for_drag (GdkDrag *drag)
{
  static GQuark drag_icon_quark = 0;
  GtkWidget *self;

  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);

  if (G_UNLIKELY (drag_icon_quark == 0))
    drag_icon_quark = g_quark_from_static_string ("-gtk-drag-icon");

  self = g_object_get_qdata (G_OBJECT (drag), drag_icon_quark);
  if (self == NULL)
    {
      self = g_object_new (GTK_TYPE_DRAG_ICON, NULL);

      GTK_DRAG_ICON (self)->surface = g_object_ref (gdk_drag_get_drag_surface (drag));

      g_object_set_qdata_full (G_OBJECT (drag), drag_icon_quark, g_object_ref_sink (self), g_object_unref);

      gtk_widget_show (self);
    }

  return self;
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

  icon = gtk_drag_icon_get_for_drag (drag);

  picture = gtk_picture_new_for_paintable (paintable);
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
  gtk_drag_icon_set_child (GTK_DRAG_ICON (icon), picture);
}

/**
 * gtk_drag_icon_set_child:
 * @self: a #GtkDragIcon
 * @child: (nullable): a #GtkWidget or %NULL
 *
 * Sets the widget to display as the drag icon.
 **/
void
gtk_drag_icon_set_child (GtkDragIcon *self,
                         GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_DRAG_ICON (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  if (self->child)
    gtk_widget_unparent (self->child);

  self->child = child;

  if (self->child)
    gtk_widget_set_parent (self->child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * gtk_drag_icon_get_child:
 * @self: a #GtkDragIcon
 *
 * Gets the widget currently used as drag icon.
 *
 * Returns: (nullable) (transfer none): The drag icon or %NULL if none.
 **/
GtkWidget *
gtk_drag_icon_get_child (GtkDragIcon *self)
{
  g_return_val_if_fail (GTK_IS_DRAG_ICON (self), NULL);

  return self->child;
}

/**
 * gtk_drag_icon_create_widget_for_value:
 * @value: a #GValue
 *
 * Creates a widget that can be used as a drag icon for the given
 * @value.
 *
 * If GTK does not know how to create a widget for a given value,
 * it will return %NULL.
 *
 * This method is used to set the default drag icon on drag'n'drop
 * operations started by #GtkDragSource, so you don't need to set
 * a drag icon using this function there.
 *
 * Returns: (nullable) (transfer full): A new #GtkWidget
 *     for displaying @value as a drag icon.
 **/
GtkWidget *
gtk_drag_icon_create_widget_for_value (const GValue *value)
{
  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    {
      return gtk_label_new (g_value_get_string (value));
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_RGBA))
    {
      GtkWidget *swatch;

      swatch = gtk_color_swatch_new ();
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (swatch), g_value_get_boxed (value));

      return swatch;
    }
  else if (G_VALUE_HOLDS (value, GTK_TYPE_TEXT_BUFFER))
    {
      GtkTextBuffer *buffer = g_value_get_object (value);
      GtkTextIter start, end;
      GdkPaintable *paintable;
      GtkWidget *picture;

      if (buffer == NULL || !gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
        return NULL;

      picture = gtk_picture_new ();
      paintable = gtk_text_util_create_rich_drag_icon (picture, buffer, &start, &end);
      gtk_picture_set_paintable (GTK_PICTURE (picture), paintable);
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
      g_object_unref (paintable);

      return picture;
    }
  else
    {
      return NULL;
    }
}

