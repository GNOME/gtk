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

#include "gtkdndprivate.h"

#include "gtkdragdest.h"
#include "gtkimageprivate.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkpicture.h"
#include "gtkselectionprivate.h"
#include "gtksettingsprivate.h"
#include "gtkstylecontext.h"
#include "gtktooltipprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowgroup.h"
#include "gtkwindowprivate.h"

#include "gdk/gdkcontentformatsprivate.h"
#include "gdk/gdktextureprivate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>


/**
 * SECTION:gtkdnd
 * @Short_description: Functions for controlling drag and drop handling
 * @Title: Drag and Drop
 *
 * GTK+ has a rich set of functions for doing inter-process communication
 * via the drag-and-drop metaphor.
 *
 * As well as the functions listed here, applications may need to use some
 * facilities provided for [Selections][gtk3-Selections]. Also, the Drag and
 * Drop API makes use of signals in the #GtkWidget class.
 */


typedef struct _GtkDragSourceInfo GtkDragSourceInfo;
typedef struct _GtkDragDestInfo GtkDragDestInfo;

struct _GtkDragSourceInfo 
{
  GtkWidget         *widget;
  GdkContentFormats *target_list; /* Targets for drag data */
  GdkDrag           *drag;     /* drag context */
  GtkWidget         *icon_window; /* Window for drag */
  GtkWidget         *icon_widget; /* Widget for drag */

  guint              drop_timeout;     /* Timeout for aborting drop */
  guint              destroy_icon : 1; /* If true, destroy icon_widget */
};

struct _GtkDragDestInfo
{
  GtkWidget         *widget;              /* Widget in which drag is in */
  GdkDrop           *drop;                /* drop */
};

#define DROP_ABORT_TIME 300000

typedef gboolean (* GtkDragDestCallback) (GtkWidget      *widget,
                                          GdkDrop        *drop,
                                          gint            x,
                                          gint            y,
                                          guint32         time);

/* Forward declarations */
static gboolean gtk_drop_find_widget            (GtkWidget        *widget,
                                                 GdkDrop          *drop,
                                                 GtkDragDestInfo  *info,
                                                 gint              x,
                                                 gint              y,
                                                 guint32           time,
                                                 GtkDragDestCallback callback);
static void     gtk_drag_dest_leave             (GtkWidget        *widget,
                                                 GdkDrop          *drop,
                                                 guint             time);
static gboolean gtk_drag_dest_motion            (GtkWidget        *widget,
                                                 GdkDrop          *drop,
                                                 gint              x,
                                                 gint              y,
                                                 guint             time);
static gboolean gtk_drag_dest_drop              (GtkWidget        *widget,
                                                 GdkDrop          *drop,
                                                 gint              x,
                                                 gint              y,
                                                 guint             time);
static void     gtk_drag_dest_set_widget        (GtkDragDestInfo  *info,
                                                 GtkWidget        *widget);

static GtkDragDestInfo *  gtk_drag_get_dest_info     (GdkDrop        *drop,
                                                      gboolean        create);
static GtkDragSourceInfo *gtk_drag_get_source_info   (GdkDrag        *drag,
                                                      gboolean        create);
static void               gtk_drag_clear_source_info (GdkDrag        *drag);

static void gtk_drag_drop                      (GtkDragSourceInfo *info);
static void gtk_drag_drop_finished             (GtkDragSourceInfo *info,
                                                GtkDragResult      result);
static void gtk_drag_cancel_internal           (GtkDragSourceInfo *info,
                                                GtkDragResult      result);

static void gtk_drag_remove_icon               (GtkDragSourceInfo *info);
static void gtk_drag_source_info_destroy       (GtkDragSourceInfo *info);

static void gtk_drag_drop_performed_cb (GdkDrag           *drag,
                                        GtkDragSourceInfo *info);
static void gtk_drag_cancel_cb         (GdkDrag             *drag,
                                        GdkDragCancelReason  reason,
                                        GtkDragSourceInfo   *info);
static void gtk_drag_dnd_finished_cb   (GdkDrag           *drag,
                                        GtkDragSourceInfo *info);

static gboolean gtk_drag_abort_timeout         (gpointer           data);

static void     set_icon_helper (GdkDrag            *drag,
                                 GtkImageDefinition *def,
                                 gint                hot_x,
                                 gint                hot_y);


/********************
 * Destination side *
 ********************/

typedef struct {
  GdkDrop *drop;
  GtkWidget *widget;
  const char *mime_type;
} GtkDragGetData;

