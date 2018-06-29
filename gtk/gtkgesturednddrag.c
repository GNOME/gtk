/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkgesturednddrag.h"

#include "gtkdnd.h"
#include "gtkgesturesingleprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkgesturednddrag
 * @Short_description: DND drag gesture
 * @Title: GtkGestureDndDrag
 * @See_also: #GtkGestureDndDrop, #GdkDrag
 *
 * #GtkGestureDndDrag is a #GtkGesture implementation that implements the
 * drag part of drag-n-drop operations. It recognizes events that start a drag
 * and then manages the appropriate objects that provide the drag-and-drop
 * to other widgets and applications.
 *
 * #GtkGestureDndDrag can be used in two ways. Either properties can be set
 * to be used automatically to perform the drag-and-drop. Alternatively,
 * an ongoing drag-and-drop operation can be tracked through
 * the #GtkGestureDndDrag::prepare-content, #GtkGestureDndDrag::drag-begin,
 * #GtkGestureDndDrag::drag-cancel and #GtkGestureDndDrag::drag-finish signals and
 * properties can be set directly on the associated #GdkDrag object.
 */

struct _GtkGestureDndDrag
{
  GtkGestureSingle parent_instance;

  gdouble start_x;
  gdouble start_y;
  GdkDrag *drag;
  GdkDragAction actions;
  GdkContentProvider *content;
};

struct _GtkGestureDndDragClass
{
  GtkGestureSingleClass parent_class;

  void                  (* drag_begin)                          (GtkGestureDndDrag      *self);
  void                  (* drag_cancel)                         (GtkGestureDndDrag      *self);
  void                  (* drag_finish)                         (GtkGestureDndDrag      *self,
                                                                 GdkDragAction           action);
};

enum {
  PROP_0,
  PROP_ACTIONS,
  PROP_CONTENT,
  N_PROPERTIES
};

enum {
  BEGIN,
  DRAG_BEGIN,
  DRAG_CANCEL,
  DRAG_FINISH,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkGestureDndDrag, gtk_gesture_dnd_drag, GTK_TYPE_GESTURE_SINGLE)

static gboolean
gtk_gesture_dnd_drag_is_dragging (GtkGestureDndDrag *self)
{
  return self->drag != NULL;
}

static void
gtk_gesture_dnd_drag_begin (GtkGesture       *gesture,
                            GdkEventSequence *sequence)
{
  GtkGestureDndDrag *self = GTK_GESTURE_DND_DRAG (gesture);

  if (gtk_gesture_dnd_drag_is_dragging (self))
    return;

  gtk_gesture_get_point (gesture, sequence, &self->start_x, &self->start_y);
}

static gboolean
gtk_gesture_dnd_drag_should_start_drag (GtkGestureDndDrag *self,
                                        double             start_x,
                                        double             start_y,
                                        double             cur_x,
                                        double             cur_y)
{
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
  
  return gtk_drag_check_threshold (widget, start_x, start_y, cur_x, cur_y);
}

static void
gtk_gesture_dnd_drag_update (GtkGesture       *gesture,
                             GdkEventSequence *sequence)
{
  GtkGestureDndDrag *self = GTK_GESTURE_DND_DRAG (gesture);
  gdouble cur_x, cur_y;

  if (gtk_gesture_dnd_drag_is_dragging (self))
    return;

  gtk_gesture_get_point (gesture, sequence, &cur_x, &cur_y);

  if (gtk_gesture_dnd_drag_should_start_drag (self,
                                              self->start_x, self->start_y,
                                              cur_x, cur_y))
    {
      GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
      GdkDevice *device = gtk_gesture_get_device (gesture);

      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (self));

      self->drag = gdk_drag_begin (gtk_widget_get_surface (widget),
                                   device,
                                   self->content,
                                   self->actions,
                                   cur_x - self->start_x,
                                   cur_y - self->start_y);
    }
}

static gboolean
gtk_gesture_dnd_drag_filter_event (GtkEventController *controller,
                                   const GdkEvent     *event)
{
  /* Let touchpad swipe events go through, only if they match n-points  */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE)
    {
      guint n_points;
      guint n_fingers;

      g_object_get (G_OBJECT (controller), "n-points", &n_points, NULL);
      gdk_event_get_touchpad_gesture_n_fingers (event, &n_fingers);

      if (n_fingers == n_points)
        return FALSE;
      else
        return TRUE;
    }

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_dnd_drag_parent_class)->filter_event (controller, event);
}

