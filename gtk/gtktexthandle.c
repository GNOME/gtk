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
#include "gtkwindowprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"

#include <gtk/gtk.h>

typedef struct _GtkTextHandlePrivate GtkTextHandlePrivate;
typedef struct _HandleWindow HandleWindow;

enum {
  DRAG_STARTED,
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
  GtkBorder border;
  gint dx;
  gint dy;
  GtkTextDirection dir;
  guint dragged : 1;
  guint mode_visible : 1;
  guint user_visible : 1;
  guint has_point : 1;
};

struct _GtkTextHandlePrivate
{
  HandleWindow windows[2];
  GtkWidget *parent;
  GtkScrollable *parent_scrollable;
  GtkAdjustment *vadj;
  GtkAdjustment *hadj;
  guint hierarchy_changed_id;
  guint scrollable_notify_id;
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
  HandleWindow *handle_window;
  GtkStyleContext *context;
  gint width, height;

  priv = handle->priv;
  handle_window = &priv->windows[pos];

  context = gtk_widget_get_style_context (handle_window->widget);
  _gtk_text_handle_get_size (handle, &width, &height);

  cairo_save (cr);
  cairo_translate (cr, handle_window->border.left, handle_window->border.top);

  gtk_render_handle (context, cr, 0, 0, width, height);

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

#if 0
  /* Show the invisible border */
  cairo_set_source_rgba (cr, 1, 0, 0, 0.5);
  cairo_paint (cr);
#endif

  _gtk_text_handle_draw (handle, cr, pos);
  return TRUE;
}

static void
gtk_text_handle_set_state (GtkTextHandle *handle,
                           gint           pos,
                           GtkStateFlags  state)
{
  GtkTextHandlePrivate *priv = handle->priv;

  if (!priv->windows[pos].widget)
    return;

  gtk_widget_set_state_flags (priv->windows[pos].widget, state, FALSE);
  gtk_widget_queue_draw (priv->windows[pos].widget);
}

static void
gtk_text_handle_unset_state (GtkTextHandle *handle,
                             gint           pos,
                             GtkStateFlags  state)
{
  GtkTextHandlePrivate *priv = handle->priv;

  if (!priv->windows[pos].widget)
    return;

  gtk_widget_unset_state_flags (priv->windows[pos].widget, state);
  gtk_widget_queue_draw (priv->windows[pos].widget);
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
      gtk_text_handle_set_state (handle, pos, GTK_STATE_FLAG_ACTIVE);
      g_signal_emit (handle, signals[DRAG_STARTED], 0, pos);
    }
  else if (event->type == GDK_BUTTON_RELEASE)
    {
      g_signal_emit (handle, signals[DRAG_FINISHED], 0, pos);
      priv->windows[pos].dragged = FALSE;
      gtk_text_handle_unset_state (handle, pos, GTK_STATE_FLAG_ACTIVE);
    }
  else if (event->type == GDK_ENTER_NOTIFY)
    gtk_text_handle_set_state (handle, pos, GTK_STATE_FLAG_PRELIGHT);
  else if (event->type == GDK_LEAVE_NOTIFY)
    {
      if (!priv->windows[pos].dragged &&
          (event->crossing.mode == GDK_CROSSING_NORMAL ||
           event->crossing.mode == GDK_CROSSING_UNGRAB))
        gtk_text_handle_unset_state (handle, pos, GTK_STATE_FLAG_PRELIGHT);
    }
  else if (event->type == GDK_MOTION_NOTIFY &&
           event->motion.state & GDK_BUTTON1_MASK &&
           priv->windows[pos].dragged)
    {
      gint x, y, handle_width, handle_height;
      cairo_rectangle_int_t rect;
      GtkAllocation allocation;
      GtkWidget *window;

      window = gtk_widget_get_parent (priv->windows[pos].widget);
      gtk_widget_get_allocation (priv->windows[pos].widget, &allocation);
      _gtk_text_handle_get_size (handle, &handle_width, &handle_height);

      _gtk_window_get_popover_position (GTK_WINDOW (window),
                                        priv->windows[pos].widget,
                                        NULL, &rect);

      x = rect.x + event->motion.x - priv->windows[pos].dx;
      y = rect.y + event->motion.y - priv->windows[pos].dy +
        priv->windows[pos].border.top / 2;

      if (pos == GTK_TEXT_HANDLE_POSITION_CURSOR &&
          priv->mode == GTK_TEXT_HANDLE_MODE_CURSOR)
        x += handle_width / 2;
      else if ((pos == GTK_TEXT_HANDLE_POSITION_CURSOR &&
                priv->windows[pos].dir == GTK_TEXT_DIR_RTL) ||
               (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_START &&
                priv->windows[pos].dir != GTK_TEXT_DIR_RTL))
        x += handle_width;

      gtk_widget_translate_coordinates (window, priv->parent, x, y, &x, &y);
      g_signal_emit (handle, signals[HANDLE_DRAGGED], 0, pos, x, y);
    }

  return TRUE;
}

