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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <stdlib.h>
#include <string.h>

#include "gdk/gdk.h"

#include "gtkdnd.h"
#include "deprecated/gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkimageprivate.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "deprecated/gtkstock.h"
#include "gtkwindow.h"
#include "gtkintl.h"
#include "gtkquartz.h"
#include "gdk/quartz/gdkquartz.h"
#include "gtkselectionprivate.h"
#include "gtksettings.h"

typedef struct _GtkDragSourceSite GtkDragSourceSite;
typedef struct _GtkDragSourceInfo GtkDragSourceInfo;
typedef struct _GtkDragDestSite GtkDragDestSite;
typedef struct _GtkDragDestInfo GtkDragDestInfo;
typedef struct _GtkDragFindData GtkDragFindData;

static void     gtk_drag_find_widget            (GtkWidget        *widget,
						 GtkDragFindData  *data);
static void     gtk_drag_dest_site_destroy      (gpointer          data);
static void     gtk_drag_dest_leave             (GtkWidget        *widget,
						 GdkDragContext   *context,
						 guint             time);
static GtkDragDestInfo *gtk_drag_get_dest_info  (GdkDragContext   *context,
						 gboolean          create);
static void gtk_drag_source_site_destroy        (gpointer           data);

static GtkDragSourceInfo *gtk_drag_get_source_info (GdkDragContext *context,
						    gboolean        create);

extern GdkDragContext *gdk_quartz_drag_source_context (); /* gdk/quartz/gdkdnd-quartz.c */

struct _GtkDragSourceSite 
{
  GdkModifierType    start_button_mask;
  GtkTargetList     *target_list;        /* Targets for drag data */
  GdkDragAction      actions;            /* Possible actions */

  /* Drag icon */
  GtkImageType icon_type;
  union
  {
    GtkImagePixbufData pixbuf;
    GtkImageStockData stock;
    GtkImageIconNameData name;
  } icon_data;

  /* Stored button press information to detect drag beginning */
  gint               state;
  gint               x, y;
};

struct _GtkDragSourceInfo 
{
  GtkWidget         *source_widget;
  GtkWidget         *widget;
  GtkTargetList     *target_list; /* Targets for drag data */
  GdkDragAction      possible_actions; /* Actions allowed by source */
  GdkDragContext    *context;	  /* drag context */
  NSEvent           *nsevent;     /* what started it */
  gint hot_x, hot_y;		  /* Hot spot for drag */
  GdkPixbuf         *icon_pixbuf;
  gboolean           success;
  gboolean           delete;
};

struct _GtkDragDestSite 
{
  GtkDestDefaults    flags;
  GtkTargetList     *target_list;
  GdkDragAction      actions;
  guint              have_drag : 1;
  guint              track_motion : 1;
};

struct _GtkDragDestInfo 
{
  GtkWidget         *widget;	   /* Widget in which drag is in */
  GdkDragContext    *context;	   /* Drag context */
  guint              dropped : 1;     /* Set after we receive a drop */
  gint               drop_x, drop_y; /* Position of drop */
};

struct _GtkDragFindData 
{
  gint x;
  gint y;
  GdkDragContext *context;
  GtkDragDestInfo *info;
  gboolean found;
  gboolean toplevel;
  gboolean (*callback) (GtkWidget *widget, GdkDragContext *context,
			gint x, gint y, guint32 time);
  guint32 time;
};


@interface GtkDragSourceOwner : NSObject {
  GtkDragSourceInfo *info;
}

@end

@implementation GtkDragSourceOwner
-(void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSString *)type
{
  guint target_info;
  GtkSelectionData selection_data;

  selection_data.selection = GDK_NONE;
  selection_data.data = NULL;
  selection_data.length = -1;
  selection_data.target = gdk_quartz_pasteboard_type_to_atom_libgtk_only (type);
  selection_data.display = gdk_display_get_default ();

  if (gtk_target_list_find (info->target_list, 
			    selection_data.target, 
			    &target_info)) 
    {
      g_signal_emit_by_name (info->widget, "drag-data-get",
			     info->context,
			     &selection_data,
			     target_info,
			     time);

      if (selection_data.length >= 0)
        _gtk_quartz_set_selection_data_for_pasteboard (sender, &selection_data);
      
      g_free (selection_data.data);
    }
}

- (id)initWithInfo:(GtkDragSourceInfo *)anInfo
{
  self = [super init];

  if (self) 
    {
      info = anInfo;
    }

  return self;
}

@end

/**
 * gtk_drag_get_data: (method)
 * @widget: the widget that will receive the
 *   #GtkWidget::drag-data-received signal.
 * @context: the drag context
 * @target: the target (form of the data) to retrieve.
 * @time_: a timestamp for retrieving the data. This will
 *   generally be the time received in a #GtkWidget::drag-motion"
 *   or #GtkWidget::drag-drop" signal.
 */
void 
gtk_drag_get_data (GtkWidget      *widget,
		   GdkDragContext *context,
		   GdkAtom         target,
		   guint32         time)
{
  id <NSDraggingInfo> dragging_info;
  NSPasteboard *pasteboard;
  GtkSelectionData *selection_data;
  GtkDragDestInfo *info;
  GtkDragDestSite *site;

  dragging_info = gdk_quartz_drag_context_get_dragging_info_libgtk_only (context);
  pasteboard = [dragging_info draggingPasteboard];

  info = gtk_drag_get_dest_info (context, FALSE);
  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");

  selection_data = _gtk_quartz_get_selection_data_from_pasteboard (pasteboard,
								   target, 0);

  if (site && site->target_list)
    {
      guint target_info;
      
      if (gtk_target_list_find (site->target_list, 
				selection_data->target,
				&target_info))
	{
	  if (!(site->flags & GTK_DEST_DEFAULT_DROP) ||
	      selection_data->length >= 0)
	    g_signal_emit_by_name (widget,
				   "drag-data-received",
				   context, info->drop_x, info->drop_y,
				   selection_data,
				   target_info, time);
	}
    }
  else
    {
      g_signal_emit_by_name (widget,
			     "drag-data-received",
			     context, info->drop_x, info->drop_y,
			     selection_data,
			     0, time);
    }
  
  if (site && site->flags & GTK_DEST_DEFAULT_DROP)
    {
      gtk_drag_finish (context, 
		       (selection_data->length >= 0),
		       (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE),
		       time);
    }      
}

