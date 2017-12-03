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

#include "gtkdnd.h"
#include "gtkdndprivate.h"
#include "gtksettingsprivate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gdk/gdk.h"

#include "gdk/gdkcontentformatsprivate.h"

#include "gtkdragdest.h"
#include "gtkimageprivate.h"
#include "gtkintl.h"
#include "gtktooltipprivate.h"
#include "gtkwindow.h"
#include "gtkselectionprivate.h"
#include "gtkwindowgroup.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"


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


static GSList *source_widgets = NULL;

typedef struct _GtkDragSourceInfo GtkDragSourceInfo;
typedef struct _GtkDragDestInfo GtkDragDestInfo;

struct _GtkDragSourceInfo 
{
  GtkWidget         *widget;
  GdkContentFormats *target_list; /* Targets for drag data */
  GdkDragAction      possible_actions; /* Actions allowed by source */
  GdkDragContext    *context;     /* drag context */
  GtkWidget         *icon_window; /* Window for drag */
  GtkWidget         *icon_widget; /* Widget for drag */
  GtkWidget         *ipc_widget;  /* GtkInvisible for grab, message passing */

  GList             *selections;  /* selections we've claimed */

  guint              drop_timeout;     /* Timeout for aborting drop */
  guint              destroy_icon : 1; /* If true, destroy icon_widget */
};

struct _GtkDragDestInfo
{
  GtkWidget         *widget;              /* Widget in which drag is in */
  GdkDragContext    *context;             /* Drag context */
  guint              dropped : 1;         /* Set after we receive a drop */
  gint               drop_x, drop_y;      /* Position of drop */
};

#define DROP_ABORT_TIME 300000

typedef gboolean (* GtkDragDestCallback) (GtkWidget      *widget,
                                          GdkDragContext *context,
                                          gint            x,
                                          gint            y,
                                          guint32         time);

/* Forward declarations */
static void          gtk_drag_get_event_actions (const GdkEvent  *event,
                                                 gint             button,
                                                 GdkDragAction    actions,
                                                 GdkDragAction   *suggested_action,
                                                 GdkDragAction   *possible_actions);
static GtkWidget    *gtk_drag_get_ipc_widget            (GtkWidget *widget);
static GtkWidget    *gtk_drag_get_ipc_widget_for_display(GdkDisplay*display);
static void          gtk_drag_release_ipc_widget (GtkWidget      *widget);

static void     gtk_drag_selection_received     (GtkWidget        *widget,
                                                 GtkSelectionData *selection_data,
                                                 guint             time,
                                                 gpointer          data);
static gboolean gtk_drag_find_widget            (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 GtkDragDestInfo  *info,
                                                 gint              x,
                                                 gint              y,
                                                 guint32           time,
                                                 GtkDragDestCallback callback);
static void     gtk_drag_dest_leave             (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 guint             time);
static gboolean gtk_drag_dest_motion            (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 gint              x,
                                                 gint              y,
                                                 guint             time);
static gboolean gtk_drag_dest_drop              (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 gint              x,
                                                 gint              y,
                                                 guint             time);
static void     gtk_drag_dest_set_widget        (GtkDragDestInfo  *info,
                                                 GtkWidget        *widget);

static GtkDragDestInfo *  gtk_drag_get_dest_info     (GdkDragContext *context,
                                                      gboolean        create);
static GtkDragSourceInfo *gtk_drag_get_source_info   (GdkDragContext *context,
                                                      gboolean        create);
static void               gtk_drag_clear_source_info (GdkDragContext *context);

static void gtk_drag_source_check_selection    (GtkDragSourceInfo *info, 
                                                GdkAtom            selection,
                                                guint32            time);
static void gtk_drag_source_release_selections (GtkDragSourceInfo *info,
                                                guint32            time);
static void gtk_drag_drop                      (GtkDragSourceInfo *info,
                                                guint32            time);
static void gtk_drag_drop_finished             (GtkDragSourceInfo *info,
                                                GtkDragResult      result,
                                                guint              time);
static void gtk_drag_cancel_internal           (GtkDragSourceInfo *info,
                                                GtkDragResult      result,
                                                guint32            time);

static void gtk_drag_selection_get             (GtkWidget         *widget, 
                                                GtkSelectionData  *selection_data,
                                                guint32            time,
                                                gpointer           data);
static void gtk_drag_remove_icon               (GtkDragSourceInfo *info);
static void gtk_drag_source_info_destroy       (GtkDragSourceInfo *info);

static void gtk_drag_context_drop_performed_cb (GdkDragContext    *context,
                                                guint              time,
                                                GtkDragSourceInfo *info);
static void gtk_drag_context_cancel_cb         (GdkDragContext      *context,
                                                GdkDragCancelReason  reason,
                                                GtkDragSourceInfo   *info);
static void gtk_drag_context_dnd_finished_cb   (GdkDragContext    *context,
                                                GtkDragSourceInfo *info);

static gboolean gtk_drag_abort_timeout         (gpointer           data);

static void     set_icon_helper (GdkDragContext    *context,
                                 GtkImageDefinition*def,
                                 gint               hot_x,
                                 gint               hot_y);


/*********************
 * Utility functions *
 *********************/