static void
gtk_drag_get_data_finish (GtkDragGetData *data,
                          guchar         *bytes,
                          gsize           size)
{
  GtkDragDestSite *site;
  GtkSelectionData sdata;

  site = g_object_get_data (G_OBJECT (data->widget), "gtk-drag-dest");

  sdata.target = data->mime_type;
  sdata.type = data->mime_type;
  sdata.format = 8;
  sdata.length = size;
  sdata.data = bytes;
  sdata.display = gtk_widget_get_display (data->widget);
  
  if (site && site->target_list)
    {
      if (gdk_content_formats_contain_mime_type (site->target_list, data->mime_type))
        {
          if (!(site->flags & GTK_DEST_DEFAULT_DROP) ||
              size >= 0)
            g_signal_emit_by_name (data->widget,
                                   "drag-data-received",
                                   data->drop,
                                   &sdata);
        }
    }
  else
    {
      g_signal_emit_by_name (data->widget,
                             "drag-data-received",
                             data->drop,
                             &sdata);
    }
  
  if (site && site->flags & GTK_DEST_DEFAULT_DROP)
    {
      GdkDragAction action = site->actions & gdk_drop_get_actions (data->drop);

      if (size == 0)
        action = 0;

      if (!gdk_drag_action_is_unique (action))
        {
          if (action & GDK_ACTION_COPY)
            action = GDK_ACTION_COPY;
          else if (action & GDK_ACTION_MOVE)
            action = GDK_ACTION_MOVE;
        }

      gdk_drop_finish (data->drop, action); 
    }
  
  g_object_unref (data->widget);
  g_object_unref (data->drop);
  g_slice_free (GtkDragGetData, data);
}

static void
gtk_drag_get_data_got_data (GObject      *source,
                            GAsyncResult *result,
                            gpointer      data)
{
  gssize written;

  written = g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, NULL);
  if (written < 0)
    {
      gtk_drag_get_data_finish (data, NULL, 0);
    }
  else
    {
      gtk_drag_get_data_finish (data,
                                g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (source)),
                                g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (source)));
    }
}

static void
gtk_drag_get_data_got_stream (GObject      *source,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GtkDragGetData *data = user_data;
  GInputStream *input_stream;
  GOutputStream *output_stream;

  input_stream = gdk_drop_read_finish (GDK_DROP (source), result, &data->mime_type, NULL);
  if (input_stream == NULL)
    {
      gtk_drag_get_data_finish (data, NULL, 0);
      return;
    }

  output_stream = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (output_stream,
                                input_stream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                gtk_drag_get_data_got_data,
                                data);
  g_object_unref (output_stream);
  g_object_unref (input_stream);

}

/**
 * gtk_drag_get_data: (method)
 * @widget: the widget that will receive the
 *   #GtkWidget::drag-data-received signal
 * @drop: the #GdkDrop
 * @target: the target (form of the data) to retrieve
 *
 * Gets the data associated with a drag. When the data
 * is received or the retrieval fails, GTK+ will emit a
 * #GtkWidget::drag-data-received signal. Failure of the retrieval
 * is indicated by the length field of the @selection_data
 * signal parameter being negative. However, when gtk_drag_get_data()
 * is called implicitely because the %GTK_DEST_DEFAULT_DROP was set,
 * then the widget will not receive notification of failed
 * drops.
 */
void
gtk_drag_get_data (GtkWidget *widget,
                   GdkDrop   *drop,
                   GdkAtom    target)
{
  GtkDragGetData *data;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DROP (drop));

  data = g_slice_new0 (GtkDragGetData);
  data->widget = g_object_ref (widget);
  data->drop = g_object_ref (drop);
  data->mime_type = target;

  gdk_drop_read_async (drop,
                       (const gchar *[2]) { target, NULL },
                       G_PRIORITY_DEFAULT,
                       NULL,
                       gtk_drag_get_data_got_stream,
                       data);
}

/**
 * gtk_drag_get_source_widget:
 * @drag: a drag context
 *
 * Determines the source widget for a drag.
 *
 * Returns: (nullable) (transfer none): if the drag is occurring
 *     within a single application, a pointer to the source widget.
 *     Otherwise, %NULL.
 */
GtkWidget *
gtk_drag_get_source_widget (GdkDrag *drag)
{
  GtkDragSourceInfo *info;

  g_return_val_if_fail (GDK_IS_DRAG (drag), NULL);
  
  info = gtk_drag_get_source_info (drag, FALSE);
  if (info == NULL)
    return NULL;

  return info->widget;
}

/**
 * gtk_drag_highlight: (method)
 * @widget: a widget to highlight
 *
 * Highlights a widget as a currently hovered drop target.
 * To end the highlight, call gtk_drag_unhighlight().
 * GTK+ calls this automatically if %GTK_DEST_DEFAULT_HIGHLIGHT is set.
 */
void
gtk_drag_highlight (GtkWidget  *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
}

/**
 * gtk_drag_unhighlight: (method)
 * @widget: a widget to remove the highlight from
 *
 * Removes a highlight set by gtk_drag_highlight() from
 * a widget.
 */
void
gtk_drag_unhighlight (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_DROP_ACTIVE);
}

/*
 * _gtk_drag_dest_handle_event:
 * @toplevel: Toplevel widget that received the event
 * @event: the event to handle
 *
 * Called from widget event handling code on Drag events
 * for destinations.
 */