static void
gtk_gesture_dnd_drag_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkGestureDndDrag *self = GTK_GESTURE_DND_DRAG (object);

  switch (prop_id)
    {
    case PROP_ACTIONS:
      g_value_set_flags (value, self->actions);
      break;

    case PROP_CONTENT:
      g_value_set_object (value, self->content);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_dnd_drag_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkGestureDndDrag *self = GTK_GESTURE_DND_DRAG (object);

  switch (prop_id)
    {
    case PROP_ACTIONS:
      gtk_gesture_dnd_drag_set_actions (self, g_value_get_flags (value));
      break;

    case PROP_CONTENT:
      gtk_gesture_dnd_drag_set_content (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_dnd_drag_dispose (GObject *object)
{
  GtkGestureDndDrag *self = GTK_GESTURE_DND_DRAG (object);

  g_clear_object (&self->drag);
  g_clear_object (&self->content);

  return G_OBJECT_CLASS (gtk_gesture_dnd_drag_parent_class)->dispose (object);
}

static void
gtk_gesture_dnd_drag_class_init (GtkGestureDndDragClass *klass)
{
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);
  GtkEventControllerClass *event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gesture_class->begin = gtk_gesture_dnd_drag_begin;
  gesture_class->update = gtk_gesture_dnd_drag_update;

  event_controller_class->filter_event = gtk_gesture_dnd_drag_filter_event;

  gobject_class->set_property = gtk_gesture_dnd_drag_set_property;
  gobject_class->get_property = gtk_gesture_dnd_drag_get_property;
  gobject_class->dispose = gtk_gesture_dnd_drag_dispose;

  /**
   * GtkGestureDndDrag:actions:
   *
   * The actions supported by this gesture.
   */
  properties[PROP_ACTIONS] =
    g_param_spec_flags ("actions",
                        P_("Actions"),
                        P_("The supported actions"),
                        GDK_TYPE_DRAG_ACTION,
                        GDK_ACTION_COPY,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGestureDndDrag:content:
   *
   * The content to be dragged.
   */
  properties[PROP_CONTENT] =
    g_param_spec_object ("content",
                         P_("Content"),
                         P_("The content to be dragged"),
                         GDK_TYPE_CONTENT_PROVIDER,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);

  /**
   * GtkGestureDndDrag::drag-begin:
   * @self: the object which received the signal
   *
   * This signal is emitted whenever a drag has started.
   */
  signals[DRAG_BEGIN] =
    g_signal_new (I_("drag-begin"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureDndDragClass, drag_begin),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * GtkGestureDndDrag::drag-cancel:
   * @self: the object which received the signal
   *
   * This signal is emitted whenever a drag-and-drop operation was
   * cancelled.
   * Either this signal or GtkGestureDndDrag::drag-finish are emitted
   * right before @self ends its drag operation.
   */
  signals[DRAG_CANCEL] =
    g_signal_new (I_("drag-cancel"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureDndDragClass, drag_cancel),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkGestureDndDrag::drag-finish:
   * @self: the object which received the signal
   * @action: The unique action that the drag operation was performed in.
   *
   * This signal is emitted whenever a drag-and-drop operation was
   * successfully completed.
   * Either this signal or GtkGestureDndDrag::drag-cancel are emitted
   * right before @self ends its drag operation.
   */
  signals[DRAG_FINISH] =
    g_signal_new (I_("drag-finish"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureDndDragClass, drag_finish),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_DRAG_ACTION);
}

static void
gtk_gesture_dnd_drag_init (GtkGestureDndDrag *self)
{
  self->actions = GDK_ACTION_COPY;
}

/**
 * gtk_gesture_dnd_drag_new:
 *
 * Returns a newly created #GtkGesture that manages drags for drag-and-drop
 * operations.
 *
 * Returns: a newly created #GtkGestureDndDrag
 **/
GtkGesture *
gtk_gesture_dnd_drag_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_DND_DRAG,
                       NULL);
}

GdkDragAction
gtk_gesture_dnd_drag_get_actions (GtkGestureDndDrag *self)
{
  g_return_val_if_fail (GTK_IS_GESTURE_DND_DRAG (self), 0);

  return self->actions;
}

void
gtk_gesture_dnd_drag_set_actions (GtkGestureDndDrag *self,
                                  GdkDragAction      actions)
{
  g_return_if_fail (GTK_IS_GESTURE_DND_DRAG (self));

  if (self->actions == actions)
    return;

  self->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIONS]);
}

GdkContentProvider *
gtk_gesture_dnd_drag_get_content (GtkGestureDndDrag *self)
{
  g_return_val_if_fail (GTK_IS_GESTURE_DND_DRAG (self), NULL);

  return self->content;
}

void
gtk_gesture_dnd_drag_set_content (GtkGestureDndDrag  *self,
                                  GdkContentProvider *content)
{
  g_return_if_fail (GTK_IS_GESTURE_DND_DRAG (self));
  g_return_if_fail (content == NULL || GDK_IS_CONTENT_PROVIDER (content));

  if (!g_set_object (&self->content, content))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTENT]);
}

/**
 * gtk_gesture_dnd_drag_get_start_point:
 * @self: a #GtkGestureDndDrag
 * @x: (out) (nullable): X coordinate for the drag start point
 * @y: (out) (nullable): Y coordinate for the drag start point
 *
 * If the @gesture is currently performing a drag, this function returns
 * %TRUE and fills in @x and @y with the coordinates the drag started from.
 *
 * Returns: %TRUE if the gesture is performing a drag
 **/
gboolean
gtk_gesture_dnd_drag_get_start_point (GtkGestureDndDrag *self,
                                      gdouble           *x,
                                      gdouble           *y)
{
  double result_x, result_y;
  gboolean result;

  g_return_val_if_fail (GTK_IS_GESTURE_DND_DRAG (self), FALSE);

  if (!gtk_gesture_dnd_drag_is_dragging (self))
    {
      result = FALSE;
      result_x = 0;
      result_y = 0;
    }
  else
    {
      result = TRUE;
      result_x = self->start_x;
      result_y = self->start_y;
    }

  if (x)
    *x = result_x;
  if (y)
    *y = result_y;

  return result;
}

/**
 * gtk_gesture_dnd_drag_get_drag:
 * @self: a #GtkGestureDndDrag
 *
 * Returns the #GdkDrag if a drag is currently being performed. Otherwise,
 * %NULL is returned.
 *
 * Returns: (transfer none) (nullable): The current drag
 **/
GdkDrag *
gtk_gesture_dnd_drag_get_drag (GtkGestureDndDrag *self)
{
  g_return_val_if_fail (GTK_IS_GESTURE_DND_DRAG (self), NULL);

  return self->drag;
}
