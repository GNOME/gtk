/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkdroptarget.h"

#include "gtkdropprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtknative.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"


/**
 * SECTION:gtkdroptarget
 * @Short_description: Event controller to receive DND drops
 * @Title: GtkDropTarget
 * @See_also: #GdkDrop, #GtkDropTargetAsync
 *
 * GtkDropTarget is an event controller implementing a simple way to
 * receive Drag-and-Drop operations.
 *
 * The most basic way to use a #GtkDropTarget to receive drops on a
 * widget is to create it via gtk_drop_target_new() passing in the
 * #GType of the data you want to receive and connect to the
 * GtkDropTarget::drop signal to receive the data.
 *
 * #GtkDropTarget supports more options, such as:
 *
 *  * rejecting potential drops via the #GtkDropTarget::accept signal
 *    and the gtk_drop_target_reject() function to let other drop
 *    targets handle the drop
 *  * tracking an ongoing drag operation before the drop via the
 *    #GtkDropTarget::enter, #GtkDropTarget::motion and
 *    #GtkDropTarget::leave signals
 *  * configuring how to receive data by setting the
 *    #GtkDropTarget:preload property and listening for its availability
 *    via the #GtkDropTarget:value property
 *
 * However, #GtkDropTarget is ultimately modeled in a synchronous way
 * and only supports data transferred via #GType.  
 * If you want full control over an ongoing drop, the #GtkDropTargetAsync
 * object gives you this ability.
 *
 * While a pointer is dragged over the drop target's widget and the drop
 * has not been rejected, that widget will receive the
 * %GTK_STATE_FLAG_DROP_ACTIVE state, which can be used to style the widget.
 */

struct _GtkDropTarget
{
  GtkEventController parent_object;

  GdkContentFormats *formats;
  GdkDragAction actions;
  guint preload : 1;

  guint dropping : 1;
  graphene_point_t coords;
  GdkDrop *drop;
  GCancellable *cancellable; /* NULL unless doing a read of value */
  GValue value;
};

struct _GtkDropTargetClass
{
  GtkEventControllerClass parent_class;

  gboolean              (* accept)                              (GtkDropTarget  *self,
                                                                 GdkDrop        *drop);
  GdkDragAction         (* enter)                               (GtkDropTarget  *self,
                                                                 double          x,
                                                                 double          y);
  GdkDragAction         (* motion)                              (GtkDropTarget  *self,
                                                                 double          x,
                                                                 double          y);
  void                  (* leave)                               (GtkDropTarget  *self,
                                                                 GdkDrop        *drop);
  gboolean              (* drop)                                (GtkDropTarget  *self,
                                                                 const GValue   *value,
                                                                 double          x,
                                                                 double          y);
};

enum {
  PROP_0,
  PROP_ACTIONS,
  PROP_DROP,
  PROP_FORMATS,
  PROP_PRELOAD,
  PROP_VALUE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  ACCEPT,
  ENTER,
  MOTION,
  LEAVE,
  DROP,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

G_DEFINE_TYPE (GtkDropTarget, gtk_drop_target, GTK_TYPE_EVENT_CONTROLLER);

static void
gtk_drop_target_end_drop (GtkDropTarget *self)
{
  if (self->drop == NULL)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (self->dropping)
    {
      gdk_drop_finish (self->drop, 0);
      self->dropping = FALSE;
    }

  g_clear_object (&self->drop);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DROP]);

  if (G_IS_VALUE (&self->value))
    {
      g_value_unset (&self->value);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
    }

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  gtk_widget_unset_state_flags (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self)),
                                GTK_STATE_FLAG_DROP_ACTIVE);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
gtk_drop_target_do_drop (GtkDropTarget *self)
{
  gboolean success;

  g_assert (self->dropping);
  g_assert (G_IS_VALUE (&self->value));

  g_signal_emit (self, signals[DROP], 0, &self->value, self->coords.x, self->coords.y, &success);

  if (success)
    gdk_drop_finish (self->drop, gdk_drop_get_actions (self->drop));
  else
    gdk_drop_finish (self->drop, 0);

  self->dropping = FALSE;

  gtk_drop_target_end_drop (self);
}