void
_gtk_drag_dest_handle_event (GtkWidget *toplevel,
                             GdkEvent  *event)
{
  GtkDragDestInfo *info;
  GdkDrop *drop;
  guint32 time;
  GdkEventType event_type;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  event_type = gdk_event_get_event_type (event);
  drop = gdk_event_get_drop (event);
  time = gdk_event_get_time (event);

  info = gtk_drag_get_dest_info (drop, TRUE);

  /* Find the widget for the event */
  switch ((guint) event_type)
    {
    case GDK_DRAG_ENTER:
      break;
      
    case GDK_DRAG_LEAVE:
      if (info->widget)
        {
          gtk_drag_dest_leave (info->widget, drop, time);
          gtk_drag_dest_set_widget (info, NULL);
        }
      break;
      
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      {
        double x, y;
        gboolean found;

        if (event_type == GDK_DROP_START)
          {
            /* We send a leave here so that the widget unhighlights
             * properly.
             */
            if (info->widget)
              {
                gtk_drag_dest_leave (info->widget, drop, time);
                gtk_drag_dest_set_widget (info, NULL);
              }
          }

        gdk_event_get_coords (event, &x, &y);

        found = gtk_drop_find_widget (toplevel,
                                      drop,
                                      info,
                                      x,
                                      y,
                                      time,
                                      (event_type == GDK_DRAG_MOTION) ?
                                      gtk_drag_dest_motion :
                                      gtk_drag_dest_drop);

        if (info->widget && !found)
          {
            gtk_drag_dest_leave (info->widget, drop, time);
            gtk_drag_dest_set_widget (info, NULL);
          }
        
        /* Send a reply.
         */
        if (!found)
          gdk_drop_status (drop, 0);
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
gtk_drop_find_widget (GtkWidget           *event_widget,
                      GdkDrop             *drop,
                      GtkDragDestInfo     *info,
                      gint                 x,
                      gint                 y,
                      guint32              time,
                      GtkDragDestCallback  callback)
{
  GtkWidget *widget;

  if (!gtk_widget_get_mapped (event_widget) ||
      !gtk_widget_get_sensitive (event_widget))
    return FALSE;

  widget = gtk_widget_pick (event_widget, x, y, GTK_PICK_DEFAULT);

  if (!widget)
    return FALSE;

  gtk_widget_translate_coordinates (event_widget, widget, x, y, &x, &y);

  while (widget)
    {
      GtkWidget *parent;
      GList *hierarchy = NULL;
      gboolean found = FALSE;

      if (!gtk_widget_get_mapped (widget))
        return FALSE;

      if (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_INSENSITIVE)
        {
          widget = gtk_widget_get_parent (widget);
          continue;
        }

      /* need to reference the entire hierarchy temporarily in case the
       * ::drag-motion/::drag-drop callbacks change the widget hierarchy.
       */
      for (parent = widget;
           parent;
           parent = gtk_widget_get_parent (parent))
        {
          hierarchy = g_list_prepend (hierarchy, g_object_ref (parent));
        }

      /* If the current widget is registered as a drop site, check to
       * emit "drag-motion" to check if we are actually in a drop
       * site.
       */
      if (g_object_get_data (G_OBJECT (widget), "gtk-drag-dest"))
        {
          found = callback (widget, drop, x, y, time);

          /* If so, send a "drag-leave" to the last widget */
          if (found && info->widget != widget)
            {
              if (info->widget)
                gtk_drag_dest_leave (info->widget, drop, time);

              gtk_drag_dest_set_widget (info, widget);
            }
        }

      if (!found)
        {
          /* Get the parent before unreffing the hierarchy because
           * invoking the callback might have destroyed the widget
           */
          parent = gtk_widget_get_parent (widget);

          /* The parent might be going away when unreffing the
           * hierarchy, so also protect againt that
           */
          if (parent)
            g_object_add_weak_pointer (G_OBJECT (parent), (gpointer *) &parent);
        }

      g_list_free_full (hierarchy, g_object_unref);

      if (found)
        return TRUE;

      if (parent)
        g_object_remove_weak_pointer (G_OBJECT (parent), (gpointer *) &parent);
      else
        return FALSE;

      if (!gtk_widget_translate_coordinates (widget, parent, x, y, &x, &y))
        return FALSE;

      widget = parent;
    }

  return FALSE;
}

static void
gtk_drag_dest_set_widget (GtkDragDestInfo *info,
                          GtkWidget       *widget)
{
  if (info->widget)
    g_object_remove_weak_pointer (G_OBJECT (info->widget), (gpointer *) &info->widget);

  info->widget = widget;

  if (info->widget)
    g_object_add_weak_pointer (G_OBJECT (info->widget), (gpointer *) &info->widget);
}

static void
gtk_drag_dest_info_destroy (gpointer data)
{
  GtkDragDestInfo *info = (GtkDragDestInfo *)data;

  gtk_drag_dest_set_widget (info, NULL);

  g_slice_free (GtkDragDestInfo, data);
}

static GtkDragDestInfo *
gtk_drag_get_dest_info (GdkDrop  *drop,
                        gboolean  create)
{
  GtkDragDestInfo *info;
  static GQuark info_quark = 0;
  if (!info_quark)
    info_quark = g_quark_from_static_string ("gtk-dest-info");
  
  info = g_object_get_qdata (G_OBJECT (drop), info_quark);
  if (!info && create)
    {
      info = g_slice_new0 (GtkDragDestInfo);
      info->drop = drop;
      g_object_set_qdata_full (G_OBJECT (drop), info_quark,
                               info, gtk_drag_dest_info_destroy);
    }

  return info;
}

static GQuark dest_info_quark = 0;

static GtkDragSourceInfo *
gtk_drag_get_source_info (GdkDrag  *drag,
                          gboolean  create)
{
  GtkDragSourceInfo *info;
  if (!dest_info_quark)
    dest_info_quark = g_quark_from_static_string ("gtk-source-info");
  
  info = g_object_get_qdata (G_OBJECT (drag), dest_info_quark);
  if (!info && create)
    {
      info = g_new0 (GtkDragSourceInfo, 1);
      info->drag = drag;
      g_object_set_qdata (G_OBJECT (drag), dest_info_quark, info);
    }

  return info;
}

static void
gtk_drag_clear_source_info (GdkDrag *drag)
{
  g_object_set_qdata (G_OBJECT (drag), dest_info_quark, NULL);
}

/*
 * Default drag handlers
 */
static void  
gtk_drag_dest_leave (GtkWidget      *widget,
                     GdkDrop        *drop,
                     guint           time)
{
  GtkDragDestSite *site;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_if_fail (site != NULL);

  if ((site->flags & GTK_DEST_DEFAULT_HIGHLIGHT) && site->have_drag)
    gtk_drag_unhighlight (widget);

  if (!(site->flags & GTK_DEST_DEFAULT_MOTION) || site->have_drag ||
      site->track_motion)
    g_signal_emit_by_name (widget, "drag-leave", drop, time);
  
  site->have_drag = FALSE;
}

static gboolean
gtk_drag_dest_motion (GtkWidget *widget,
                      GdkDrop   *drop,
                      gint       x,
                      gint       y,
                      guint      time)
{
  GtkDragDestSite *site;
  gboolean retval;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  if (site->track_motion || site->flags & GTK_DEST_DEFAULT_MOTION)
    {
      GdkDragAction actions;
      
      actions = gdk_drop_get_actions (drop);

      if ((actions & site->actions) == 0)
        actions = 0;

      if (actions && gtk_drag_dest_find_target (widget, drop, NULL))
        {
          if (!site->have_drag)
            {
              site->have_drag = TRUE;
              if (site->flags & GTK_DEST_DEFAULT_HIGHLIGHT)
                gtk_drag_highlight (widget);
            }

          gdk_drop_status (drop, site->actions);
        }
      else
        {
          gdk_drop_status (drop, 0);
          if (!site->track_motion)
            return TRUE;
        }
    }

  g_signal_emit_by_name (widget, "drag-motion",
                         drop, x, y, &retval);

  return (site->flags & GTK_DEST_DEFAULT_MOTION) ? TRUE : retval;
}

static gboolean
gtk_drag_dest_drop (GtkWidget *widget,
                    GdkDrop   *drop,
                    gint       x,
                    gint       y,
                    guint      time)
{
  GtkDragDestSite *site;
  GtkDragDestInfo *info;
  gboolean retval;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  info = gtk_drag_get_dest_info (drop, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  if (site->flags & GTK_DEST_DEFAULT_DROP)
    {
      GdkAtom target = gtk_drag_dest_find_target (widget, drop, NULL);

      if (target == NULL)
        {
          gdk_drop_finish (drop, 0);
          return TRUE;
        }
      else 
        gtk_drag_get_data (widget, drop, target);
    }

  g_signal_emit_by_name (widget, "drag-drop",
                         drop, x, y, &retval);

  return (site->flags & GTK_DEST_DEFAULT_DROP) ? TRUE : retval;
}

/***************
 * Source side *
 ***************/

#define GTK_TYPE_DRAG_CONTENT            (gtk_drag_content_get_type ())
#define GTK_DRAG_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_DRAG_CONTENT, GtkDragContent))
#define GTK_IS_DRAG_CONTENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_DRAG_CONTENT))
#define GTK_DRAG_CONTENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_DRAG_CONTENT, GtkDragContentClass))
#define GTK_IS_DRAG_CONTENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DRAG_CONTENT))
#define GTK_DRAG_CONTENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_DRAG_CONTENT, GtkDragContentClass))

typedef struct _GtkDragContent GtkDragContent;
typedef struct _GtkDragContentClass GtkDragContentClass;

struct _GtkDragContent
{
  GdkContentProvider parent;

  GtkWidget *widget;
  GdkDrag *drag;
  GdkContentFormats *formats;
  guint32 time;
};

struct _GtkDragContentClass
{
  GdkContentProviderClass parent_class;
};

GType gtk_drag_content_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkDragContent, gtk_drag_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gtk_drag_content_ref_formats (GdkContentProvider *provider)
{
  GtkDragContent *content = GTK_DRAG_CONTENT (provider);

  return gdk_content_formats_ref (content->formats);
}

static void
gtk_drag_content_write_mime_type_done (GObject      *stream,
                                       GAsyncResult *result,
                                       gpointer      task)
{
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (stream),
                                         result,
                                         NULL,
                                         &error))
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  g_object_unref (task);
}

static void
gtk_drag_content_write_mime_type_async (GdkContentProvider  *provider,
                                        const char          *mime_type,
                                        GOutputStream       *stream,
                                        int                  io_priority,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GtkDragContent *content = GTK_DRAG_CONTENT (provider);
  GtkSelectionData sdata = { 0, };
  GTask *task;

  task = g_task_new (content, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gtk_drag_content_write_mime_type_async);

  sdata.target = g_intern_string (mime_type);
  sdata.length = -1;
  sdata.display = gtk_widget_get_display (content->widget);
  
  g_signal_emit_by_name (content->widget, "drag-data-get",
                         content->drag,
                         &sdata);

  if (sdata.length == -1)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Cannot provide contents as “%s”"), mime_type);
      g_object_unref (task);
      return;
    }
  g_task_set_task_data (task, sdata.data, g_free);

  g_output_stream_write_all_async (stream,
                                   sdata.data,
                                   sdata.length,
                                   io_priority,
                                   cancellable,
                                   gtk_drag_content_write_mime_type_done,
                                   task);
}

static gboolean
gtk_drag_content_write_mime_type_finish (GdkContentProvider  *provider,
                                         GAsyncResult        *result,
                                         GError             **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_drag_content_write_mime_type_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gtk_drag_content_finalize (GObject *object)
{
  GtkDragContent *content = GTK_DRAG_CONTENT (object);

  g_clear_object (&content->widget);
  g_clear_pointer (&content->formats, gdk_content_formats_unref);

  G_OBJECT_CLASS (gtk_drag_content_parent_class)->finalize (object);
}

static void
gtk_drag_content_class_init (GtkDragContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_drag_content_finalize;

  provider_class->ref_formats = gtk_drag_content_ref_formats;
  provider_class->write_mime_type_async = gtk_drag_content_write_mime_type_async;
  provider_class->write_mime_type_finish = gtk_drag_content_write_mime_type_finish;
}

static void
gtk_drag_content_init (GtkDragContent *content)
{
}

/* Like gtk_drag_begin(), but also takes a GtkImageDefinition
 * so that we can set the icon from the source site information
 */
GdkDrag *
gtk_drag_begin_internal (GtkWidget          *widget,
                         GdkDevice          *device,
                         GtkImageDefinition *icon,
                         GdkContentFormats  *target_list,
                         GdkDragAction       actions,
                         int                 x,
                         int                 y)
{
  GtkDragSourceInfo *info;
  GtkRoot *root;
  GdkDrag *drag;
  double px, py;
  int dx, dy;
  GtkDragContent *content;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  root = gtk_widget_get_root (widget);
  gtk_widget_translate_coordinates (widget, GTK_WIDGET (root), x, y, &x, &y);
  gdk_surface_get_device_position (gtk_widget_get_surface (widget),
                                   device,
                                   &px, &py,
                                   NULL);
  dx = round (px) - x;
  dy = round (py) - y;

  content = g_object_new (GTK_TYPE_DRAG_CONTENT, NULL);
  content->widget = g_object_ref (widget);
  content->formats = gdk_content_formats_ref (target_list);

  drag = gdk_drag_begin (gtk_widget_get_surface (GTK_WIDGET (root)), device, GDK_CONTENT_PROVIDER (content), actions, dx, dy);
  if (drag == NULL)
    {
      g_object_unref (content);
      return NULL;
    }

  content->drag = drag;
  g_object_unref (content);

  info = gtk_drag_get_source_info (drag, TRUE);

  g_object_set_data (G_OBJECT (widget), I_("gtk-info"), info);

  info->widget = g_object_ref (widget);

  info->target_list = target_list;
  gdk_content_formats_ref (target_list);

  info->icon_window = NULL;
  info->icon_widget = NULL;
  info->destroy_icon = FALSE;

  gtk_widget_reset_controllers (widget);

  g_signal_emit_by_name (widget, "drag-begin", info->drag);

  /* Ensure that we have an icon before we start the drag; the
   * application may have set one in ::drag_begin, or it may
   * not have set one.
   */
  if (!info->icon_widget)
    {
      if (icon)
        {
          set_icon_helper (info->drag, icon, 0, 0);
        }
      else
        {
          icon = gtk_image_definition_new_icon_name ("text-x-generic");
          set_icon_helper (info->drag, icon, 0, 0);
          gtk_image_definition_unref (icon);
        }
    }

  g_signal_connect (drag, "drop-performed",
                    G_CALLBACK (gtk_drag_drop_performed_cb), info);
  g_signal_connect (drag, "dnd-finished",
                    G_CALLBACK (gtk_drag_dnd_finished_cb), info);
  g_signal_connect (drag, "cancel",
                    G_CALLBACK (gtk_drag_cancel_cb), info);

  return info->drag;
}

/**
 * gtk_drag_begin: (method)
 * @widget: the source widget
 * @device: (nullable): the device that starts the drag or %NULL to use the default pointer
 * @targets: The targets (data formats) in which the source can provide the data
 * @actions: A bitmask of the allowed drag actions for this drag
 * @x: The initial x coordinate to start dragging from, in the coordinate space of @widget.
 * @y: The initial y coordinate to start dragging from, in the coordinate space of @widget.
 *
 * Initiates a drag on the source side. The function only needs to be used
 * when the application is starting drags itself, and is not needed when
 * gtk_drag_source_set() is used.
 *
 * Returns: (transfer none): the context for this drag
 */
GdkDrag *
gtk_drag_begin (GtkWidget         *widget,
                GdkDevice         *device,
                GdkContentFormats *targets,
                GdkDragAction      actions,
                gint               x,
                gint               y)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (device == NULL || GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gtk_widget_get_realized (widget), NULL);
  g_return_val_if_fail (targets != NULL, NULL);

  if (device == NULL)
    {
      GdkSeat *seat;

      seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
      device = gdk_seat_get_pointer (seat);
    }

  return gtk_drag_begin_internal (widget,
                                  device,
                                  NULL,
                                  targets,
                                  actions,
                                  x, y);
}

static void
icon_widget_destroyed (GtkWidget         *widget,
                       GtkDragSourceInfo *info)
{
  g_clear_object (&info->icon_widget);
}

static void
gtk_drag_set_icon_widget_internal (GdkDrag        *drag,
                                   GtkWidget      *widget,
                                   gint            hot_x,
                                   gint            hot_y,
                                   gboolean        destroy_on_release)
{
  GtkDragSourceInfo *info;

  g_return_if_fail (!GTK_IS_WINDOW (widget));

  info = gtk_drag_get_source_info (drag, FALSE);
  if (info == NULL)
    {
      if (destroy_on_release)
        gtk_widget_destroy (widget);
      return;
    }

  gtk_drag_remove_icon (info);

  if (widget)
    g_object_ref (widget);

  info->icon_widget = widget;
  info->destroy_icon = destroy_on_release;

  if (!widget)
    return;

  g_signal_connect (widget, "destroy", G_CALLBACK (icon_widget_destroyed), info);

  gdk_drag_set_hotspot (drag, hot_x, hot_y);

  if (!info->icon_window)
    {
      GdkDisplay *display;

      display = gdk_drag_get_display (drag);

      info->icon_window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_type_hint (GTK_WINDOW (info->icon_window), GDK_SURFACE_TYPE_HINT_DND);
      gtk_window_set_display (GTK_WINDOW (info->icon_window), display);
      gtk_widget_set_size_request (info->icon_window, 24, 24);
      gtk_style_context_remove_class (gtk_widget_get_style_context (info->icon_window), "background");

      gtk_window_set_hardcoded_surface (GTK_WINDOW (info->icon_window),
                                        gdk_drag_get_drag_surface (drag));
      gtk_widget_show (info->icon_window);
    }

  if (gtk_bin_get_child (GTK_BIN (info->icon_window)))
    gtk_container_remove (GTK_CONTAINER (info->icon_window), gtk_bin_get_child (GTK_BIN (info->icon_window)));
  gtk_container_add (GTK_CONTAINER (info->icon_window), widget);
}

/**
 * gtk_drag_set_icon_widget:
 * @drag: the context for a drag
 * @widget: a widget to use as an icon
 * @hot_x: the X offset within @widget of the hotspot
 * @hot_y: the Y offset within @widget of the hotspot
 * 
 * Changes the icon for drag operation to a given widget.
 * GTK+ will not destroy the widget, so if you don’t want
 * it to persist, you should connect to the “drag-end” 
 * signal and destroy it yourself.
 */
void 
gtk_drag_set_icon_widget (GdkDrag        *drag,
                          GtkWidget      *widget,
                          gint            hot_x,
                          gint            hot_y)
{
  g_return_if_fail (GDK_IS_DRAG (drag));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_drag_set_icon_widget_internal (drag, widget, hot_x, hot_y, FALSE);
}

static void
set_icon_helper (GdkDrag            *drag,
                 GtkImageDefinition *def,
                 gint                hot_x,
                 gint                hot_y)
{
  GtkWidget *widget;

  widget = gtk_image_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), "drag-icon");
  gtk_image_set_from_definition (GTK_IMAGE (widget), def);
  gtk_drag_set_icon_widget_internal (drag, widget, hot_x, hot_y, TRUE);
}