/**
 * gtk_drag_finish: (method)
 * @context: the drag context.
 * @success: a flag indicating whether the drop was successful
 * @del: a flag indicating whether the source should delete the
 *   original data. (This should be %TRUE for a move)
 * @time_: the timestamp from the #GtkWidget::drag-drop signal.
 */
void 
gtk_drag_finish (GdkDragContext *context,
		 gboolean        success,
		 gboolean        del,
		 guint32         time)
{
  GtkDragSourceInfo *info;
  GdkDragContext* source_context = gdk_quartz_drag_source_context ();

  if (source_context)
    {
      info = gtk_drag_get_source_info (source_context, FALSE);
      if (info)
        {
          info->success = success;
          info->delete = del;
        }
    }
}

static void
gtk_drag_dest_info_destroy (gpointer data)
{
  GtkDragDestInfo *info = data;

  g_free (info);
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
      info = g_new (GtkDragDestInfo, 1);
      info->widget = NULL;
      info->context = context;
      info->dropped = FALSE;
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

/**
 * gtk_drag_get_source_widget: (method)
 * @context: a (destination side) drag context
 *
 * Returns: (transfer none):
 */
GtkWidget *
gtk_drag_get_source_widget (GdkDragContext *context)
{
  GtkDragSourceInfo *info;
  GdkDragContext* real_source_context = gdk_quartz_drag_source_context();

  if (!real_source_context)
    return NULL;

  info = gtk_drag_get_source_info (real_source_context, FALSE);
  if (!info)
     return NULL;

  return info->source_widget;
}

/*************************************************************
 * gtk_drag_highlight_draw:
 *     Callback for expose_event for highlighted widgets.
 *   arguments:
 *     widget:
 *     event:
 *     data:
 *   results:
 *************************************************************/

static gboolean
gtk_drag_highlight_draw (GtkWidget *widget,
                         cairo_t   *cr,
			 gpointer   data)
{
  int width = gtk_widget_get_allocated_width (widget);
  int height = gtk_widget_get_allocated_height (widget);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_DND);

  gtk_render_frame (context, cr, 0, 0, width, height);

  gtk_style_context_restore (context);

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black */
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr,
                   0.5, 0.5,
                   width - 1, height - 1);
  cairo_stroke (cr);
 
  return FALSE;
}

/**
 * gtk_drag_highlight: (method)
 * @widget: a widget to highlight
 */
void 
gtk_drag_highlight (GtkWidget  *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_signal_connect_after (widget, "draw",
			  G_CALLBACK (gtk_drag_highlight_draw),
			  NULL);

  gtk_widget_queue_draw (widget);
}

/**
 * gtk_drag_unhighlight: (method)
 * @widget: a widget to remove the highlight from.
 */
void 
gtk_drag_unhighlight (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_signal_handlers_disconnect_by_func (widget,
					gtk_drag_highlight_draw,
					NULL);
  
  gtk_widget_queue_draw (widget);
}

static NSWindow *
get_toplevel_nswindow (GtkWidget *widget)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
  GdkWindow *window = gtk_widget_get_window (toplevel);
  
  if (gtk_widget_is_toplevel (toplevel) && window)
    return [gdk_quartz_window_get_nsview (window) window];
  else
    return NULL;
}

static void
register_types (GtkWidget *widget, GtkDragDestSite *site)
{
  if (site->target_list)
    {
      NSWindow *nswindow = get_toplevel_nswindow (widget);
      NSSet *types;
      NSAutoreleasePool *pool;

      if (!nswindow)
	return;

      pool = [[NSAutoreleasePool alloc] init];
      types = _gtk_quartz_target_list_to_pasteboard_types (site->target_list);

      [nswindow registerForDraggedTypes:[types allObjects]];

      [types release];
      [pool release];
    }
}

static void
gtk_drag_dest_realized (GtkWidget *widget, 
			gpointer   user_data)
{
  GtkDragDestSite *site = user_data;

  register_types (widget, site);
}

static void
gtk_drag_dest_hierarchy_changed (GtkWidget *widget,
				 GtkWidget *previous_toplevel,
				 gpointer   user_data)
{
  GtkDragDestSite *site = user_data;

  register_types (widget, site);
}

static void
gtk_drag_dest_site_destroy (gpointer data)
{
  GtkDragDestSite *site = data;
    
  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  g_free (site);
}

/**
 * gtk_drag_dest_set: (method)
 * @widget: a #GtkWidget
 * @flags: which types of default drag behavior to use
 * @targets: (allow-none) (array length=n_targets): a pointer to an array of #GtkTargetEntrys
 *     indicating the drop types that this @widget will accept, or %NULL.
 *     Later you can access the list with gtk_drag_dest_get_target_list()
 *     and gtk_drag_dest_find_target().
 * @n_targets: the number of entries in @targets
 * @actions: a bitmask of possible actions for a drop onto this @widget.
 */
void 
gtk_drag_dest_set (GtkWidget            *widget,
		   GtkDestDefaults       flags,
		   const GtkTargetEntry *targets,
		   gint                  n_targets,
		   GdkDragAction         actions)
{
  GtkDragDestSite *old_site, *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");

  site = g_new (GtkDragDestSite, 1);
  site->flags = flags;
  site->have_drag = FALSE;
  if (targets)
    site->target_list = gtk_target_list_new (targets, n_targets);
  else
    site->target_list = NULL;
  site->actions = actions;

  if (old_site)
    site->track_motion = old_site->track_motion;
  else
    site->track_motion = FALSE;

  gtk_drag_dest_unset (widget);

  if (gtk_widget_get_realized (widget))
    gtk_drag_dest_realized (widget, site);

  g_signal_connect (widget, "realize",
		    G_CALLBACK (gtk_drag_dest_realized), site);
  g_signal_connect (widget, "hierarchy-changed",
		    G_CALLBACK (gtk_drag_dest_hierarchy_changed), site);

  g_object_set_data_full (G_OBJECT (widget), I_("gtk-drag-dest"),
			  site, gtk_drag_dest_site_destroy);
}

