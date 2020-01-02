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

#include "gtkdnd.h"
#include "gtkdndprivate.h"
#include "gtkintl.h"
#include "gtknative.h"
#include "gtktypebuiltins.h"
#include "gtkeventcontroller.h"
#include "gtkmarshalers.h"


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

static void
gtk_drag_dest_site_destroy (gpointer data)
{
  GtkDragDestSite *site = data;

  g_clear_object (&site->dest);

  g_slice_free (GtkDragDestSite, site);
}

static void
gtk_drag_dest_set_internal (GtkWidget       *widget,
                            GtkDragDestSite *site)
{
  GtkDragDestSite *old_site;

  old_site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));
  if (old_site)
    {
      g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_realized, old_site);
      g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_hierarchy_changed, old_site);
      gtk_drop_target_set_track_motion (site->dest, gtk_drop_target_get_track_motion (old_site->dest));
    }

  if (gtk_widget_get_realized (widget))
    gtk_drag_dest_realized (widget);

  g_signal_connect (widget, "realize", G_CALLBACK (gtk_drag_dest_realized), site);
  g_signal_connect (widget, "notify::root", G_CALLBACK (gtk_drag_dest_hierarchy_changed), site);

  g_object_set_data_full (G_OBJECT (widget), I_("gtk-drag-dest"),
                          site, gtk_drag_dest_site_destroy);
}

static void
gtk_drag_dest_unset (GtkWidget *widget)
{
  GtkDragDestSite *old_site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));
  if (old_site)
    {
      g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_realized, old_site);
      g_signal_handlers_disconnect_by_func (widget, gtk_drag_dest_hierarchy_changed, old_site);
    }

  g_object_set_data (G_OBJECT (widget), I_("gtk-drag-dest"), NULL);
}


/**
 * SECTION:gtkdroptarget
 * @Short_description: An object to receive DND drops
 * @Title: GtkDropTarget
 *
 * GtkDropTarget is an auxiliary object that is used to receive
 * Drag-and-Drop operations.
 *
 * To use a GtkDropTarget to receive drops on a widget, you create
 * a GtkDropTarget object, connect to its signals, and then attach
 * it to the widget with gtk_drop_target_attach().
 */

struct _GtkDropTarget
{
  GObject parent_instance;

  GdkContentFormats *formats;
  GdkDragAction actions;
  GtkDestDefaults defaults;
  gboolean track_motion;

  GtkWidget *widget;
  GdkDrop *drop;
};

struct _GtkDropTargetClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FORMATS = 1,
  PROP_ACTIONS,
  PROP_DEFAULTS,
  PROP_TRACK_MOTION,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  DRAG_LEAVE,
  DRAG_MOTION,
  DRAG_DROP,
  DRAG_DATA_RECEIVED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

G_DEFINE_TYPE (GtkDropTarget, gtk_drop_target, G_TYPE_OBJECT);

