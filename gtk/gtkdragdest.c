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

#include "gtkdragdest.h"
#include "gtkdragdestprivate.h"

#include "gtkintl.h"
#include "gtknative.h"
#include "gtktypebuiltins.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkmarshalers.h"
#include "gtkselectionprivate.h"


/**
 * SECTION:gtkdroptarget
 * @Short_description: Event controller to receive DND drops
 * @Title: GtkDropTarget
 *
 * GtkDropTarget is an auxiliary object that is used to receive
 * Drag-and-Drop operations.
 *
 * To use a GtkDropTarget to receive drops on a widget, you create
 * a GtkDropTarget object, configure which data formats and actions
 * you support, connect to its signals, and then attach
 * it to the widget with gtk_widget_add_controller().
 *
 * During a drag operation, the first signal that a GtkDropTarget
 * emits is #GtkDropTarget::accept, which is meant to determine
 * whether the target is a possible drop site for the ongoing drag.
 * The default handler for the ::accept signal accepts the drag
 * if it finds a compatible data format an an action that is supported
 * on both sides.
 *
 * If it is, and the widget becomes the current target, you will
 * receive a #GtkDropTarget::drag-enter signal, followed by
 * #GtkDropTarget::drag-motion signals as the pointer moves, and
 * finally either a #GtkDropTarget::drag-leave signal when the pointer
 * move off the widget, or a #GtkDropTarget::drag-drop signal when
 * a drop happens.
 *
 * The ::drag-enter and ::drag-motion handler can call gdk_drop_status()
 * to update the status of the ongoing operation. The ::drag-drop handler
 * should initiate the data transfer and finish the operation by calling
 * gdk_drop_finish().
 *
 * Between the ::drag-enter and ::drag-leave signals the widget is the
 * current drop target, and will receive the %GTK_STATE_FLAG_DROP_ACTIVE
 * state, which can be used to style the widget as a drop targett.
 */

struct _GtkDropTarget
{
  GtkEventController parent_object;

  GdkContentFormats *formats;
  GdkDragAction actions;

  GtkWidget *widget;
  GdkDrop *drop;
  gboolean contains;
  gboolean contains_pending;
};

struct _GtkDropTargetClass
{
  GtkEventControllerClass parent_class;

  gboolean (*accept ) (GtkDropTarget *dest,
                       GdkDrop       *drop);
};

enum {
  PROP_FORMATS = 1,
  PROP_ACTIONS,
  PROP_CONTAINS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  ACCEPT,
  DRAG_ENTER,
  DRAG_MOTION,
  DRAG_LEAVE,
  DRAG_DROP,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

static gboolean gtk_drop_target_accept (GtkDropTarget *dest,
                                        GdkDrop       *drop);

static gboolean gtk_drop_target_handle_event (GtkEventController *controller,
                                              const GdkEvent     *event);
static gboolean gtk_drop_target_filter_event (GtkEventController *controller,
                                              const GdkEvent     *event);
static void     gtk_drop_target_set_widget   (GtkEventController *controller,
                                              GtkWidget          *widget);
static void     gtk_drop_target_unset_widget (GtkEventController *controller);

static gboolean gtk_drop_target_get_contains (GtkDropTarget *dest);
static void     gtk_drop_target_set_contains (GtkDropTarget *dest,
                                              gboolean       contains);

typedef enum {
  GTK_DROP_STATUS_NONE,
  GTK_DROP_STATUS_ACCEPTED,
  GTK_DROP_STATUS_DENIED
} GtkDropStatus;

static GtkDropStatus gtk_drop_target_get_drop_status (GtkDropTarget *dest,
                                                      GdkDrop       *drop);
static void          gtk_drop_target_set_drop_status (GtkDropTarget *dest,
                                                      GdkDrop       *drop,
                                                      GtkDropStatus  status);

G_DEFINE_TYPE (GtkDropTarget, gtk_drop_target, GTK_TYPE_EVENT_CONTROLLER);

static void
gtk_drop_target_init (GtkDropTarget *dest)
{
}

static void
gtk_drop_target_finalize (GObject *object)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (object);

  g_clear_pointer (&dest->formats, gdk_content_formats_unref);

  G_OBJECT_CLASS (gtk_drop_target_parent_class)->finalize (object);
}