void 
gtk_drag_set_icon_definition (GdkDrag            *drag,
                              GtkImageDefinition *def,
                              gint                hot_x,
                              gint                hot_y)
{
  g_return_if_fail (GDK_IS_DRAG (drag));
  g_return_if_fail (def != NULL);

  set_icon_helper (drag, def, hot_x, hot_y);
}

/**
 * gtk_drag_set_icon_paintable:
 * @drag: the context for a drag
 * @paintable: the #GdkPaintable to use as icon
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 *
 * Sets @paintable as the icon for a given drag. GTK+ retains
 * references for the arguments, and will release them when
 * they are no longer needed.
 *
 * To position the @paintable relative to the mouse, its top
 * left will be positioned @hot_x, @hot_y pixels from the
 * mouse cursor.
 */
void
gtk_drag_set_icon_paintable (GdkDrag        *drag,
                             GdkPaintable   *paintable,
                             int             hot_x,
                             int             hot_y)
{
  GtkWidget *widget;

  g_return_if_fail (GDK_IS_DRAG (drag));
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));

  widget = gtk_picture_new_for_paintable (paintable);
  gtk_picture_set_can_shrink (GTK_PICTURE (widget), FALSE);

  gtk_drag_set_icon_widget_internal (drag, widget, hot_x, hot_y, TRUE);
}

