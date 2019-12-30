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

#include "gtkdragsource.h"

#include "gtkdnd.h"
#include "gtkdndprivate.h"
#include "gtkgesturedrag.h"
#include "gtkgesturesingleprivate.h"
#include "gtkimagedefinitionprivate.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtkstylecontext.h"
#include "gtkimageprivate.h"
#include "gtkdragiconprivate.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"


typedef struct _GtkDragSourceSite GtkDragSourceSite;

struct _GtkDragSourceSite 
{
  GdkModifierType    start_button_mask;
  GdkContentFormats *target_list;        /* Targets for drag data */
  GdkDragAction      actions;            /* Possible actions */

  GtkImageDefinition *image_def;
  GtkGesture        *drag_gesture;
};
  
static void
gtk_drag_source_gesture_begin (GtkGesture       *gesture,
                               GdkEventSequence *sequence,
                               gpointer          data)
{
  GtkDragSourceSite *site = data;
  guint button;

  if (gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture)))
    button = 1;
  else
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  g_assert (button >= 1);

  if (!site->start_button_mask ||
      !(site->start_button_mask & (GDK_BUTTON1_MASK << (button - 1))))
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
gtk_drag_source_gesture_update (GtkGesture       *gesture,
                                GdkEventSequence *sequence,
                                gpointer          data)
{
  gdouble start_x, start_y, offset_x, offset_y;
  GtkDragSourceSite *site = data;
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (gtk_gesture_is_recognized (site->drag_gesture))
    {
      gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (site->drag_gesture),
                                        &start_x, &start_y);
      gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (site->drag_gesture),
                                   &offset_x, &offset_y);

      if (gtk_drag_check_threshold (widget, start_x, start_y,
                                    start_x + offset_x, start_y + offset_y))
        {
          GdkDevice *device = gtk_gesture_get_device (site->drag_gesture);

          gtk_event_controller_reset (GTK_EVENT_CONTROLLER (site->drag_gesture));

          gtk_drag_begin_internal (widget,
                                   device,
                                   site->image_def, site->target_list,
                                   site->actions,
                                   start_x, start_y);
        }
    }
}

static void
gtk_drag_source_site_destroy (gpointer data)
{
  GtkDragSourceSite *site = data;

  if (site->target_list)
    gdk_content_formats_unref (site->target_list);

  gtk_image_definition_unref (site->image_def);
  /* This gets called only during widget finalization.
   * And widget finalization takes care of gestures. */
  g_slice_free (GtkDragSourceSite, site);
}

/**
 * gtk_drag_source_set: (method)
 * @widget: a #GtkWidget
 * @start_button_mask: the bitmask of buttons that can start the drag
 * @formats: (allow-none): the formats that the drag will support,
 *     may be %NULL
 * @actions: the bitmask of possible actions for a drag from this widget
 *
 * Sets up a widget so that GTK+ will start a drag operation when the user
 * clicks and drags on the widget. The widget must have a window.
 */
void
gtk_drag_source_set (GtkWidget         *widget,
                     GdkModifierType    start_button_mask,
                     GdkContentFormats *targets,
                     GdkDragAction      actions)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  if (site)
    {
      if (site->target_list)
        gdk_content_formats_unref (site->target_list);
    }
  else
    {
      site = g_slice_new0 (GtkDragSourceSite);
      site->image_def = gtk_image_definition_new_empty ();
      site->drag_gesture = gtk_gesture_drag_new ();
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (site->drag_gesture),
                                                  GTK_PHASE_CAPTURE);
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (site->drag_gesture), 0);
      g_signal_connect (site->drag_gesture, "begin",
                        G_CALLBACK (gtk_drag_source_gesture_begin),
                        site);
      g_signal_connect (site->drag_gesture, "update",
                        G_CALLBACK (gtk_drag_source_gesture_update),
                        site);
      gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (site->drag_gesture));

      g_object_set_data_full (G_OBJECT (widget),
                              I_("gtk-site-data"), 
                              site, gtk_drag_source_site_destroy);
    }

  site->start_button_mask = start_button_mask;

  if (targets)
    site->target_list = gdk_content_formats_ref (targets);
  else
    site->target_list = NULL;

  site->actions = actions;
}