static void
gtk_drop_target_load_done (GObject      *source,
                           GAsyncResult *res,
                           gpointer      data)
{
  GtkDropTarget *self = data;
  const GValue *value;
  GError *error = NULL;

  value = gdk_drop_read_value_finish (GDK_DROP (source), res, &error);
  if (value == NULL)
    {
      /* If this happens, data/self is invalid */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_clear_error (&error);
          return;
        }

      g_clear_object (&self->cancellable);
      /* XXX: Should this be a warning? */
      g_warning ("Failed to receive drop data: %s", error->message);
      g_clear_error (&error);
      gtk_drop_target_end_drop (self);
      return;
    }

  g_clear_object (&self->cancellable);
  g_value_init (&self->value, G_VALUE_TYPE (value));
  g_value_copy (value, &self->value);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);

  if (self->dropping)
    gtk_drop_target_do_drop (self);
}

static gboolean
gtk_drop_target_load_local (GtkDropTarget *self,
                            GType          type)
{
  GdkDrag *drag;

  drag = gdk_drop_get_drag (self->drop);
  if (drag == NULL)
    return FALSE;

  g_value_init (&self->value, type);
  if (gdk_content_provider_get_value (gdk_drag_get_content (drag),
                                      &self->value,
                                      NULL))
    return TRUE;

  g_value_unset (&self->value);
  return FALSE;
}

static gboolean
gtk_drop_target_load (GtkDropTarget *self)
{
  GType type;

  g_assert (self->drop);

  if (G_IS_VALUE (&self->value))
    return TRUE;

  if (self->cancellable)
    return FALSE;

  type = gdk_content_formats_match_gtype (self->formats, gdk_drop_get_formats (self->drop));

  if (gtk_drop_target_load_local (self, type))
    return TRUE;

  self->cancellable = g_cancellable_new ();

  gdk_drop_read_value_async (self->drop, 
                             type,
                             G_PRIORITY_DEFAULT,
                             self->cancellable,
                             gtk_drop_target_load_done,
                             g_object_ref (self));
  return FALSE;
}

static void
gtk_drop_target_start_drop (GtkDropTarget *self,
                            GdkDrop       *drop)
{
  g_object_freeze_notify (G_OBJECT (self));

  gtk_drop_target_end_drop (self);

  self->drop = g_object_ref (drop);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DROP]);

  if (self->preload)
    gtk_drop_target_load (self);

  gtk_widget_set_state_flags (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self)),
                              GTK_STATE_FLAG_DROP_ACTIVE,
                              FALSE);

  g_object_thaw_notify (G_OBJECT (self));
}

static gboolean
gtk_drop_target_accept (GtkDropTarget *self,
                        GdkDrop       *drop)
{
  if ((gdk_drop_get_actions (drop) & gtk_drop_target_get_actions (self)) == 0)
    return FALSE;

  if (self->formats == NULL)
    return TRUE;

  return gdk_content_formats_match (self->formats, gdk_drop_get_formats (drop));
}

static GdkDragAction
make_action_unique (GdkDragAction actions)
{
  if (actions & GDK_ACTION_COPY)
    return GDK_ACTION_COPY;

  if (actions & GDK_ACTION_MOVE)
    return GDK_ACTION_MOVE;
  
  if (actions & GDK_ACTION_LINK)
    return GDK_ACTION_LINK;

  return 0;
}
  
static GdkDragAction
gtk_drop_target_enter (GtkDropTarget  *self,
                       double          x,
                       double          y)
{
  return make_action_unique (self->actions & gdk_drop_get_actions (self->drop));
}

static GdkDragAction         
gtk_drop_target_motion (GtkDropTarget  *self,
                        double          x,
                        double          y)
{
  return make_action_unique (self->actions & gdk_drop_get_actions (self->drop));
}

static gboolean
gtk_drop_target_drop (GtkDropTarget  *self,
                      const GValue   *value,
                      double          x,
                      double          y)
{
  return FALSE;
}

static gboolean
gtk_drop_target_filter_event (GtkEventController *controller,
                              GdkEvent           *event)
{
  switch ((int) gdk_event_get_event_type (event))
    {
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      return GTK_EVENT_CONTROLLER_CLASS (gtk_drop_target_parent_class)->filter_event (controller, event);

    default:;
    }

  return TRUE;
}