static GtkWidget *
gtk_drag_get_ipc_widget_for_display (GdkDisplay *display)
{
  GtkWidget *result;
  GSList *drag_widgets = g_object_get_data (G_OBJECT (display), 
                                            "gtk-dnd-ipc-widgets");
  
  if (drag_widgets)
    {
      GSList *tmp = drag_widgets;
      result = drag_widgets->data;
      drag_widgets = drag_widgets->next;
      g_object_set_data (G_OBJECT (display),
                         I_("gtk-dnd-ipc-widgets"),
                         drag_widgets);
      g_slist_free_1 (tmp);
    }
  else
    {
      result = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_display (GTK_WINDOW (result), display);
      gtk_window_resize (GTK_WINDOW (result), 1, 1);
      gtk_window_move (GTK_WINDOW (result), -99, -99);
      gtk_widget_show (result);
    }  

  return result;
}

static GtkWidget *
gtk_drag_get_ipc_widget (GtkWidget *widget)
{
  GtkWidget *result;
  GtkWidget *toplevel;

  result = gtk_drag_get_ipc_widget_for_display (gtk_widget_get_display (widget));
  
  toplevel = gtk_widget_get_toplevel (widget);
  
  if (GTK_IS_WINDOW (toplevel))
    {
      if (gtk_window_has_group (GTK_WINDOW (toplevel)))
        gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                     GTK_WINDOW (result));
    }

  return result;
}

/*
 * gtk_drag_release_ipc_widget:
 * @widget: the widget to release
 *
 * Releases widget retrieved with gtk_drag_get_ipc_widget().
 */
static void
gtk_drag_release_ipc_widget (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GdkDisplay *display = gtk_widget_get_display (widget);
  GSList *drag_widgets = g_object_get_data (G_OBJECT (display),
                                            "gtk-dnd-ipc-widgets");

  if (gtk_window_has_group (window))
    gtk_window_group_remove_window (gtk_window_get_group (window),
                                    window);
  drag_widgets = g_slist_prepend (drag_widgets, widget);
  g_object_set_data (G_OBJECT (display),
                     I_("gtk-dnd-ipc-widgets"),
                     drag_widgets);
}

static void
gtk_drag_get_event_actions (const GdkEvent *event,
                            gint            button,
                            GdkDragAction   actions,
                            GdkDragAction  *suggested_action,
                            GdkDragAction  *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if (event)
    {
      GdkModifierType state = 0;

      gdk_event_get_state (event, &state);

      if ((button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY) && (actions & GDK_ACTION_ASK))
        {
          *suggested_action = GDK_ACTION_ASK;
          *possible_actions = actions;
        }
      else if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
        {
          if ((state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK))
            {
              if (actions & GDK_ACTION_LINK)
                {
                  *suggested_action = GDK_ACTION_LINK;
                  *possible_actions = GDK_ACTION_LINK;
                }
            }
          else if (state & GDK_CONTROL_MASK)
            {
              if (actions & GDK_ACTION_COPY)
                {
                  *suggested_action = GDK_ACTION_COPY;
                  *possible_actions = GDK_ACTION_COPY;
                }
            }
          else
            {
              if (actions & GDK_ACTION_MOVE)
                {
                  *suggested_action = GDK_ACTION_MOVE;
                  *possible_actions = GDK_ACTION_MOVE;
                }
            }
        }
      else
        {
          *possible_actions = actions;

          if ((state & (GDK_MOD1_MASK)) && (actions & GDK_ACTION_ASK))
            *suggested_action = GDK_ACTION_ASK;
          else if (actions & GDK_ACTION_COPY)
            *suggested_action =  GDK_ACTION_COPY;
          else if (actions & GDK_ACTION_MOVE)
            *suggested_action = GDK_ACTION_MOVE;
          else if (actions & GDK_ACTION_LINK)
            *suggested_action = GDK_ACTION_LINK;
        }
    }
  else
    {
      *possible_actions = actions;
      
      if (actions & GDK_ACTION_COPY)
        *suggested_action =  GDK_ACTION_COPY;
      else if (actions & GDK_ACTION_MOVE)
        *suggested_action = GDK_ACTION_MOVE;
      else if (actions & GDK_ACTION_LINK)
        *suggested_action = GDK_ACTION_LINK;
    }
}

/********************
 * Destination side *
 ********************/

/**
 * gtk_drag_get_data: (method)
 * @widget: the widget that will receive the
 *   #GtkWidget::drag-data-received signal
 * @context: the drag context
 * @target: the target (form of the data) to retrieve
 * @time_: a timestamp for retrieving the data. This will
 *   generally be the time received in a #GtkWidget::drag-motion
 *   or #GtkWidget::drag-drop signal
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
gtk_drag_get_data (GtkWidget      *widget,
                   GdkDragContext *context,
                   GdkAtom         target,
                   guint32         time_)
{
  GtkWidget *selection_widget;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  selection_widget = gtk_drag_get_ipc_widget (widget);

  g_object_ref (context);
  g_object_ref (widget);

  g_signal_connect (selection_widget, "selection-received",
                    G_CALLBACK (gtk_drag_selection_received), widget);

  g_object_set_data (G_OBJECT (selection_widget), I_("drag-context"), context);

  gtk_selection_convert (selection_widget,
                         gdk_drag_get_selection (context),
                         target,
                         time_);
}

/**
 * gtk_drag_get_source_widget: (method)
 * @context: a (destination side) drag context
 *
 * Determines the source widget for a drag.
 *
 * Returns: (nullable) (transfer none): if the drag is occurring
 *     within a single application, a pointer to the source widget.
 *     Otherwise, %NULL.
 */