/**
 * gtk_drag_source_unset: (method)
 * @widget: a #GtkWidget
 *
 * Undoes the effects of gtk_drag_source_set().
 */
void 
gtk_drag_source_unset (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set_data (G_OBJECT (widget), I_("gtk-site-data"), NULL);
}

/**
 * gtk_drag_source_get_target_list: (method)
 * @widget: a #GtkWidget
 *
 * Gets the list of targets this widget can provide for
 * drag-and-drop.
 *
 * Returns: (nullable) (transfer none): the #GdkContentFormats, or %NULL if none
 */
GdkContentFormats *
gtk_drag_source_get_target_list (GtkWidget *widget)
{
  GtkDragSourceSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  return site ? site->target_list : NULL;
}

/**
 * gtk_drag_source_set_target_list: (method)
 * @widget: a #GtkWidget that’s a drag source
 * @target_list: (allow-none): list of draggable targets, or %NULL for none
 *
 * Changes the target types that this widget offers for drag-and-drop.
 * The widget must first be made into a drag source with
 * gtk_drag_source_set().
 */
void
gtk_drag_source_set_target_list (GtkWidget         *widget,
                                 GdkContentFormats *target_list)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  if (site == NULL)
    {
      g_warning ("gtk_drag_source_set_target_list() requires the widget "
                 "to already be a drag source.");
      return;
    }

  if (target_list)
    gdk_content_formats_ref (target_list);

  if (site->target_list)
    gdk_content_formats_unref (site->target_list);

  site->target_list = target_list;
}

/**
 * gtk_drag_source_add_text_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the text targets supported by #GtkSelectionData to
 * the target list of the drag source.  The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_content_formats_add_text_targets() and
 * gtk_drag_source_set_target_list().
 */
void
gtk_drag_source_add_text_targets (GtkWidget *widget)
{
  GdkContentFormats *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gdk_content_formats_ref (target_list);
  else
    target_list = gdk_content_formats_new (NULL, 0);
  target_list = gtk_content_formats_add_text_targets (target_list);
  gtk_drag_source_set_target_list (widget, target_list);
  gdk_content_formats_unref (target_list);
}

/**
 * gtk_drag_source_add_image_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the writable image targets supported by #GtkSelectionData to
 * the target list of the drag source. The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_image_targets() and
 * gtk_drag_source_set_target_list().
 */
void
gtk_drag_source_add_image_targets (GtkWidget *widget)
{
  GdkContentFormats *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gdk_content_formats_ref (target_list);
  else
    target_list = gdk_content_formats_new (NULL, 0);
  target_list = gtk_content_formats_add_image_targets (target_list, TRUE);
  gtk_drag_source_set_target_list (widget, target_list);
  gdk_content_formats_unref (target_list);
}

/**
 * gtk_drag_source_add_uri_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the URI targets supported by #GtkSelectionData to
 * the target list of the drag source.  The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_content_formats_add_uri_targets() and
 * gtk_drag_source_set_target_list().
 */
void
gtk_drag_source_add_uri_targets (GtkWidget *widget)
{
  GdkContentFormats *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gdk_content_formats_ref (target_list);
  else
    target_list = gdk_content_formats_new (NULL, 0);
  target_list = gtk_content_formats_add_uri_targets (target_list);
  gtk_drag_source_set_target_list (widget, target_list);
  gdk_content_formats_unref (target_list);
}

/**
 * gtk_drag_source_set_icon_name: (method)
 * @widget: a #GtkWidget
 * @icon_name: name of icon to use
 *
 * Sets the icon that will be used for drags from a particular source
 * to a themed icon. See the docs for #GtkIconTheme for more details.
 */
void
gtk_drag_source_set_icon_name (GtkWidget   *widget,
                               const gchar *icon_name)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (icon_name != NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  gtk_image_definition_unref (site->image_def);
  site->image_def = gtk_image_definition_new_icon_name (icon_name);
}