static void
gtk_drop_target_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (object);

  switch (prop_id)
    {
    case PROP_FORMATS:
      gtk_drop_target_set_formats (dest, g_value_get_boxed (value));
      break;

    case PROP_ACTIONS:
      gtk_drop_target_set_actions (dest, g_value_get_flags (value));
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
  GtkDropTarget *dest = GTK_DROP_TARGET (object);

  switch (prop_id)
    {
    case PROP_FORMATS:
      g_value_set_boxed (value, gtk_drop_target_get_formats (dest));
      break;

    case PROP_ACTIONS:
      g_value_set_flags (value, gtk_drop_target_get_actions (dest));
      break;

    case PROP_CONTAINS:
      g_value_set_boolean (value, gtk_drop_target_get_contains (dest));
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
  controller_class->set_widget = gtk_drop_target_set_widget;
  controller_class->unset_widget = gtk_drop_target_unset_widget;

  class->accept = gtk_drop_target_accept;

  /**
   * GtkDropTarget:formats:
   *
   * The #GdkContentFormats that determines the supported data formats
   */
  properties[PROP_FORMATS] =
       g_param_spec_boxed ("formats", P_("Formats"), P_("Formats"),
                           GDK_TYPE_CONTENT_FORMATS,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTarget:actions:
   *
   * The #GdkDragActions that this drop target supports
   */ 
  properties[PROP_ACTIONS] =
       g_param_spec_flags ("actions", P_("Actions"), P_("Actions"),
                           GDK_TYPE_DRAG_ACTION, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTarget:contains:
   *
   * Whether the drop target is currently the targed of an ongoing drag operation,
   * and highlighted.
   */
  properties[PROP_CONTAINS] =
       g_param_spec_boolean ("contains", P_("Contains an ongoing drag"), P_("Contains the current drag"),
                             FALSE,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkDropTarget::drag-enter:
   * @dest: the #GtkDropTarget
   * @drop: the #GdkDrop
   *
   * The ::drag-enter signal is emitted on the drop site when the cursor
   * enters the widget. It can be used to set up custom highlighting.
   */
  signals[DRAG_ENTER] =
      g_signal_new (I_("drag-enter"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 1,
                    GDK_TYPE_DROP);

  /**
   * GtkDropTarget::drag-leave:
   * @dest: the #GtkDropTarget
   * @drop: the #GdkDrop
   *
   * The ::drag-leave signal is emitted on the drop site when the cursor
   * leaves the widget. Its main purpose it to undo things done in
   * #GtkDropTarget::drag-enter.
   */
  signals[DRAG_LEAVE] =
      g_signal_new (I_("drag-leave"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 1,
                    GDK_TYPE_DROP);

  /**
   * GtkDropTarget::drag-motion:
   * @dest: the #GtkDropTarget
   * @drop: the #GdkDrop
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   *
   * The ::drag motion signal is emitted while the pointer is moving
   * over the drop target.
   */
  signals[DRAG_MOTION] =
      g_signal_new (I_("drag-motion"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 3,
                    GDK_TYPE_DROP, G_TYPE_INT, G_TYPE_INT);

 /**
   * GtkWidget::accept:
   * @dest: the #GtkDropTarget
   * @drop: the #GdkDrop
   *
   * The ::accept signal is emitted on the drop site when the user
   * moves the cursor over the widget during a drag. The signal handler
   * must determine whether the cursor position is in a drop zone or not.
   * If it is not in a drop zone, it returns %FALSE and no further processing
   * is necessary. Otherwise, the handler returns %TRUE. In this case, the
   * handler is responsible for providing the necessary information for
   * displaying feedback to the user, by calling gdk_drag_status().
   *
   * The default handler for this signal decides whether to accept the drop
   * based on the type of the data.
   *
   * If the decision whether the drop will be accepted or rejected can't be
   * made based solely the data format, handler may inspect the dragged data
   * by calling one of the #GdkDrop read functions and return %TRUE to
   * tentatively accept the drop. When the data arrives and is found to not be
   * acceptable, a call to gtk_drop_target_deny_drop() should be made to reject
   * the drop.
   *
   * Returns: whether the cursor position is in a drop zone
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
   * GtkDropTarget::drag-drop:
   * @dest: the #GtkDropTarget
   * @drop: the #GdkDrop
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   *
   * The ::drag-drop signal is emitted on the drop site when the user drops
   * the data onto the widget. The signal handler must determine whether
   * the cursor position is in a drop zone or not. If it is not in a drop
   * zone, it returns %FALSE and no further processing is necessary.
   *
   * Otherwise, the handler returns %TRUE. In this case, the handler must
   * ensure that gdk_drop_finish() is called to let the source know that
   * the drop is done. The call to gtk_drag_finish() can be done either
   * directly or after receiving the data.
   *
   * To receive the data, use one of the read functions provides by #GtkDrop
   * and #GtkDragDest: gdk_drop_read_async(), gdk_drop_read_value_async(),
   * gdk_drop_read_text_async(), gtk_drop_target_read_selection().
   *
   * You can use gtk_drop_target_get_drop() to obtain the #GtkDrop object
   * for the ongoing operation in your signal handler. If you call one of the
   * read functions in your handler, GTK will ensure that the #GtkDrop object
   * stays alive until the read is completed. If you delay obtaining the data
   * (e.g. to handle %GDK_ACTION_ASK by showing a #GtkPopover), you need to
   * hold a reference on the #GtkDrop.
   *
   * Returns: whether the cursor position is in a drop zone
   */
  signals[DRAG_DROP] =
      g_signal_new (I_("drag-drop"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 3,
                    GDK_TYPE_DROP, G_TYPE_INT, G_TYPE_INT);
}

/**
 * gtk_drop_target_new:
 * @formats: (nullable): the supported data formats
 * @actions: the supported actions
 *
 * Creates a new #GtkDropTarget object.
 *
 * Returns: the new #GtkDropTarget
 */
GtkDropTarget *
gtk_drop_target_new (GdkContentFormats *formats,
                     GdkDragAction      actions)
{
  return g_object_new (GTK_TYPE_DROP_TARGET,
                       "formats", formats,
                       "actions", actions,
                       NULL);
}

/**
 * gtk_drop_target_set_formats:
 * @dest: a #GtkDropTarget
 * @formats: (nullable): the supported data formats
 *
 * Sets the data formats that this drop target will accept.
 */
void
gtk_drop_target_set_formats (GtkDropTarget     *dest,
                             GdkContentFormats *formats)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  if (dest->formats == formats)
    return;

  if (dest->formats)
    gdk_content_formats_unref (dest->formats);

  dest->formats = formats;

  if (dest->formats)
    gdk_content_formats_ref (dest->formats);

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_FORMATS]);
}

/**
 * gtk_drop_target_get_formats:
 * @dest: a #GtkDropTarget
 *
 * Gets the data formats that this drop target accepts.
 *
 * Returns: the supported data formats
 */
GdkContentFormats *
gtk_drop_target_get_formats (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);
  
  return dest->formats;
}

/**
 * gtk_drop_target_set_actions:
 * @dest: a #GtkDropTarget
 * @actions: the supported actions
 *
 * Sets the actions that this drop target supports.
 */
void
gtk_drop_target_set_actions (GtkDropTarget *dest,
                             GdkDragAction  actions)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));
  
  if (dest->actions == actions)
    return;

  dest->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_ACTIONS]);
}