static void
gtk_text_handle_widget_style_updated (GtkWidget     *widget,
                                      GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv;

  priv = handle->priv;
  gtk_style_context_set_parent (gtk_widget_get_style_context (widget),
                                gtk_widget_get_style_context (priv->parent));

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
      GtkStyleContext *context;

      widget = gtk_event_box_new ();
      gtk_event_box_set_visible_window (GTK_EVENT_BOX (widget), TRUE);
      gtk_widget_set_events (widget,
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_ENTER_NOTIFY_MASK |
                             GDK_LEAVE_NOTIFY_MASK |
                             GDK_POINTER_MOTION_MASK);

      gtk_widget_set_direction (widget, priv->windows[pos].dir);

      g_signal_connect (widget, "draw",
                        G_CALLBACK (gtk_text_handle_widget_draw), handle);
      g_signal_connect (widget, "event",
                        G_CALLBACK (gtk_text_handle_widget_event), handle);
      g_signal_connect (widget, "style-updated",
                        G_CALLBACK (gtk_text_handle_widget_style_updated),
                        handle);

      priv->windows[pos].widget = g_object_ref_sink (widget);
      window = gtk_widget_get_ancestor (priv->parent, GTK_TYPE_WINDOW);
      _gtk_window_add_popover (GTK_WINDOW (window), widget, priv->parent, FALSE);

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_set_parent (context, gtk_widget_get_style_context (priv->parent));
      gtk_css_node_set_name (gtk_widget_get_css_node (widget), I_("cursor-handle"));
      if (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_END)
        {
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
          if (priv->mode == GTK_TEXT_HANDLE_MODE_CURSOR)
            gtk_style_context_add_class (context, GTK_STYLE_CLASS_INSERTION_CURSOR);
        }
      else
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);
    }

  return priv->windows[pos].widget;
}

static void
_handle_update_child_visible (GtkTextHandle         *handle,
                              GtkTextHandlePosition  pos)
{
  HandleWindow *handle_window;
  GtkTextHandlePrivate *priv;
  cairo_rectangle_int_t rect;
  GtkAllocation allocation;
  GtkWidget *parent;

  priv = handle->priv;
  handle_window = &priv->windows[pos];

  if (!priv->parent_scrollable)
    {
      gtk_widget_set_child_visible (handle_window->widget, TRUE);
      return;
    }

  parent = gtk_widget_get_parent (GTK_WIDGET (priv->parent_scrollable));
  rect = handle_window->pointing_to;

  gtk_widget_translate_coordinates (priv->parent, parent,
                                    rect.x, rect.y, &rect.x, &rect.y);

  gtk_widget_get_allocation (GTK_WIDGET (parent), &allocation);

  if (rect.x < 0 || rect.x + rect.width > allocation.width ||
      rect.y < 0 || rect.y + rect.height > allocation.height)
    gtk_widget_set_child_visible (handle_window->widget, FALSE);
  else
    gtk_widget_set_child_visible (handle_window->widget, TRUE);
}