/**
 * gtk_drag_source_set_icon_gicon: (method)
 * @widget: a #GtkWidget
 * @icon: A #GIcon
 * 
 * Sets the icon that will be used for drags from a particular source
 * to @icon. See the docs for #GtkIconTheme for more details.
 */
void
gtk_drag_source_set_icon_gicon (GtkWidget *widget,
                                GIcon     *icon)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (icon != NULL);
  
  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  gtk_image_definition_unref (site->image_def);
  site->image_def = gtk_image_definition_new_gicon (icon);
}

/**
 * gtk_drag_source_set_icon_paintable: (method)
 * @widget: a #GtkWidget
 * @paintable: A #GdkPaintable
 *
 * Sets the icon that will be used for drags from a particular source
 * to @paintable.
 */
void
gtk_drag_source_set_icon_paintable (GtkWidget    *widget,
                                    GdkPaintable *paintable)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  gtk_image_definition_unref (site->image_def);
  site->image_def = gtk_image_definition_new_paintable (paintable);
}


/**
 * SECTION:gtkdragsource
 * @Short_description: An object to initiate DND operations
 * @Title: GtkDragSource
 *
 * GtkDragSource is an auxiliary object that is used to initiate
 * Drag-And-Drop operations. It can be set up with the necessary
 * ingredients for a DND operation ahead of time. This includes
 * the source for the data that is being transferred, in the form
 * of a #GdkContentProvider, the desired action, and the icon to
 * use during the drag operation.
 *
 * GtkDragSource can be used in two ways:
 * - for static drag-source configuration
 * - for one-off drag operations
 *
 * To configure a widget as a permanent source for DND operations,
 * set up the GtkDragSource, then call gtk_drag_source_attach().
 * This sets up a drag gesture on the widget that will trigger
 * DND actions.
 *
 * To initiate a on-off drag operation, set up the GtkDragSource,
 * then call gtk_drag_source_drag_begin(). GTK keeps a reference
 * on the drag source until the DND operation is done, so you
 * can unref the source after calling drag_being().
 *
 * During the DND operation, GtkDragSource emits signals that
 * can be used to obtain updates about the status of the operation,
 * but it is not normally necessary to connect to any signals,
 * except for one case: when the supported actions include
 * %GDK_DRAG_MOVE, you need to listen for the
 * #GtkDragSource::drag-data-deleted signal and delete the
 * drag data after it has been transferred.
 */

struct _GtkDragSource
{
  GObject parent_instance;

  GdkContentProvider *content;
  GdkDragAction actions;

  GtkWidget *icon_window;
  GdkPaintable *paintable;
  int hot_x;
  int hot_y;

  GtkGesture *gesture;
  int start_button_mask;

  GdkDrag *drag;
  GtkWidget *widget;
};

struct _GtkDragSourceClass
{
  GObjectClass parent_class;
};

enum {
  PROP_CONTENT = 1,
  PROP_ACTIONS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  DRAG_BEGIN,
  DRAG_END,
  DRAG_FAILED,
  DRAG_DATA_DELETE,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

G_DEFINE_TYPE (GtkDragSource, gtk_drag_source, G_TYPE_OBJECT);

static void
gtk_drag_source_init (GtkDragSource *source)
{
}

static void
gtk_drag_source_finalize (GObject *object)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);

  gtk_drag_source_detach (source);

  g_clear_object (&source->content);
  g_clear_object (&source->paintable);
  g_clear_object (&source->icon_window);

  G_OBJECT_CLASS (gtk_drag_source_parent_class)->finalize (object);
}

