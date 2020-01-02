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

#include "gtkdragdestprivate.h"
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
#include "gtknative.h"
#include "gtkdragiconprivate.h"

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


typedef struct _GtkDragDestInfo GtkDragDestInfo;

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
  GdkContentFormats *target_list = NULL;
  GdkDragAction actions = 0;
  GtkDestDefaults flags = 0;

  site = g_object_get_data (G_OBJECT (data->widget), "gtk-drag-dest");

  sdata.target = data->mime_type;
  sdata.type = data->mime_type;
  sdata.format = 8;
  sdata.length = size;
  sdata.data = bytes ? bytes : (guchar *)g_strdup ("");
  sdata.display = gtk_widget_get_display (data->widget);

  if (site)
    {
      target_list = gtk_drop_target_get_formats (site->dest);
      actions = gtk_drop_target_get_actions (site->dest);
      flags = gtk_drop_target_get_defaults (site->dest);
    }

  if (target_list)
    {
      if (gdk_content_formats_contain_mime_type (target_list, data->mime_type))
        {
          if (!(flags & GTK_DEST_DEFAULT_DROP) || size >= 0)
            gtk_drop_target_emit_drag_data_received (site->dest, data->drop, &sdata);
        }
    }
  else
    {
      gtk_drop_target_emit_drag_data_received (site->dest, data->drop, &sdata);
    }
  
  if (flags & GTK_DEST_DEFAULT_DROP)
    {
      GdkDragAction action = actions & gdk_drop_get_actions (data->drop);

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

/*
 * Default drag handlers
 */
static void  
gtk_drag_dest_leave (GtkWidget      *widget,
                     GdkDrop        *drop,
                     guint           time)
{
  GtkDragDestSite *site;
  GtkDestDefaults flags;
  gboolean track_motion;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_if_fail (site != NULL);

  flags = gtk_drop_target_get_defaults (site->dest);
  track_motion = gtk_drop_target_get_track_motion (site->dest);

  if ((flags & GTK_DEST_DEFAULT_HIGHLIGHT) && site->have_drag)
    gtk_drag_unhighlight (widget);

  if (!(flags & GTK_DEST_DEFAULT_MOTION) || site->have_drag || track_motion)
    gtk_drop_target_emit_drag_leave (site->dest, drop, time);
  
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
  GdkDragAction dest_actions;
  GtkDestDefaults flags;
  gboolean track_motion;
  gboolean retval;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  dest_actions = gtk_drop_target_get_actions (site->dest);
  flags = gtk_drop_target_get_defaults (site->dest);
  track_motion = gtk_drop_target_get_track_motion (site->dest);

  if (track_motion || flags & GTK_DEST_DEFAULT_MOTION)
    {
      GdkDragAction actions;
      GdkAtom target;
      
      actions = gdk_drop_get_actions (drop);

      if ((dest_actions & actions) == 0)
        actions = 0;

      target = gtk_drop_target_match (site->dest, drop);

      if (actions && target)
        {
          if (!site->have_drag)
            {
              site->have_drag = TRUE;
              if (flags & GTK_DEST_DEFAULT_HIGHLIGHT)
                gtk_drag_highlight (widget);
            }

          gdk_drop_status (drop, dest_actions);
        }
      else
        {
          gdk_drop_status (drop, 0);
          if (!track_motion)
            return TRUE;
        }
    }

  retval = gtk_drop_target_emit_drag_motion (site->dest, drop, x, y);

  return (flags & GTK_DEST_DEFAULT_MOTION) ? TRUE : retval;
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
  GtkDestDefaults flags;
  gboolean retval;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  flags = gtk_drop_target_get_defaults (site->dest);

  info = gtk_drag_get_dest_info (drop, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  if (flags & GTK_DEST_DEFAULT_DROP)
    {
      GdkAtom target;

      target = gtk_drop_target_match (site->dest, drop);

      if (target == NULL)
        {
          gdk_drop_finish (drop, 0);
          return TRUE;
        }
      else 
        gtk_drag_get_data (widget, drop, target);
    }

  retval = gtk_drop_target_emit_drag_drop (site->dest, drop, x, y);

  return (flags & GTK_DEST_DEFAULT_DROP) ? TRUE : retval;
}
