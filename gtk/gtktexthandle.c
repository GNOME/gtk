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
#include "gtkprivatetypebuiltins.h"
#include "gtktexthandleprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include <gtk/gtk.h>

typedef struct _GtkTextHandlePrivate GtkTextHandlePrivate;
typedef struct _HandleWindow HandleWindow;

enum {
  HANDLE_DRAGGED,
  DRAG_FINISHED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_PARENT,
  PROP_RELATIVE_TO
};

struct _HandleWindow
{
  GdkWindow *window;
  GdkRectangle pointing_to;
  gint dx;
  gint dy;
  guint dragged : 1;
  guint mode_visible : 1;
  guint user_visible : 1;
  guint has_point : 1;
};

struct _GtkTextHandlePrivate
{
  HandleWindow windows[2];
  GtkWidget *parent;
  GdkWindow *relative_to;
  GtkStyleContext *style_context;

  gulong draw_signal_id;
  gulong event_signal_id;
  gulong style_updated_id;
  gulong composited_changed_id;
  guint realized : 1;
  guint mode : 2;
};

G_DEFINE_TYPE (GtkTextHandle, _gtk_text_handle, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };

static void
_gtk_text_handle_get_size (GtkTextHandle *handle,
                           gint          *width,
                           gint          *height)
{
  GtkTextHandlePrivate *priv;
  gint w, h;

  priv = handle->priv;

  gtk_widget_style_get (priv->parent,
                        "text-handle-width", &w,
                        "text-handle-height", &h,
                        NULL);
  if (width)
    *width = w;

  if (height)
    *height = h;
}

static void
_gtk_text_handle_draw (GtkTextHandle         *handle,
                       cairo_t               *cr,
                       GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;
  gint width, height;

  priv = handle->priv;
  cairo_save (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);

  if (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_END)
    cairo_translate (cr, 0, priv->windows[pos].pointing_to.height);

  gtk_style_context_save (priv->style_context);
  gtk_style_context_add_class (priv->style_context,
                               GTK_STYLE_CLASS_CURSOR_HANDLE);

  if (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_END)
    {
      gtk_style_context_add_class (priv->style_context,
                                   GTK_STYLE_CLASS_BOTTOM);

      if (priv->mode == GTK_TEXT_HANDLE_MODE_CURSOR)
        gtk_style_context_add_class (priv->style_context,
                                     GTK_STYLE_CLASS_INSERTION_CURSOR);
    }
  else
    gtk_style_context_add_class (priv->style_context,
                                 GTK_STYLE_CLASS_TOP);

  _gtk_text_handle_get_size (handle, &width, &height);
  gtk_render_background (priv->style_context, cr, 0, 0, width, height);

  gtk_style_context_restore (priv->style_context);
  cairo_restore (cr);
}

static void
_gtk_text_handle_update_shape (GtkTextHandle         *handle,
                               GdkWindow             *window,
                               GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;
  cairo_rectangle_int_t rect;
  cairo_surface_t *surface;
  cairo_region_t *region;
  cairo_t *cr;

  priv = handle->priv;

  surface =
    gdk_window_create_similar_surface (window,
                                       CAIRO_CONTENT_COLOR_ALPHA,
                                       gdk_window_get_width (window),
                                       gdk_window_get_height (window));

  cr = cairo_create (surface);
  _gtk_text_handle_draw (handle, cr, pos);
  cairo_destroy (cr);

  region = gdk_cairo_region_create_from_surface (surface);

  if (gtk_widget_is_composited (priv->parent))
    gdk_window_shape_combine_region (window, NULL, 0, 0);
  else
    gdk_window_shape_combine_region (window, region, 0, 0);

  cairo_region_get_extents (region, &rect);
  cairo_region_destroy (region);

  /* Preserve x/width, but extend input shape
   * vertically to all window height */
  rect.y = 0;
  rect.height = gdk_window_get_height (window);
  region = cairo_region_create_rectangle (&rect);

  gdk_window_input_shape_combine_region (window, region, 0, 0);

  cairo_surface_destroy (surface);
  cairo_region_destroy (region);
}

static GdkWindow *
_gtk_text_handle_create_window (GtkTextHandle         *handle,
                                GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;
  GdkRGBA bg = { 0, 0, 0, 0 };
  GdkWindowAttr attributes;
  GdkWindow *window;
  GdkVisual *visual;
  gint mask;

  priv = handle->priv;

  attributes.x = 0;
  attributes.y = 0;
  _gtk_text_handle_get_size (handle, &attributes.width, &attributes.height);
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.event_mask = (GDK_EXPOSURE_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_BUTTON1_MOTION_MASK);

  mask = GDK_WA_X | GDK_WA_Y;

  visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (priv->parent));

  if (visual)
    {
      attributes.visual = visual;
      mask |= GDK_WA_VISUAL;
    }

  window = gdk_window_new (gtk_widget_get_root_window (priv->parent),
			   &attributes, mask);
  gtk_widget_register_window (priv->parent, window);
  gdk_window_set_background_rgba (window, &bg);

  _gtk_text_handle_update_shape (handle, window, pos);

  return window;
}