static void
gtk_drag_source_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);
  
  switch (prop_id)
    {
    case PROP_CONTENT:
      gtk_drag_source_set_content (source, g_value_get_object (value));
      break;

    case PROP_ACTIONS:
      gtk_drag_source_set_actions (source, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drag_source_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_set_object (value, gtk_drag_source_get_content (source));
      break;

    case PROP_ACTIONS:
      g_value_set_flags (value, gtk_drag_source_get_actions (source));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drag_source_class_init (GtkDragSourceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_drag_source_finalize;
  object_class->set_property = gtk_drag_source_set_property;
  object_class->get_property = gtk_drag_source_get_property;

  /**
   * GtkDragSource:content:
   *
   * The data that is offered by drag operations from this source,
   * in the form of a #GdkContentProvider.
   */ 
  properties[PROP_CONTENT] =
       g_param_spec_object ("content",
                            P_("Content"),
                            P_("The content provider for the dragged data"),
                           GDK_TYPE_CONTENT_PROVIDER,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDragSource:actions:
   *
   * The actions that are supported by drag operations from the source.
   *
   * Note that you must handle the #GtkDragSource::drag-data-deleted signal
   * if the actions include %GDK_ACTION_MOVE.
   */ 
  properties[PROP_ACTIONS] =
       g_param_spec_flags ("actions",
                           P_("Actions"),
                           P_("Supported actions"),
                           GDK_TYPE_DRAG_ACTION, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkDragSource::drag-begin:
   * @source: the #GtkDragSource
   *
   * The ::drag-begin signal is emitted on the drag source when a drag
   * is started. It can be used to e.g. set a custom drag icon with
   * gtk_drag_source_set_icon(). But all of the setup can also be
   * done before calling gtk_drag_source_drag_begin(), so this is not
   * really necessary.
   */
  signals[DRAG_BEGIN] =
      g_signal_new (I_("drag-begin"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

  /**
   * GtkDragSource::drag-end:
   * @source: the #GtkDragSource
   *
   * The ::drag-end signal is emitted on the drag source when a drag is
   * finished. A typical reason to connect to this signal is to undo
   * things done in #GtkDragSource::drag-begin.
   */ 
  signals[DRAG_END] =
      g_signal_new (I_("drag-end"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

  /**
   * GtkDragSource::drag-failed:
   * @source: the #GtkDragSource
   * @reason: information on why the drag failed
   *
   * The ::drag-failed signal is emitted on the drag source when a drag has
   * failed. The signal handler may handle a failed drag operation based on
   * the type of error. It should return %TRUE if the failure has been handled
   * and the default "drag operation failed" should not be shown.
   *
   * Returns: %TRUE if the failed drag operation has been already handled
   */
  signals[DRAG_FAILED] =
      g_signal_new (I_("drag-failed"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    _gtk_boolean_handled_accumulator, NULL,
                    _gtk_marshal_BOOLEAN__ENUM,
                    G_TYPE_BOOLEAN, 1,
                    GDK_TYPE_DRAG_CANCEL_REASON);

  /**
   * GtkDragSource::drag-data-delete:
   * @source: the #GtkDragSource
   *
   * The ::drag-data-delete signal is emitted on the drag source when a drag
   * with the action %GDK_ACTION_MOVE is successfully completed. The signal
   * handler is responsible for deleting the data that has been dropped. What
   * "delete" means depends on the context of the drag operation.
   */
  signals[DRAG_DATA_DELETE] =
      g_signal_new (I_("drag-data-delete"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);
}

static void gtk_drag_source_dnd_finished_cb   (GdkDrag             *drag,
                                               GtkDragSource       *source);
static void gtk_drag_source_drop_performed_cb (GdkDrag             *drag,
                                               GtkDragSource       *source);
static void gtk_drag_source_cancel_cb         (GdkDrag             *drag,
                                               GdkDragCancelReason  reason,
                                               GtkDragSource       *source);

static void
drag_end (GtkDragSource *source)
{
  g_signal_handlers_disconnect_by_func (source->drag, gtk_drag_source_drop_performed_cb, source);
  g_signal_handlers_disconnect_by_func (source->drag, gtk_drag_source_dnd_finished_cb, source);
  g_signal_handlers_disconnect_by_func (source->drag, gtk_drag_source_cancel_cb, source);

  g_signal_emit (source, signals[DRAG_END], 0);

  g_object_set_data (G_OBJECT (source->drag), I_("gtk-drag-source"), NULL);
  g_clear_object (&source->drag);
  source->widget = NULL;
  g_object_unref (source);
}

static void
gtk_drag_source_dnd_finished_cb (GdkDrag       *drag,
                                 GtkDragSource *source)
{
  if (gdk_drag_get_selected_action (drag) == GDK_ACTION_MOVE)
    g_signal_emit (source, signals[DRAG_DATA_DELETE], 0);

  drag_end (source);
}

static void
gtk_drag_source_cancel_cb (GdkDrag             *drag,
                           GdkDragCancelReason  reason,
                           GtkDragSource       *source)
{
  gboolean success = FALSE;

  g_signal_emit (source, signals[DRAG_FAILED], 0, reason, &success);
  drag_end (source);
}

static void
gtk_drag_source_drop_performed_cb (GdkDrag       *drag,
                                   GtkDragSource *source)
{
  if (source->icon_window)
    gtk_widget_hide (source->icon_window);
}

/**
 * gtk_drag_source_drag_begin:
 * @source: a #GtkDragSource
 * @widget: the widget where the drag operation originates
 * @device: the device that is driving the drag operation
 * @x: start point X coordinate
 * @y: start point Y xoordinate
 *
 * Starts a DND operation with @source.
 *
 * The start point coordinates are relative to @widget.
 *
 * GTK keeps a reference on @source for the duration of
 * the DND operation, so it is safe to unref @source
 * after this call.
 */
void
gtk_drag_source_drag_begin (GtkDragSource *source,
                            GtkWidget     *widget,
                            GdkDevice     *device,
                            int            x,
                            int            y)
{
  GtkNative *native;
  GdkSurface *surface;
  double px, py;
  int dx, dy;
  GtkWidget *icon;

  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (source->content != NULL);
  g_return_if_fail (source->actions != 0);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  native = gtk_widget_get_native (widget);
  surface = gtk_native_get_surface (native);

  gtk_widget_translate_coordinates (widget, GTK_WIDGET (native), x, y, &x, &y);
  gdk_surface_get_device_position (surface, device, &px, &py, NULL);

  dx = round (px) - x;
  dy = round (py) - y;

  source->drag = gdk_drag_begin (surface, device, source->content, source->actions, dx, dy);
  if (source->drag == NULL)
    {
      g_print ("no drag :(\n");
      return;
    }

  g_object_set_data (G_OBJECT (source->drag), I_("gtk-drag-source"), source);
  source->widget = widget;

  gtk_widget_reset_controllers (widget);

  /* We hold a ref until ::drag-end is emitted */
  g_object_ref (source);

  g_signal_emit (source, signals[DRAG_BEGIN], 0);

  if (!source->paintable)
    {
      GtkIconTheme *theme;
 
      theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (widget));
      source->paintable = gtk_icon_theme_load_icon (theme, "text-x-generic", 32, 0, NULL);
      source->hot_x = 0;
      source->hot_y = 0;
    }

  gdk_drag_set_hotspot (source->drag, source->hot_x, source->hot_y);
  source->icon_window = gtk_drag_icon_new ();
  g_object_ref_sink (source->icon_window);
  gtk_drag_icon_set_surface (GTK_DRAG_ICON (source->icon_window),
                             gdk_drag_get_drag_surface (source->drag));

  icon = gtk_picture_new_for_paintable (source->paintable);
  gtk_picture_set_can_shrink (GTK_PICTURE (icon), FALSE);
  gtk_drag_icon_set_widget (GTK_DRAG_ICON (source->icon_window), icon);

  gtk_widget_show (source->icon_window);

  g_signal_connect (source->drag, "drop-performed",
                    G_CALLBACK (gtk_drag_source_drop_performed_cb), source);
  g_signal_connect (source->drag, "dnd-finished",
                    G_CALLBACK (gtk_drag_source_dnd_finished_cb), source);
  g_signal_connect (source->drag, "cancel",
                    G_CALLBACK (gtk_drag_source_cancel_cb), source);
}

/**
 * gtk_drag_source_new:
 * @content: (nullable): the #GdkContentProvider to use, or %NULL
 * @actions: the actions to offer
 *
 * Creates a new #GtkDragSource object.
 *
 * Returns: the new #GtkDragSource
 */
GtkDragSource *
gtk_drag_source_new (GdkContentProvider *content,
                     GdkDragAction       actions)
{
  return g_object_new (GTK_TYPE_DRAG_SOURCE,
                       "content", content,
                       "actions", actions,
                       NULL);
}

/**
 * gtk_drag_source_get_content:
 * @source: a #GtkDragSource
 *
 * Gets the current content provider of a #GtkDragSource.
 *
 * Returns: the #GtkContentProvider of @source
 */
GdkContentProvider *
gtk_drag_source_get_content (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), NULL);

  return source->content;  
}

/**
 * gtk_drag_source_set_content:
 * @source: a #GtkDragSource
 * @content: (nullable): a #GtkContntProvider, or %NULL
 *
 * Sets a content provider on a #GtkDragSource.
 *
 * When the data is requested in the cause of a
 * DND operation, it will be obtained from the
 * content provider.
 */
void
gtk_drag_source_set_content (GtkDragSource      *source,
                             GdkContentProvider *content)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (!g_set_object (&source->content, content))
    return;

  g_object_notify_by_pspec (G_OBJECT (source), properties[PROP_CONTENT]);
}

/**
 * gtk_drag_source_get_actions:
 * @source: a #GtkDragSource
 *
 * Gets the actions that are currently set on the #GtkDragSource.
 *
 * Returns: the actions set on @source
 */
GdkDragAction
gtk_drag_source_get_actions (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), 0);

  return source->actions;
}

/**
 * gtk_drag_source_set_actions:
 * @source: a #GtkDragSource
 * @actions: the actions to offer
 *
 * Sets the actions on the #GtkDragSource.
 *
 * During a DND operation, the actions are offered
 * to potential drop targets.
 */
void
gtk_drag_source_set_actions (GtkDragSource *source,
                             GdkDragAction  actions)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (source->actions == actions)
    return;

  source->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (source), properties[PROP_ACTIONS]);
}

/**
 * gtk_drag_source_set_icon:
 * @source: a #GtkDragSource
 * @paintable: (nullable): the #GtkPaintable to use as icon, or %NULL
 * @hot_x: the hotspot X coordinate on the icon
 * @hot_y: the hotspot Y coordinate on the icon
 *
 * Sets a paintable to use as icon during DND operations.
 *
 * The hotspot coordinates determine the point on the icon
 * that gets aligned with the hotspot of the cursor.
 *
 * If @paintable is %NULL, a default icon is used.
 */
void
gtk_drag_source_set_icon (GtkDragSource *source,
                          GdkPaintable  *paintable,
                          int            hot_x,
                          int            hot_y)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  g_set_object (&source->paintable, paintable);

  source->hot_x = hot_x;
  source->hot_y = hot_y;
}

static void
source_gesture_begin (GtkGesture       *gesture,
                      GdkEventSequence *sequence,
                      GtkDragSource    *source)
{
  guint button;

  if (gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture)))
    button = 1;
  else
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  g_assert (button >= 1);

  if ((source->start_button_mask & (GDK_BUTTON1_MASK << (button - 1))) == 0)
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
source_gesture_update (GtkGesture       *gesture,
                       GdkEventSequence *sequence,
                       GtkDragSource    *source)
                       