/**
 * gtk_drag_dest_set_proxy: (method)
 * @widget: a #GtkWidget
 * @proxy_window: the window to which to forward drag events
 * @protocol: the drag protocol which the @proxy_window accepts
 *   (You can use gdk_drag_get_protocol() to determine this)
 * @use_coordinates: If %TRUE, send the same coordinates to the
 *   destination, because it is an embedded
 *   subwindow.
 */
void 
gtk_drag_dest_set_proxy (GtkWidget      *widget,
			 GdkWindow      *proxy_window,
			 GdkDragProtocol protocol,
			 gboolean        use_coordinates)
{
  g_warning ("gtk_drag_dest_set_proxy is not supported on Mac OS X.");
}

/**
 * gtk_drag_dest_unset: (method)
 * @widget: a #GtkWidget
 */
void 
gtk_drag_dest_unset (GtkWidget *widget)
{
  GtkDragDestSite *old_site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  if (old_site)
    {
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_realized,
                                            old_site);
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_hierarchy_changed,
                                            old_site);
    }

  g_object_set_data (G_OBJECT (widget), I_("gtk-drag-dest"), NULL);
}

/**
 * gtk_drag_dest_get_target_list: (method)
 * @widget: a #GtkWidget
 */
GtkTargetList*
gtk_drag_dest_get_target_list (GtkWidget *widget)
{
  GtkDragDestSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");

  return site ? site->target_list : NULL;  
}

/**
 * gtk_drag_dest_set_target_list: (method)
 * @widget: a #GtkWidget that’s a drag destination
 * @target_list: (allow-none): list of droppable targets, or %NULL for none
 */
void
gtk_drag_dest_set_target_list (GtkWidget      *widget,
                               GtkTargetList  *target_list)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  
  if (!site)
    {
      g_warning ("Can't set a target list on a widget until you've called gtk_drag_dest_set() "
                 "to make the widget into a drag destination");
      return;
    }

  if (target_list)
    gtk_target_list_ref (target_list);
  
  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  site->target_list = target_list;

  register_types (widget, site);
}

/**
 * gtk_drag_dest_add_text_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 */
void
gtk_drag_dest_add_text_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}


/**
 * gtk_drag_dest_add_image_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 */
void
gtk_drag_dest_add_image_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (target_list, 0, FALSE);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_dest_add_uri_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 */
void
gtk_drag_dest_add_uri_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

static void
prepend_and_ref_widget (GtkWidget *widget,
			gpointer   data)
{
  GSList **slist_p = data;

  *slist_p = g_slist_prepend (*slist_p, g_object_ref (widget));
}

static void
gtk_drag_find_widget (GtkWidget       *widget,
		      GtkDragFindData *data)
{
  GtkAllocation new_allocation;
  gint allocation_to_window_x = 0;
  gint allocation_to_window_y = 0;
  gint x_offset = 0;
  gint y_offset = 0;

  if (data->found || !gtk_widget_get_mapped (widget) || !gtk_widget_get_sensitive (widget))
    return;

  /* Note that in the following code, we only count the
   * position as being inside a WINDOW widget if it is inside
   * widget->window; points that are outside of widget->window
   * but within the allocation are not counted. This is consistent
   * with the way we highlight drag targets.
   *
   * data->x,y are relative to widget->parent->window (if
   * widget is not a toplevel, widget->window otherwise).
   * We compute the allocation of widget in the same coordinates,
   * clipping to widget->window, and all intermediate
   * windows. If data->x,y is inside that, then we translate
   * our coordinates to be relative to widget->window and
   * recurse.
   */  
  gtk_widget_get_allocation (widget, &new_allocation);

  if (gtk_widget_get_parent (widget))
    {
      gint tx, ty;
      GdkWindow *window = gtk_widget_get_window (widget);
      GdkWindow *parent_window;
      GtkAllocation allocation;

      parent_window = gtk_widget_get_window (gtk_widget_get_parent (widget));

      /* Compute the offset from allocation-relative to
       * window-relative coordinates.
       */
      gtk_widget_get_allocation (widget, &allocation);
      allocation_to_window_x = allocation.x;
      allocation_to_window_y = allocation.y;

      if (gtk_widget_get_has_window (widget))
	{
	  /* The allocation is relative to the parent window for
	   * window widgets, not to widget->window.
	   */
          gdk_window_get_position (window, &tx, &ty);
	  
          allocation_to_window_x -= tx;
          allocation_to_window_y -= ty;
	}

      new_allocation.x = 0 + allocation_to_window_x;
      new_allocation.y = 0 + allocation_to_window_y;
      
      while (window && window != parent_window)
	{
	  GdkRectangle window_rect = { 0, 0, 0, 0 };
	  
          window_rect.width = gdk_window_get_width (window);
          window_rect.height = gdk_window_get_height (window);

	  gdk_rectangle_intersect (&new_allocation, &window_rect, &new_allocation);

	  gdk_window_get_position (window, &tx, &ty);
	  new_allocation.x += tx;
	  x_offset += tx;
	  new_allocation.y += ty;
	  y_offset += ty;
	  
	  window = gdk_window_get_parent (window);
	}

      if (!window)		/* Window and widget heirarchies didn't match. */
	return;
    }

  if (data->toplevel ||
      ((data->x >= new_allocation.x) && (data->y >= new_allocation.y) &&
       (data->x < new_allocation.x + new_allocation.width) && 
       (data->y < new_allocation.y + new_allocation.height)))
    {
      /* First, check if the drag is in a valid drop site in
       * one of our children 
       */
      if (GTK_IS_CONTAINER (widget))
	{
	  GtkDragFindData new_data = *data;
	  GSList *children = NULL;
	  GSList *tmp_list;
	  
	  new_data.x -= x_offset;
	  new_data.y -= y_offset;
	  new_data.found = FALSE;
	  new_data.toplevel = FALSE;
	  
	  /* need to reference children temporarily in case the
	   * ::drag-motion/::drag-drop callbacks change the widget hierarchy.
	   */
	  gtk_container_forall (GTK_CONTAINER (widget), prepend_and_ref_widget, &children);
	  for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
	    {
	      if (!new_data.found && gtk_widget_is_drawable (tmp_list->data))
		gtk_drag_find_widget (tmp_list->data, &new_data);
	      g_object_unref (tmp_list->data);
	    }
	  g_slist_free (children);
	  
	  data->found = new_data.found;
	}

      /* If not, and this widget is registered as a drop site, check to
       * emit "drag-motion" to check if we are actually in
       * a drop site.
       */
      if (!data->found &&
	  g_object_get_data (G_OBJECT (widget), "gtk-drag-dest"))
	{
	  data->found = data->callback (widget,
					data->context,
					data->x - x_offset - allocation_to_window_x,
					data->y - y_offset - allocation_to_window_y,
					data->time);
	  /* If so, send a "drag-leave" to the last widget */
	  if (data->found)
	    {
	      if (data->info->widget && data->info->widget != widget)
		{
		  gtk_drag_dest_leave (data->info->widget, data->context, data->time);
		}
	      data->info->widget = widget;
	    }
	}
    }
}

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
gtk_drag_dest_motion (GtkWidget	     *widget,
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
gtk_drag_dest_drop (GtkWidget	     *widget,
		    GdkDragContext   *context,
		    gint              x,
		    gint              y,
		    guint             time)
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

      if (target == GDK_NONE)
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

/**
 * gtk_drag_dest_set_track_motion: (method)
 * @widget: a #GtkWidget that’s a drag destination
 * @track_motion: whether to accept all targets
 */
void
gtk_drag_dest_set_track_motion (GtkWidget *widget,
				gboolean   track_motion)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  
  g_return_if_fail (site != NULL);

  site->track_motion = track_motion != FALSE;
}