static void
_gtk_text_handle_update (GtkTextHandle         *handle,
                         GtkTextHandlePosition  pos)
{
  GtkTextHandlePrivate *priv;
  HandleWindow *handle_window;
  GtkBorder *border;

  priv = handle->priv;
  handle_window = &priv->windows[pos];
  border = &handle_window->border;

  if (!priv->parent || !gtk_widget_is_drawable (priv->parent))
    return;

  if (handle_window->has_point &&
      handle_window->mode_visible && handle_window->user_visible)
    {
      cairo_rectangle_int_t rect;
      gint width, height;
      GtkWidget *window;
      GtkAllocation alloc;
      gint w, h;

      _gtk_text_handle_ensure_widget (handle, pos);
      _gtk_text_handle_get_size (handle, &width, &height);

      border->top = height;
      border->bottom = height;
      border->left = width;
      border->right = width;

      rect.x = handle_window->pointing_to.x;
      rect.y = handle_window->pointing_to.y + handle_window->pointing_to.height - handle_window->border.top;
      rect.width = width;
      rect.height = 0;

      _handle_update_child_visible (handle, pos);

      window = gtk_widget_get_parent (handle_window->widget);
      gtk_widget_translate_coordinates (priv->parent, window,
                                        rect.x, rect.y, &rect.x, &rect.y);

      if (pos == GTK_TEXT_HANDLE_POSITION_CURSOR &&
          priv->mode == GTK_TEXT_HANDLE_MODE_CURSOR)
        rect.x -= rect.width / 2;
      else if ((pos == GTK_TEXT_HANDLE_POSITION_CURSOR &&
                handle_window->dir == GTK_TEXT_DIR_RTL) ||
               (pos == GTK_TEXT_HANDLE_POSITION_SELECTION_START &&
                handle_window->dir != GTK_TEXT_DIR_RTL))
        rect.x -= rect.width;

      /* The goal is to make the window 3 times as wide and high. The handle
       * will be rendered in the center, making the rest an invisible border.
       * If we hit the edge of the toplevel, we shrink the border to avoid
       * mispositioning the handle, if at all possible. This calculation uses
       * knowledge about how popover_get_rect() works.
       */

      gtk_widget_get_allocation (window, &alloc);

      w = width + border->left + border->right;
      h = height + border->top + border->bottom;

      if (rect.x + rect.width/2 - w/2 < alloc.x)
        border->left = MAX (0, border->left - (alloc.x - (rect.x + rect.width/2 - w/2)));
      if (rect.y + rect.height/2 - h/2 < alloc.y)
        border->top = MAX (0, border->top - (alloc.y - (rect.y + rect.height/2 - h/2)));
      if (rect.x + rect.width/2 + w/2 > alloc.x + alloc.width)
        border->right = MAX (0, border->right - (rect.x + rect.width/2 + w/2 - (alloc.x + alloc.width)));
      if (rect.y + rect.height/2 + h/2 > alloc.y + alloc.height)
        border->bottom = MAX (0, border->bottom - (rect.y + rect.height/2 + h/2 - (alloc.y + alloc.height)));

      width += border->left + border->right;
      height += border->top + border->bottom;

      gtk_widget_set_size_request (handle_window->widget, width, height);
      gtk_widget_show (handle_window->widget);
      _gtk_window_raise_popover (GTK_WINDOW (window), handle_window->widget);
      _gtk_window_set_popover_position (GTK_WINDOW (window),
                                        handle_window->widget,
                                        GTK_POS_BOTTOM, &rect);
    }
  else if (handle_window->widget)
    gtk_widget_hide (handle_window->widget);
}

static void
adjustment_changed_cb (GtkAdjustment *adjustment,
                       GtkTextHandle *handle)
{
  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END);
}