/**
 * gtk_drag_set_icon_name:
 * @drag: the context for a drag
 * @icon_name: name of icon to use
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 * 
 * Sets the icon for a given drag from a named themed icon. See
 * the docs for #GtkIconTheme for more details. Note that the
 * size of the icon depends on the icon theme (the icon is
 * loaded at the symbolic size #GTK_ICON_SIZE_DND), thus 
 * @hot_x and @hot_y have to be used with care.
 */
void 
gtk_drag_set_icon_name (GdkDrag        *drag,
                        const gchar    *icon_name,
                        gint            hot_x,
                        gint            hot_y)
{
  GtkImageDefinition *def;

  g_return_if_fail (GDK_IS_DRAG (drag));
  g_return_if_fail (icon_name != NULL && icon_name[0] != '\0');

  def = gtk_image_definition_new_icon_name (icon_name);
  set_icon_helper (drag, def, hot_x, hot_y);

  gtk_image_definition_unref (def);
}

/**
 * gtk_drag_set_icon_gicon:
 * @drag: the context for a drag
 * @icon: a #GIcon
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 * 
 * Sets the icon for a given drag from the given @icon.
 * See the documentation for gtk_drag_set_icon_name()
 * for more details about using icons in drag and drop.
 */
void 
gtk_drag_set_icon_gicon (GdkDrag        *drag,
                         GIcon          *icon,
                         gint            hot_x,
                         gint            hot_y)
{
  GtkImageDefinition *def;

  g_return_if_fail (GDK_IS_DRAG (drag));
  g_return_if_fail (icon != NULL);

  def = gtk_image_definition_new_gicon (icon);
  set_icon_helper (drag, def, hot_x, hot_y);

  gtk_image_definition_unref (def);
}