/**
 * gtk_drag_dest_get_track_motion: (method)
 * @widget: a #GtkWidget that’s a drag destination
 */
gboolean
gtk_drag_dest_get_track_motion (GtkWidget *widget)
{
  GtkDragDestSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");

  if (site)
    return site->track_motion;

  return FALSE;
}

void
_gtk_drag_dest_handle_event (GtkWidget *toplevel,
			     GdkEvent  *event)
{
  GtkDragDestInfo *info;
  GdkDragContext *context;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  context = event->dnd.context;

  info = gtk_drag_get_dest_info (context, TRUE);

  /* Find the widget for the event */
  switch (event->type)
    {
    case GDK_DRAG_ENTER:
      break;

    case GDK_DRAG_LEAVE:
      if (info->widget)
	{
	  gtk_drag_dest_leave (info->widget, context, event->dnd.time);
	  info->widget = NULL;
	}
      break;

    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      {
	GtkDragFindData data;
	gint tx, ty;

	if (event->type == GDK_DROP_START)
	  {
	    info->dropped = TRUE;
	    /* We send a leave here so that the widget unhighlights
	     * properly.
	     */
	    if (info->widget)
	      {
		gtk_drag_dest_leave (info->widget, context, event->dnd.time);
		info->widget = NULL;
	      }
	  }

	gdk_window_get_position (gtk_widget_get_window (toplevel), &tx, &ty);
	
	data.x = event->dnd.x_root - tx;
	data.y = event->dnd.y_root - ty;
 	data.context = context;
	data.info = info;
	data.found = FALSE;
	data.toplevel = TRUE;
	data.callback = (event->type == GDK_DRAG_MOTION) ?
	  gtk_drag_dest_motion : gtk_drag_dest_drop;
	data.time = event->dnd.time;
	
	gtk_drag_find_widget (toplevel, &data);

	if (info->widget && !data.found)
	  {
	    gtk_drag_dest_leave (info->widget, context, event->dnd.time);
	    info->widget = NULL;
	  }

	/* Send a reply.
	 */
	if (event->type == GDK_DRAG_MOTION)
	  {
	    if (!data.found)
	      gdk_drag_status (context, 0, event->dnd.time);
	  }

	break;
      default:
	g_assert_not_reached ();
      }
    }
}


/**
 * gtk_drag_dest_find_target: (method)
 * @widget: drag destination widget
 * @context: drag context
 * @target_list: (allow-none): list of droppable targets, or %NULL to use
 *    gtk_drag_dest_get_target_list (@widget).
 *
 * Returns: (transfer none):
 */
GdkAtom
gtk_drag_dest_find_target (GtkWidget      *widget,
                           GdkDragContext *context,
                           GtkTargetList  *target_list)
{
  id <NSDraggingInfo> dragging_info;
  NSPasteboard *pasteboard;
  GtkWidget *source_widget;
  GList *tmp_target;
  GList *tmp_source = NULL;
  GList *source_targets;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GDK_NONE);
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_NONE);

  dragging_info = gdk_quartz_drag_context_get_dragging_info_libgtk_only (context);
  pasteboard = [dragging_info draggingPasteboard];

  source_widget = gtk_drag_get_source_widget (context);

  if (target_list == NULL)
    target_list = gtk_drag_dest_get_target_list (widget);
  
  if (target_list == NULL)
    return GDK_NONE;

  source_targets = _gtk_quartz_pasteboard_types_to_atom_list ([pasteboard types]);
  tmp_target = target_list->list;
  while (tmp_target)
    {
      GtkTargetPair *pair = tmp_target->data;
      tmp_source = source_targets;
      while (tmp_source)
	{
	  if (tmp_source->data == GUINT_TO_POINTER (pair->target))
	    {
	      if ((!(pair->flags & GTK_TARGET_SAME_APP) || source_widget) &&
		  (!(pair->flags & GTK_TARGET_SAME_WIDGET) || (source_widget == widget)))
		{
		  g_list_free (source_targets);
		  return pair->target;
		}
	      else
		break;
	    }
	  tmp_source = tmp_source->next;
	}
      tmp_target = tmp_target->next;
    }

  g_list_free (source_targets);
  return GDK_NONE;
}