{
  gdouble start_x, start_y, offset_x, offset_y;
  GtkWidget *widget;

  if (!gtk_gesture_is_recognized (gesture))
    return;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (gesture), &start_x, &start_y);
  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (gesture), &offset_x, &offset_y);

  if (gtk_drag_check_threshold (widget,
                                start_x, start_y,
                                start_x + offset_x, start_y + offset_y))
    {
      GdkDevice *device = gtk_gesture_get_device (gesture);

      gtk_drag_source_drag_begin (source, widget, device, start_x, start_y);
   }
}

static void
gesture_removed (gpointer data)
{
  GtkDragSource *source = data;

  /* if we get here, the widget we are attached to was destroyed,
   * and removed our gesture.
   *
   * Clean up, and drop the ref we held for being attached.
   */

  source->gesture = NULL;
  g_object_unref (source);
}

/**
 * gtk_drag_source_attach:
 * @source: (transfer full): a #GtkDragSource
 * @widget: the widget to attach @source to
 * @start_button_mask: mask determining which mouse buttons trigger
 *
 * Attaches the @source to a @widget by creating a drag gesture
 * on @widget that will trigger DND operations with @source.
 *
 * The @start_button_mask determines which mouse buttons trigger
 * a DND operation.
 *
 * To undo the effect of this call, use gtk_drag_source_detach().
 */
