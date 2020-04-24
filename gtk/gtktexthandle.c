/* GTK - The GIMP Toolkit
 * Copyright © 2012 Carlos Garnacho <carlosg@gnome.org>
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

#include "gtkcssnumbervalueprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtktexthandleprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkwindowprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkintl.h"

#include <gtk/gtk.h>

enum {
  DRAG_STARTED,
  HANDLE_DRAGGED,
  DRAG_FINISHED,
  LAST_SIGNAL
};

struct _GtkTextHandle
{
  GtkWidget parent_instance;

  GtkWidget *parent;
  GdkSurface *surface;
  GskRenderer *renderer;

  GdkRectangle pointing_to;
  GtkBorder border;
  gint dx;
  gint dy;
  guint role : 2;
  guint dragged : 1;
  guint mode_visible : 1;
  guint user_visible : 1;
  guint has_point : 1;
};

static void gtk_text_handle_native_interface_init (GtkNativeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTextHandle, gtk_text_handle, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_NATIVE,
                                                gtk_text_handle_native_interface_init))

static guint signals[LAST_SIGNAL] = { 0 };

static GdkSurface *
gtk_text_handle_native_get_surface (GtkNative *native)
{
  return GTK_TEXT_HANDLE (native)->surface;
}

static GskRenderer *
gtk_text_handle_native_get_renderer (GtkNative *native)
{
  return GTK_TEXT_HANDLE (native)->renderer;
}

static void
gtk_text_handle_native_get_surface_transform (GtkNative *native,
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
gtk_text_handle_get_size (GtkTextHandle *handle,
                          gint          *width,
                          gint          *height)
{
  GtkWidget *widget = GTK_WIDGET (handle);
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  if (width)
    *width = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_MIN_WIDTH), 100);
  if (height)
    *height = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_MIN_HEIGHT), 100);
}

static void
gtk_text_handle_get_padding (GtkTextHandle *handle,
                             GtkBorder     *border)
{
  GtkWidget *widget = GTK_WIDGET (handle);
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  border->left = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_LEFT), 100);
  border->right = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_RIGHT), 100);
  border->top = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_TOP), 100);
  border->bottom = _gtk_css_number_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_PADDING_BOTTOM), 100);
}

static void
gtk_text_handle_present_surface (GtkTextHandle *handle)
{
  GtkWidget *widget = GTK_WIDGET (handle);
  GdkPopupLayout *layout;
  GdkRectangle rect;
  GtkRequisition req;

  gtk_widget_get_preferred_size (widget, NULL, &req);
  gtk_text_handle_get_padding (handle, &handle->border);

  rect.x = handle->pointing_to.x;
  rect.y = handle->pointing_to.y + handle->pointing_to.height - handle->border.top;
  rect.width = req.width - handle->border.left - handle->border.right;
  rect.height = 1;

  gtk_widget_translate_coordinates (gtk_widget_get_parent (widget),
                                    gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW),
                                    rect.x, rect.y, &rect.x, &rect.y);

  if (handle->role == GTK_TEXT_HANDLE_ROLE_CURSOR)
    rect.x -= rect.width / 2;
  else if ((handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_END &&
            gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ||
           (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_START &&
            gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL))
    rect.x -= rect.width;

  layout = gdk_popup_layout_new (&rect,
                                 GDK_GRAVITY_SOUTH,
                                 GDK_GRAVITY_NORTH);
  gdk_popup_layout_set_anchor_hints (layout,
                                     GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_SLIDE_X);

  gdk_popup_present (GDK_POPUP (handle->surface),
                     MAX (req.width, 1),
                     MAX (req.height, 1),
                     layout);
  gdk_popup_layout_unref (layout);

  gtk_widget_allocate (widget,
                       gdk_surface_get_width (handle->surface),
                       gdk_surface_get_height (handle->surface),
                       -1, NULL);
}

static void
gtk_text_handle_native_check_resize (GtkNative *native)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (native);
  GtkWidget *widget = GTK_WIDGET (native);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    gtk_text_handle_present_surface (handle);
}

static void
gtk_text_handle_native_interface_init (GtkNativeInterface *iface)
{
  iface->get_surface = gtk_text_handle_native_get_surface;
  iface->get_renderer = gtk_text_handle_native_get_renderer;
  iface->get_surface_transform = gtk_text_handle_native_get_surface_transform;
  iface->check_resize = gtk_text_handle_native_check_resize;
}

static gboolean
surface_render (GdkSurface     *surface,
                cairo_region_t *region,
                GtkTextHandle  *handle)
{
  gtk_widget_render (GTK_WIDGET (handle), surface, region);
  return TRUE;
}

static gboolean
surface_event (GdkSurface    *surface,
               GdkEvent      *event,
               GtkTextHandle *handle)
{
  gtk_main_do_event (event);
  return TRUE;
}

static void
surface_mapped_changed (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  gtk_widget_set_visible (widget, gdk_surface_get_mapped (handle->surface));
}

static void
gtk_text_handle_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  gtk_css_style_snapshot_icon (style,
                               snapshot,
                               gtk_widget_get_width (widget),
                               gtk_widget_get_height (widget));
}

static void
gtk_text_handle_realize (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);
  GdkSurface *parent_surface;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);
  parent_surface = gtk_native_get_surface (gtk_widget_get_native (parent));

  handle->surface = gdk_surface_new_popup (parent_surface, FALSE);
  gdk_surface_set_widget (handle->surface, widget);

  g_signal_connect_swapped (handle->surface, "notify::mapped",
                            G_CALLBACK (surface_mapped_changed), widget);
  g_signal_connect (handle->surface, "render", G_CALLBACK (surface_render), widget);
  g_signal_connect (handle->surface, "event", G_CALLBACK (surface_event), widget);

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->realize (widget);

  handle->renderer = gsk_renderer_new_for_surface (handle->surface);
}

static void
gtk_text_handle_unrealize (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (handle->renderer);
  g_clear_object (&handle->renderer);

  g_signal_handlers_disconnect_by_func (handle->surface, surface_render, widget);
  g_signal_handlers_disconnect_by_func (handle->surface, surface_event, widget);
  g_signal_handlers_disconnect_by_func (handle->surface, surface_mapped_changed, widget);

  gdk_surface_set_widget (handle->surface, NULL);
  gdk_surface_destroy (handle->surface);
  g_clear_object (&handle->surface);
}

static void
gtk_text_handle_map (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->map (widget);

  if (handle->has_point)
    gtk_text_handle_present_surface (handle);
}

static void
gtk_text_handle_unmap (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->unmap (widget);
  gdk_surface_hide (handle->surface);
}

static void
gtk_text_handle_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  gint size;

  if (orientation == GTK_ORIENTATION_VERTICAL)
    gtk_text_handle_get_size (GTK_TEXT_HANDLE (widget), NULL, &size);
  else
    gtk_text_handle_get_size (GTK_TEXT_HANDLE (widget), &size, NULL);

  *natural = size;
  *minimum = size;
  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
gtk_text_handle_class_init (GtkTextHandleClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  widget_class->snapshot = gtk_text_handle_snapshot;
  widget_class->realize = gtk_text_handle_realize;
  widget_class->unrealize = gtk_text_handle_unrealize;
  widget_class->map = gtk_text_handle_map;
  widget_class->unmap = gtk_text_handle_unmap;
  widget_class->measure = gtk_text_handle_measure;

  signals[HANDLE_DRAGGED] =
    g_signal_new (I_("handle-dragged"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
		  _gtk_marshal_VOID__INT_INT,
		  G_TYPE_NONE, 2,
                  G_TYPE_INT, G_TYPE_INT);
  signals[DRAG_STARTED] =
    g_signal_new (I_("drag-started"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0, G_TYPE_NONE);
  signals[DRAG_FINISHED] =
    g_signal_new (I_("drag-finished"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0, G_TYPE_NONE);
}

static void
handle_drag_begin (GtkGestureDrag *gesture,
                   gdouble         x,
                   gdouble         y,
                   GtkTextHandle  *handle)
{
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (handle->role == GTK_TEXT_HANDLE_ROLE_CURSOR)
    x -= gtk_widget_get_width (widget) / 2;
  else if ((handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_END &&
            gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ||
           (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_START &&
            gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL))
    x -= gtk_widget_get_width (widget);

  y += handle->border.top / 2;

  handle->dx = x;
  handle->dy = y;
  handle->dragged = TRUE;
  g_signal_emit (handle, signals[DRAG_STARTED], 0);
}

static void
handle_drag_update (GtkGestureDrag *gesture,
                    gdouble         offset_x,
                    gdouble         offset_y,
                    GtkWidget      *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);
  gdouble start_x, start_y;
  gint x, y;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

  x = handle->pointing_to.x + handle->pointing_to.width / 2 +
    start_x + offset_x - handle->dx;
  y = handle->pointing_to.y + handle->pointing_to.height +
    start_y + offset_y - handle->dy;

  if (handle->role == GTK_TEXT_HANDLE_ROLE_CURSOR)
    x -= gtk_widget_get_width (widget) / 2;
  else if ((handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_END &&
            gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ||
           (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_START &&
            gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL))
    x -= gtk_widget_get_width (widget);

  g_signal_emit (widget, signals[HANDLE_DRAGGED], 0, x, y);
}

static void
handle_drag_end (GtkGestureDrag *gesture,
                 gdouble         offset_x,
                 gdouble         offset_y,
                 GtkTextHandle  *handle)
{
  g_signal_emit (handle, signals[DRAG_FINISHED], 0);
  handle->dragged = FALSE;
}

static void
gtk_text_handle_update_for_role (GtkTextHandle *handle)
{
  GtkWidget *widget = GTK_WIDGET (handle);

  if (handle->role == GTK_TEXT_HANDLE_ROLE_CURSOR)
    {
      gtk_widget_remove_css_class (widget, GTK_STYLE_CLASS_TOP);
      gtk_widget_add_css_class (widget, GTK_STYLE_CLASS_BOTTOM);
      gtk_widget_add_css_class (widget, GTK_STYLE_CLASS_INSERTION_CURSOR);
    }
  else if (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_END)
    {
      gtk_widget_remove_css_class (widget, GTK_STYLE_CLASS_TOP);
      gtk_widget_add_css_class (widget, GTK_STYLE_CLASS_BOTTOM);
      gtk_widget_remove_css_class (widget, GTK_STYLE_CLASS_INSERTION_CURSOR);
    }
  else if (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_START)
    {
      gtk_widget_add_css_class (widget, GTK_STYLE_CLASS_TOP);
      gtk_widget_remove_css_class (widget, GTK_STYLE_CLASS_BOTTOM);
      gtk_widget_remove_css_class (widget, GTK_STYLE_CLASS_INSERTION_CURSOR);
    }
}

static void
gtk_text_handle_init (GtkTextHandle *widget)
{
  GtkEventController *controller;

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_drag_new ());
  g_signal_connect (controller, "drag-begin",
                    G_CALLBACK (handle_drag_begin), widget);
  g_signal_connect (controller, "drag-update",
                    G_CALLBACK (handle_drag_update), widget);
  g_signal_connect (controller, "drag-end",
                    G_CALLBACK (handle_drag_end), widget);
  gtk_widget_add_controller (GTK_WIDGET (widget), controller);

  gtk_text_handle_update_for_role (GTK_TEXT_HANDLE (widget));
}

GtkTextHandle *
gtk_text_handle_new (GtkWidget *parent)
{
  GtkTextHandle *handle;

  handle = g_object_new (GTK_TYPE_TEXT_HANDLE,
                         "css-name", I_("cursor-handle"),
                         NULL);
  gtk_widget_set_parent (GTK_WIDGET (handle), parent);

  return handle;
}

void
gtk_text_handle_set_role (GtkTextHandle     *handle,
                          GtkTextHandleRole  role)
{
  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  if (handle->role == role)
    return;

  handle->role = role;
  gtk_text_handle_update_for_role (handle);

  if (gtk_widget_get_visible (GTK_WIDGET (handle)))
    {
      if (handle->has_point)
        gtk_text_handle_present_surface (handle);
    }
}

GtkTextHandleRole
gtk_text_handle_get_role (GtkTextHandle *handle)
{
  g_return_val_if_fail (GTK_IS_TEXT_HANDLE (handle), GTK_TEXT_HANDLE_ROLE_CURSOR);

  return handle->role;
}

void
gtk_text_handle_set_position (GtkTextHandle      *handle,
                              const GdkRectangle *rect)
{
  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  if (handle->pointing_to.x == rect->x &&
      handle->pointing_to.y == rect->y &&
      handle->pointing_to.width == rect->width &&
      handle->pointing_to.height == rect->height)
    return;

  handle->pointing_to = *rect;
  handle->has_point = TRUE;

  if (gtk_widget_is_visible (GTK_WIDGET (handle)))
    gtk_text_handle_present_surface (handle);
}

gboolean
gtk_text_handle_get_is_dragged (GtkTextHandle *handle)
{
  g_return_val_if_fail (GTK_IS_TEXT_HANDLE (handle), FALSE);

  return handle->dragged;
}