static gboolean
gtk_text_handle_widget_draw (GtkWidget     *widget,
                             cairo_t       *cr,
                             GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv;
  GtkTextHandlePosition pos;
  HandleWindow *handle_window;

  priv = handle->priv;

  if (!priv->realized)
    return FALSE;

  if (gtk_cairo_should_draw_window (cr, priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window))
    pos = GTK_TEXT_HANDLE_POSITION_SELECTION_START;
  else if (gtk_cairo_should_draw_window (cr, priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window))
    pos = GTK_TEXT_HANDLE_POSITION_SELECTION_END;
  else
    return FALSE;

  handle_window = &priv->windows[pos];
  if (gdk_window_is_visible (handle_window->window))
    _gtk_text_handle_draw (handle, cr, pos);

  return FALSE;
}

static gboolean
gtk_text_handle_widget_event (GtkWidget     *widget,
                              GdkEvent      *event,
                              GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv;
  GtkTextHandlePosition pos;

  priv = handle->priv;

  if (event->any.window == priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window)
    pos = GTK_TEXT_HANDLE_POSITION_SELECTION_START;
  else if (event->any.window == priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window)
    pos = GTK_TEXT_HANDLE_POSITION_SELECTION_END;
  else
    return FALSE;

  if (event->type == GDK_BUTTON_PRESS)
    {
      priv->windows[pos].dx = event->button.x;
      priv->windows[pos].dy = event->button.y;
      priv->windows[pos].dragged = TRUE;
    }
  else if (event->type == GDK_BUTTON_RELEASE)
    {
      g_signal_emit (handle, signals[DRAG_FINISHED], 0, pos);
      priv->windows[pos].dx =  priv->windows[pos].dy = 0;
      priv->windows[pos].dragged = FALSE;
    }
  else if (event->type == GDK_MOTION_NOTIFY && priv->windows[pos].dragged)
    {
      gint x, y, width, height;

      _gtk_text_handle_get_size (handle, &width, &height);
      gdk_window_get_origin (priv->relative_to, &x, &y);

      x = event->motion.x_root - priv->windows[pos].dx + (width / 2) - x;
      y = event->motion.y_root - priv->windows[pos].dy - y;

      if (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_START)
        y += height;

      y += priv->windows[pos].pointing_to.height / 2;

      g_signal_emit (handle, signals[HANDLE_DRAGGED], 0, pos, x, y);
    }

  return TRUE;
}

static void
_gtk_text_handle_update_window_state (GtkTextHandle         *handle,
                                      GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;
  HandleWindow *handle_window;

  priv = handle->priv;
  handle_window = &priv->windows[pos];

  if (!handle_window->window)
    return;

  if (handle_window->has_point &&
      handle_window->mode_visible && handle_window->user_visible)
    {
      gint x, y, width, height;

      x = handle_window->pointing_to.x;
      y = handle_window->pointing_to.y;
      _gtk_text_handle_get_size (handle, &width, &height);

      if (pos != GTK_TEXT_HANDLE_POSITION_CURSOR)
        y -= height;

      height += handle_window->pointing_to.height;
      x -= width / 2;

      gdk_window_move_resize (handle_window->window, x, y, width, height);
      gdk_window_show (handle_window->window);
    }
  else
    gdk_window_hide (handle_window->window);
}

static void
_gtk_text_handle_update_window (GtkTextHandle         *handle,
                                GtkTextHandlePosition  pos,
                                gboolean               recreate)
{
  GtkTextHandlePrivate *priv;
  HandleWindow *handle_window;

  priv = handle->priv;
  handle_window = &priv->windows[pos];

  if (!handle_window->window)
    return;

  if (recreate)
    {
      gtk_widget_unregister_window (priv->parent, handle_window->window);
      gdk_window_destroy (handle_window->window);
      handle_window->window = _gtk_text_handle_create_window (handle, pos);
    }

  _gtk_text_handle_update_window_state (handle, pos);
}

static void
_gtk_text_handle_update_windows (GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv = handle->priv;

  gtk_style_context_invalidate (priv->style_context);
  _gtk_text_handle_update_window (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START, FALSE);
  _gtk_text_handle_update_window (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END, FALSE);
}

static void
_gtk_text_handle_composited_changed (GtkTextHandle *handle)
{
  _gtk_text_handle_update_window (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START, TRUE);
  _gtk_text_handle_update_window (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END, TRUE);
}