/**
 * gtk_drag_set_icon_default:
 * @drag: the context for a drag
 * 
 * Sets the icon for a particular drag to the default
 * icon.
 */
void 
gtk_drag_set_icon_default (GdkDrag *drag)
{
  g_return_if_fail (GDK_IS_DRAG (drag));

  gtk_drag_set_icon_name (drag, "text-x-generic", -2, -2);
}

/* Clean up from the drag, and display snapback, if necessary. */
static void
gtk_drag_drop_finished (GtkDragSourceInfo *info,
                        GtkDragResult      result)
{
  gboolean success;

  success = (result == GTK_DRAG_RESULT_SUCCESS);

  if (!success)
    g_signal_emit_by_name (info->widget, "drag-failed",
                           info->drag, result, &success);

  gdk_drag_drop_done (info->drag, success);
  gtk_drag_source_info_destroy (info);
}

static void
gtk_drag_drop (GtkDragSourceInfo *info)
{
  if (info->icon_window)
    gtk_widget_hide (info->icon_window);

  info->drop_timeout = g_timeout_add (DROP_ABORT_TIME, gtk_drag_abort_timeout, info);
  g_source_set_name_by_id (info->drop_timeout, "[gtk] gtk_drag_abort_timeout");
}

/*
 * Source side callbacks.
 */