static gboolean
gtk_drag_begin_idle (gpointer arg)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  GdkDragContext* context = (GdkDragContext*) arg;
  GtkDragSourceInfo* info = gtk_drag_get_source_info (context, FALSE);
  NSWindow *nswindow;
  NSPasteboard *pasteboard;
  GtkDragSourceOwner *owner;
  NSPoint point;
  NSSet *types;
  NSImage *drag_image;

  g_assert (info != NULL);

  pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  owner = [[GtkDragSourceOwner alloc] initWithInfo:info];

  types = _gtk_quartz_target_list_to_pasteboard_types (info->target_list);

  [pasteboard declareTypes:[types allObjects] owner:owner];

  [owner release];
  [types release];

  if ((nswindow = get_toplevel_nswindow (info->source_widget)) == NULL)
     return G_SOURCE_REMOVE;
  
  /* Ref the context. It's unreffed when the drag has been aborted */
  g_object_ref (info->context);

  /* FIXME: If the event isn't a mouse event, use the global cursor position instead */
  point = [info->nsevent locationInWindow];

  drag_image = _gtk_quartz_create_image_from_pixbuf (info->icon_pixbuf);
  if (drag_image == NULL)
    {
      g_object_unref (info->context);
      return G_SOURCE_REMOVE;
    }

  point.x -= info->hot_x;
  point.y -= info->hot_y;

  [nswindow dragImage:drag_image
                   at:point
               offset:NSZeroSize
                event:info->nsevent
           pasteboard:pasteboard
               source:nswindow
            slideBack:YES];

  [info->nsevent release];
  [drag_image release];

  [pool release];

  return G_SOURCE_REMOVE;
}
/* Fake protocol to let us call GdkNSView gdkWindow without including
 * gdk/GdkNSView.h (which we can’t because it pulls in the internal-only
 * gdkwindow.h).
 */
@protocol GdkNSView
- (GdkWindow *)gdkWindow;
@end

static GdkDragContext *
gtk_drag_begin_internal (GtkWidget         *widget,
			 GtkDragSourceSite *site,
			 GtkTargetList     *target_list,
			 GdkDragAction      actions,
			 gint               button,
			 GdkEvent          *event,
			 gint               x,
			 gint               y)
{
  GtkDragSourceInfo *info;
  GdkDevice *pointer;
  GdkWindow *window;
  GdkDragContext *context;
  NSWindow *nswindow = get_toplevel_nswindow (widget);
  NSPoint point = {0, 0};
  double time = (double)g_get_real_time ();
  NSEvent *nsevent;
  NSTimeInterval nstime;

  if ((x != -1 && y != -1) || event)
    {
      GdkWindow *window;
      gdouble dx, dy;
      if (x != -1 && y != -1)
	{
	  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
	  window = gtk_widget_get_window (toplevel);
	  gtk_widget_translate_coordinates (widget, toplevel, x, y, &x, &y);
	  gdk_window_get_root_coords (gtk_widget_get_window (toplevel), x, y,
							     &x, &y);
	  dx = (gdouble)x;
	  dy = (gdouble)y;
	}
      else if (event)
	{
	  if (gdk_event_get_coords (event, &dx, &dy))
	    {
	      /* We need to translate (x, y) to coordinates relative to the
	       * toplevel GdkWindow, which should be the GdkWindow backing
	       * nswindow. Then, we convert to the NSWindow coordinate system.
	       */
	      window = event->any.window;
	      GdkWindow *toplevel = gdk_window_get_effective_toplevel (window);

	      while (window != toplevel)
		{
		  double old_x = dx;
		  double old_y = dy;

		  gdk_window_coords_to_parent (window, old_x, old_y,
					       &dx, &dy);
		  window = gdk_window_get_effective_parent (window);
		}
	    }
	  time = (double)gdk_event_get_time (event);
	}
      point.x = dx;
      point.y = gdk_window_get_height (window) - dy;
    }

  nstime = [[NSDate dateWithTimeIntervalSince1970: time / 1000] timeIntervalSinceReferenceDate];
  nsevent = [NSEvent mouseEventWithType: NSLeftMouseDown
                      location: point
                      modifierFlags: 0
                      timestamp: nstime
                      windowNumber: [nswindow windowNumber]
                      context: [nswindow graphicsContext]
                      eventNumber: 0
                      clickCount: 1
                      pressure: 0.0 ];

  window = [(id<GdkNSView>)[nswindow contentView] gdkWindow];
  g_return_val_if_fail (nsevent != NULL, NULL);

  context = gdk_drag_begin (window, g_list_copy (target_list->list));
  g_return_val_if_fail (context != NULL, NULL);

  info = gtk_drag_get_source_info (context, TRUE);
  info->nsevent = nsevent;
  [info->nsevent retain];

  info->source_widget = g_object_ref (widget);
  info->widget = g_object_ref (widget);
  info->target_list = target_list;
  gtk_target_list_ref (target_list);

  info->possible_actions = actions;

  g_signal_emit_by_name (widget, "drag-begin", info->context);

  /* Ensure that we have an icon before we start the drag; the
   * application may have set one in ::drag_begin, or it may
   * not have set one.
   */
  if (!info->icon_pixbuf)
    {
      if (!site || site->icon_type == GTK_IMAGE_EMPTY)
        gtk_drag_set_icon_default (context);
      else
        {
          switch (site->icon_type)
            {
              case GTK_IMAGE_PIXBUF:
                  gtk_drag_set_icon_pixbuf (context,
                                            site->icon_data.pixbuf.pixbuf,
                                            -2, -2);
                  break;
              case GTK_IMAGE_STOCK:
                  gtk_drag_set_icon_stock (context,
                                           site->icon_data.stock.stock_id,
                                           -2, -2);
                  break;
              case GTK_IMAGE_ICON_NAME:
                  gtk_drag_set_icon_name (context,
                                          site->icon_data.name.icon_name,
                                          -2, -2);
                  break;
              case GTK_IMAGE_EMPTY:
              default:
                  g_assert_not_reached();
                  break;
            }
        }
    }

  /* drag will begin in an idle handler to avoid nested run loops */

  g_idle_add_full (G_PRIORITY_HIGH_IDLE, gtk_drag_begin_idle, context, NULL);

  pointer = gdk_drag_context_get_device (info->context);
  gdk_device_ungrab (pointer, 0);

  return context;
}