static void
gtk_drop_target_init (GtkDropTarget *dest)
{
  dest->defaults = GTK_DEST_DEFAULT_ALL;
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

    case PROP_DEFAULTS:
      gtk_drop_target_set_defaults (dest, g_value_get_flags (value));
      break;

    case PROP_TRACK_MOTION:
      gtk_drop_target_set_track_motion (dest, g_value_get_boolean (value));
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

    case PROP_DEFAULTS:
      g_value_set_flags (value, gtk_drop_target_get_defaults (dest));
      break;

    case PROP_TRACK_MOTION:
      g_value_set_boolean (value, gtk_drop_target_get_track_motion (dest));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drop_target_class_init (GtkDropTargetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_drop_target_finalize;
  object_class->set_property = gtk_drop_target_set_property;
  object_class->get_property = gtk_drop_target_get_property;

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
   * GtkDropTargets:defaults:
   *
   * Flags that determine the default behavior 
   */
  properties[PROP_DEFAULTS] =
       g_param_spec_flags ("defaults", P_("Defaults"), P_("Defaults"),
                           GTK_TYPE_DEST_DEFAULTS, GTK_DEST_DEFAULT_ALL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDropTarget:track-motion:
   *
   * Whether the drop target should emit #GtkDropTarget::drag-motion signals
   * unconditionally
   */
  properties[PROP_TRACK_MOTION] =
       g_param_spec_boolean ("track-motion", P_("Track motion"), P_("Track motion"),
                             FALSE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkDropTarget::drag-leave:
   * @dest: the #GtkDropTarget
   *
   * The ::drag-leave signal is emitted on the drop site when the cursor
   * leaves the widget. A typical reason to connect to this signal is to
   * undo things done in #GtkDropTarget::drag-motion, e.g. undo highlighting.
   *
   * Likewise, the #GtkWidget::drag-leave signal is also emitted before the 
   * #GtkDropTarget::drag-drop signal, for instance to allow cleaning up of
   * a preview item created in the #GtkDropTarget::drag-motion signal handler.
   */
  signals[DRAG_LEAVE] =
      g_signal_new (I_("drag-leave"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

 /**
   * GtkWidget::drag-motion:
   * @dest: the #GtkDropTarget
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   *
   * The ::drag-motion signal is emitted on the drop site when the user
   * moves the cursor over the widget during a drag. The signal handler
   * must determine whether the cursor position is in a drop zone or not.
   * If it is not in a drop zone, it returns %FALSE and no further processing
   * is necessary. Otherwise, the handler returns %TRUE. In this case, the
   * handler is responsible for providing the necessary information for
   * displaying feedback to the user, by calling gdk_drag_status().
   *
   * If the decision whether the drop will be accepted or rejected can't be
   * made based solely on the cursor position and the type of the data, the
   * handler may inspect the dragged data by calling one of the #GdkDrop
   * read functions, and defer the gdk_drag_status() call to when it has
   * received the data.
   *
   * Note that you must pass #GTK_DEST_DEFAULT_MOTION to gtk_drop_target_attach()
   * when using the ::drag-motion signal that way.
   *
   * Also note that there is no drag-enter signal. The drag receiver has to
   * keep track of whether he has received any drag-motion signals since the
   * last #GtkWidget::drag-leave and if not, treat the drag-motion signal as
   * an "enter" signal. Upon an "enter", the handler will typically highlight
   * the drop site with gtk_drag_highlight().
   *
   * Returns: whether the cursor position is in a drop zone
   */
  signals[DRAG_MOTION] =
      g_signal_new (I_("drag-motion"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 2,
                    G_TYPE_INT, G_TYPE_INT);

  /**
   * GtkDropTarget::drag-drop:
   * @dest: the #GtkDropTarget
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
   * (e.g. to handle #GDK_ACTION_ASK by showing a #GtkPopover), you need to
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
                    G_TYPE_BOOLEAN, 2,
                    G_TYPE_INT, G_TYPE_INT);

  signals[DRAG_DATA_RECEIVED] =
      g_signal_new (I_("drag-data-received"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    _gtk_marshal_VOID__BOXED,
                    G_TYPE_NONE, 1,
                    GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals[DRAG_DATA_RECEIVED],
                              G_TYPE_FROM_CLASS (class),
                              _gtk_marshal_VOID__BOXEDv);
}

/**
 * gtk_drop_target_new:
 * @defaults: flags determining the default behaviour
 * @formats: (nullable): the supported data formats
 * @actions: the supported actions
 *
 * Creates a new #GtkDropTarget object
 *
 * Returns: the new #GtkDropTarget
 */
GtkDropTarget *
gtk_drop_target_new (GtkDestDefaults    defaults,
                     GdkContentFormats *formats,
                     GdkDragAction      actions)
{
  return g_object_new (GTK_TYPE_DROP_TARGET,
                       "defaults", defaults,
                       "formats", formats,
                       "actions", actions,
                       NULL);
}

/**
 * gtk_drop_target_set_formats:
 * @dest: a #GtkDropTarget
 * @formats: (nullable): the supported data formats
 *
 * Sets th data formats that this drop target will accept.
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
 * @dst: a #GtkDropTarget
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

/**
 * gtk_drop_target_set_defaults:
 * @dest: a #GtkDropTarget
 * @defaults: flags determining the default behaviour
 *
 * Sets the flags determining the behavior of the drop target.
 */
void
gtk_drop_target_set_defaults (GtkDropTarget   *dest,
                              GtkDestDefaults  defaults)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));
  
  if (dest->defaults == defaults)
    return;

  dest->defaults = defaults;

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_DEFAULTS]);
}

/**
 * gtk_drop_target_get_defaults:
 * @dest: a #GtkDropTarget
 *
 * Gets the flags determining the behavior of the drop target.
 *
 * Returns: flags determining the behaviour of the drop target
 */
GtkDestDefaults
gtk_drop_target_get_defaults (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), GTK_DEST_DEFAULT_ALL);

  return dest->defaults;
}