static void
gtk_text_handle_constructed (GObject *object)
{
  GtkTextHandlePrivate *priv;

  priv = GTK_TEXT_HANDLE (object)->priv;
  g_assert (priv->parent != NULL);

  priv->draw_signal_id =
    g_signal_connect (priv->parent, "draw",
                      G_CALLBACK (gtk_text_handle_widget_draw),
                      object);
  priv->event_signal_id =
    g_signal_connect (priv->parent, "event",
                      G_CALLBACK (gtk_text_handle_widget_event),
                      object);
  priv->composited_changed_id =
    g_signal_connect_swapped (priv->parent, "composited-changed",
                              G_CALLBACK (_gtk_text_handle_composited_changed),
                              object);
  priv->style_updated_id =
    g_signal_connect_swapped (priv->parent, "style-updated",
                              G_CALLBACK (_gtk_text_handle_update_windows),
                              object);
}

static void
gtk_text_handle_finalize (GObject *object)
{
  GtkTextHandlePrivate *priv;

  priv = GTK_TEXT_HANDLE (object)->priv;

  if (priv->relative_to)
    g_object_unref (priv->relative_to);

  if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window)
    {
      gtk_widget_unregister_window (priv->parent,
				    priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window);
      gdk_window_destroy (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window);
    }

  if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window)
    {
      gtk_widget_unregister_window (priv->parent,
				    priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window);
      gdk_window_destroy (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window);
    }

  if (g_signal_handler_is_connected (priv->parent, priv->draw_signal_id))
    g_signal_handler_disconnect (priv->parent, priv->draw_signal_id);

  if (g_signal_handler_is_connected (priv->parent, priv->event_signal_id))
    g_signal_handler_disconnect (priv->parent, priv->event_signal_id);

  if (g_signal_handler_is_connected (priv->parent, priv->composited_changed_id))
    g_signal_handler_disconnect (priv->parent, priv->composited_changed_id);

  if (g_signal_handler_is_connected (priv->parent, priv->style_updated_id))
    g_signal_handler_disconnect (priv->parent, priv->style_updated_id);

  g_object_unref (priv->style_context);

  G_OBJECT_CLASS (_gtk_text_handle_parent_class)->finalize (object);
}

static void
gtk_text_handle_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkTextHandlePrivate *priv;
  GtkTextHandle *handle;

  handle = GTK_TEXT_HANDLE (object);
  priv = handle->priv;

  switch (prop_id)
    {
    case PROP_PARENT:
      priv->parent = g_value_get_object (value);
      break;
    case PROP_RELATIVE_TO:
      _gtk_text_handle_set_relative_to (handle,
                                        g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_text_handle_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkTextHandlePrivate *priv;

  priv = GTK_TEXT_HANDLE (object)->priv;

  switch (prop_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    case PROP_RELATIVE_TO:
      g_value_set_object (value, priv->relative_to);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gtk_text_handle_class_init (GtkTextHandleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gtk_text_handle_constructed;
  object_class->finalize = gtk_text_handle_finalize;
  object_class->set_property = gtk_text_handle_set_property;
  object_class->get_property = gtk_text_handle_get_property;

  signals[HANDLE_DRAGGED] =
    g_signal_new (I_("handle-dragged"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTextHandleClass, handle_dragged),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT_INT,
		  G_TYPE_NONE, 3,
                  GTK_TYPE_TEXT_HANDLE_POSITION,
                  G_TYPE_INT, G_TYPE_INT);
  signals[DRAG_FINISHED] =
    g_signal_new (I_("drag-finished"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_TEXT_HANDLE_POSITION);

  g_object_class_install_property (object_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        P_("Parent widget"),
                                                        P_("Parent widget"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_RELATIVE_TO,
                                   g_param_spec_object ("relative-to",
                                                        P_("Window"),
                                                        P_("Window the coordinates are based upon"),
                                                        GDK_TYPE_WINDOW,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkTextHandlePrivate));
}

static void
_gtk_text_handle_init (GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv;
  GtkWidgetPath *path;

  handle->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (handle,
                                                     GTK_TYPE_TEXT_HANDLE,
                                                     GtkTextHandlePrivate);

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_TEXT_HANDLE);

  priv->style_context = gtk_style_context_new ();
  gtk_style_context_set_path (priv->style_context, path);
  gtk_widget_path_free (path);
}

GtkTextHandle *
_gtk_text_handle_new (GtkWidget *parent)
{
  return g_object_new (GTK_TYPE_TEXT_HANDLE,
                       "parent", parent,
                       NULL);
}

void
_gtk_text_handle_set_relative_to (GtkTextHandle *handle,
                                  GdkWindow     *window)
{
  GtkTextHandlePrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));
  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  priv = handle->priv;

  if (priv->relative_to)
    {
      gtk_widget_unregister_window (priv->parent,
				    priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window);
      gdk_window_destroy (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window);
      gtk_widget_unregister_window (priv->parent,
				    priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window);
      gdk_window_destroy (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window);
      g_object_unref (priv->relative_to);
    }

  if (window)
    {
      priv->relative_to = g_object_ref (window);
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window =
        _gtk_text_handle_create_window (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START);
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window =
        _gtk_text_handle_create_window (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END);
      priv->realized = TRUE;
    }
  else
    {
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window = NULL;
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].window = NULL;
      priv->relative_to = NULL;
      priv->realized = FALSE;
    }

  g_object_notify (G_OBJECT (handle), "relative-to");
}

void
_gtk_text_handle_set_mode (GtkTextHandle     *handle,
                           GtkTextHandleMode  mode)
{
  GtkTextHandlePrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  priv = handle->priv;

  if (priv->mode == mode)
    return;

  priv->mode = mode;

  switch (mode)
    {
    case GTK_TEXT_HANDLE_MODE_CURSOR:
      priv->windows[GTK_TEXT_HANDLE_POSITION_CURSOR].mode_visible = TRUE;
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].mode_visible = FALSE;
      break;
    case GTK_TEXT_HANDLE_MODE_SELECTION:
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].mode_visible = TRUE;
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].mode_visible = TRUE;
      break;
    case GTK_TEXT_HANDLE_MODE_NONE:
    default:
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].mode_visible = FALSE;
      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].mode_visible = FALSE;
      break;
    }

  _gtk_text_handle_update_shape (handle,
                                 priv->windows[GTK_TEXT_HANDLE_POSITION_CURSOR].window,
                                 GTK_TEXT_HANDLE_POSITION_CURSOR);
  _gtk_text_handle_update_shape (handle,
                                 priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].window,
                                 GTK_TEXT_HANDLE_POSITION_SELECTION_START);

  _gtk_text_handle_update_window_state (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  _gtk_text_handle_update_window_state (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END);
}

