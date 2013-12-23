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
  PROP_PARENT
};

struct _HandleWindow
{
  GtkWidget *widget;
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
  guint mode : 2;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkTextHandle, _gtk_text_handle, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };

static void _gtk_text_handle_update (GtkTextHandle         *handle,
                                     GtkTextHandlePosition  pos);

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
  GtkStyleContext *context;
  gint width, height;

  priv = handle->priv;
  context = gtk_widget_get_style_context (priv->parent);

  cairo_save (cr);

  if (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_END)
    cairo_translate (cr, 0, priv->windows[pos].pointing_to.height);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context,
                               GTK_STYLE_CLASS_CURSOR_HANDLE);

  if (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_END)
    {
      gtk_style_context_add_class (context,
                                   GTK_STYLE_CLASS_BOTTOM);

      if (priv->mode == GTK_TEXT_HANDLE_MODE_CURSOR)
        gtk_style_context_add_class (context,
                                     GTK_STYLE_CLASS_INSERTION_CURSOR);
    }
  else
    gtk_style_context_add_class (context,
                                 GTK_STYLE_CLASS_TOP);

  _gtk_text_handle_get_size (handle, &width, &height);
  gtk_render_background (context, cr, 0, 0, width, height);

  gtk_style_context_restore (context);
  cairo_restore (cr);
}

static gint
_text_handle_pos_from_widget (GtkTextHandle *handle,
                              GtkWidget     *widget)
{
  GtkTextHandlePrivate *priv = handle->priv;

  if (widget == priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget)
    return GTK_TEXT_HANDLE_POSITION_SELECTION_START;
  else if (widget == priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget)
    return GTK_TEXT_HANDLE_POSITION_SELECTION_END;
  else
    return -1;
}

static gboolean
gtk_text_handle_widget_draw (GtkWidget     *widget,
                             cairo_t       *cr,
                             GtkTextHandle *handle)
{
  gint pos;

  pos = _text_handle_pos_from_widget (handle, widget);

  if (pos < 0)
    return FALSE;

  _gtk_text_handle_draw (handle, cr, pos);
  return TRUE;
}

static gboolean
gtk_text_handle_widget_event (GtkWidget     *widget,
                              GdkEvent      *event,
                              GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv;
  gint pos;

  priv = handle->priv;
  pos = _text_handle_pos_from_widget (handle, widget);

  if (pos < 0)
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
      priv->windows[pos].dragged = FALSE;
    }
  else if (event->type == GDK_MOTION_NOTIFY &&
           event->motion.state & GDK_BUTTON1_MASK &&
           priv->windows[pos].dragged)
    {
      gint x, y, width, height;

      _gtk_text_handle_get_size (handle, &width, &height);
      x = event->motion.x - priv->windows[pos].dx + (width / 2);
      y = event->motion.y - priv->windows[pos].dy;

      if (pos != GTK_TEXT_HANDLE_POSITION_CURSOR)
        y += height;

      y += priv->windows[pos].pointing_to.height / 2;

      gtk_widget_translate_coordinates (widget, priv->parent, x, y, &x, &y);
      g_signal_emit (handle, signals[HANDLE_DRAGGED], 0, pos, x, y);
    }

  return TRUE;
}

static void
gtk_text_handle_widget_style_updated (GtkTextHandle *handle)
{
  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END);
}

static GtkWidget *
_gtk_text_handle_ensure_widget (GtkTextHandle         *handle,
                                GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;

  priv = handle->priv;

  if (!priv->windows[pos].widget)
    {
      GtkWidget *widget, *window;

      widget = gtk_event_box_new ();
      gtk_widget_set_events (widget,
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK);

      g_signal_connect (widget, "draw",
                        G_CALLBACK (gtk_text_handle_widget_draw), handle);
      g_signal_connect (widget, "event",
                        G_CALLBACK (gtk_text_handle_widget_event), handle);
      g_signal_connect (widget, "style-updated",
                        G_CALLBACK (gtk_text_handle_widget_style_updated),
                        handle);

      priv->windows[pos].widget = widget;
      window = gtk_widget_get_ancestor (priv->parent, GTK_TYPE_WINDOW);
      gtk_window_add_popover (GTK_WINDOW (window), widget);
    }

  return priv->windows[pos].widget;
}