/**
 * gtk_drop_target_get_actions:
 * @dest: a #GtkDropTarget
 *
 * Gets the actions that this drop target supports.
 *
 * Returns: the actions that this drop target supports
 */
GdkDragAction
gtk_drop_target_get_actions (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), 0);

  return dest->actions;
}

static void
gtk_drag_dest_realized (GtkWidget *widget)
{
  GtkNative *native = gtk_widget_get_native (widget);

  gdk_surface_register_dnd (gtk_native_get_surface (native));
}

static void
gtk_drag_dest_hierarchy_changed (GtkWidget  *widget,
                                 GParamSpec *pspec,
                                 gpointer    data)
{
  GtkNative *native = gtk_widget_get_native (widget);

  if (native && gtk_widget_get_realized (GTK_WIDGET (native)))
    gdk_surface_register_dnd (gtk_native_get_surface (native));
}

/**
 * gtk_drop_target_get_drop:
 * @dest: a #GtkDropTarget
 *
 * Returns the underlying #GtkDrop object for an ongoing drag.
 *
 * Returns: (nullable) (transfer none): the #GtkDrop of the current drag operation, or %NULL
 */
GdkDrop *
gtk_drop_target_get_drop (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);

  return dest->drop;
}

static const char *
gtk_drop_target_match (GtkDropTarget *dest,
                       GdkDrop       *drop)
{
  GdkContentFormats *formats;
  const char *match;

  formats = gdk_content_formats_ref (dest->formats);
  formats = gdk_content_formats_union_deserialize_mime_types (formats);

  match = gdk_content_formats_match_mime_type (formats, gdk_drop_get_formats (drop));

  gdk_content_formats_unref (formats);

  return match;
}