static gboolean
gtk_drop_target_handle_event (GtkEventController *controller,
                              GdkEvent           *event,
                              double              x,
                              double              y)
{
  GtkDropTarget *self = GTK_DROP_TARGET (controller);

  /* All drops have been rejected. New drops only arrive via crossing
   * events, so we can: */
  if (self->drop == NULL)
    return FALSE;

  switch ((int) gdk_event_get_event_type (event))
    {
    case GDK_DRAG_MOTION:
      {
        GtkWidget *widget = gtk_event_controller_get_widget (controller);
        GdkDragAction preferred;

        /* sanity check */
        g_return_val_if_fail (self->drop == gdk_dnd_event_get_drop (event), FALSE);

        graphene_point_init (&self->coords, x, y);
        g_signal_emit (self, signals[MOTION], 0, x, y, &preferred);
        if (preferred &&
            gtk_drop_status (self->drop, self->actions, preferred))
          {
            gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
          }
        else
          {
            gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);
          }
      }
      return FALSE;

    case GDK_DROP_START:
      {
        /* sanity check */
        g_return_val_if_fail (self->drop == gdk_dnd_event_get_drop (event), FALSE);

        graphene_point_init (&self->coords, x, y);
        self->dropping = TRUE;
        if (gtk_drop_target_load (self))
          gtk_drop_target_do_drop (self);

        return TRUE;
      }

    default:
      return FALSE;
    }
}

static void
gtk_drop_target_handle_crossing (GtkEventController    *controller,
                                 const GtkCrossingData *crossing,
                                 double                 x,
                                 double                 y)
{
  GtkDropTarget *self = GTK_DROP_TARGET (controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);

  if (crossing->type != GTK_CROSSING_DROP)
    return;

  /* sanity check */
  g_warn_if_fail (self->drop == NULL || self->drop == crossing->drop);

  if (crossing->direction == GTK_CROSSING_IN)
    {
      gboolean accept = FALSE;
      GdkDragAction preferred;

      if (self->drop != NULL)
        return;

      /* if we were a target already but self->drop == NULL, the drop
       * was rejected already */
      if (crossing->old_descendent != NULL ||
          crossing->old_target == widget)
        return;

      g_signal_emit (self, signals[ACCEPT], 0, crossing->drop, &accept);
      if (!accept)
        return;

      graphene_point_init (&self->coords, x, y);
      gtk_drop_target_start_drop (self, crossing->drop);

      g_signal_emit (self, signals[ENTER], 0, x, y, &preferred);
      if (preferred &&
          gtk_drop_status (self->drop, self->actions, preferred))
        {
          gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
        }
      else
        {
          gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);
        }
    }
  else
    {
      if (crossing->new_descendent != NULL ||
          crossing->new_target == widget)
        return;

      g_signal_emit (self, signals[LEAVE], 0, self->drop);
      if (!self->dropping)
        gtk_drop_target_end_drop (self);
      gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);
    }
}

static void
gtk_drop_target_finalize (GObject *object)
{
  GtkDropTarget *self = GTK_DROP_TARGET (object);

  g_clear_pointer (&self->formats, gdk_content_formats_unref);

  G_OBJECT_CLASS (gtk_drop_target_parent_class)->finalize (object);
}