/**
 * gtk_drag_begin_with_coordinates: (method)
 * @widget:
 * @targets:
 * @actions:
 * @button:
 * @event:
 * @x:
 * @y:
 *
 * Returns: (transfer none):
 */
GdkDragContext *
gtk_drag_begin_with_coordinates (GtkWidget         *widget,
				 GtkTargetList     *targets,
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
/**
 * gtk_drag_begin: (method)
 * @widget: the source widget.
 * @targets: The targets (data formats) in which the
 *    source can provide the data.
 * @actions: A bitmask of the allowed drag actions for this drag.
 * @button: The button the user clicked to start the drag.
 * @event: The event that triggered the start of the drag.
 *
 * Returns: (transfer none):
 */
GdkDragContext *
gtk_drag_begin (GtkWidget         *widget,
		GtkTargetList     *targets,
		GdkDragAction      actions,
		gint               button,
		GdkEvent          *event)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_get_realized (widget), NULL);
  g_return_val_if_fail (targets != NULL, NULL);

  return gtk_drag_begin_internal (widget, NULL, targets,
				  actions, button, event, -1, -1);
}


static gboolean
gtk_drag_source_event_cb (GtkWidget      *widget,
			  GdkEvent       *event,
			  gpointer        data)
{
  GtkDragSourceSite *site;
  gboolean retval = FALSE;
  site = (GtkDragSourceSite *)data;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      if ((GDK_BUTTON1_MASK << (event->button.button - 1)) & site->start_button_mask)
	{
	  site->state |= (GDK_BUTTON1_MASK << (event->button.button - 1));
	  site->x = event->button.x;
	  site->y = event->button.y;
	}
      break;
      
    case GDK_BUTTON_RELEASE:
      if ((GDK_BUTTON1_MASK << (event->button.button - 1)) & site->start_button_mask)
	site->state &= ~(GDK_BUTTON1_MASK << (event->button.button - 1));
      break;
      
    case GDK_MOTION_NOTIFY:
      if (site->state & event->motion.state & site->start_button_mask)
	{
	  /* FIXME: This is really broken and can leave us
	   * with a stuck grab
	   */
	  int i;
	  for (i=1; i<6; i++)
	    {
	      if (site->state & event->motion.state &
		  GDK_BUTTON1_MASK << (i - 1))
		break;
	    }

	  if (gtk_drag_check_threshold (widget, site->x, site->y,
					event->motion.x, event->motion.y))
	    {
	      site->state = 0;
	      gtk_drag_begin_internal (widget, site, site->target_list,
				       site->actions, i, event, -1, -1);

	      retval = TRUE;
	    }
	}
      break;
      
    default:			/* hit for 2/3BUTTON_PRESS */
      break;
    }
  
  return retval;
}

/**
 * gtk_drag_source_set: (method)
 * @widget: a #GtkWidget
 * @start_button_mask: the bitmask of buttons that can start the drag
 * @targets: (allow-none) (array length=n_targets): the table of targets that the drag will support,
 *     may be %NULL
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag from this widget
 */
void 
gtk_drag_source_set (GtkWidget            *widget,
		     GdkModifierType       start_button_mask,
		     const GtkTargetEntry *targets,
		     gint                  n_targets,
		     GdkDragAction         actions)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  gtk_widget_add_events (widget,
			 gtk_widget_get_events (widget) |
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			 GDK_BUTTON_MOTION_MASK);

  if (site)
    {
      if (site->target_list)
	gtk_target_list_unref (site->target_list);
    }
  else
    {
      site = g_new0 (GtkDragSourceSite, 1);

      site->icon_type = GTK_IMAGE_EMPTY;
      
      g_signal_connect (widget, "button-press-event",
			G_CALLBACK (gtk_drag_source_event_cb),
			site);
      g_signal_connect (widget, "button-release-event",
			G_CALLBACK (gtk_drag_source_event_cb),
			site);
      g_signal_connect (widget, "motion-notify-event",
			G_CALLBACK (gtk_drag_source_event_cb),
			site);
      
      g_object_set_data_full (G_OBJECT (widget),
			      I_("gtk-site-data"), 
			      site, gtk_drag_source_site_destroy);
    }

  site->start_button_mask = start_button_mask;

  site->target_list = gtk_target_list_new (targets, n_targets);

  site->actions = actions;
}

/**
 * gtk_drag_source_unset: (method)
 * @widget: a #GtkWidget
 */
void 
gtk_drag_source_unset (GtkWidget *widget)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  if (site)
    {
      g_signal_handlers_disconnect_by_func (widget,
					    gtk_drag_source_event_cb,
					    site);
      g_object_set_data (G_OBJECT (widget), I_("gtk-site-data"), NULL);
    }
}

/**
 * gtk_drag_source_get_target_list: (method)
 * @widget: a #GtkWidget
 *
 * Returns: (transfer none):
 */
GtkTargetList *
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
 */
void
gtk_drag_source_set_target_list (GtkWidget     *widget,
                                 GtkTargetList *target_list)
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
    gtk_target_list_ref (target_list);

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  site->target_list = target_list;
}

/**
 * gtk_drag_source_add_text_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the text targets supported by #GtkSelection to
 * the target list of the drag source.  The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_text_targets() and
 * gtk_drag_source_set_target_list().
 * 
 * Since: 2.6
 **/
void
gtk_drag_source_add_text_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_add_image_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 */
void
gtk_drag_source_add_image_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (target_list, 0, TRUE);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_add_uri_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 */
void
gtk_drag_source_add_uri_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