/**
 * gtk_drop_target_find_mimetype:
 * @dest: a #GtkDropTarget
 *
 * Returns a mimetype that is supported both by @dest and the ongoing
 * drag. For more detailed control, you can use gdk_drop_get_formats()
 * to obtain the content formats that are supported by the source.
 *
 * Returns: (nullable): a matching mimetype for the ongoing drag, or %NULL
 */
const char *
gtk_drop_target_find_mimetype (GtkDropTarget *dest)
{
  if (!dest->drop)
    return NULL;

  return gtk_drop_target_match (dest, dest->drop);
}

static gboolean
gtk_drop_target_accept (GtkDropTarget *dest,
                        GdkDrop       *drop)
{
  if ((gdk_drop_get_actions (drop) & gtk_drop_target_get_actions (dest)) == 0)
    return FALSE;

  return gdk_content_formats_match (dest->formats, gdk_drop_get_formats (drop));
}

static void
set_drop (GtkDropTarget *dest,
          GdkDrop       *drop)
{
  if (dest->drop == drop)
    return;

  if (dest->drop)
    g_object_remove_weak_pointer (G_OBJECT (dest->drop), (gpointer *)&dest->drop);

  dest->drop = drop;

  if (dest->drop)
    g_object_add_weak_pointer (G_OBJECT (dest->drop), (gpointer *)&dest->drop);
}

static void
gtk_drop_target_emit_drag_enter (GtkDropTarget    *dest,
                                 GdkDrop          *drop)
{
  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_ENTER], 0, drop);
}

static void
gtk_drop_target_emit_drag_leave (GtkDropTarget    *dest,
                                 GdkDrop          *drop)
{
  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_LEAVE], 0, drop);
  set_drop (dest, NULL);
}

static gboolean
gtk_drop_target_emit_accept (GtkDropTarget *dest,
                             GdkDrop       *drop)
{
  gboolean result = FALSE;

  set_drop (dest, drop);
  g_signal_emit (dest, signals[ACCEPT], 0, drop, &result);

  return result;
}

static void
gtk_drop_target_emit_drag_motion (GtkDropTarget *dest,
                                  GdkDrop       *drop,
                                  int            x,
                                  int            y)
{
  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_MOTION], 0, drop, x, y);
}

static gboolean
gtk_drop_target_emit_drag_drop (GtkDropTarget    *dest,
                                GdkDrop          *drop,
                                int               x,
                                int               y)
{
  gboolean result = FALSE;

  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_DROP], 0, drop, x, y, &result);

  return result;
}

static void
gtk_drop_target_set_contains (GtkDropTarget *dest,
                              gboolean       contains)
{
  if (dest->contains == contains)
    return;

  dest->contains = contains;

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_CONTAINS]);
}

static gboolean
gtk_drop_target_get_contains (GtkDropTarget *dest)
{
  return dest->contains;
}

static gboolean
gtk_drop_target_filter_event (GtkEventController *controller,
                              const GdkEvent     *event)
{
  switch ((int)gdk_event_get_event_type (event))
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

static void
clear_current_dest (gpointer data, GObject *former_object)
{
  g_object_set_data (G_OBJECT (data), "current-dest", NULL);
}

static void
unset_current_dest (gpointer data)
{
  GtkDropTarget *dest = data;
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));

  gtk_drop_target_set_contains (dest, FALSE);
  if (widget)
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);
}

static GtkDropTarget *
gtk_drop_get_current_dest (GdkDrop *drop)
{
  return GTK_DROP_TARGET (g_object_get_data (G_OBJECT (drop), "current-dest"));
}

static void
gtk_drop_set_current_dest (GdkDrop       *drop,
                           GtkDropTarget *dest)
{
  GtkDropTarget *old_dest;
  GtkWidget *widget;

  old_dest = g_object_get_data (G_OBJECT (drop), "current-dest");

  if (old_dest == dest)
    return;

  if (old_dest)
    {
      gtk_drop_target_set_contains (old_dest, FALSE);

      widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (old_dest));
      if (widget)
        gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);

      gtk_drop_target_emit_drag_leave (old_dest, drop);

      g_object_weak_unref (G_OBJECT (old_dest), clear_current_dest, drop);
    }

  g_object_set_data_full (G_OBJECT (drop), "current-dest", dest, unset_current_dest);

  if (dest)
    {
      g_object_weak_ref (G_OBJECT (dest), clear_current_dest, drop);

      gtk_drop_target_emit_drag_enter (dest, drop);

      widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));
      if (widget)
        gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);

      gtk_drop_target_set_contains (dest, TRUE);
    }
}