GtkTextHandleMode
_gtk_text_handle_get_mode (GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv;

  g_return_val_if_fail (GTK_IS_TEXT_HANDLE (handle), GTK_TEXT_HANDLE_MODE_NONE);

  priv = handle->priv;
  return priv->mode;
}

void
_gtk_text_handle_set_position (GtkTextHandle         *handle,
                               GtkTextHandlePosition  pos,
                               GdkRectangle          *rect)
{
  GtkTextHandlePrivate *priv;
  HandleWindow *handle_window;
  gboolean size_changed;

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  priv = handle->priv;
  pos = CLAMP (pos, GTK_TEXT_HANDLE_POSITION_CURSOR,
               GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  handle_window = &priv->windows[pos];

  if (!priv->realized)
    return;

  if (priv->mode == GTK_TEXT_HANDLE_MODE_NONE ||
      (priv->mode == GTK_TEXT_HANDLE_MODE_CURSOR &&
       pos != GTK_TEXT_HANDLE_POSITION_CURSOR))
    return;

  size_changed = (rect->width != handle_window->pointing_to.width ||
                  rect->height != handle_window->pointing_to.height);

  handle_window->pointing_to = *rect;
  handle_window->has_point = TRUE;
  gdk_window_get_root_coords (priv->relative_to,
                              rect->x, rect->y,
                              &handle_window->pointing_to.x,
                              &handle_window->pointing_to.y);

  _gtk_text_handle_update_window_state (handle, pos);

  if (size_changed)
    _gtk_text_handle_update_shape (handle, handle_window->window, pos);
}

void
_gtk_text_handle_set_visible (GtkTextHandle         *handle,
                              GtkTextHandlePosition  pos,
                              gboolean               visible)
{
  GtkTextHandlePrivate *priv;
  GdkWindow *window;

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  priv = handle->priv;
  pos = CLAMP (pos, GTK_TEXT_HANDLE_POSITION_CURSOR,
               GTK_TEXT_HANDLE_POSITION_SELECTION_START);

  if (!priv->realized)
    return;

  window = priv->windows[pos].window;

  if (!window)
    return;

  if (!gdk_window_is_visible (window))
    _gtk_text_handle_update_shape (handle, window, pos);

  priv->windows[pos].user_visible = visible;
  _gtk_text_handle_update_window_state (handle, pos);
}

gboolean
_gtk_text_handle_get_is_dragged (GtkTextHandle         *handle,
                                 GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;

  g_return_val_if_fail (GTK_IS_TEXT_HANDLE (handle), FALSE);

  priv = handle->priv;
  pos = CLAMP (pos, GTK_TEXT_HANDLE_POSITION_CURSOR,
               GTK_TEXT_HANDLE_POSITION_SELECTION_START);

  return priv->windows[pos].dragged;
}