/**
 * gtk_drop_target_set_track_motion:
 * @dest: a #GtkDropTarget
 * @track_motion: whether to accept all targets
 *
 * Tells the drop target to emit #GtkDropTarget::drag-motion and
 * #GtkDropTarget::drag-leave events regardless of the targets and
 * the %GTK_DEST_DEFAULT_MOTION flag.
 *
 * This may be used when a drop target wants to do generic
 * actions regardless of the targets that the source offers.
 */
void
gtk_drop_target_set_track_motion (GtkDropTarget *dest,
                                  gboolean       track_motion)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  if (dest->track_motion == track_motion)
    return;

  dest->track_motion = track_motion;

  g_object_notify_by_pspec (G_OBJECT (dest), properties[PROP_TRACK_MOTION]);
}

/**
 * gtk_drop_target_get_track_motion:
 * @dest: a #GtkDropTarget
 *
 * Gets the value of the #GtkDropTarget::track-motion property.
 *
 * Returns: whether to accept all targets
 */
gboolean
gtk_drop_target_get_track_motion (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), FALSE);

  return dest->track_motion;
}

/**
 * gtk_drop_target_attach:
 * @dest: (transfer full): a #GtkDropTarget
 * @widget the widget to attach @dest to
 *
 * Attaches the @dest to widget and makes it accept drops
 * on the widgets.
 *
 * To undo the effect of this call, use gtk_drop_target_detach().
 */
void
gtk_drop_target_attach (GtkDropTarget *dest,
                        GtkWidget     *widget)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_DROP_TARGET (dest));
  g_return_if_fail (dest->widget == NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  dest->widget = widget;

  site = g_slice_new0 (GtkDragDestSite);

  site->dest = dest;
  site->have_drag = FALSE;

  gtk_drag_dest_set_internal (widget, site);
}

/**
 * gtk_drop_target_detach:
 * @dest: a #GtkDropTarget
 *
 * Undoes the effect of a prior gtk_drop_target_attach() call.
 */
void
gtk_drop_target_detach (GtkDropTarget *dest)
{
  g_return_if_fail (GTK_IS_DROP_TARGET (dest));

  if (dest->widget)
    {
      gtk_drag_dest_unset (dest->widget);
      dest->widget = NULL;
    }
}

/**
 * gtk_drop_target_get_target:
 * @dest: a #GtkDropTarget
 *
 * Gets the widget that the drop target is attached to.
 *
 * Returns: (nullable): get the widget that @dest is attached to
 */
GtkWidget *
gtk_drop_target_get_target (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);

  return dest->widget;
}

/**
 * gtk_drop_target_get_drop:
 * @dest: a #GtkDropTarget
 *
 * Returns the underlying #GtkDrop object for an ongoing drag.
 *
 * Returns: (nullable): the #GtkDrop of the current drag operation, or %NULL
 */
GdkDrop *
gtk_drop_target_get_drop (GtkDropTarget *dest)
{
  g_return_val_if_fail (GTK_IS_DROP_TARGET (dest), NULL);

  return dest->drop;
}

const char *
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
 * drag.
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

void
gtk_drop_target_emit_drag_leave (GtkDropTarget    *dest,
                                 GdkDrop          *drop,
                                 guint             time)
{
  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_LEAVE], 0, time);
  set_drop (dest, NULL);
}

gboolean
gtk_drop_target_emit_drag_motion (GtkDropTarget    *dest,
                                  GdkDrop          *drop,
                                  int               x,
                                  int               y)
{
  gboolean result = FALSE;

  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_MOTION], 0, x, y, &result);

  return result;
}

gboolean
gtk_drop_target_emit_drag_drop (GtkDropTarget    *dest,
                                GdkDrop          *drop,
                                int               x,
                                int               y)
{
  gboolean result = FALSE;

  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_DROP], 0, x, y, &result);

  return result;
}

void
gtk_drop_target_emit_drag_data_received (GtkDropTarget    *dest,
                                         GdkDrop          *drop,
                                         GtkSelectionData *sdata)
{
  set_drop (dest, drop);
  g_signal_emit (dest, signals[DRAG_DATA_RECEIVED], 0, sdata);
}

/**
 * gtk_drag_highlight: (method)
 * @widget: a widget to highlight
 *
 * Highlights a widget as a currently hovered drop target.
 * To end the highlight, call gtk_drag_unhighlight().
 *
 * GTK calls this automatically if %GTK_DEST_DEFAULT_HIGHLIGHT is set.
 */
void
gtk_drag_highlight (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
}

/**
 * gtk_drag_unhighlight: (method)
 * @widget: a widget to remove the highlight from
 *
 * Removes a highlight set by gtk_drag_highlight() from a widget.
 */
void
gtk_drag_unhighlight (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);
}