static void
gtk_drag_remove_icon (GtkDragSourceInfo *info)
{
  if (info->icon_widget)
    {
      GtkWidget *widget;

      widget = info->icon_widget;
      info->icon_widget = NULL;

      g_signal_handlers_disconnect_by_func (widget, icon_widget_destroyed, info);

      gtk_widget_hide (widget);
      gtk_widget_set_opacity (widget, 1.0);

      if (info->destroy_icon)
        gtk_widget_destroy (widget);
      else
        gtk_container_remove (GTK_CONTAINER (info->icon_window), widget);

      g_object_unref (widget);
    }
}

static void
gtk_drag_source_info_free (GtkDragSourceInfo *info)
{
  gtk_drag_remove_icon (info);
  gtk_widget_destroy (info->icon_window);
  g_free (info);
}

static void
gtk_drag_source_info_destroy (GtkDragSourceInfo *info)
{
  g_signal_handlers_disconnect_by_func (info->drag, gtk_drag_drop_performed_cb, info);
  g_signal_handlers_disconnect_by_func (info->drag, gtk_drag_dnd_finished_cb, info);
  g_signal_handlers_disconnect_by_func (info->drag, gtk_drag_cancel_cb, info);

  g_signal_emit_by_name (info->widget, "drag-end", info->drag);

  g_object_set_data (G_OBJECT (info->widget), I_("gtk-info"), NULL);

  g_clear_object (&info->widget);

  gdk_content_formats_unref (info->target_list);

  if (info->drop_timeout)
    g_source_remove (info->drop_timeout);

  /* keep the icon_window alive until the (possible) drag cancel animation is done */
  g_object_set_data_full (G_OBJECT (info->drag), "former-gtk-source-info", info, (GDestroyNotify)gtk_drag_source_info_free);

  gtk_drag_clear_source_info (info->drag);
  g_object_unref (info->drag);
}