static void
_gtk_text_handle_set_scrollable (GtkTextHandle *handle,
                                 GtkScrollable *scrollable)
{
  GtkTextHandlePrivate *priv;

  priv = handle->priv;

  if (priv->vadj)
    {
      g_signal_handlers_disconnect_by_data (priv->vadj, handle);
      g_clear_object (&priv->vadj);
    }

  if (priv->hadj)
    {
      g_signal_handlers_disconnect_by_data (priv->hadj, handle);
      g_clear_object (&priv->hadj);
    }

  if (priv->parent_scrollable)
    g_object_remove_weak_pointer (G_OBJECT (priv->parent_scrollable), (gpointer *) &priv->parent_scrollable);

  priv->parent_scrollable = scrollable;

  if (scrollable)
    {
      g_object_add_weak_pointer (G_OBJECT (priv->parent_scrollable), (gpointer *) &priv->parent_scrollable);

      priv->vadj = gtk_scrollable_get_vadjustment (scrollable);
      priv->hadj = gtk_scrollable_get_hadjustment (scrollable);

      if (priv->vadj)
        {
          g_object_ref (priv->vadj);
          g_signal_connect (priv->vadj, "changed",
                            G_CALLBACK (adjustment_changed_cb), handle);
          g_signal_connect (priv->vadj, "value-changed",
                            G_CALLBACK (adjustment_changed_cb), handle);
        }

      if (priv->hadj)
        {
          g_object_ref (priv->hadj);
          g_signal_connect (priv->hadj, "changed",
                            G_CALLBACK (adjustment_changed_cb), handle);
          g_signal_connect (priv->hadj, "value-changed",
                            G_CALLBACK (adjustment_changed_cb), handle);
        }
    }
}

static void
_gtk_text_handle_scrollable_notify (GObject       *object,
                                    GParamSpec    *pspec,
                                    GtkTextHandle *handle)
{
  if (pspec->value_type == GTK_TYPE_ADJUSTMENT)
    _gtk_text_handle_set_scrollable (handle, GTK_SCROLLABLE (object));
}

static void
_gtk_text_handle_update_scrollable (GtkTextHandle *handle,
                                    GtkScrollable *scrollable)
{
  GtkTextHandlePrivate *priv;

  priv = handle->priv;

  if (priv->parent_scrollable == scrollable)
    return;

  if (priv->parent_scrollable && priv->scrollable_notify_id &&
      g_signal_handler_is_connected (priv->parent_scrollable,
                                     priv->scrollable_notify_id))
    g_signal_handler_disconnect (priv->parent_scrollable,
                                 priv->scrollable_notify_id);

  _gtk_text_handle_set_scrollable (handle, scrollable);

  if (priv->parent_scrollable)
    priv->scrollable_notify_id =
      g_signal_connect (priv->parent_scrollable, "notify",
                        G_CALLBACK (_gtk_text_handle_scrollable_notify),
                        handle);
}

static GtkWidget *
gtk_text_handle_lookup_scrollable (GtkTextHandle *handle)
{
  GtkTextHandlePrivate *priv;
  GtkWidget *scrolled_window;

  priv = handle->priv;
  scrolled_window = gtk_widget_get_ancestor (priv->parent,
                                             GTK_TYPE_SCROLLED_WINDOW);
  if (!scrolled_window)
    return NULL;

  return gtk_bin_get_child (GTK_BIN (scrolled_window));
}

static void
_gtk_text_handle_parent_hierarchy_changed (GtkWidget     *widget,
                                           GtkWindow     *previous_toplevel,
                                           GtkTextHandle *handle)
{
  GtkWidget *toplevel, *scrollable;
  GtkTextHandlePrivate *priv;

  priv = handle->priv;
  toplevel = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);

  if (previous_toplevel && !toplevel)
    {
      if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget)
        {
          _gtk_window_remove_popover (GTK_WINDOW (previous_toplevel),
                                      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget);
          g_object_unref (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget);
          priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget = NULL;
        }

      if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget)
        {
          _gtk_window_remove_popover (GTK_WINDOW (previous_toplevel),
                                      priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget);
          g_object_unref (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget);
          priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget = NULL;
        }
    }

  scrollable = gtk_text_handle_lookup_scrollable (handle);
  _gtk_text_handle_update_scrollable (handle, GTK_SCROLLABLE (scrollable));
}