static gboolean
gtk_drop_target_handle_event (GtkEventController *controller,
                              const GdkEvent     *event)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (controller);
  GdkDrop *drop;
  double x, y;
  GtkDropStatus status;
  gboolean found = FALSE;

  drop = gdk_event_get_drop (event);

  status = gtk_drop_target_get_drop_status (dest, drop);
  if (status == GTK_DROP_STATUS_DENIED)
    return FALSE;

  gdk_event_get_coords (event, &x, &y);

  switch ((int)gdk_event_get_event_type (event))
    {
    case GDK_DRAG_MOTION:
      if (status != GTK_DROP_STATUS_ACCEPTED)
        {
          found = gtk_drop_target_emit_accept (dest, drop);
          if (found)
            gtk_drop_target_set_drop_status (dest, drop, GTK_DROP_STATUS_ACCEPTED);
        }
      else
        found = TRUE;

      if (found)
        {
          gdk_drop_status (drop, gtk_drop_target_get_actions (dest));
          gtk_drop_set_current_dest (drop, dest);
          gtk_drop_target_emit_drag_motion (dest, drop, x, y);
        }
      break;

    case GDK_DROP_START:
      found = gtk_drop_target_emit_drag_drop (dest, drop, x, y);
      break;

    default:
      break;
    }

  return found;
}

/*
 * This function is called if none of the event
 * controllers has handled a drag event.
 */
void
gtk_drag_dest_handle_event (GtkWidget *toplevel,
                            GdkEvent  *event)
{
  GdkDrop *drop;
  GdkEventType event_type;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  event_type = gdk_event_get_event_type (event);
  drop = gdk_event_get_drop (event);

  switch ((guint) event_type)
    {
    case GDK_DRAG_ENTER:
      break;

    case GDK_DRAG_LEAVE:
      gtk_drop_set_current_dest (drop, NULL);
      break;

    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      gtk_drop_set_current_dest (drop, NULL);
      gdk_drop_status (drop, 0);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_drop_target_set_widget (GtkEventController *controller,
                            GtkWidget          *widget)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (controller);

  GTK_EVENT_CONTROLLER_CLASS (gtk_drop_target_parent_class)->set_widget (controller, widget);

  if (gtk_widget_get_realized (widget))
    gtk_drag_dest_realized (widget);

  g_signal_connect (widget, "realize", G_CALLBACK (gtk_drag_dest_realized), dest);
  g_signal_connect (widget, "notify::root", G_CALLBACK (gtk_drag_dest_hierarchy_changed), dest);
}

static void
gtk_drop_target_unset_widget (GtkEventController *controller)
{
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (controller);

  g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_realized, controller);
  g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_hierarchy_changed, controller);

  GTK_EVENT_CONTROLLER_CLASS (gtk_drop_target_parent_class)->unset_widget (controller);
}

static void
gtk_drag_get_data_got_data (GObject      *source,
                            GAsyncResult *result,
                            gpointer      data)
{
  GTask *task = data;
  gssize written;
  GError *error = NULL;
  guchar *bytes;
  gsize size;
  GtkSelectionData *sdata;
  GtkDropTarget *dest;
  GdkDrop *drop;
  GdkDisplay *display;

  written = g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, &error);
  if (written < 0)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  bytes = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (source));
  size = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (source));

  dest = GTK_DROP_TARGET (g_task_get_source_object (task));
  drop = GDK_DROP (g_object_get_data (G_OBJECT (task), "drop"));
  display = GDK_DISPLAY (g_object_get_data (G_OBJECT (task), "display"));

  sdata = g_slice_new0 (GtkSelectionData);
  sdata->target = g_task_get_task_data (task);
  sdata->type = g_task_get_task_data (task);
  sdata->format = 8;
  sdata->length = size;
  sdata->data = bytes ? bytes : (guchar *)g_strdup ("");
  sdata->display = display;

  set_drop (dest, drop);
  g_task_return_pointer (task, sdata, NULL);
  set_drop (dest, NULL);

  g_object_unref (task);
}