static void
_gtk_text_handle_update (GtkTextHandle         *handle,
                         GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;
  HandleWindow *handle_window;

  priv = handle->priv;
  handle_window = &priv->windows[pos];

  if (!priv->parent || !gtk_widget_is_drawable (priv->parent))
    return;

  if (handle_window->has_point &&
      handle_window->mode_visible && handle_window->user_visible)
    {
      cairo_rectangle_int_t rect;
      GtkPositionType handle_pos;
      gint width, height;
      GtkWidget *window;

      _gtk_text_handle_ensure_widget (handle, pos);
      rect.x = handle_window->pointing_to.x;
      rect.y = handle_window->pointing_to.y;
      _gtk_text_handle_get_size (handle, &width, &height);

      window = gtk_widget_get_parent (handle_window->widget);
      gtk_widget_translate_coordinates (priv->parent, window,
                                        rect.x, rect.y, &rect.x, &rect.y);

      if (pos == GTK_TEXT_HANDLE_POSITION_CURSOR)
        handle_pos = GTK_POS_BOTTOM;
      else
        {
          handle_pos = GTK_POS_TOP;
          rect.y += handle_window->pointing_to.height;
        }

      height += handle_window->pointing_to.height;
      rect.width = width;
      rect.height = 0;
      rect.x -= rect.width / 2;

      gtk_widget_set_size_request (handle_window->widget, width, height);
      gtk_widget_show (handle_window->widget);
      gtk_window_set_popover_position (GTK_WINDOW (window),
                                       handle_window->widget,
                                       handle_pos, &rect);
    }
  else if (handle_window->widget)
    gtk_widget_hide (handle_window->widget);
}

static void
gtk_text_handle_finalize (GObject *object)
{
  GtkTextHandlePrivate *priv;

  priv = GTK_TEXT_HANDLE (object)->priv;

  if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget)
    gtk_widget_destroy (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget);

  if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget)
    gtk_widget_destroy (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gtk_text_handle_class_init (GtkTextHandleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
}

static void
_gtk_text_handle_init (GtkTextHandle *handle)
{
  handle->priv = _gtk_text_handle_get_instance_private (handle);
}

GtkTextHandle *
_gtk_text_handle_new (GtkWidget *parent)
{
  return g_object_new (GTK_TYPE_TEXT_HANDLE,
                       "parent", parent,
                       NULL);
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

  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END);
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

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  priv = handle->priv;
  pos = CLAMP (pos, GTK_TEXT_HANDLE_POSITION_CURSOR,
               GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  handle_window = &priv->windows[pos];

  if (priv->mode == GTK_TEXT_HANDLE_MODE_NONE ||
      (priv->mode == GTK_TEXT_HANDLE_MODE_CURSOR &&
       pos != GTK_TEXT_HANDLE_POSITION_CURSOR))
    return;

  handle_window->pointing_to = *rect;
  handle_window->has_point = TRUE;

  if (gtk_widget_is_visible (priv->parent))
    _gtk_text_handle_update (handle, pos);
}

void
_gtk_text_handle_set_visible (GtkTextHandle         *handle,
                              GtkTextHandlePosition  pos,
                              gboolean               visible)
{
  GtkTextHandlePrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  priv = handle->priv;
  pos = CLAMP (pos, GTK_TEXT_HANDLE_POSITION_CURSOR,
               GTK_TEXT_HANDLE_POSITION_SELECTION_START);

  priv->windows[pos].user_visible = visible;

  if (gtk_widget_is_visible (priv->parent))
    _gtk_text_handle_update (handle, pos);
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