void
gtk_drag_source_attach (GtkDragSource   *source,
                        GtkWidget       *widget,
                        GdkModifierType  start_button_mask)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (source->gesture == NULL);
  g_return_if_fail (start_button_mask != 0);
  g_return_if_fail ((start_button_mask & ~(GDK_BUTTON1_MASK |
                                           GDK_BUTTON2_MASK |
                                           GDK_BUTTON3_MASK |
                                           GDK_BUTTON4_MASK |
                                           GDK_BUTTON5_MASK)) == 0);

  source->gesture = gtk_gesture_drag_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (source->gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (source->gesture), 0);
  g_signal_connect (source->gesture, "begin",
                    G_CALLBACK (source_gesture_begin), source);
  g_signal_connect (source->gesture, "update",
                    G_CALLBACK (source_gesture_update), source);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (source->gesture));

  source->start_button_mask = start_button_mask;

  g_object_set_data_full (G_OBJECT (source->gesture), "gtk-drag-source",
                          source, gesture_removed);
}

/**
 * gtk_drag_source_detach:
 * @source: a #GtkDragSource
 *
 * Undoes the effect of a prior gtk_drag_source_attach() call.
 */
void
gtk_drag_source_detach (GtkDragSource *source)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (source->gesture)
    {
      GtkWidget *widget;

      g_object_ref (source);

      g_object_set_data (G_OBJECT (source->gesture), "gtk-drag-source", NULL);
      widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source->gesture));
      gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (source->gesture));

      g_object_unref (source);
    }
}

