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
  GtkDropTarget *dest;
  GtkDestDefaults flags;
  gboolean track_motion;
  gboolean armed;

  dest = gtk_drop_target_get (widget);
  g_return_if_fail (dest != NULL);

  flags = gtk_drop_target_get_defaults (dest);
  track_motion = gtk_drop_target_get_track_motion (dest);
  armed = gtk_drop_target_get_armed (dest);

  if (!(flags & GTK_DEST_DEFAULT_MOTION) || armed || track_motion)
    gtk_drop_target_emit_drag_leave (dest, drop, time);
  
  gtk_drop_target_set_armed (dest, FALSE);
}

static gboolean
gtk_drag_dest_motion (GtkWidget *widget,
                      GdkDrop   *drop,
                      gint       x,
                      gint       y,
                      guint      time)
{
  GtkDropTarget *dest;
  GdkDragAction dest_actions;
  GtkDestDefaults flags;
  gboolean track_motion;
  gboolean retval;

  dest = gtk_drop_target_get (widget);
  g_return_val_if_fail (dest != NULL, FALSE);

  dest_actions = gtk_drop_target_get_actions (dest);
  flags = gtk_drop_target_get_defaults (dest);
  track_motion = gtk_drop_target_get_track_motion (dest);

  if (track_motion || flags & GTK_DEST_DEFAULT_MOTION)
    {
      GdkDragAction actions;
      GdkAtom target;
      
      actions = gdk_drop_get_actions (drop);

      if ((dest_actions & actions) == 0)
        actions = 0;

      target = gtk_drop_target_match (dest, drop);

      if (actions && target)
        {
          gtk_drop_target_set_armed (dest, TRUE);
          gdk_drop_status (drop, dest_actions);
        }
      else
        {
          gdk_drop_status (drop, 0);
          if (!track_motion)
            return TRUE;
        }
    }

  retval = gtk_drop_target_emit_drag_motion (dest, drop, x, y);

  return (flags & GTK_DEST_DEFAULT_MOTION) ? TRUE : retval;
}

static gboolean
gtk_drag_dest_drop (GtkWidget *widget,
                    GdkDrop   *drop,
                    gint       x,
                    gint       y,
                    guint      time)
{
  GtkDropTarget *dest;
  GtkDragDestInfo *info;

  dest = gtk_drop_target_get (widget);
  g_return_val_if_fail (dest != NULL, FALSE);

  info = gtk_drag_get_dest_info (drop, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  return gtk_drop_target_emit_drag_drop (dest, drop, x, y);
}