static void
gtk_drop_target_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkDropTarget *self = GTK_DROP_TARGET (object);

  switch (prop_id)
    {
    case PROP_ACTIONS:
      gtk_drop_target_set_actions (self, g_value_get_flags (value));
      break;

    case PROP_PRELOAD:
      gtk_drop_target_set_preload (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drop_target_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkDropTarget *self = GTK_DROP_TARGET (object);

  switch (prop_id)
    {
    case PROP_ACTIONS:
      g_value_set_flags (value, self->actions);
      break;

    case PROP_DROP:
      g_value_set_object (value, self->drop);
      break;

    case PROP_FORMATS:
      g_value_set_boxed (value, self->formats);
      break;

    case PROP_PRELOAD:
      g_value_set_boolean (value, self->preload);
      break;

    case PROP_VALUE:
      if (G_IS_VALUE (&self->value))
        g_value_set_boxed (value, &self->value);
      else
        g_value_set_boxed (value, NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drop_target_class_init (GtkDropTargetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (class);

  object_class->finalize = gtk_drop_target_finalize;
  object_class->set_property = gtk_drop_target_set_property;
  object_class->get_property = gtk_drop_target_get_property;

  controller_class->handle_event = gtk_drop_target_handle_event;
  controller_class->filter_event = gtk_drop_target_filter_event;
  controller_class->handle_crossing = gtk_drop_target_handle_crossing;

  class->accept = gtk_drop_target_accept;
  class->enter = gtk_drop_target_enter;
  class->motion = gtk_drop_target_motion;
  class->drop = gtk_drop_target_drop;

  /**
   * GtkDropTarget:actions:
   *
   * The #GdkDragActions that this drop target supports
   */ 
  properties[PROP_ACTIONS] =
       g_param_spec_flags ("actions",
                           P_("Actions"),
                           P_("The actions supported by this drop target"),
                           GDK_TYPE_DRAG_ACTION, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTarget:drop:
   *
   * The #GdkDrop that is currently being performed
   */
  properties[PROP_DROP] =
       g_param_spec_object ("drop",
                            P_("Drop"),
                            P_("Current drop"),
                            GDK_TYPE_DROP,
                            GTK_PARAM_READABLE);

  /**
   * GtkDropTarget:formats:
   *
   * The #GdkContentFormats that determine the supported data formats
   */
  properties[PROP_FORMATS] =
       g_param_spec_boxed ("formats",
                           P_("Formats"),
                           P_("The supported formats"),
                           GDK_TYPE_CONTENT_FORMATS,
                           GTK_PARAM_READABLE);

  /**
   * GtkDropTarget:preload:
   *
   * Whether the drop data should be preloaded when the pointer is only
   * hovering over the widget but has not been released.
   *
   * Setting this property allows finer grained reaction to an ongoing
   * drop at the cost of loading more data.
   *
   * The default value for this property is %FALSE to avoid downloading
   * huge amounts of data by accident.  
   * For example, if somebody drags a full document of gigabytes of text
   * from a text editor across a widget with a preloading drop target,
   * this data will be downloaded, even if the data is ultimately dropped
   * elsewhere.
   *
   * For a lot of data formats, the amount of data is very small (like
   * %GDK_TYPE_RGBA), so enabling this property does not hurt at all.  
   * And for local-only drag'n'drop operations, no data transfer is done,
   * so enabling it there is free.
   */
  properties[PROP_PRELOAD] =
       g_param_spec_boolean ("preload",
                             P_("Preload"),
                             P_("Whether drop data should be preloaded while hovering"),
                             FALSE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTarget:value:
   *
   * The value for this drop operation or %NULL if the data has not been
   * loaded yet or no drop operation is going on.
   *
   * Data may be available before the GtkDropTarget::drop signal gets emitted -
   * for example when the GtkDropTarget:preload property is set.
   * You can use the GObject::notify signal to be notified of available data.
   */
  properties[PROP_VALUE] =
       g_param_spec_boxed ("value",
                           P_("Value"),
                           P_("The value for this drop operation"),
                           G_TYPE_VALUE,
                           GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

 /**
   * GtkDropTarget::accept:
   * @self: the #GtkDropTarget
   * @drop: the #GdkDrop
   *
   * The ::accept signal is emitted on the drop site when a drop operation
   * is about to begin.  
   * If the drop is not accepted, %FALSE will be returned and the drop target
   * will ignore the drop. If %TRUE is returned, the drop is accepted for now
   * but may be rejected later via a call to gtk_drop_target_reject() or
   * ultimately by returning %FALSE from GtkDropTarget::drop
   *
   * The default handler for this signal decides whether to accept the drop
   * based on the formats provided by the @drop.
   *
   * If the decision whether the drop will be accepted or rejected needs
   * inspecting the data, this function should return %TRUE, the 
   * GtkDropTarget:preload property should be set and the value
   * should be inspected via the GObject::notify:value signal and then call
   * gtk_drop_target_reject().
   *
   * Returns: %TRUE if @drop is accepted
   */
  signals[ACCEPT] =
      g_signal_new (I_("accept"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkDropTargetClass, accept),
                    g_signal_accumulator_first_wins, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 1,
                    GDK_TYPE_DROP);

  /**
   * GtkDropTarget::enter:
   * @self: the #GtkDropTarget
   * @x: the x coordinate of the current pointer position
   * @y: the y coordinate of the current pointer position
   *
   * The ::enter signal is emitted on the drop site when the pointer
   * enters the widget. It can be used to set up custom highlighting.
   *
   * Returns: Preferred action for this drag operation or 0 if dropping is not
   *     supported at the current @x,@y location.
   */
  signals[ENTER] =
      g_signal_new (I_("enter"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkDropTargetClass, enter),
                    g_signal_accumulator_first_wins, NULL,
                    NULL,
                    GDK_TYPE_DRAG_ACTION, 2,
                    G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  /**
   * GtkDropTarget::motion:
   * @self: the #GtkDropTarget
   * @x: the x coordinate of the current pointer position
   * @y: the y coordinate of the current pointer position
   *
   * The ::motion signal is emitted while the pointer is moving
   * over the drop target.
   *
   * Returns: Preferred action for this drag operation or 0 if dropping is not
   *     supported at the current @x,@y location.
   */
  signals[MOTION] =
      g_signal_new (I_("motion"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkDropTargetClass, motion),
                    g_signal_accumulator_first_wins, NULL,
                    NULL,
                    GDK_TYPE_DRAG_ACTION, 2,
                    G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  /**
   * GtkDropTarget::leave:
   * @self: the #GtkDropTarget
   * @drop: the #GdkDrop
   *
   * The ::leave signal is emitted on the drop site when the pointer
   * leaves the widget. Its main purpose it to undo things done in
   * #GtkDropTarget::enter.
   */
  signals[LEAVE] =
      g_signal_new (I_("leave"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkDropTargetClass, leave),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

  /**
   * GtkDropTarget::drop:
   * @self: the #GtkDropTarget
   * @value: the #GValue being dropped
   * @x: the x coordinate of the current pointer position
   * @y: the y coordinate of the current pointer position
   *
   * The ::drop signal is emitted on the drop site when the user drops
   * the data onto the widget. The signal handler must determine whether
   * the pointer position is in a drop zone or not. If it is not in a drop
   * zone, it returns %FALSE and no further processing is necessary.
   *
   * Otherwise, the handler returns %TRUE. In this case, this handler will
   * accept the drop. The handler is responsible for rading the given @value
   * and performing the drop operation.
   *
   * Returns: whether the drop was accepted at the given pointer position
   */
  signals[DROP] =
      g_signal_new (I_("drop"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    g_signal_accumulator_first_wins, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 3,
                    G_TYPE_VALUE, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}

static void
gtk_drop_target_init (GtkDropTarget *self)
{
  self->formats = gdk_content_formats_new (NULL, 0);
}

/**
 * gtk_drop_target_new:
 * @type: The supported type or %G_TYPE_INVALID
 * @actions: the supported actions
 *
 * Creates a new #GtkDropTarget object.
 *
 * If the drop target should support more than 1 type, pass
 * %G_TYPE_INVALID for @type and then call
 * gtk_drop_target_set_gtypes().
 *
 * Returns: the new #GtkDropTarget
 */
GtkDropTarget *
gtk_drop_target_new (GType         type,
                     GdkDragAction actions)
{
  GtkDropTarget *result;

  result = g_object_new (GTK_TYPE_DROP_TARGET,
                         "actions", actions,
                         NULL);

  if (type != G_TYPE_INVALID)
    gtk_drop_target_set_gtypes (result, &type, 1);

  return result;
}

/**
 * gtk_drop_target_get_formats:
 * @self: a #GtkDropTarget
 *
 * Gets the data formats that this drop target accepts.
 *
 * If the result is %NULL, all formats are expected to be supported.
 *
 * Returns: (nullable): the supported data formats
 */
GdkContentFormats *
gtk_drop_target_get_formats (GtkDropTarget *self)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (self), NULL);
  
  return self->formats;
}

/**
 * gtk_drop_target_set_gtypes:
 * @self: a #GtkDropTarget
 * @types: (nullable) (transfer none) (array length=n_types):
 *     all supported #GTypes that can be dropped
 * @n_types: number of @types
 *
 * Sets the supported #GTypes for this drop target.
 *
 * The GtkDropTarget::drop signal will 
 **/
void
gtk_drop_target_set_gtypes (GtkDropTarget *self,
                            GType         *types,
                            gsize          n_types)
{
  GdkContentFormatsBuilder *builder;
  gsize i;

  g_return_if_fail (GTK_IS_DROP_TARGET (self));
  g_return_if_fail (n_types == 0 || types != NULL);

  gdk_content_formats_unref (self->formats);

  builder = gdk_content_formats_builder_new ();
  for (i = 0; i < n_types; i++)
    gdk_content_formats_builder_add_gtype (builder, types[i]);
  
  self->formats = gdk_content_formats_builder_free_to_formats (builder);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FORMATS]);
}

/**
 * gtk_drop_target_get_gtypes:
 * @self: a #GtkDropTarget
 * @n_types: (out) (optional): optional pointer to take the
 *     number of #GTypes contained in the return value
 *
 * Gets the list of supported #GTypes for @self. If no type have been set,
 * %NULL will be returned.
 *
 * Returns: (transfer none) (nullable) (array length=n_types):
 *      %G_TYPE_INVALID-terminated array of types included in @formats or
 *      %NULL if none.
 **/
const GType *
gtk_drop_target_get_gtypes (GtkDropTarget *self,
                            gsize         *n_types)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (self), NULL);

  return gdk_content_formats_get_gtypes (self->formats, n_types);
}

/**
 * gtk_drop_target_set_actions:
 * @self: a #GtkDropTarget
 * @actions: the supported actions
 *
 * Sets the actions that this drop target supports.
 */
void
gtk_drop_target_set_actions (GtkDropTarget *self,
                             GdkDragAction  actions)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (self));
  
  if (self->actions == actions)
    return;

  self->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIONS]);
}

/**
 * gtk_drop_target_get_actions:
 * @self: a #GtkDropTarget
 *
 * Gets the actions that this drop target supports.
 *
 * Returns: the actions that this drop target supports
 */
GdkDragAction
gtk_drop_target_get_actions (GtkDropTarget *self)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (self), 0);

  return self->actions;
}