static void
gtk_drag_get_data_got_stream (GObject      *source,
                              GAsyncResult *result,
                              gpointer      data)
{
  GTask *task = data;
  GInputStream *input_stream;
  GOutputStream *output_stream;
  GError *error = NULL;
  const char *mime_type;

  input_stream = gdk_drop_read_finish (GDK_DROP (source), result, &mime_type, &error);
  if (input_stream == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_task_set_task_data (task, (gpointer)g_intern_string (mime_type), NULL);

  output_stream = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (output_stream,
                                input_stream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                gtk_drag_get_data_got_data,
                                task);
  g_object_unref (output_stream);
  g_object_unref (input_stream);
}

/**
 * gtk_drop_target_read_selection:
 * @dest: a #GtkDropTarget
 * @target: the data format to read
 * @cancellable: (nullable): a cancellable
 * @callback: callback to call on completion
 * @user_data: data to pass to @callback
 *
 * Asynchronously reads the dropped data from an ongoing
 * drag on a #GtkDropTarget, and returns the data in a 
 * #GtkSelectionData object.
 *
 * This function is meant for cases where a #GtkSelectionData
 * object is needed, such as when using the #GtkTreeModel DND
 * support. In most other cases, the #GdkDrop async read
 * APIs that return in input stream or #GValue are more
 * convenient and should be preferred.
 */
void
gtk_drop_target_read_selection (GtkDropTarget       *dest,
                                GdkAtom              target,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GTask *task;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  task = g_task_new (dest, NULL, callback, user_data);
  g_object_set_data_full (G_OBJECT (task), "drop", g_object_ref (dest->drop), g_object_unref);

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));
  if (widget)
    g_object_set_data (G_OBJECT (task), "display", gtk_widget_get_display (widget));

  gdk_drop_read_async (dest->drop,
                       (const char *[2]) { target, NULL },
                       G_PRIORITY_DEFAULT,
                       NULL,
                       gtk_drag_get_data_got_stream,
                       task);
}

/**
 * gtk_drop_target_read_selection_finish:
 * @dest: a #GtkDropTarget
 * @result: a #GAsyncResult
 * @error: (allow-none): location to store error information on failure, or %NULL
 *
 * Finishes an async drop read operation, see gtk_drop_target_read_selection().
 *
 * Returns: (nullable) (transfer full): the #GtkSelectionData, or %NULL
 */
GtkSelectionData *
gtk_drop_target_read_selection_finish (GtkDropTarget  *dest,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static GtkDropStatus
gtk_drop_target_get_drop_status (GtkDropTarget *dest,
                                 GdkDrop       *drop)
{
  GHashTable *denied;

  denied = (GHashTable *)g_object_get_data (G_OBJECT (drop), "denied-drags");
  if (denied)
    return GPOINTER_TO_INT (g_hash_table_lookup (denied, dest));

  return GTK_DROP_STATUS_NONE;
}

static void
gtk_drop_target_set_drop_status (GtkDropTarget *dest,
                                 GdkDrop       *drop,
                                 GtkDropStatus  status)
{
  GHashTable *drags;

  drags = (GHashTable *)g_object_get_data (G_OBJECT (drop), "denied-drags");
  if (!drags)
    {
      drags = g_hash_table_new (NULL, NULL);
      g_object_set_data_full (G_OBJECT (drop), "denied-drags", drags, (GDestroyNotify)g_hash_table_unref);
    }

  g_hash_table_insert (drags, dest, GINT_TO_POINTER (status));

  if (dest == gtk_drop_get_current_dest (drop))
    {
      gdk_drop_status (drop, 0);
      gtk_drop_set_current_dest (drop, NULL);
    }
}

/**
 * gtk_drop_target_deny_drop:
 * @dest: a #GtkDropTarget
 * @drop: the #GdkDrop of an ongoing drag operation
 *
 * Sets the @drop as not accepted on this drag site.
 *
 * This function should be used when delaying the decision
 * on whether to accept a drag or not until after reading
 * the data.
 */
void
gtk_drop_target_deny_drop (GtkDropTarget *dest,
                           GdkDrop       *drop)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));
  g_return_if_fail (GDK_IS_DROP (drop));

  gtk_drop_target_set_drop_status (dest, drop, GTK_DROP_STATUS_DENIED);
}
