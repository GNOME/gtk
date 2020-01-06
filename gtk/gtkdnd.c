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

/* Forward declarations */
static GtkWidget *gtk_drop_find_widget          (GtkWidget        *widget,
                                                 GdkEvent         *event);
static void     gtk_drag_dest_leave             (GtkWidget        *widget,
                                                 GdkDrop          *drop);
static gboolean gtk_drop_target_handle_event    (GtkDropTarget    *dest,
                                                 GdkEvent         *event);
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
  GdkEventType event_type;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  event_type = gdk_event_get_event_type (event);
  drop = gdk_event_get_drop (event);

  info = gtk_drag_get_dest_info (drop, TRUE);

  /* Find the widget for the event */
  switch ((guint) event_type)
    {
    case GDK_DRAG_ENTER:
      break;
      
    case GDK_DRAG_LEAVE:
      if (info->widget)
        {
          gtk_drag_dest_leave (info->widget, drop);
          gtk_drag_dest_set_widget (info, NULL);
        }
      break;
      
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      {
        GtkWidget *widget;

        if (event_type == GDK_DROP_START)
          {
            /* We send a leave here so that the widget unhighlights
             * properly.
             */
            if (info->widget)
              {
                gtk_drag_dest_leave (info->widget, drop);
                gtk_drag_dest_set_widget (info, NULL);
              }
          }

        widget = gtk_drop_find_widget (toplevel, event);

        if (info->widget && info->widget != widget)
          {
            gtk_drag_dest_leave (info->widget, drop);
            gtk_drag_dest_set_widget (info, NULL);
          }

        if (widget)
          gtk_drag_dest_set_widget (info, widget);
        else
          gdk_drop_status (drop, 0);
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

static GtkWidget *
gtk_drop_find_widget (GtkWidget *event_widget,
                      GdkEvent  *event)
{
  GtkWidget *widget;
  double x, y;
  int wx, wy;

  if (!gtk_widget_get_mapped (event_widget) ||
      !gtk_widget_get_sensitive (event_widget))
    return NULL;

  gdk_event_get_coords (event, &x, &y);

  widget = gtk_widget_pick (event_widget, x, y, GTK_PICK_DEFAULT);

  if (!widget)
    return NULL;

  gtk_widget_translate_coordinates (event_widget, widget, x, y, &wx, &wy);

  while (widget)
    {
      GtkDropTarget *dest;
      GtkWidget *parent;
      GList *hierarchy = NULL;
      gboolean found = FALSE;

      if (!gtk_widget_get_mapped (widget))
        return NULL;

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
      dest = gtk_drop_target_get (widget);
      if (dest)
        {
          gdk_event_set_coords (event, wx, wy);
          found = gtk_drop_target_handle_event (dest, event);
          gdk_event_set_coords (event, x, y);
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
        return widget;

      if (parent)
        g_object_remove_weak_pointer (G_OBJECT (parent), (gpointer *) &parent);
      else
        return NULL;

      if (!gtk_widget_translate_coordinates (widget, parent, wx, wy, &wx, &wy))
        return NULL;

      widget = parent;
    }

  return NULL;
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
gtk_drag_dest_leave (GtkWidget *widget,
                     GdkDrop   *drop)
{
  GtkDropTarget *dest;

  dest = gtk_drop_target_get (widget);
  g_return_if_fail (dest != NULL);

  gtk_drop_target_emit_drag_leave (dest, drop);
}

static gboolean
gtk_drop_target_handle_event (GtkDropTarget *dest,
                              GdkEvent      *event)
{
  GdkDrop *drop;
  double x, y;

  drop = gdk_event_get_drop (event);
  gdk_event_get_coords (event, &x, &y);

  switch ((int)gdk_event_get_event_type (event))
    {
    case GDK_DRAG_MOTION:
      return gtk_drop_target_emit_drag_motion (dest, drop, x, y);
    case GDK_DROP_START:
      return gtk_drop_target_emit_drag_drop (dest, drop, x, y);
    default:
      break;
    }

  return FALSE;
}