/**
 * gtk_drop_target_set_preload:
 * @self: a #GtkDropTarget
 * @preload: %TRUE to preload drop data
 *
 * Sets the GtkDropTarget:preload property.
 **/
void
gtk_drop_target_set_preload (GtkDropTarget *self,
                             gboolean       preload)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (self));
  
  if (self->preload == preload)
    return;

  self->preload = preload;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRELOAD]);
}

/**
 * gtk_drop_target_get_preload:
 * @self: a #GtkDropTarget
 *
 * Gets the value of the GtkDropTarget:preload property.
 *
 * Returns: %TRUE if drop data should be preloaded
 */
gboolean
gtk_drop_target_get_preload (GtkDropTarget *self)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (self), 0);

  return self->preload;
}

/**
 * gtk_drop_target_get_drop:
 * @self: a #GtkDropTarget
 *
 * Gets the currently handled drop operation.
 *
 * If no drop operation is going on, %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The current drop
 **/
GdkDrop *
gtk_drop_target_get_drop (GtkDropTarget *self)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (self), NULL);

  return self->drop;
}

/**
 * gtk_drop_target_get_value:
 * @self: a #GtkDropTarget
 *
 * Gets the value of the GtkDropTarget:value porperty.
 *
 * Returns: (nullable) (transfer none): The current drop data
 **/
const GValue *
gtk_drop_target_get_value (GtkDropTarget *self)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (self), NULL);

  if (!G_IS_VALUE (&self->value))
    return NULL;

  return &self->value;
}

/**
 * gtk_drop_target_reject:
 * @self: a #GtkDropTarget
 *
 * Rejects the ongoing drop operation.
 *
 * If no drop operation is ongoing - when GdkDropTarget:drop
 * returns %NULL - this function does nothing.
 *
 * This function should be used when delaying the decision
 * on whether to accept a drag or not until after reading
 * the data.
 */
void
gtk_drop_target_reject (GtkDropTarget *self)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (self));

  if (self->drop == NULL)
    return;

  gtk_drop_target_end_drop (self);
}