GtkWidget *
gtk_drag_get_source_widget (GdkDragContext *context)
{
  GSList *tmp_list;

  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);
  
  tmp_list = source_widgets;
  while (tmp_list)
    {
      GtkWidget *ipc_widget = tmp_list->data;

      if (gtk_widget_get_window (ipc_widget) == gdk_drag_context_get_source_window (context))
        {
          GtkDragSourceInfo *info;
          info = g_object_get_data (G_OBJECT (ipc_widget), "gtk-info");

          return info ? info->widget : NULL;
        }

      tmp_list = tmp_list->next;
    }

  return NULL;
}

/**
 * gtk_drag_finish: (method)
 * @context: the drag context
 * @success: a flag indicating whether the drop was successful
 * @del: a flag indicating whether the source should delete the
 *   original data. (This should be %TRUE for a move)
 * @time_: the timestamp from the #GtkWidget::drag-drop signal
 *
 * Informs the drag source that the drop is finished, and
 * that the data of the drag will no longer be required.
 */
void 
gtk_drag_finish (GdkDragContext *context,
                 gboolean        success,
                 gboolean        del,
                 guint32         time)
{
  GdkAtom target = NULL;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  if (success && del)
    {
      target = gdk_atom_intern_static_string ("DELETE");
    }

  if (target != NULL)
    {
      GtkWidget *selection_widget = gtk_drag_get_ipc_widget_for_display (gdk_window_get_display (gdk_drag_context_get_source_window (context)));

      g_object_ref (context);
      
      g_object_set_data (G_OBJECT (selection_widget), I_("drag-context"), context);
      g_signal_connect (selection_widget, "selection-received",
                        G_CALLBACK (gtk_drag_selection_received),
                        NULL);
      
      gtk_selection_convert (selection_widget,
                             gdk_drag_get_selection (context),
                             target,
                             time);
    }
  
  if (!(success && del))
    gdk_drop_finish (context, success, time);
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
  GdkDragContext *context;
  guint32 time;
  GdkEventType event_type;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  event_type = gdk_event_get_event_type (event);
  gdk_event_get_drag_context (event, &context);
  time = gdk_event_get_time (event);

  info = gtk_drag_get_dest_info (context, TRUE);

  /* Find the widget for the event */
  switch ((guint) event_type)
    {
    case GDK_DRAG_ENTER:
      break;
      
    case GDK_DRAG_LEAVE:
      if (info->widget)
        {
          gtk_drag_dest_leave (info->widget, context, time);
          gtk_drag_dest_set_widget (info, NULL);
        }
      break;
      
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      {
        GdkWindow *window;
        gint tx, ty;
        double x_root, y_root;
        gboolean found;

        if (event_type == GDK_DROP_START)
          {
            info->dropped = TRUE;
            /* We send a leave here so that the widget unhighlights
             * properly.
             */
            if (info->widget)
              {
                gtk_drag_dest_leave (info->widget, context, time);
                gtk_drag_dest_set_widget (info, NULL);
              }
          }

        window = gtk_widget_get_window (toplevel);

        gdk_window_get_position (window, &tx, &ty);
        gdk_event_get_root_coords (event, &x_root, &y_root);

        found = gtk_drag_find_widget (toplevel,
                                      context,
                                      info,
                                      x_root - tx,
                                      y_root - ty,
                                      time,
                                      (event_type == GDK_DRAG_MOTION) ?
                                      gtk_drag_dest_motion :
                                      gtk_drag_dest_drop);

        if (info->widget && !found)
          {
            gtk_drag_dest_leave (info->widget, context, time);
            gtk_drag_dest_set_widget (info, NULL);
          }
        
        /* Send a reply.
         */
        if (event_type == GDK_DRAG_MOTION)
          {
            if (!found)
              gdk_drag_status (context, 0, time);
          }
        else if (event_type == GDK_DROP_START)
          {
            gdk_drop_reply (context, found, time);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_drag_selection_received (GtkWidget        *widget,
                             GtkSelectionData *selection_data,
                             guint             time,
                             gpointer          data)
{
  GdkDragContext *context;
  GtkDragDestInfo *info;
  GtkWidget *drop_widget;
  GdkAtom target;

  drop_widget = data;

  context = g_object_get_data (G_OBJECT (widget), "drag-context");
  info = gtk_drag_get_dest_info (context, FALSE);

  target = gtk_selection_data_get_target (selection_data);
  if (target == gdk_atom_intern_static_string ("DELETE"))
    {
      gtk_drag_finish (context, TRUE, FALSE, time);
    }
  else
    {
      GtkDragDestSite *site;

      site = g_object_get_data (G_OBJECT (drop_widget), "gtk-drag-dest");

      if (site && site->target_list)
        {
          if (gdk_content_formats_contain_mime_type (site->target_list, target))
            {
              if (!(site->flags & GTK_DEST_DEFAULT_DROP) ||
                  gtk_selection_data_get_length (selection_data) >= 0)
                g_signal_emit_by_name (drop_widget,
                                       "drag-data-received",
                                       context, info->drop_x, info->drop_y,
                                       selection_data,
                                       time);
            }
        }
      else
        {
          g_signal_emit_by_name (drop_widget,
                                 "drag-data-received",
                                 context, info->drop_x, info->drop_y,
                                 selection_data,
                                 time);
        }
      
      if (site && site->flags & GTK_DEST_DEFAULT_DROP)
        {

          gtk_drag_finish (context, 
                           (gtk_selection_data_get_length (selection_data) >= 0),
                           (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE),
                           time);
        }
      
      g_object_unref (drop_widget);
    }

  g_signal_handlers_disconnect_by_func (widget,
                                        gtk_drag_selection_received,
                                        data);
  
  g_object_set_data (G_OBJECT (widget), I_("drag-context"), NULL);
  g_object_unref (context);

  gtk_drag_release_ipc_widget (widget);
}

static gboolean
gtk_drag_find_widget (GtkWidget           *widget,
                      GdkDragContext      *context,
                      GtkDragDestInfo     *info,
                      gint                 x,
                      gint                 y,
                      guint32              time,
                      GtkDragDestCallback  callback)
{
  if (!gtk_widget_get_mapped (widget) ||
      !gtk_widget_get_sensitive (widget))
    return FALSE;

  /* Get the widget at the pointer coordinates and travel up
   * the widget hierarchy from there.
   */
  widget = _gtk_widget_find_at_coords (gtk_widget_get_window (widget),
                                       x, y, &x, &y);
  if (!widget)
    return FALSE;

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
          found = callback (widget, context, x, y, time);

          /* If so, send a "drag-leave" to the last widget */
          if (found && info->widget != widget)
            {
              if (info->widget)
                gtk_drag_dest_leave (info->widget, context, time);

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
gtk_drag_get_dest_info (GdkDragContext *context,
                        gboolean        create)
{
  GtkDragDestInfo *info;
  static GQuark info_quark = 0;
  if (!info_quark)
    info_quark = g_quark_from_static_string ("gtk-dest-info");
  
  info = g_object_get_qdata (G_OBJECT (context), info_quark);
  if (!info && create)
    {
      info = g_slice_new0 (GtkDragDestInfo);
      info->context = context;
      g_object_set_qdata_full (G_OBJECT (context), info_quark,
                               info, gtk_drag_dest_info_destroy);
    }

  return info;
}

static GQuark dest_info_quark = 0;

static GtkDragSourceInfo *
gtk_drag_get_source_info (GdkDragContext *context,
                          gboolean        create)
{
  GtkDragSourceInfo *info;
  if (!dest_info_quark)
    dest_info_quark = g_quark_from_static_string ("gtk-source-info");
  
  info = g_object_get_qdata (G_OBJECT (context), dest_info_quark);
  if (!info && create)
    {
      info = g_new0 (GtkDragSourceInfo, 1);
      info->context = context;
      g_object_set_qdata (G_OBJECT (context), dest_info_quark, info);
    }

  return info;
}

static void
gtk_drag_clear_source_info (GdkDragContext *context)
{
  g_object_set_qdata (G_OBJECT (context), dest_info_quark, NULL);
}

/*
 * Default drag handlers
 */
static void  
gtk_drag_dest_leave (GtkWidget      *widget,
                     GdkDragContext *context,
                     guint           time)
{
  GtkDragDestSite *site;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_if_fail (site != NULL);

  if ((site->flags & GTK_DEST_DEFAULT_HIGHLIGHT) && site->have_drag)
    gtk_drag_unhighlight (widget);

  if (!(site->flags & GTK_DEST_DEFAULT_MOTION) || site->have_drag ||
      site->track_motion)
    g_signal_emit_by_name (widget, "drag-leave", context, time);
  
  site->have_drag = FALSE;
}

static gboolean
gtk_drag_dest_motion (GtkWidget      *widget,
                      GdkDragContext *context,
                      gint            x,
                      gint            y,
                      guint           time)
{
  GtkDragDestSite *site;
  GdkDragAction action = 0;
  gboolean retval;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  if (site->track_motion || site->flags & GTK_DEST_DEFAULT_MOTION)
    {
      if (gdk_drag_context_get_suggested_action (context) & site->actions)
        action = gdk_drag_context_get_suggested_action (context);
      else
        {
          gint i;
          
          for (i = 0; i < 8; i++)
            {
              if ((site->actions & (1 << i)) &&
                  (gdk_drag_context_get_actions (context) & (1 << i)))
                {
                  action = (1 << i);
                  break;
                }
            }
        }

      if (action && gtk_drag_dest_find_target (widget, context, NULL))
        {
          if (!site->have_drag)
            {
              site->have_drag = TRUE;
              if (site->flags & GTK_DEST_DEFAULT_HIGHLIGHT)
                gtk_drag_highlight (widget);
            }

          gdk_drag_status (context, action, time);
        }
      else
        {
          gdk_drag_status (context, 0, time);
          if (!site->track_motion)
            return TRUE;
        }
    }

  g_signal_emit_by_name (widget, "drag-motion",
                         context, x, y, time, &retval);

  return (site->flags & GTK_DEST_DEFAULT_MOTION) ? TRUE : retval;
}

static gboolean
gtk_drag_dest_drop (GtkWidget      *widget,
                    GdkDragContext *context,
                    gint            x,
                    gint            y,
                    guint           time)
{
  GtkDragDestSite *site;
  GtkDragDestInfo *info;
  gboolean retval;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  info = gtk_drag_get_dest_info (context, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  info->drop_x = x;
  info->drop_y = y;

  if (site->flags & GTK_DEST_DEFAULT_DROP)
    {
      GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

      if (target == NULL)
        {
          gtk_drag_finish (context, FALSE, FALSE, time);
          return TRUE;
        }
      else 
        gtk_drag_get_data (widget, context, target, time);
    }

  g_signal_emit_by_name (widget, "drag-drop",
                         context, x, y, time, &retval);

  return (site->flags & GTK_DEST_DEFAULT_DROP) ? TRUE : retval;
}

/***************
 * Source side *
 ***************/

/* Like gtk_drag_begin(), but also takes a GtkIconHelper
 * so that we can set the icon from the source site information
 */
GdkDragContext *
gtk_drag_begin_internal (GtkWidget          *widget,
                         GtkImageDefinition *icon,
                         GdkContentFormats  *target_list,
                         GdkDragAction       actions,
                         gint                button,
                         const GdkEvent     *event,
                         int                 x,
                         int                 y)
{
  GtkDragSourceInfo *info;
  guint32 time = GDK_CURRENT_TIME;
  GdkDragAction possible_actions, suggested_action;
  GdkDragContext *context;
  GtkWidget *ipc_widget;
  GdkDevice *pointer, *keyboard;
  GdkWindow *ipc_window;
  int start_x, start_y;
  GdkAtom selection;

  pointer = keyboard = NULL;
  ipc_widget = gtk_drag_get_ipc_widget (widget);

  gtk_drag_get_event_actions (event, button, actions,
                              &suggested_action, &possible_actions);

  if (event)
    {
      time = gdk_event_get_time (event);
      if (time == GDK_CURRENT_TIME)
        time = gtk_get_current_event_time ();

      pointer = gdk_event_get_device (event);

      if (gdk_device_get_source (pointer) == GDK_SOURCE_KEYBOARD)
        {
          keyboard = pointer;
          pointer = gdk_device_get_associated_device (keyboard);
        }
      else
        keyboard = gdk_device_get_associated_device (pointer);
    }
  else
    {
      GdkSeat *seat;

      seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
      pointer = gdk_seat_get_pointer (seat);
      keyboard = gdk_seat_get_keyboard (seat);
    }

  if (!pointer)
    return NULL;

  ipc_window = gtk_widget_get_window (ipc_widget);

  source_widgets = g_slist_prepend (source_widgets, ipc_widget);

  if (x != -1 && y != -1)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
      gtk_widget_translate_coordinates (widget, toplevel,
                                        x, y, &x, &y);
      gdk_window_get_root_coords (gtk_widget_get_window (toplevel),
                                  x, y, &start_x, &start_y);
    }
  else if (event && gdk_event_get_event_type (event) == GDK_MOTION_NOTIFY)
    {
      double x, y;

      gdk_event_get_root_coords (event, &x, &y);
      start_x = (int)x;
      start_y = (int)y;
    }
  else
    gdk_device_get_position (pointer, &start_x, &start_y);

  context = gdk_drag_begin_from_point (ipc_window, pointer, target_list, start_x, start_y);

  gdk_drag_context_set_device (context, pointer);

  if (!gdk_drag_context_manage_dnd (context, ipc_window, actions))
    {
      gtk_drag_release_ipc_widget (ipc_widget);
      g_object_unref (context);
      return NULL;
    }

  info = gtk_drag_get_source_info (context, TRUE);

  info->ipc_widget = ipc_widget;
  g_object_set_data (G_OBJECT (info->ipc_widget), I_("gtk-info"), info);

  info->widget = g_object_ref (widget);

  info->target_list = target_list;
  gdk_content_formats_ref (target_list);

  info->possible_actions = actions;

  info->selections = NULL;
  info->icon_window = NULL;
  info->icon_widget = NULL;
  info->destroy_icon = FALSE;

  gtk_widget_reset_controllers (widget);

  g_signal_emit_by_name (widget, "drag-begin", info->context);

  /* Ensure that we have an icon before we start the drag; the
   * application may have set one in ::drag_begin, or it may
   * not have set one.
   */
  if (!info->icon_widget)
    {
      if (icon)
        {
          set_icon_helper (info->context, icon, 0, 0);
        }
      else
        {
          icon = gtk_image_definition_new_icon_name ("text-x-generic");
          set_icon_helper (info->context, icon, 0, 0);
          gtk_image_definition_unref (icon);
        }
    }

  g_signal_connect (context, "drop-performed",
                    G_CALLBACK (gtk_drag_context_drop_performed_cb), info);
  g_signal_connect (context, "dnd-finished",
                    G_CALLBACK (gtk_drag_context_dnd_finished_cb), info);
  g_signal_connect (context, "cancel",
                    G_CALLBACK (gtk_drag_context_cancel_cb), info);

  selection = gdk_drag_get_selection (context);
  if (selection)
    gtk_drag_source_check_selection (info, selection, time);

  g_signal_connect (info->ipc_widget, "selection-get",
                    G_CALLBACK (gtk_drag_selection_get), info);

  return info->context;
}

/**
 * gtk_drag_begin_with_coordinates: (method)
 * @widget: the source widget
 * @targets: The targets (data formats) in which the
 *    source can provide the data
 * @actions: A bitmask of the allowed drag actions for this drag
 * @button: The button the user clicked to start the drag
 * @event: (nullable): The event that triggered the start of the drag,
 *    or %NULL if none can be obtained.
 * @x: The initial x coordinate to start dragging from, in the coordinate space
 *    of @widget. If -1 is passed, the coordinates are retrieved from @event or
 *    the current pointer position
 * @y: The initial y coordinate to start dragging from, in the coordinate space
 *    of @widget. If -1 is passed, the coordinates are retrieved from @event or
 *    the current pointer position
 *
 * Initiates a drag on the source side. The function only needs to be used
 * when the application is starting drags itself, and is not needed when
 * gtk_drag_source_set() is used.
 *
 * The @event is used to retrieve the timestamp that will be used internally to
 * grab the pointer.  If @event is %NULL, then %GDK_CURRENT_TIME will be used.
 * However, you should try to pass a real event in all cases, since that can be
 * used to get information about the drag.
 *
 * Generally there are three cases when you want to start a drag by hand by
 * calling this function:
 *
 * 1. During a #GtkWidget::button-press-event handler, if you want to start a drag
 * immediately when the user presses the mouse button.  Pass the @event
 * that you have in your #GtkWidget::button-press-event handler.
 *
 * 2. During a #GtkWidget::motion-notify-event handler, if you want to start a drag
 * when the mouse moves past a certain threshold distance after a button-press.
 * Pass the @event that you have in your #GtkWidget::motion-notify-event handler.
 *
 * 3. During a timeout handler, if you want to start a drag after the mouse
 * button is held down for some time.  Try to save the last event that you got
 * from the mouse, using gdk_event_copy(), and pass it to this function
 * (remember to free the event with gdk_event_free() when you are done).
 * If you can really not pass a real event, pass %NULL instead.
 *
 * Returns: (transfer none): the context for this drag
 *
 * Since: 3.10
 */
GdkDragContext *
gtk_drag_begin_with_coordinates (GtkWidget         *widget,
                                 GdkContentFormats *targets,
                                 GdkDragAction      actions,
                                 gint               button,
                                 GdkEvent          *event,
                                 gint               x,
                                 gint               y)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_get_realized (widget), NULL);
  g_return_val_if_fail (targets != NULL, NULL);

  return gtk_drag_begin_internal (widget, NULL, targets,
                                  actions, button, event, x, y);
}

static void
icon_widget_destroyed (GtkWidget         *widget,
                       GtkDragSourceInfo *info)
{
  g_clear_object (&info->icon_widget);
}

static void
gtk_drag_set_icon_widget_internal (GdkDragContext *context,
                                   GtkWidget      *widget,
                                   gint            hot_x,
                                   gint            hot_y,
                                   gboolean        destroy_on_release)
{
  GtkDragSourceInfo *info;

  g_return_if_fail (!GTK_IS_WINDOW (widget));

  info = gtk_drag_get_source_info (context, FALSE);
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

  gdk_drag_context_set_hotspot (context, hot_x, hot_y);

  if (!info->icon_window)
    {
      GdkDisplay *display;

      display = gdk_window_get_display (gdk_drag_context_get_source_window (context));

      info->icon_window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_type_hint (GTK_WINDOW (info->icon_window), GDK_WINDOW_TYPE_HINT_DND);
      gtk_window_set_display (GTK_WINDOW (info->icon_window), display);
      gtk_widget_set_size_request (info->icon_window, 24, 24);
      gtk_style_context_remove_class (gtk_widget_get_style_context (info->icon_window), "background");

      gtk_window_set_hardcoded_window (GTK_WINDOW (info->icon_window),
                                       gdk_drag_context_get_drag_window (context));
      gtk_widget_show (info->icon_window);
    }

  if (gtk_bin_get_child (GTK_BIN (info->icon_window)))
    gtk_container_remove (GTK_CONTAINER (info->icon_window), gtk_bin_get_child (GTK_BIN (info->icon_window)));
  gtk_container_add (GTK_CONTAINER (info->icon_window), widget);
}

/**
 * gtk_drag_set_icon_widget: (method)
 * @context: the context for a drag. (This must be called 
          with a context for the source side of a drag)
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
gtk_drag_set_icon_widget (GdkDragContext *context,
                          GtkWidget      *widget,
                          gint            hot_x,
                          gint            hot_y)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_drag_set_icon_widget_internal (context, widget, hot_x, hot_y, FALSE);
}

static void
set_icon_helper (GdkDragContext     *context,
                 GtkImageDefinition *def,
                 gint                hot_x,
                 gint                hot_y)
{
  GtkWidget *widget;

  widget = gtk_image_new ();
  gtk_widget_show (widget);

  gtk_image_set_from_definition (GTK_IMAGE (widget), def);

  gtk_drag_set_icon_widget_internal (context, widget, hot_x, hot_y, TRUE);
}

void 
gtk_drag_set_icon_definition (GdkDragContext     *context,
                              GtkImageDefinition *def,
                              gint                hot_x,
                              gint                hot_y)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (def != NULL);

  set_icon_helper (context, def, hot_x, hot_y);
}

/**
 * gtk_drag_set_icon_surface: (method)
 * @context: the context for a drag (This must be called
 *     with a context for the source side of a drag)
 * @surface: the surface to use as icon
 *
 * Sets @surface as the icon for a given drag. GTK+ retains
 * references for the arguments, and will release them when
 * they are no longer needed.
 *
 * To position the surface relative to the mouse, use
 * cairo_surface_set_device_offset() on @surface. The mouse
 * cursor will be positioned at the (0,0) coordinate of the
 * surface.
 */
void
gtk_drag_set_icon_surface (GdkDragContext  *context,
                           cairo_surface_t *surface)
{
  GtkWidget *widget;
  double hot_x, hot_y;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (surface != NULL);

  cairo_surface_get_device_offset (surface, &hot_x, &hot_y);
  cairo_surface_set_device_offset (surface, 0, 0);

  widget = gtk_image_new_from_surface (surface);

  gtk_drag_set_icon_widget_internal (context, widget, (int)hot_x, (int)hot_y, TRUE);
}

/**
 * gtk_drag_set_icon_texture: (method)
 * @context: the context for a drag (This must be called
 *     with a context for the source side of a drag)
 * @texture: the #GdkTexture to use as icon
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 *
 * Sets @texture as the icon for a given drag. GTK+ retains
 * references for the arguments, and will release them when
 * they are no longer needed.
 *
 * To position the texture relative to the mouse, its top
 * left will be positioned @hot_x, @hot_y pixels from the
 * mouse cursor.
 */
void
gtk_drag_set_icon_texture (GdkDragContext *context,
                           GdkTexture     *texture,
                           int             hot_x,
                           int             hot_y)
{
  GtkWidget *widget;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GDK_IS_TEXTURE (texture));

  widget = gtk_image_new_from_texture (texture);

  gtk_drag_set_icon_widget_internal (context, widget, hot_x, hot_y, TRUE);
}

/**
 * gtk_drag_set_icon_name: (method)
 * @context: the context for a drag (This must be called 
 *     with a context for the source side of a drag)
 * @icon_name: name of icon to use
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 * 
 * Sets the icon for a given drag from a named themed icon. See
 * the docs for #GtkIconTheme for more details. Note that the
 * size of the icon depends on the icon theme (the icon is
 * loaded at the symbolic size #GTK_ICON_SIZE_DND), thus 
 * @hot_x and @hot_y have to be used with care.
 *
 * Since: 2.8
 */
void 
gtk_drag_set_icon_name (GdkDragContext *context,
                        const gchar    *icon_name,
                        gint            hot_x,
                        gint            hot_y)
{
  GtkImageDefinition *def;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (icon_name != NULL && icon_name[0] != '\0');

  def = gtk_image_definition_new_icon_name (icon_name);
  set_icon_helper (context, def, hot_x, hot_y);

  gtk_image_definition_unref (def);
}

/**
 * gtk_drag_set_icon_gicon: (method)
 * @context: the context for a drag (This must be called 
 *     with a context for the source side of a drag)
 * @icon: a #GIcon
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 * 
 * Sets the icon for a given drag from the given @icon.
 * See the documentation for gtk_drag_set_icon_name()
 * for more details about using icons in drag and drop.
 *
 * Since: 3.2
 */
void 
gtk_drag_set_icon_gicon (GdkDragContext *context,
                         GIcon          *icon,
                         gint            hot_x,
                         gint            hot_y)
{
  GtkImageDefinition *def;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (icon != NULL);

  def = gtk_image_definition_new_gicon (icon);
  set_icon_helper (context, def, hot_x, hot_y);

  gtk_image_definition_unref (def);
}

/**
 * gtk_drag_set_icon_default: (method)
 * @context: the context for a drag (This must be called 
 *     with a  context for the source side of a drag)
 * 
 * Sets the icon for a particular drag to the default
 * icon.
 */
void 
gtk_drag_set_icon_default (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  gtk_drag_set_icon_name (context, "text-x-generic", -2, -2);
}

static void
gtk_drag_source_check_selection (GtkDragSourceInfo *info, 
                                 GdkAtom            selection,
                                 guint32            time)
{
  GList *tmp_list;

  tmp_list = info->selections;
  while (tmp_list)
    {
      if (GDK_POINTER_TO_ATOM (tmp_list->data) == selection)
        return;
      tmp_list = tmp_list->next;
    }

  gtk_selection_owner_set_for_display (gtk_widget_get_display (info->widget),
                                       info->ipc_widget,
                                       selection,
                                       time);
  info->selections = g_list_prepend (info->selections,
                                     GUINT_TO_POINTER (selection));

  gtk_selection_add_targets (info->ipc_widget,
                             selection,
                             info->target_list);

  gtk_selection_add_target (info->ipc_widget,
                            selection,
                            gdk_atom_intern_static_string ("DELETE"));
}


/* Clean up from the drag, and display snapback, if necessary. */
static void
gtk_drag_drop_finished (GtkDragSourceInfo *info,
                        GtkDragResult      result,
                        guint              time)
{
  gboolean success;

  success = (result == GTK_DRAG_RESULT_SUCCESS);
  gtk_drag_source_release_selections (info, time); 

  if (!success)
    g_signal_emit_by_name (info->widget, "drag-failed",
                           info->context, result, &success);

  gdk_drag_drop_done (info->context, success);
  gtk_drag_source_info_destroy (info);
}

static void
gtk_drag_source_release_selections (GtkDragSourceInfo *info,
                                    guint32            time)
{
  GdkDisplay *display = gtk_widget_get_display (info->widget);
  GList *tmp_list = info->selections;
  
  while (tmp_list)
    {
      GdkAtom selection = GDK_POINTER_TO_ATOM (tmp_list->data);
      if (gdk_selection_owner_get_for_display (display, selection) == gtk_widget_get_window (info->ipc_widget))
        gtk_selection_owner_set_for_display (display, NULL, selection, time);

      tmp_list = tmp_list->next;
    }

  g_list_free (info->selections);
  info->selections = NULL;
}

static void
gtk_drag_drop (GtkDragSourceInfo *info, 
               guint32            time)
{
  if (info->icon_window)
    gtk_widget_hide (info->icon_window);

  info->drop_timeout = gdk_threads_add_timeout (DROP_ABORT_TIME,
                                                gtk_drag_abort_timeout,
                                                info);
  g_source_set_name_by_id (info->drop_timeout, "[gtk+] gtk_drag_abort_timeout");
}

/*
 * Source side callbacks.
 */
static void
gtk_drag_selection_get (GtkWidget        *widget, 
                        GtkSelectionData *selection_data,
                        guint32           time,
                        gpointer          data)
{
  GtkDragSourceInfo *info = data;
  static GdkAtom null_atom = NULL;

  if (!null_atom)
    null_atom = gdk_atom_intern_static_string ("NULL");

  if (gtk_selection_data_get_target (selection_data) == gdk_atom_intern_static_string ("DELETE"))
    {
      g_signal_emit_by_name (info->widget,
                             "drag-data-delete", 
                             info->context);
      gtk_selection_data_set (selection_data, null_atom, 8, NULL, 0);
    }
  else if (gdk_content_formats_contain_mime_type (info->target_list, 
                                                  gtk_selection_data_get_target (selection_data)))
    {
      g_signal_emit_by_name (info->widget, "drag-data-get",
                             info->context,
                             selection_data,
                             time);
    }
}

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
  g_signal_handlers_disconnect_by_func (info->context,
                                        gtk_drag_context_drop_performed_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->context,
                                        gtk_drag_context_dnd_finished_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->context,
                                        gtk_drag_context_cancel_cb,
                                        info);

  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_selection_get,
                                        info);

  g_signal_emit_by_name (info->widget, "drag-end", info->context);

  g_clear_object (&info->widget);

  gtk_selection_remove_all (info->ipc_widget);
  g_object_set_data (G_OBJECT (info->ipc_widget), I_("gtk-info"), NULL);
  source_widgets = g_slist_remove (source_widgets, info->ipc_widget);
  gtk_drag_release_ipc_widget (info->ipc_widget);

  gdk_content_formats_unref (info->target_list);

  if (info->drop_timeout)
    g_source_remove (info->drop_timeout);

  /* keep the icon_window alive until the (possible) drag cancel animation is done */
  g_object_set_data_full (G_OBJECT (info->context), "former-gtk-source-info", info, (GDestroyNotify)gtk_drag_source_info_free);

  gtk_drag_clear_source_info (info->context);
  g_object_unref (info->context);
}

/* Called on cancellation of a drag, either by the user
 * or programmatically.
 */
static void
gtk_drag_cancel_internal (GtkDragSourceInfo *info,
                          GtkDragResult      result,
                          guint32            time)
{
  gtk_drag_drop_finished (info, result, time);
}

static void
gtk_drag_context_drop_performed_cb (GdkDragContext    *context,
                                    guint32            time_,
                                    GtkDragSourceInfo *info)
{
  gtk_drag_drop (info, time_);
}

static void
gtk_drag_context_cancel_cb (GdkDragContext      *context,
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
  gtk_drag_cancel_internal (info, result, GDK_CURRENT_TIME);
}

static void
gtk_drag_context_dnd_finished_cb (GdkDragContext    *context,
                                  GtkDragSourceInfo *info)
{
  gtk_drag_source_release_selections (info, GDK_CURRENT_TIME);

  if (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE)
    {
      g_signal_emit_by_name (info->widget,
                             "drag-data-delete",
                             context);
    }

  gtk_drag_source_info_destroy (info);
}

static gboolean
gtk_drag_abort_timeout (gpointer data)
{
  GtkDragSourceInfo *info = data;
  guint32 time = GDK_CURRENT_TIME;

  info->drop_timeout = 0;
  gtk_drag_drop_finished (info, GTK_DRAG_RESULT_TIMEOUT_EXPIRED, time);
  
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
 * gtk_drag_cancel: (method)
 * @context: a #GdkDragContext, as e.g. returned by gtk_drag_begin_with_coordinates()
 *
 * Cancels an ongoing drag operation on the source side.
 *
 * If you want to be able to cancel a drag operation in this way,
 * you need to keep a pointer to the drag context, either from an
 * explicit call to gtk_drag_begin_with_coordinates(), or by
 * connecting to #GtkWidget::drag-begin.
 *
 * If @context does not refer to an ongoing drag operation, this
 * function does nothing.
 *
 * If a drag is cancelled in this way, the @result argument of
 * #GtkWidget::drag-failed is set to @GTK_DRAG_RESULT_ERROR.
 *
 * Since: 3.16
 */
void
gtk_drag_cancel (GdkDragContext *context)
{
  GtkDragSourceInfo *info;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  info = gtk_drag_get_source_info (context, FALSE);
  if (info != NULL)
    gtk_drag_cancel_internal (info, GTK_DRAG_RESULT_ERROR, gtk_get_current_event_time ());
}