/* Called on cancellation of a drag, either by the user
 * or programmatically.
 */
static void
gtk_drag_cancel_internal (GtkDragSourceInfo *info,
                          GtkDragResult      result)
{
  gtk_drag_drop_finished (info, result);
}

static void
gtk_drag_drop_performed_cb (GdkDrag           *drag,
                            GtkDragSourceInfo *info)
{
  gtk_drag_drop (info);
}

static void
gtk_drag_cancel_cb (GdkDrag             *drag,
                    GdkDragCancelReason  reason,
                    GtkDragSourceInfo   *info)
{
  GtkDragResult result;

  switch (reason)
    {
    case GDK_DRAG_CANCEL_NO_TARGET:
      result = GTK_DRAG_RESULT_NO_TARGET;
      break;
    case GDK_DRAG_CANCEL_USER_CANCELLED:
      result = GTK_DRAG_RESULT_USER_CANCELLED;
      break;
    case GDK_DRAG_CANCEL_ERROR:
    default:
      result = GTK_DRAG_RESULT_ERROR;
      break;
    }
  gtk_drag_cancel_internal (info, result);
}

static void
gtk_drag_dnd_finished_cb (GdkDrag           *drag,
                          GtkDragSourceInfo *info)
{
  if (gdk_drag_get_selected_action (drag) == GDK_ACTION_MOVE)
    {
      g_signal_emit_by_name (info->widget,
                             "drag-data-delete",
                             drag);
    }

  gtk_drag_source_info_destroy (info);
}

static gboolean
gtk_drag_abort_timeout (gpointer data)
{
  GtkDragSourceInfo *info = data;

  info->drop_timeout = 0;
  gtk_drag_drop_finished (info, GTK_DRAG_RESULT_TIMEOUT_EXPIRED);
  
  return G_SOURCE_REMOVE;
}

/**
 * gtk_drag_check_threshold: (method)
 * @widget: a #GtkWidget
 * @start_x: X coordinate of start of drag
 * @start_y: Y coordinate of start of drag
 * @current_x: current X coordinate
 * @current_y: current Y coordinate
 * 
 * Checks to see if a mouse drag starting at (@start_x, @start_y) and ending
 * at (@current_x, @current_y) has passed the GTK+ drag threshold, and thus
 * should trigger the beginning of a drag-and-drop operation.
 *
 * Returns: %TRUE if the drag threshold has been passed.
 */
gboolean
gtk_drag_check_threshold (GtkWidget *widget,
                          gint       start_x,
                          gint       start_y,
                          gint       current_x,
                          gint       current_y)
{
  gint drag_threshold;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  drag_threshold = gtk_settings_get_dnd_drag_threshold (gtk_widget_get_settings (widget));

  return (ABS (current_x - start_x) > drag_threshold ||
          ABS (current_y - start_y) > drag_threshold);
}

/**
 * gtk_drag_cancel:
 * @drag: a drag context, as e.g. returned by gtk_drag_begin()
 *
 * Cancels an ongoing drag operation on the source side.
 *
 * If you want to be able to cancel a drag operation in this way,
 * you need to keep a pointer to the drag context, either from an
 * explicit call to gtk_drag_begin(), or by connecting to
 * #GtkWidget::drag-begin.
 *
 * If @context does not refer to an ongoing drag operation, this
 * function does nothing.
 *
 * If a drag is cancelled in this way, the @result argument of
 * #GtkWidget::drag-failed is set to @GTK_DRAG_RESULT_ERROR.
 */
void
gtk_drag_cancel (GdkDrag *drag)
{
  GtkDragSourceInfo *info;

  g_return_if_fail (GDK_IS_DRAG (drag));

  info = gtk_drag_get_source_info (drag, FALSE);
  if (info != NULL)
    gtk_drag_cancel_internal (info, GTK_DRAG_RESULT_ERROR);
}