static void
gtk_drag_source_unset_icon (GtkDragSourceSite *site)
{
  switch (site->icon_type)
    {
    case GTK_IMAGE_EMPTY:
      break;
    case GTK_IMAGE_PIXBUF:
      g_object_unref (site->icon_data.pixbuf.pixbuf);
      break;
    case GTK_IMAGE_STOCK:
      g_free (site->icon_data.stock.stock_id);
      break;
    case GTK_IMAGE_ICON_NAME:
      g_free (site->icon_data.name.icon_name);
      break;
    default:
      g_assert_not_reached();
      break;
    }
  site->icon_type = GTK_IMAGE_EMPTY;
}

static void 
gtk_drag_source_site_destroy (gpointer data)
{
  GtkDragSourceSite *site = data;

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  gtk_drag_source_unset_icon (site);
  g_free (site);
}

/**
 * gtk_drag_source_set_icon_pixbuf: (method)
 * @widget: a #GtkWidget
 * @pixbuf: the #GdkPixbuf for the drag icon
 */
void 
gtk_drag_source_set_icon_pixbuf (GtkWidget   *widget,
				 GdkPixbuf   *pixbuf)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL); 
  g_object_ref (pixbuf);

  gtk_drag_source_unset_icon (site);

  site->icon_type = GTK_IMAGE_PIXBUF;
  site->icon_data.pixbuf.pixbuf = pixbuf;
}

/**
 * gtk_drag_source_set_icon_stock: (method)
 * @widget: a #GtkWidget
 * @stock_id: the ID of the stock icon to use
 *
 * Sets the icon that will be used for drags from a particular source
 * to a stock icon. 
 **/
void 
gtk_drag_source_set_icon_stock (GtkWidget   *widget,
				const gchar *stock_id)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (stock_id != NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);
  
  gtk_drag_source_unset_icon (site);

  site->icon_type = GTK_IMAGE_STOCK;
  site->icon_data.stock.stock_id = g_strdup (stock_id);
}

/**
 * gtk_drag_source_set_icon_name: (method)
 * @widget: a #GtkWidget
 * @icon_name: name of icon to use
 * 
 * Sets the icon that will be used for drags from a particular source
 * to a themed icon. See the docs for #GtkIconTheme for more details.
 *
 * Since: 2.8
 **/
void 
gtk_drag_source_set_icon_name (GtkWidget   *widget,
			       const gchar *icon_name)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (icon_name != NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  gtk_drag_source_unset_icon (site);

  site->icon_type = GTK_IMAGE_ICON_NAME;
  site->icon_data.name.icon_name = g_strdup (icon_name);
}


/**
 * gtk_drag_set_icon_widget: (method)
 * @context: the context for a drag. (This must be called 
          with a  context for the source side of a drag)
 * @widget: a toplevel window to use as an icon.
 * @hot_x: the X offset within @widget of the hotspot.
 * @hot_y: the Y offset within @widget of the hotspot.
 * 
 * Changes the icon for a widget to a given widget. GTK+
 * will not destroy the icon, so if you don’t want
 * it to persist, you should connect to the “drag-end” 
 * signal and destroy it yourself.
 **/
void 
gtk_drag_set_icon_widget (GdkDragContext    *context,
			  GtkWidget         *widget,
			  gint               hot_x,
			  gint               hot_y)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_warning ("gtk_drag_set_icon_widget is not supported on Mac OS X");
}

static void
set_icon_stock_pixbuf (GdkDragContext    *context,
		       const gchar       *stock_id,
		       GdkPixbuf         *pixbuf,
		       gint               hot_x,
		       gint               hot_y)
{
  GtkDragSourceInfo *info;

  info = gtk_drag_get_source_info (context, FALSE);

  if (stock_id)
    {
      pixbuf = gtk_widget_render_icon_pixbuf (info->widget, stock_id,
				              GTK_ICON_SIZE_DND);

      if (!pixbuf)
	{
	  g_warning ("Cannot load drag icon from stock_id %s", stock_id);
	  return;
	}
    }
  else
    g_object_ref (pixbuf);

  if (info->icon_pixbuf)
    g_object_unref (info->icon_pixbuf);
  info->icon_pixbuf = pixbuf;
  info->hot_x = hot_x;
  info->hot_y = hot_y;
}

/**
 * gtk_drag_set_icon_pixbuf:
 * @context: the context for a drag. (This must be called 
 *            with a  context for the source side of a drag)
 * @pixbuf: the #GdkPixbuf to use as the drag icon.
 * @hot_x: the X offset within @widget of the hotspot.
 * @hot_y: the Y offset within @widget of the hotspot.
 * 
 * Sets @pixbuf as the icon for a given drag.
 **/
void 
gtk_drag_set_icon_pixbuf  (GdkDragContext *context,
			   GdkPixbuf      *pixbuf,
			   gint            hot_x,
			   gint            hot_y)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  set_icon_stock_pixbuf (context, NULL, pixbuf, hot_x, hot_y);
}

/**
 * gtk_drag_set_icon_stock:
 * @context: the context for a drag. (This must be called 
 *            with a  context for the source side of a drag)
 * @stock_id: the ID of the stock icon to use for the drag.
 * @hot_x: the X offset within the icon of the hotspot.
 * @hot_y: the Y offset within the icon of the hotspot.
 * 
 * Sets the icon for a given drag from a stock ID.
 **/
void 
gtk_drag_set_icon_stock  (GdkDragContext *context,
			  const gchar    *stock_id,
			  gint            hot_x,
			  gint            hot_y)
{

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (stock_id != NULL);

  set_icon_stock_pixbuf (context, stock_id, NULL, hot_x, hot_y);
}