/**
 * gtk_drag_get_source:
 * @drag: a #GdkDrag
 *
 * Obtains the #GtkDragSource from which a #GdkDrag originates.
 *
 * This function should rarely be needed. Once case where it can
 * be used is together with gtk_drop_get_drag(), to determine
 * whether a 'local' drag is coming from the same widget.
 *
 * Returns: (transfer none) (nullable): a #GtkDragSource, or %NULL
 */
GtkDragSource *
gtk_drag_get_source (GdkDrag *drag)
{
  gpointer data;

  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);

  data = g_object_get_data (G_OBJECT (drag), I_("gtk-drag-source"));

  if (data)
    return GTK_DRAG_SOURCE (data);

  return NULL;
}

/**
 * gtk_drag_source_get_origin:
 * @source: a #GtkDragSource
 *
 * Returns the widget that an ongoing drag is started from.
 *
 * Returns: (nullable): the origin of the current drag operation, or %NULL
 */
GtkWidget *
gtk_drag_source_get_origin (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), NULL);

  return source->widget;
}

/**
 * gtk_drag_source_get_drag:
 * @source: a #GtkDragSource
 *
 * Returns the underlying #GtkDrag object for an ongoing drag.
 *
 * Returns: (nullable): the #GdkDrag of the current drag operation, or %NULL
 */
GdkDrag *
gtk_drag_source_get_drag (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), NULL);

  return source->drag;
}

/**
 * gtk_drag_source_drag_cancel:
 * @source: a #GtkDragSource
 *
 * Cancels a currently ongoing drag operation.
 */
void
gtk_drag_source_drag_cancel (GtkDragSource *source)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (source->drag)
    {
      gboolean success = FALSE;

      g_signal_emit (source, signals[DRAG_FAILED], 0, GDK_DRAG_CANCEL_ERROR, &success);

      gdk_drag_drop_done (source->drag, success);
    }
}