static void
_gtk_text_handle_set_parent (GtkTextHandle *handle,
                             GtkWidget     *parent)
{
  GtkTextHandlePrivate *priv;
  GtkWidget *scrollable = NULL;

  priv = handle->priv;

  if (priv->parent == parent)
    return;

  if (priv->parent && priv->hierarchy_changed_id &&
      g_signal_handler_is_connected (priv->parent, priv->hierarchy_changed_id))
    g_signal_handler_disconnect (priv->parent, priv->hierarchy_changed_id);

  priv->parent = parent;

  if (parent)
    {
      priv->hierarchy_changed_id =
        g_signal_connect (parent, "hierarchy-changed",
                          G_CALLBACK (_gtk_text_handle_parent_hierarchy_changed),
                          handle);

      scrollable = gtk_text_handle_lookup_scrollable (handle);
    }

  _gtk_text_handle_update_scrollable (handle, GTK_SCROLLABLE (scrollable));
}

static void
gtk_text_handle_finalize (GObject *object)
{
  GtkTextHandlePrivate *priv;

  priv = GTK_TEXT_HANDLE (object)->priv;

  _gtk_text_handle_set_parent (GTK_TEXT_HANDLE (object), NULL);

  /* We sank the references, unref here */
  if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget)
    g_object_unref (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START].widget);

  if (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget)
    g_object_unref (priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END].widget);

  G_OBJECT_CLASS (_gtk_text_handle_parent_class)->finalize (object);
}

static void
gtk_text_handle_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkTextHandle *handle;

  handle = GTK_TEXT_HANDLE (object);

  switch (prop_id)
    {
    case PROP_PARENT:
      _gtk_text_handle_set_parent (handle, g_value_get_object (value));
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
  signals[DRAG_STARTED] =
    g_signal_new (I_("drag-started"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_TEXT_HANDLE_POSITION);
  signals[DRAG_FINISHED] =
    g_signal_new (I_("drag-finished"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  NULL, NULL,
                  NULL,
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
  HandleWindow *start, *end;

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  priv = handle->priv;

  if (priv->mode == mode)
    return;

  priv->mode = mode;
  start = &priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_START];
  end = &priv->windows[GTK_TEXT_HANDLE_POSITION_SELECTION_END];

  switch (mode)
    {
    case GTK_TEXT_HANDLE_MODE_CURSOR:
      start->mode_visible = FALSE;
      /* end = cursor */
      end->mode_visible = TRUE;
      break;
    case GTK_TEXT_HANDLE_MODE_SELECTION:
      start->mode_visible = TRUE;
      end->mode_visible = TRUE;
      break;
    case GTK_TEXT_HANDLE_MODE_NONE:
    default:
      start->mode_visible = FALSE;
      end->mode_visible = FALSE;
      break;
    }

  if (end->widget)
    {
      if (mode == GTK_TEXT_HANDLE_MODE_CURSOR)
        gtk_style_context_add_class (gtk_widget_get_style_context (end->widget), GTK_STYLE_CLASS_INSERTION_CURSOR);
      else
        gtk_style_context_remove_class (gtk_widget_get_style_context (end->widget), GTK_STYLE_CLASS_INSERTION_CURSOR);
    }

  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  _gtk_text_handle_update (handle, GTK_TEXT_HANDLE_POSITION_SELECTION_END);

  if (start->widget && start->mode_visible)
    gtk_widget_queue_draw (start->widget);
  if (end->widget && end->mode_visible)
    gtk_widget_queue_draw (end->widget);
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

void
_gtk_text_handle_set_direction (GtkTextHandle         *handle,
                                GtkTextHandlePosition  pos,
                                GtkTextDirection       dir)
{
  GtkTextHandlePrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  priv = handle->priv;
  pos = CLAMP (pos, GTK_TEXT_HANDLE_POSITION_CURSOR,
               GTK_TEXT_HANDLE_POSITION_SELECTION_START);
  priv->windows[pos].dir = dir;

  if (priv->windows[pos].widget)
    {
      gtk_widget_set_direction (priv->windows[pos].widget, dir);
      _gtk_text_handle_update (handle, pos);
    }
}