/* XXX: This function is in gdk, too. Should it be in Cairo? */
static gboolean
_gtk_cairo_surface_extents (cairo_surface_t *surface,
                            GdkRectangle *extents)
{
  double x1, x2, y1, y2;
  cairo_t *cr;

  g_return_val_if_fail (surface != NULL, FALSE);
  g_return_val_if_fail (extents != NULL, FALSE);

  cr = cairo_create (surface);
  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

  x1 = floor (x1);
  y1 = floor (y1);
  x2 = ceil (x2);
  y2 = ceil (y2);
  x2 -= x1;
  y2 -= y1;
  
  if (x1 < G_MININT || x1 > G_MAXINT ||
      y1 < G_MININT || y1 > G_MAXINT ||
      x2 > G_MAXINT || y2 > G_MAXINT)
    {
      extents->x = extents->y = extents->width = extents->height = 0;
      return FALSE;
    }

  extents->x = x1;
  extents->y = y1;
  extents->width = x2;
  extents->height = y2;

  return TRUE;
}

/**
 * gtk_drag_set_icon_surface:
 * @context: the context for a drag. (This must be called
 *            with a context for the source side of a drag)
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
 **/
void
gtk_drag_set_icon_surface (GdkDragContext  *context,
                           cairo_surface_t *surface)
{
  GdkPixbuf *pixbuf;
  GdkRectangle extents;
  double x_offset, y_offset;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (surface != NULL);

  _gtk_cairo_surface_extents (surface, &extents);
  cairo_surface_get_device_offset (surface, &x_offset, &y_offset);

  pixbuf = gdk_pixbuf_get_from_surface (surface,
                                        extents.x, extents.y,
                                        extents.width, extents.height);
  gtk_drag_set_icon_pixbuf (context, pixbuf, -x_offset, -y_offset);
  g_object_unref (pixbuf);
}

/**
 * gtk_drag_set_icon_name:
 * @context: the context for a drag. (This must be called 
 *            with a context for the source side of a drag)
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
 **/
void 
gtk_drag_set_icon_name (GdkDragContext *context,
			const gchar    *icon_name,
			gint            hot_x,
			gint            hot_y)
{
  GdkScreen *screen;
  GtkSettings *settings;
  GtkIconTheme *icon_theme;
  GdkPixbuf *pixbuf;
  gint width, height, icon_size;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (icon_name != NULL);

  screen = gdk_window_get_screen (gdk_drag_context_get_source_window (context));
  g_return_if_fail (screen != NULL);

  settings = gtk_settings_get_for_screen (screen);
  if (gtk_icon_size_lookup_for_settings (settings,
					 GTK_ICON_SIZE_DND,
					 &width, &height))
    icon_size = MAX (width, height);
  else 
    icon_size = 32; /* default value for GTK_ICON_SIZE_DND */ 

  icon_theme = gtk_icon_theme_get_for_screen (screen);

  pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name,
		  		     icon_size, 0, NULL);
  if (pixbuf)
    set_icon_stock_pixbuf (context, NULL, pixbuf, hot_x, hot_y);
  else
    g_warning ("Cannot load drag icon from icon name %s", icon_name);
}

/**
 * gtk_drag_set_icon_default: (method)
 * @context: the context for a drag. (This must be called 
             with a  context for the source side of a drag)
 * 
 * Sets the icon for a particular drag to the default
 * icon.
 **/
void 
gtk_drag_set_icon_default (GdkDragContext    *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  gtk_drag_set_icon_name (context, "text-x-generic", -2, -2);
}

static void
gtk_drag_source_info_destroy (GtkDragSourceInfo *info)
{
  NSPasteboard *pasteboard;
  NSAutoreleasePool *pool;

  if (info->icon_pixbuf)
    g_object_unref (info->icon_pixbuf);

  g_signal_emit_by_name (info->widget, "drag-end", 
			 info->context);

  if (info->source_widget)
    g_object_unref (info->source_widget);

  if (info->widget)
    g_object_unref (info->widget);

  gtk_target_list_unref (info->target_list);

  pool = [[NSAutoreleasePool alloc] init];

  /* Empty the pasteboard, so that it will not accidentally access
   * info->context after it has been destroyed.
   */
  pasteboard = [NSPasteboard pasteboardWithName: NSDragPboard];
  [pasteboard declareTypes: nil owner: nil];

  [pool release];

  gtk_drag_clear_source_info (info->context);
  g_object_unref (info->context);

  g_free (info);
  info = NULL;
}

static gboolean
drag_drop_finished_idle_cb (gpointer data)
{
  gtk_drag_source_info_destroy (data);
  return G_SOURCE_REMOVE;
}

static void
gtk_drag_drop_finished (GtkDragSourceInfo *info)
{
  if (info->success && info->delete)
    g_signal_emit_by_name (info->source_widget, "drag-data-delete",
                           info->context);

  /* Workaround for the fact that the NS API blocks until the drag is
   * over. This way the context is still valid when returning from
   * drag_begin, even if it will still be quite useless. See bug #501588.
  */
  g_idle_add (drag_drop_finished_idle_cb, info);
}

/*************************************************************
 * _gtk_drag_source_handle_event:
 *     Called from widget event handling code on Drag events
 *     for drag sources.
 *
 *   arguments:
 *     toplevel: Toplevel widget that received the event
 *     event:
 *   results:
 *************************************************************/

void
_gtk_drag_source_handle_event (GtkWidget *widget,
			       GdkEvent  *event)
{
  GtkDragSourceInfo *info;
  GdkDragContext *context;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (event != NULL);

  context = event->dnd.context;
  info = gtk_drag_get_source_info (context, FALSE);
  if (!info)
    return;

  switch (event->type)
    {
    case GDK_DROP_FINISHED:
      gtk_drag_drop_finished (info);
      break;
    default:
      g_assert_not_reached ();
    }  
}

/**
 * gtk_drag_check_threshold: (method)
 * @widget: a #GtkWidget
 * @start_x: X coordinate of start of drag
 * @start_y: Y coordinate of start of drag
 * @current_x: current X coordinate
 * @current_y: current Y coordinate
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

  g_object_get (gtk_widget_get_settings (widget),
		"gtk-dnd-drag-threshold", &drag_threshold,
		NULL);
  
  return (ABS (current_x - start_x) > drag_threshold ||
	  ABS (current_y - start_y) > drag_threshold);
}
