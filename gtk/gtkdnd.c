/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdlib.h>

#include "gdk/gdkx.h"
#include "gdk/gdkkeysyms.h"

#include "gtkdnd.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkwindow.h"

static GSList *drag_widgets = NULL;
static GSList *source_widgets = NULL;

typedef struct _GtkDragSourceSite GtkDragSourceSite;
typedef struct _GtkDragSourceInfo GtkDragSourceInfo;
typedef struct _GtkDragDestSite GtkDragDestSite;
typedef struct _GtkDragDestInfo GtkDragDestInfo;
typedef struct _GtkDragAnim GtkDragAnim;
typedef struct _GtkDragFindData GtkDragFindData;


typedef enum 
{
  GTK_DRAG_STATUS_DRAG,
  GTK_DRAG_STATUS_WAIT,
  GTK_DRAG_STATUS_DROP
} GtkDragStatus;

struct _GtkDragSourceSite 
{
  GdkModifierType    start_button_mask;
  GtkTargetList     *target_list;        /* Targets for drag data */
  GdkDragAction      actions;            /* Possible actions */
  GdkColormap       *colormap;	         /* Colormap for drag icon */
  GdkPixmap         *pixmap;             /* Icon for drag data */
  GdkBitmap         *mask;

  /* Stored button press information to detect drag beginning */
  gint               state;
  gint               x, y;
};
  
struct _GtkDragSourceInfo 
{
  GtkWidget         *widget;
  GtkTargetList     *target_list; /* Targets for drag data */
  GdkDragAction      possible_actions; /* Actions allowed by source */
  GdkDragContext    *context;	  /* drag context */
  GtkWidget         *icon_window; /* Window for drag */
  GtkWidget         *ipc_widget;  /* GtkInvisible for grab, message passing */
  GdkCursor         *cursor;	  /* Cursor for drag */
  gint hot_x, hot_y;		  /* Hot spot for drag */
  gint button;			  /* mouse button starting drag */

  GtkDragStatus      status;	  /* drag status */
  GdkEvent          *last_event;  /* motion event waiting for response */

  gint               start_x, start_y; /* Initial position */
  gint               cur_x, cur_y;     /* Current Position */

  GList             *selections;  /* selections we've claimed */
  
  GtkDragDestInfo   *proxy_dest;  /* Set if this is a proxy drag */

  guint              drop_timeout;     /* Timeout for aborting drop */
  guint              destroy_icon : 1; /* If true, destroy icon_window
					*/
};

struct _GtkDragDestSite 
{
  GtkDestDefaults    flags;
  GtkTargetList     *target_list;
  GdkDragAction      actions;
  GdkWindow         *proxy_window;
  GdkDragProtocol    proxy_protocol;
  gboolean           do_proxy : 1;
  gboolean           proxy_coords : 1;
  gboolean           have_drag : 1;
};
  
struct _GtkDragDestInfo 
{
  GtkWidget         *widget;	   /* Widget in which drag is in */
  GdkDragContext    *context;	   /* Drag context */
  GtkDragSourceInfo *proxy_source; /* Set if this is a proxy drag */
  GtkSelectionData  *proxy_data;   /* Set while retrieving proxied data */
  gboolean           dropped : 1;     /* Set after we receive a drop */
  guint32            proxy_drop_time; /* Timestamp for proxied drop */
  gboolean           proxy_drop_wait : 1; /* Set if we are waiting for a
					   * status reply before sending
					   * a proxied drop on.
					   */
  gint               drop_x, drop_y; /* Position of drop */
};

#define DROP_ABORT_TIME 300000

#define ANIM_STEP_TIME 50
#define ANIM_STEP_LENGTH 50
#define ANIM_MIN_STEPS 5
#define ANIM_MAX_STEPS 10

struct _GtkDragAnim 
{
  GtkDragSourceInfo *info;
  gint step;
  gint n_steps;
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

/* Enumeration for some targets we handle internally */

enum {
  TARGET_MOTIF_SUCCESS = 0x40000000,
  TARGET_MOTIF_FAILURE,
  TARGET_DELETE
};

/* Drag icons */

static GdkPixmap   *default_icon_pixmap = NULL;
static GdkPixmap   *default_icon_mask = NULL;
static GdkColormap *default_icon_colormap = NULL;
static gint         default_icon_hot_x;
static gint         default_icon_hot_y;

/* Forward declarations */
static void          gtk_drag_get_event_actions (GdkEvent        *event, 
					         gint             button,
					         GdkDragAction    actions,
					         GdkDragAction   *suggested_action,
					         GdkDragAction   *possible_actions);
static GdkCursor *   gtk_drag_get_cursor         (GdkDragAction action);
static GtkWidget    *gtk_drag_get_ipc_widget     (void);
static void          gtk_drag_release_ipc_widget (GtkWidget      *widget);

static void          gtk_drag_highlight_paint    (GtkWidget      *widget);
static gboolean      gtk_drag_highlight_expose   (GtkWidget      *widget,
					  	  GdkEventExpose *event,
						  gpointer        data);


static GdkAtom   gtk_drag_dest_find_target    (GtkWidget          *widget,
					       GtkDragDestSite    *site,
			                       GdkDragContext     *context);
static void      gtk_drag_selection_received  (GtkWidget          *widget,
					       GtkSelectionData   *selection_data,
					       guint32             time,
					       gpointer            data);
static void      gtk_drag_find_widget         (GtkWidget          *widget,
					       GtkDragFindData    *data);
static void      gtk_drag_proxy_begin         (GtkWidget          *widget,
					       GtkDragDestInfo    *dest_info);
static void      gtk_drag_dest_info_destroy   (gpointer            data);
static void      gtk_drag_dest_realized       (GtkWidget          *widget);
static void      gtk_drag_dest_site_destroy   (gpointer            data);
static void      gtk_drag_dest_leave          (GtkWidget          *widget,
					       GdkDragContext     *context,
					       guint               time);
static gboolean  gtk_drag_dest_motion         (GtkWidget	  *widget,
				               GdkDragContext     *context,
					       gint                x,
					       gint                y,
					       guint               time);
static gboolean  gtk_drag_dest_drop           (GtkWidget	  *widget,
					       GdkDragContext     *context,
					       gint                x,
					       gint                y,
					       guint               time);

static void gtk_drag_source_check_selection    (GtkDragSourceInfo *info, 
					        GdkAtom            selection,
					        guint32            time);
static void gtk_drag_source_release_selections (GtkDragSourceInfo *info,
						guint32            time);
static void gtk_drag_drop                      (GtkDragSourceInfo *info,
						guint32            time);
static void gtk_drag_drop_finished             (GtkDragSourceInfo *info,
						gboolean           success,
						guint              time);

static gint gtk_drag_source_event_cb           (GtkWidget         *widget,
						GdkEvent          *event,
						gpointer           data);
static void gtk_drag_source_site_destroy       (gpointer           data);
static void gtk_drag_selection_get             (GtkWidget         *widget, 
						GtkSelectionData  *selection_data,
						guint              sel_info,
						guint32            time,
						gpointer           data);
static gint gtk_drag_anim_timeout              (gpointer           data);
static void gtk_drag_remove_icon               (GtkDragSourceInfo *info);
static void gtk_drag_source_info_destroy       (gpointer           data);
static void gtk_drag_update                    (GtkDragSourceInfo *info,
						gint               x_root,
						gint               y_root,
						GdkEvent          *event);
static gint gtk_drag_motion_cb                 (GtkWidget         *widget, 
					        GdkEventMotion    *event, 
					        gpointer           data);
static gint gtk_drag_key_cb                    (GtkWidget         *widget, 
					        GdkEventKey       *event, 
					        gpointer           data);
static gint gtk_drag_button_release_cb         (GtkWidget         *widget, 
					        GdkEventButton    *event, 
					        gpointer           data);
static gint gtk_drag_abort_timeout             (gpointer           data);

/************************
 * Cursor and Icon data *
 ************************/

#define action_ask_width 16
#define action_ask_height 16
static const guchar action_ask_bits[] = {
  0x00, 0x00, 0xfe, 0x7f, 0xfe, 0x1f, 0x06, 0xc0, 0x76, 0xf8, 0xb6, 0xf7, 
  0xd6, 0xec, 0x66, 0xdb, 0x66, 0xdb, 0x96, 0xec, 0x76, 0xf7, 0x76, 0xfb, 
  0xf6, 0xfc, 0x72, 0xfb, 0x7a, 0xfb, 0xf8, 0xfc, };

#define action_ask_mask_width 16
#define action_ask_mask_height 16
static const guchar action_ask_mask_bits[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x8f, 0x07, 0xcf, 0x0f, 
  0xef, 0x1f, 0xff, 0x3c, 0xff, 0x3c, 0x6f, 0x1f, 0x8f, 0x0f, 0x8f, 0x07, 
  0x0f, 0x03, 0x8f, 0x07, 0x87, 0x07, 0x07, 0x03, };

#define action_copy_width 16
#define action_copy_height 16
static const guchar action_copy_bits[] = {
  0x00, 0x00, 0xfe, 0x7f, 0xfe, 0x1f, 0x06, 0xc0, 0x76, 0xfb, 0x76, 0xfb, 
  0x76, 0xfb, 0x06, 0x83, 0xf6, 0xbf, 0xf6, 0xbf, 0x06, 0x83, 0x76, 0xfb, 
  0x76, 0xfb, 0x72, 0xfb, 0x7a, 0xf8, 0xf8, 0xff, };

#define action_copy_mask_width 16
#define action_copy_mask_height 16
static const guchar action_copy_mask_bits[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x8f, 0x07, 0x8f, 0x07, 
  0x8f, 0x07, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0x8f, 0x07, 
  0x8f, 0x07, 0x8f, 0x07, 0x87, 0x07, 0x07, 0x00, };

#define action_move_width 16
#define action_move_height 16
static const guchar action_move_bits[] = {
  0x00, 0x00, 0xfe, 0x7f, 0xfe, 0x1f, 0x06, 0xc0, 0x96, 0xff, 0x26, 0xff, 
  0xc6, 0xf8, 0xd6, 0xe3, 0x96, 0x8f, 0xb6, 0xbf, 0x36, 0xc3, 0x76, 0xfb, 
  0x76, 0xfa, 0xf2, 0xfa, 0xfa, 0xf8, 0xf8, 0xff, };

#define action_move_mask_width 16
#define action_move_mask_height 16
static const guchar action_move_mask_bits[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x6f, 0x00, 0xff, 0x00, 
  0xff, 0x07, 0xef, 0x1f, 0xef, 0x7f, 0xcf, 0x7f, 0xcf, 0x3f, 0x8f, 0x07, 
  0x8f, 0x07, 0x0f, 0x07, 0x07, 0x07, 0x07, 0x00, };

#define action_link_width 16
#define action_link_height 16
static const guchar action_link_bits[] = {
  0x00, 0x00, 0xfe, 0x7f, 0xfe, 0x1f, 0x06, 0xc0, 0x36, 0xf8, 0xd6, 0xf7, 
  0x66, 0xec, 0xa6, 0xe8, 0x26, 0xdf, 0xe6, 0xbd, 0xd6, 0xa7, 0xb6, 0xa8, 
  0xb6, 0xb1, 0x72, 0xdf, 0xfa, 0xe0, 0xf8, 0xff, };

#define action_link_mask_width 16
#define action_link_mask_height 16
static const guchar action_link_mask_bits[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xcf, 0x07, 0xef, 0x0f, 
  0xff, 0x1f, 0x7f, 0x1f, 0xff, 0x3f, 0xff, 0x7f, 0xef, 0x7f, 0xcf, 0x77, 
  0xcf, 0x7f, 0x8f, 0x3f, 0x07, 0x1f, 0x07, 0x00, };

#define action_none_width 16
#define action_none_height 16
static const guchar action_none_bits[] = {
  0x00, 0x00, 0xfe, 0x7f, 0xfe, 0x1f, 0x06, 0xc0, 0xf6, 0xff, 0xf6, 0xff, 
  0xf6, 0xff, 0xf6, 0xff, 0xf6, 0xff, 0xf6, 0xff, 0xf6, 0xff, 0xf6, 0xff, 
  0xf6, 0xff, 0xf2, 0xff, 0xfa, 0xff, 0xf8, 0xff, };

#define action_none_mask_width 16
#define action_none_mask_height 16
static const guchar action_none_mask_bits[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x0f, 0x00, 0x0f, 0x00, 
  0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 
  0x0f, 0x00, 0x0f, 0x00, 0x07, 0x00, 0x07, 0x00, };

#define CURSOR_WIDTH 16
#define CURSOR_HEIGHT 16

static struct {
  GdkDragAction action;
  const char *bits;
  const char *mask;
  GdkCursor    *cursor;
} drag_cursors[] = {
  { GDK_ACTION_DEFAULT, 0 },
  { GDK_ACTION_ASK,   action_ask_bits,  action_ask_mask_bits,  NULL },
  { GDK_ACTION_COPY,  action_copy_bits, action_copy_mask_bits, NULL },
  { GDK_ACTION_MOVE,  action_move_bits, action_move_mask_bits, NULL },
  { GDK_ACTION_LINK,  action_link_bits, action_link_mask_bits, NULL },
  { 0              ,  action_none_bits, action_none_mask_bits, NULL },
};

static const gint n_drag_cursors = sizeof (drag_cursors) / sizeof (drag_cursors[0]);

/* XPM */
static const char *drag_default_xpm[] = {
"32 32 3 1",
" 	c None",
".	c #000000",
"+	c #FFFFFF",
"                                ",
"                                ",
"                ..              ",
"              ..+.              ",
"             ..++..             ",
"           ...++++.             ",
"         ...++++++..            ",
"       ...+++++++++.            ",
"     ...+++++++++++..           ",
"    ..+.++++++++++++..          ",
"     .++.++++++++++++..         ",
"     .+++.++++++++++++..        ",
"     .++++.++++++++++++.        ",
"     .+++.+++++++++++++..       ",
"     .++.+++++++++++++++..      ",
"     .+.+++++++++++++++++..     ",
"     ..+++++++++++++++++++..    ",
"     ..++++++++++++++++++++.    ",
"     .++++++++++++++++++++..    ",
"     ..+++++++++++++++++..      ",
"      .++++++++++++++++..       ",
"      ..+++++++++++++...        ",
"       .++++++++++++..          ",
"       ..+++++++++..            ",
"        .++++++++..             ",
"        ..++++++..              ",
"         .+++++..               ",
"          .++..                 ",
"           ...                  ",
"           ..                   ",
"                                ",
"                                "};

/*********************
 * Utility functions *
 *********************/

/*************************************************************
 * gtk_drag_get_ipc_widget:
 *     Return a invisible, off-screen, override-redirect
 *     widget for IPC.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static GtkWidget *
gtk_drag_get_ipc_widget (void)
{
  GtkWidget *result;
  
  if (drag_widgets)
    {
      GSList *tmp = drag_widgets;
      result = drag_widgets->data;
      drag_widgets = drag_widgets->next;
      g_slist_free_1 (tmp);
    }
  else
    {
      result = gtk_invisible_new ();
      gtk_widget_show (result);
    }

  return result;
}

/***************************************************************
 * gtk_drag_release_ipc_widget:
 *     Releases widget retrieved with gtk_drag_get_ipc_widget ()
 *   arguments:
 *     widget: the widget to release.
 *   results:
 ***************************************************************/

static void
gtk_drag_release_ipc_widget (GtkWidget *widget)
{
  drag_widgets = g_slist_prepend (drag_widgets, widget);
}

static guint32
gtk_drag_get_event_time (GdkEvent *event)
{
  guint32 tm = GDK_CURRENT_TIME;
  
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
	tm = event->motion.time; break;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
	tm = event->button.time; break;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
	tm = event->key.time; break;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	tm = event->crossing.time; break;
      case GDK_PROPERTY_NOTIFY:
	tm = event->property.time; break;
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
	tm = event->selection.time; break;
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
	tm = event->proximity.time; break;
      default:			/* use current time */
	break;
      }
  
  return tm;
}

static void
gtk_drag_get_event_actions (GdkEvent *event, 
			    gint button, 
			    GdkDragAction  actions,
			    GdkDragAction *suggested_action,
			    GdkDragAction *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if (event)
    {
      GdkModifierType state = 0;
      
      switch (event->type)
	{
	case GDK_MOTION_NOTIFY:
	  state = event->motion.state;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	  state = event->button.state;
	  break;
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
	  state = event->key.state;
	  break;
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  state = event->crossing.state;
	  break;
	default:
	  break;
	}
      
      if ((button == 2 || button == 3) && (actions & GDK_ACTION_ASK))
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
	      return;
	    }
	  else
	    {
	      if (actions & GDK_ACTION_MOVE)
		{
		  *suggested_action = GDK_ACTION_MOVE;
		  *possible_actions = GDK_ACTION_MOVE;
		}
	      return;
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
  
  return;
}

static GdkCursor *
gtk_drag_get_cursor (GdkDragAction action)
{
  gint i;
  
  for (i = 0 ; i < n_drag_cursors - 1; i++)
    if (drag_cursors[i].action == action)
      break;

  if (drag_cursors[i].cursor == NULL)
    {
      GdkColor fg, bg;

      GdkPixmap *pixmap = 
	gdk_bitmap_create_from_data (NULL, 
				     drag_cursors[i].bits,
				     CURSOR_WIDTH, CURSOR_HEIGHT);
      GdkPixmap *mask = 
	gdk_bitmap_create_from_data (NULL, 
				     drag_cursors[i].mask,
				     CURSOR_WIDTH, CURSOR_HEIGHT);

      gdk_color_white (gdk_colormap_get_system (), &bg);
      gdk_color_black (gdk_colormap_get_system (), &fg);
      
      drag_cursors[i].cursor = gdk_cursor_new_from_pixmap (pixmap, mask, &fg, &bg, 0, 0);

      gdk_pixmap_unref (pixmap);
      gdk_pixmap_unref (mask);
    }

  return drag_cursors[i].cursor;
}

/********************
 * Destination side *
 ********************/

/*************************************************************
 * gtk_drag_get_data:
 *     Get the data for a drag or drop
 *   arguments:
 *     context - drag context
 *     target  - format to retrieve the data in.
 *     time    - timestamp of triggering event.
 *     
 *   results:
 *************************************************************/

void 
gtk_drag_get_data (GtkWidget      *widget,
		   GdkDragContext *context,
		   GdkAtom         target,
		   guint32         time)
{
  GtkWidget *selection_widget;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (context != NULL);

  selection_widget = gtk_drag_get_ipc_widget ();

  gdk_drag_context_ref (context);
  gtk_widget_ref (widget);
  
  gtk_signal_connect (GTK_OBJECT (selection_widget), "selection_received",
		      GTK_SIGNAL_FUNC (gtk_drag_selection_received), widget);

  gtk_object_set_data (GTK_OBJECT (selection_widget), "drag-context", context);

  gtk_selection_convert (selection_widget,
			 gdk_drag_get_selection (context),
			 target,
			 time);
}


/*************************************************************
 * gtk_drag_get_source_widget:
 *     Get the widget the was the source of this drag, if
 *     the drag originated from this application.
 *   arguments:
 *     context: The drag context for this drag
 *   results:
 *     The source widget, or NULL if the drag originated from
 *     a different application.
 *************************************************************/

GtkWidget *
gtk_drag_get_source_widget (GdkDragContext *context)
{
  GSList *tmp_list;

  tmp_list = source_widgets;
  while (tmp_list)
    {
      GtkWidget *ipc_widget = tmp_list->data;
      
      if (ipc_widget->window == context->source_window)
	{
	  GtkDragSourceInfo *info;
	  info = gtk_object_get_data (GTK_OBJECT (ipc_widget), "gtk-info");

	  return info ? info->widget : NULL;
	}

      tmp_list = tmp_list->next;
    }

  return NULL;
}

/*************************************************************
 * gtk_drag_finish:
 *     Notify the drag source that the transfer of data
 *     is complete.
 *   arguments:
 *     context: The drag context for this drag
 *     success: Was the data successfully transferred?
 *     time:    The timestamp to use when notifying the destination.
 *   results:
 *************************************************************/

void 
gtk_drag_finish (GdkDragContext *context,
		 gboolean        success,
		 gboolean        del,
		 guint32         time)
{
  GdkAtom target = GDK_NONE;

  g_return_if_fail (context != NULL);

  if (success && del)
    {
      target = gdk_atom_intern ("DELETE", FALSE);
    }
  else if (context->protocol == GDK_DRAG_PROTO_MOTIF)
    {
      target = gdk_atom_intern (success ? 
				  "XmTRANSFER_SUCCESS" : 
				  "XmTRANSFER_FAILURE",
				FALSE);
    }

  if (target != GDK_NONE)
    {
      GtkWidget *selection_widget = gtk_drag_get_ipc_widget ();

      gdk_drag_context_ref (context);
      
      gtk_object_set_data (GTK_OBJECT (selection_widget), "drag-context", context);
      gtk_signal_connect (GTK_OBJECT (selection_widget), "selection_received",
			  GTK_SIGNAL_FUNC (gtk_drag_selection_received),
			  NULL);
      
      gtk_selection_convert (selection_widget,
			     gdk_drag_get_selection (context),
			     target,
			     time);
    }
  
  if (!del)
    gdk_drop_finish (context, success, time);
}

/*************************************************************
 * gtk_drag_highlight_paint:
 *     Paint a highlight indicating drag status onto the widget.
 *   arguments:
 *     widget:
 *   results:
 *************************************************************/

static void 
gtk_drag_highlight_paint (GtkWidget  *widget)
{
  gint x, y, width, height;

  g_return_if_fail (widget != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (GTK_WIDGET_NO_WINDOW (widget))
	{
	  x = widget->allocation.x;
	  y = widget->allocation.y;
	  width = widget->allocation.width;
	  height = widget->allocation.height;
	}
      else
	{
	  x = 0;
	  y = 0;
	  gdk_window_get_size (widget->window, &width, &height);
	}
      
      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		       x, y, width, height);
      
      gdk_draw_rectangle (widget->window,
			  widget->style->black_gc,
			  FALSE,
			  x, y, width - 1, height - 1);
    }
}

/*************************************************************
 * gtk_drag_highlight_expose:
 *     Callback for expose_event for highlighted widgets.
 *   arguments:
 *     widget:
 *     event:
 *     data:
 *   results:
 *************************************************************/

static gboolean
gtk_drag_highlight_expose (GtkWidget      *widget,
			   GdkEventExpose *event,
			   gpointer        data)
{
  gtk_drag_highlight_paint (widget);
  return TRUE;
}

/*************************************************************
 * gtk_drag_highlight:
 *     Highlight the given widget in the default manner.
 *   arguments:
 *     widget:
 *   results:
 *************************************************************/

void 
gtk_drag_highlight (GtkWidget  *widget)
{
  gtk_signal_connect_after (GTK_OBJECT (widget), "draw",
			    GTK_SIGNAL_FUNC (gtk_drag_highlight_paint),
			    NULL);
  gtk_signal_connect (GTK_OBJECT (widget), "expose_event",
		      GTK_SIGNAL_FUNC (gtk_drag_highlight_expose),
		      NULL);

  gtk_widget_queue_draw (widget);
}

/*************************************************************
 * gtk_drag_unhighlight:
 *     Refresh the given widget to remove the highlight.
 *   arguments:
 *     widget:
 *   results:
 *************************************************************/

void 
gtk_drag_unhighlight (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);

  gtk_signal_disconnect_by_func (GTK_OBJECT (widget),
				 GTK_SIGNAL_FUNC (gtk_drag_highlight_paint),
				 NULL);
  gtk_signal_disconnect_by_func (GTK_OBJECT (widget),
				 GTK_SIGNAL_FUNC (gtk_drag_highlight_expose),
				 NULL);
  
  gtk_widget_queue_clear (widget);
}

/*************************************************************
 * gtk_drag_dest_set:
 *     Register a drop site, and possibly add default behaviors.
 *   arguments:
 *     widget:    
 *     flags:     Which types of default drag behavior to use
 *     targets:   Table of targets that can be accepted
 *     n_targets: Number of of entries in targets
 *     actions:   
 *   results:
 *************************************************************/

void 
gtk_drag_dest_set   (GtkWidget            *widget,
		     GtkDestDefaults       flags,
		     const GtkTargetEntry *targets,
		     gint                  n_targets,
		     GdkDragAction         actions)
{
  GtkDragDestSite *site;
  
  g_return_if_fail (widget != NULL);

  /* HACK, do this in the destroy */
  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-drag-dest");
  if (site)
    gtk_signal_disconnect_by_data (GTK_OBJECT (widget), site);

  if (GTK_WIDGET_REALIZED (widget))
    gtk_drag_dest_realized (widget);

  gtk_signal_connect (GTK_OBJECT (widget), "realize",
		      GTK_SIGNAL_FUNC (gtk_drag_dest_realized), NULL);

  site = g_new (GtkDragDestSite, 1);

  site->flags = flags;
  site->have_drag = FALSE;
  if (targets)
    site->target_list = gtk_target_list_new (targets, n_targets);
  else
    site->target_list = NULL;

  site->actions = actions;
  site->do_proxy = FALSE;

  gtk_object_set_data_full (GTK_OBJECT (widget), "gtk-drag-dest",
			    site, gtk_drag_dest_site_destroy);
}

/*************************************************************
 * gtk_drag_dest_set_proxy:
 *     Set up this widget to proxy drags elsewhere.
 *   arguments:
 *     widget:          
 *     proxy_window:    window to which forward drag events
 *     protocol:        Drag protocol which the dest widget accepts
 *     use_coordinates: If true, send the same coordinates to the
 *                      destination, because it is a embedded 
 *                      subwindow.
 *   results:
 *************************************************************/

void 
gtk_drag_dest_set_proxy (GtkWidget      *widget,
			 GdkWindow      *proxy_window,
			 GdkDragProtocol protocol,
			 gboolean        use_coordinates)
{
  GtkDragDestSite *site;
  
  g_return_if_fail (widget != NULL);

  /* HACK, do this in the destroy */
  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-drag-dest");
  if (site)
    gtk_signal_disconnect_by_data (GTK_OBJECT (widget), site);

  if (GTK_WIDGET_REALIZED (widget))
    gtk_drag_dest_realized (widget);

  gtk_signal_connect (GTK_OBJECT (widget), "realize",
		      GTK_SIGNAL_FUNC (gtk_drag_dest_realized), NULL);

  site = g_new (GtkDragDestSite, 1);

  site->flags = 0;
  site->have_drag = FALSE;
  site->target_list = NULL;
  site->actions = 0;
  site->proxy_window = proxy_window;
  if (proxy_window)
    gdk_window_ref (proxy_window);
  site->do_proxy = TRUE;
  site->proxy_protocol = protocol;
  site->proxy_coords = use_coordinates;

  gtk_object_set_data_full (GTK_OBJECT (widget), "gtk-drag-dest",
			    site, gtk_drag_dest_site_destroy);
}

/*************************************************************
 * gtk_drag_dest_unset
 *     Unregister this widget as a drag target.
 *   arguments:
 *     widget:
 *   results:
 *************************************************************/

void 
gtk_drag_dest_unset (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);

  gtk_object_set_data (GTK_OBJECT (widget), "gtk-drag-dest", NULL);
}

/*************************************************************
 * gtk_drag_dest_handle_event:
 *     Called from widget event handling code on Drag events
 *     for destinations.
 *
 *   arguments:
 *     toplevel: Toplevel widget that received the event
 *     event:
 *   results:
 *************************************************************/

void
gtk_drag_dest_handle_event (GtkWidget *toplevel,
			    GdkEvent  *event)
{
  GtkDragDestInfo *info;
  GdkDragContext *context;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  context = event->dnd.context;

  info = g_dataset_get_data (context, "gtk-info");
  if (!info)
    {
      info = g_new (GtkDragDestInfo, 1);
      info->widget = NULL;
      info->context = event->dnd.context;
      info->proxy_source = NULL;
      info->proxy_data = NULL;
      info->dropped = FALSE;
      info->proxy_drop_wait = FALSE;
      g_dataset_set_data_full (context,
			       "gtk-info",
			       info,
			       gtk_drag_dest_info_destroy);
    }

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

	gdk_window_get_origin (toplevel->window, &tx, &ty);

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
	else if (event->type == GDK_DROP_START && !info->proxy_source)
	  {
	    gdk_drop_reply (context, data.found, event->dnd.time);
            if ((context->protocol == GDK_DRAG_PROTO_MOTIF) && !data.found)
	      gtk_drag_finish (context, FALSE, FALSE, event->dnd.time);
	  }
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

/*************************************************************
 * gtk_drag_dest_find_target:
 *     Decide on a target for the drag.
 *   arguments:
 *     site:
 *     context:
 *   results:
 *************************************************************/

static GdkAtom
gtk_drag_dest_find_target (GtkWidget       *widget,
			   GtkDragDestSite *site,
			   GdkDragContext  *context)
{
  GList *tmp_target;
  GList *tmp_source = NULL;
  GtkWidget *source_widget = gtk_drag_get_source_widget (context);

  tmp_target = site->target_list->list;
  while (tmp_target)
    {
      GtkTargetPair *pair = tmp_target->data;
      tmp_source = context->targets;
      while (tmp_source)
	{
	  if (tmp_source->data == GUINT_TO_POINTER (pair->target))
	    {
	      if ((!(pair->flags & GTK_TARGET_SAME_APP) || source_widget) &&
		  (!(pair->flags & GTK_TARGET_SAME_WIDGET) || (source_widget == widget)))
		return pair->target;
	      else
		break;
	    }
	  tmp_source = tmp_source->next;
	}
      tmp_target = tmp_target->next;
    }

  return GDK_NONE;
}

static void
gtk_drag_selection_received (GtkWidget        *widget,
			     GtkSelectionData *selection_data,
			     guint32           time,
			     gpointer          data)
{
  GdkDragContext *context;
  GtkDragDestInfo *info;
  GtkWidget *drop_widget;

  drop_widget = data;

  context = gtk_object_get_data (GTK_OBJECT (widget), "drag-context");
  info = g_dataset_get_data (context, "gtk-info");

  if (info->proxy_data && 
      info->proxy_data->target == selection_data->target)
    {
      gtk_selection_data_set (info->proxy_data,
			      selection_data->type,
			      selection_data->format,
			      selection_data->data,
			      selection_data->length);
      gtk_main_quit ();
      return;
    }

  if (selection_data->target == gdk_atom_intern ("DELETE", FALSE))
    {
      gtk_drag_finish (context, TRUE, FALSE, time);
    }
  else if ((selection_data->target == gdk_atom_intern ("XmTRANSFER_SUCCESS", FALSE)) ||
	   (selection_data->target == gdk_atom_intern ("XmTRANSFER_FAILURE", FALSE)))
    {
      /* Do nothing */
    }
  else
    {
      GtkDragDestSite *site;

      site = gtk_object_get_data (GTK_OBJECT (drop_widget), "gtk-drag-dest");

      if (site->target_list)
	{
	  guint target_info;

	  if (gtk_target_list_find (site->target_list, 
				    selection_data->target,
				    &target_info))
	    {
	      if (!(site->flags & GTK_DEST_DEFAULT_DROP) ||
		  selection_data->length >= 0)
		gtk_signal_emit_by_name (GTK_OBJECT (drop_widget), 
					 "drag_data_received",
					 context, info->drop_x, info->drop_y,
					 selection_data, 
					 target_info, time);
	    }
	}
      else
	{
	  gtk_signal_emit_by_name (GTK_OBJECT (drop_widget), 
				   "drag_data_received",
				   context, info->drop_x, info->drop_y,
				   selection_data, 0, time);
	}
      
      if (site->flags & GTK_DEST_DEFAULT_DROP)
	{

	  gtk_drag_finish (context, 
			   (selection_data->length >= 0),
			   (context->action == GDK_ACTION_MOVE),
			   time);
	}
      
      gtk_widget_unref (drop_widget);
    }

  gtk_signal_disconnect_by_func (GTK_OBJECT (widget), 
				 GTK_SIGNAL_FUNC (gtk_drag_selection_received),
				 data);
  
  gtk_object_set_data (GTK_OBJECT (widget), "drag-context", NULL);
  gdk_drag_context_unref (context);

  gtk_drag_release_ipc_widget (widget);
}

static void
get_all_children_callback (GtkWidget *widget,
			   gpointer   client_data)
{
  GList **children;

  children = (GList**) client_data;
  gtk_widget_ref (widget);
  
  *children = g_list_prepend (*children, widget);
}

static GList*
get_all_children (GtkContainer *container)
{
  GList *children;

  children = NULL;

  gtk_container_forall (container,
			get_all_children_callback,
			&children);

  return g_list_reverse (children);
}



/*************************************************************
 * gtk_drag_find_widget:
 *     Recursive callback used to locate widgets for 
 *     DRAG_MOTION and DROP_START events.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static void
gtk_drag_find_widget (GtkWidget       *widget,
		      GtkDragFindData *data)
{
  GtkAllocation new_allocation;
  gint x_offset = 0;
  gint y_offset = 0;

  new_allocation = widget->allocation;

  if (data->found || !GTK_WIDGET_MAPPED (widget))
    return;

  /* Note that in the following code, we only count the
   * position as being inside a WINDOW widget if it is inside
   * widget->window; points that are outside of widget->window
   * but within the allocation are not counted. This is consistent
   * with the way we highlight drag targets.
   */
  if (!GTK_WIDGET_NO_WINDOW (widget))
    {
      new_allocation.x = 0;
      new_allocation.y = 0;
    }
  
  if (widget->parent)
    {
      GdkWindow *window = widget->window;
      while (window != widget->parent->window)
	{
	  gint tx, ty, twidth, theight;
	  gdk_window_get_size (window, &twidth, &theight);

	  if (new_allocation.x < 0)
	    {
	      new_allocation.width += new_allocation.x;
	      new_allocation.x = 0;
	    }
	  if (new_allocation.y < 0)
	    {
	      new_allocation.height += new_allocation.y;
	      new_allocation.y = 0;
	    }
	  if (new_allocation.x + new_allocation.width > twidth)
	    new_allocation.width = twidth - new_allocation.x;
	  if (new_allocation.y + new_allocation.height > theight)
	    new_allocation.height = theight - new_allocation.y;

	  gdk_window_get_position (window, &tx, &ty);
	  new_allocation.x += tx;
	  x_offset += tx;
	  new_allocation.y += ty;
	  y_offset += ty;
	  
	  window = gdk_window_get_parent (window);
	}
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
	  GList *children = get_all_children (GTK_CONTAINER (widget));
	  GList *tmp_list;

	  new_data.x -= x_offset;
	  new_data.y -= y_offset;
	  new_data.found = FALSE;
	  new_data.toplevel = FALSE;

	  tmp_list = children;
	  while (tmp_list)
	    {
	      GtkWidget *child = tmp_list->data;

	      if (child->parent == widget)
		gtk_drag_find_widget (child, &new_data);
	      
	      gtk_widget_unref (child);
	      tmp_list = tmp_list->next;
	    }

	  g_list_free (children);

	  data->found = new_data.found;
	}

      /* If not, and this widget is registered as a drop site, check to
       * emit "drag_motion" to check if we are actually in
       * a drop site.
       */
      if (!data->found &&
	  gtk_object_get_data (GTK_OBJECT (widget), "gtk-drag-dest"))
	{
	  data->found = data->callback (widget,
					data->context,
					data->x - x_offset,
					data->y - y_offset,
					data->time);
	  /* If so, send a "drag_leave" to the last widget */
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
gtk_drag_proxy_begin (GtkWidget       *widget, 
		      GtkDragDestInfo *dest_info)
{
  GtkDragSourceInfo *source_info;
  GList *tmp_list;
  
  source_info = g_new0 (GtkDragSourceInfo, 1);
  source_info->ipc_widget = gtk_drag_get_ipc_widget ();
  
  source_info->widget = widget;
  gtk_widget_ref (source_info->widget);
  source_info->context = gdk_drag_begin (source_info->ipc_widget->window,
					 dest_info->context->targets);

  source_info->target_list = gtk_target_list_new (NULL, 0);
  tmp_list = dest_info->context->targets;
  while (tmp_list)
    {
      gtk_target_list_add (source_info->target_list, 
			   GPOINTER_TO_UINT (tmp_list->data), 0, 0);
      tmp_list = tmp_list->next;
    }

  source_info->proxy_dest = dest_info;
  
  g_dataset_set_data (source_info->context, "gtk-info", source_info);
  
  gtk_signal_connect (GTK_OBJECT (source_info->ipc_widget), 
		      "selection_get",
		      GTK_SIGNAL_FUNC (gtk_drag_selection_get), 
		      source_info);
  
  dest_info->proxy_source = source_info;
}

static void
gtk_drag_dest_info_destroy (gpointer data)
{
  GtkDragDestInfo *info = data;

  g_free (info);
}

static void
gtk_drag_dest_realized (GtkWidget *widget)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
  gdk_window_register_dnd (toplevel->window);
}

static void
gtk_drag_dest_site_destroy (gpointer data)
{
  GtkDragDestSite *site = data;

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  g_free (site);
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

  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-drag-dest");
  g_return_if_fail (site != NULL);

  if (site->do_proxy)
    {
      GtkDragDestInfo *info = g_dataset_get_data (context, "gtk-info");

      if (info->proxy_source && !info->dropped)
	gdk_drag_abort (info->proxy_source->context, time);
      
      return;
    }
  else
    {
      if ((site->flags & GTK_DEST_DEFAULT_HIGHLIGHT) && site->have_drag)
	gtk_drag_unhighlight (widget);

      if (!(site->flags & GTK_DEST_DEFAULT_MOTION) || site->have_drag)
	gtk_signal_emit_by_name (GTK_OBJECT (widget), "drag_leave",
				 context, time);
      
      site->have_drag = FALSE;
    }
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

  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  if (site->do_proxy)
    {
      GdkAtom selection;
      GdkEvent *current_event;
      GdkWindow *dest_window;
      GdkDragProtocol proto;
	
      GtkDragDestInfo *info = g_dataset_get_data (context, "gtk-info");

      if (!info->proxy_source)
	gtk_drag_proxy_begin (widget, info);

      current_event = gtk_get_current_event ();

      if (site->proxy_window)
	{
	  dest_window = site->proxy_window;
	  proto = site->proxy_protocol;
	}
      else
	{
	  gdk_drag_find_window (info->proxy_source->context,
				NULL,
				current_event->dnd.x_root, 
				current_event->dnd.y_root,
				&dest_window, &proto);
	}
      
      gdk_drag_motion (info->proxy_source->context, 
		       dest_window, proto,
		       current_event->dnd.x_root, 
		       current_event->dnd.y_root, 
		       context->suggested_action, 
		       context->actions, time);

      if (!site->proxy_window && dest_window)
	gdk_window_unref (dest_window);

      selection = gdk_drag_get_selection (info->proxy_source->context);
      if (selection && 
	  selection != gdk_drag_get_selection (info->context))
	gtk_drag_source_check_selection (info->proxy_source, selection, time);

      gdk_event_free (current_event);
      
      return TRUE;
    }

  if (site->flags & GTK_DEST_DEFAULT_MOTION)
    {
      if (context->suggested_action & site->actions)
	action = context->suggested_action;
      else
	{
	  gint i;
	  
	  for (i=0; i<8; i++)
	    {
	      if ((site->actions & (1 << i)) &&
		  (context->actions & (1 << i)))
		{
		  action = (1 << i);
		  break;
		}
	    }
	}
      
      if (action && gtk_drag_dest_find_target (widget, site, context))
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
	  return TRUE;
	}
    }

  gtk_signal_emit_by_name (GTK_OBJECT (widget), "drag_motion",
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

  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  info = g_dataset_get_data (context, "gtk-info");
  g_return_val_if_fail (info != NULL, FALSE);

  info->drop_x = x;
  info->drop_y = y;

  if (site->do_proxy)
    {
      if (info->proxy_source || 
	  (info->context->protocol == GDK_DRAG_PROTO_ROOTWIN))
	{
	  gtk_drag_drop (info->proxy_source, time);
	}
      else
	{
	  /* We need to synthesize a motion event, wait for a status,
	   * and, if we get a good one, do a drop.
	   */
	  
	  GdkEvent *current_event;
	  GdkAtom selection;
	  GdkWindow *dest_window;
	  GdkDragProtocol proto;
	  
	  gtk_drag_proxy_begin (widget, info);
	  info->proxy_drop_wait = TRUE;
	  info->proxy_drop_time = time;
	  
	  current_event = gtk_get_current_event ();

	  if (site->proxy_window)
	    {
	      dest_window = site->proxy_window;
	      proto = site->proxy_protocol;
	    }
	  else
	    {
	      gdk_drag_find_window (info->proxy_source->context,
				    NULL,
				    current_event->dnd.x_root, 
				    current_event->dnd.y_root,
				    &dest_window, &proto);
	    }

	  gdk_drag_motion (info->proxy_source->context, 
			   dest_window, proto,
			   current_event->dnd.x_root, 
			   current_event->dnd.y_root, 
			   context->suggested_action, 
			   context->actions, time);

	  if (!site->proxy_window && dest_window)
	    gdk_window_unref (dest_window);

	  selection = gdk_drag_get_selection (info->proxy_source->context);
	  if (selection && 
	      selection != gdk_drag_get_selection (info->context))
	    gtk_drag_source_check_selection (info->proxy_source, selection, time);

	  gdk_event_free (current_event);
      
	}

      return TRUE;
    }
  else
    {
      gboolean retval;

      if (site->flags & GTK_DEST_DEFAULT_DROP)
	{
	  GdkAtom target = gtk_drag_dest_find_target (widget, site, context);
      
	  if (target == GDK_NONE)
	    return FALSE;
	  
	  gtk_drag_get_data (widget, context, target, time);
	}

      gtk_signal_emit_by_name (GTK_OBJECT (widget), "drag_drop",
			       context, x, y, time, &retval);

      return (site->flags & GTK_DEST_DEFAULT_DROP) ? TRUE : retval;
    }
}

/***************
 * Source side *
 ***************/

/*************************************************************
 * gtk_drag_begin: Start a drag operation
 *     
 *   arguments:
 *     widget:   Widget from which drag starts
 *     handlers: List of handlers to supply the data for the drag
 *     button:   Button user used to start drag
 *     time:     Time of event starting drag
 *
 *   results:
 *************************************************************/

GdkDragContext *
gtk_drag_begin (GtkWidget         *widget,
		GtkTargetList     *target_list,
		GdkDragAction      actions,
		gint               button,
		GdkEvent          *event)
{
  GtkDragSourceInfo *info;
  GList *targets = NULL;
  GList *tmp_list;
  guint32 time = GDK_CURRENT_TIME;
  GdkDragAction possible_actions, suggested_action;

  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), NULL);
  g_return_val_if_fail (target_list != NULL, NULL);

  if (event)
    time = gdk_event_get_time (event);

  info = g_new0 (GtkDragSourceInfo, 1);
  info->ipc_widget = gtk_drag_get_ipc_widget ();
  source_widgets = g_slist_prepend (source_widgets, info->ipc_widget);

  gtk_object_set_data (GTK_OBJECT (info->ipc_widget), "gtk-info", info);

  tmp_list = g_list_last (target_list->list);
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;
      targets = g_list_prepend (targets, 
				GINT_TO_POINTER (pair->target));
      tmp_list = tmp_list->prev;
    }

  info->widget = widget;
  gtk_widget_ref (info->widget);
  
  info->context = gdk_drag_begin (info->ipc_widget->window, targets);
  g_list_free (targets);
  
  g_dataset_set_data (info->context, "gtk-info", info);

  info->button = button;
  info->target_list = target_list;
  gtk_target_list_ref (target_list);

  info->possible_actions = actions;

  info->cursor = NULL;
  info->status = GTK_DRAG_STATUS_DRAG;
  info->last_event = NULL;
  info->selections = NULL;
  info->icon_window = NULL;
  info->destroy_icon = FALSE;

  gtk_drag_get_event_actions (event, info->button, actions,
			      &suggested_action, &possible_actions);
  
  info->cursor = gtk_drag_get_cursor (suggested_action);

  /* Set cur_x, cur_y here so if the "drag_begin" signal shows
   * the drag icon, it will be in the right place
   */
  if (event && event->type == GDK_MOTION_NOTIFY)
    {
      info->cur_x = event->motion.x_root;
      info->cur_y = event->motion.y_root;
    }
  else 
    {
      gint x, y;
      gdk_window_get_pointer (GDK_ROOT_PARENT (), &x, &y, NULL);

      info->cur_x = x;
      info->cur_y = y;
    }

  gtk_signal_emit_by_name (GTK_OBJECT (widget), "drag_begin",
			   info->context);
  
  if (event && event->type == GDK_MOTION_NOTIFY)
    gtk_drag_motion_cb (info->ipc_widget, (GdkEventMotion *)event, info);

  info->start_x = info->cur_x;
  info->start_y = info->cur_y;

  gtk_signal_connect (GTK_OBJECT (info->ipc_widget), "button_release_event",
		      GTK_SIGNAL_FUNC (gtk_drag_button_release_cb), info);
  gtk_signal_connect (GTK_OBJECT (info->ipc_widget), "motion_notify_event",
		      GTK_SIGNAL_FUNC (gtk_drag_motion_cb), info);
  gtk_signal_connect (GTK_OBJECT (info->ipc_widget), "key_press_event",
		      GTK_SIGNAL_FUNC (gtk_drag_key_cb), info);
  gtk_signal_connect (GTK_OBJECT (info->ipc_widget), "key_release_event",
		      GTK_SIGNAL_FUNC (gtk_drag_key_cb), info);
  gtk_signal_connect (GTK_OBJECT (info->ipc_widget), "selection_get",
		      GTK_SIGNAL_FUNC (gtk_drag_selection_get), info);

  /* We use a GTK grab here to override any grabs that the widget
   * we are dragging from might have held
   */
  gtk_grab_add (info->ipc_widget);
  if (gdk_pointer_grab (info->ipc_widget->window, FALSE,
			GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
			GDK_BUTTON_RELEASE_MASK, NULL,
			info->cursor, time) == 0)
    {
      if (gdk_keyboard_grab (info->ipc_widget->window, FALSE, time) != 0)
	{
	  /* FIXME: This should be cleaned up... */
	  GdkEventButton ev;

	  ev.time = time;
	  ev.type = GDK_BUTTON_RELEASE;
	  ev.button = info->button;

	  gtk_drag_button_release_cb (widget, &ev, info);

	  return NULL;
	}
    }

  return info->context;
}

/*************************************************************
 * gtk_drag_source_set:
 *     Register a drop site, and possibly add default behaviors.
 *   arguments:
 *     widget:
 *     start_button_mask: Mask of allowed buttons to start drag
 *     targets:           Table of targets for this source
 *     n_targets:
 *     actions:           Actions allowed for this source
 *   results:
 *************************************************************/

void 
gtk_drag_source_set (GtkWidget            *widget,
		     GdkModifierType       start_button_mask,
		     const GtkTargetEntry *targets,
		     gint                  n_targets,
		     GdkDragAction         actions)
{
  GtkDragSourceSite *site;

  g_return_if_fail (widget != NULL);

  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-site-data");

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
      
      gtk_signal_connect (GTK_OBJECT (widget), "button_press_event",
			  GTK_SIGNAL_FUNC (gtk_drag_source_event_cb),
			  site);
      gtk_signal_connect (GTK_OBJECT (widget), "motion_notify_event",
			  GTK_SIGNAL_FUNC (gtk_drag_source_event_cb),
			  site);
      
      gtk_object_set_data_full (GTK_OBJECT (widget),
				"gtk-site-data", 
				site, gtk_drag_source_site_destroy);
    }

  site->start_button_mask = start_button_mask;

  if (targets)
    site->target_list = gtk_target_list_new (targets, n_targets);
  else
    site->target_list = NULL;

  site->actions = actions;

}

/*************************************************************
 * gtk_drag_source_unset
 *     Unregister this widget as a drag source.
 *   arguments:
 *     widget:
 *   results:
 *************************************************************/

void 
gtk_drag_source_unset (GtkWidget        *widget)
{
  GtkDragSourceSite *site;

  g_return_if_fail (widget != NULL);

  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-site-data");

  if (site)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (widget), site);
      gtk_object_set_data (GTK_OBJECT (widget), "gtk-site-data", NULL);
    }
}

/*************************************************************
 * gtk_drag_source_set_icon:
 *     Set an icon for drags from this source.
 *   arguments:
 *     colormap: Colormap for this icon
 *     pixmap:
 *     mask
 *   results:
 *************************************************************/

void 
gtk_drag_source_set_icon (GtkWidget     *widget,
			  GdkColormap   *colormap,
			  GdkPixmap     *pixmap,
			  GdkBitmap     *mask)
{
  GtkDragSourceSite *site;

  g_return_if_fail (widget != NULL);

  site = gtk_object_get_data (GTK_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);
  
  if (site->colormap)
    gdk_colormap_unref (site->colormap);
  if (site->pixmap)
    gdk_pixmap_unref (site->pixmap);
  if (site->mask)
    gdk_pixmap_unref (site->mask);

  site->colormap = colormap;
  if (colormap)
    gdk_colormap_ref (colormap);

  site->pixmap = pixmap;
  if (pixmap)
    gdk_pixmap_ref (pixmap);

  site->mask = mask;
  if (mask)
    gdk_pixmap_ref (mask);
}

/*************************************************************
 * gtk_drag_set_icon_window:
 *     Set a widget as the icon for a drag.
 *   arguments:
 *     context:
 *     widget:
 *     hot_x:    Hot spot
 *     hot_y:
 *   results:
 *************************************************************/

static void 
gtk_drag_set_icon_window (GdkDragContext *context,
			  GtkWidget      *widget,
			  gint            hot_x,
			  gint            hot_y,
			  gboolean        destroy_on_release)
{
  GtkDragSourceInfo *info;

  g_return_if_fail (context != NULL);
  g_return_if_fail (widget != NULL);

  info = g_dataset_get_data (context, "gtk-info");
  gtk_drag_remove_icon (info);

  info->icon_window = widget;
  info->hot_x = hot_x;
  info->hot_y = hot_y;

  if (widget)
    {
      gtk_widget_set_uposition (widget, 
				info->cur_x - info->hot_x, 
				info->cur_y - info->hot_y);
      gtk_widget_ref (widget);
      gdk_window_raise (widget->window);
      gtk_widget_show (widget);
    }

  info->destroy_icon = destroy_on_release;
}

/*************************************************************
 * gtk_drag_set_icon_widget:
 *     Set a widget as the icon for a drag.
 *   arguments:
 *     context:
 *     widget:
 *     hot_x:    Hot spot
 *     hot_y:
 *   results:
 *************************************************************/

void 
gtk_drag_set_icon_widget (GdkDragContext    *context,
			  GtkWidget         *widget,
			  gint               hot_x,
			  gint               hot_y)
{
  g_return_if_fail (context != NULL);
  g_return_if_fail (widget != NULL);

  gtk_drag_set_icon_window (context, widget, hot_x, hot_y, FALSE);
}

/*************************************************************
 * gtk_drag_set_icon_pixmap:
 *     Set a widget as the icon for a drag.
 *   arguments:
 *     context:
 *     colormap: Colormap for the icon window.
 *     pixmap:   
 *     mask:
 *     hot_x:    Hot spot
 *     hot_y:
 *   results:
 *************************************************************/

void 
gtk_drag_set_icon_pixmap (GdkDragContext    *context,
			  GdkColormap       *colormap,
			  GdkPixmap         *pixmap,
			  GdkBitmap         *mask,
			  gint               hot_x,
			  gint               hot_y)
{
  GtkWidget *window;
  gint width, height;
      
  g_return_if_fail (context != NULL);
  g_return_if_fail (colormap != NULL);
  g_return_if_fail (pixmap != NULL);

  gdk_window_get_size (pixmap, &width, &height);

  gtk_widget_push_visual (gdk_colormap_get_visual (colormap));
  gtk_widget_push_colormap (colormap);

  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_events (window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);

  gtk_widget_pop_visual ();
  gtk_widget_pop_colormap ();

  gtk_widget_set_usize (window, width, height);
  gtk_widget_realize (window);

  gdk_window_set_back_pixmap (window->window, pixmap, FALSE);
  
  if (mask)
    gtk_widget_shape_combine_mask (window, mask, 0, 0);

  gtk_drag_set_icon_window (context, window, hot_x, hot_y, TRUE);
}

/*************************************************************
 * gtk_drag_set_icon_default:
 *     Set the icon for a drag to the default icon.
 *   arguments:
 *     context:
 *   results:
 *************************************************************/

void 
gtk_drag_set_icon_default (GdkDragContext    *context)
{
  g_return_if_fail (context != NULL);

  if (!default_icon_pixmap)
    {
      default_icon_colormap = gdk_colormap_get_system ();
      default_icon_pixmap = 
	gdk_pixmap_colormap_create_from_xpm_d (NULL,
					       default_icon_colormap,
					       &default_icon_mask,
					       NULL, (gchar **)drag_default_xpm);
      default_icon_hot_x = -2;
      default_icon_hot_y = -2;
    }

  gtk_drag_set_icon_pixmap (context, 
			    default_icon_colormap, 
			    default_icon_pixmap, 
			    default_icon_mask,
			    default_icon_hot_x,
			    default_icon_hot_y);
}

/*************************************************************
 * gtk_drag_set_default_icon:
 *     Set a default icon for all drags as a pixmap.
 *   arguments:
 *     colormap: Colormap for the icon window.
 *     pixmap:   
 *     mask:
 *     hot_x:    Hot spot
 *     hot_y:
 *   results:
 *************************************************************/

void 
gtk_drag_set_default_icon (GdkColormap   *colormap,
			   GdkPixmap     *pixmap,
			   GdkBitmap     *mask,
			   gint           hot_x,
			   gint           hot_y)
{
  g_return_if_fail (colormap != NULL);
  g_return_if_fail (pixmap != NULL);
  
  if (default_icon_colormap)
    gdk_colormap_unref (default_icon_colormap);
  if (default_icon_pixmap)
    gdk_pixmap_unref (default_icon_pixmap);
  if (default_icon_mask)
    gdk_pixmap_unref (default_icon_mask);

  default_icon_colormap = colormap;
  gdk_colormap_ref (colormap);
  
  default_icon_pixmap = pixmap;
  gdk_pixmap_ref (pixmap);

  default_icon_mask = mask;
  if (mask)
    gdk_pixmap_ref (mask);
  
  default_icon_hot_x = hot_x;
  default_icon_hot_y = hot_y;
}


/*************************************************************
 * gtk_drag_source_handle_event:
 *     Called from widget event handling code on Drag events
 *     for drag sources.
 *
 *   arguments:
 *     toplevel: Toplevel widget that received the event
 *     event:
 *   results:
 *************************************************************/

void
gtk_drag_source_handle_event (GtkWidget *widget,
			      GdkEvent  *event)
{
  GtkDragSourceInfo *info;
  GdkDragContext *context;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (event != NULL);

  context = event->dnd.context;
  info = g_dataset_get_data (context, "gtk-info");
  if (!info)
    return;

  switch (event->type)
    {
    case GDK_DRAG_STATUS:
      {
	GdkCursor *cursor;

	if (info->proxy_dest)
	  {
	    if (!event->dnd.send_event)
	      {
		if (info->proxy_dest->proxy_drop_wait)
		  {
		    gboolean result = context->action != 0;
		    
		    /* Aha - we can finally pass the MOTIF DROP on... */
		    gdk_drop_reply (info->proxy_dest->context, result, info->proxy_dest->proxy_drop_time);
		    if (result)
		      gdk_drag_drop (info->context, info->proxy_dest->proxy_drop_time);
		    else
		      gtk_drag_finish (info->proxy_dest->context, FALSE, FALSE, info->proxy_dest->proxy_drop_time);
		  }
		else
		  {
		    gdk_drag_status (info->proxy_dest->context,
				     event->dnd.context->action,
				     event->dnd.time);
		  }
	      }
	  }
	else
	  {
	    cursor = gtk_drag_get_cursor (event->dnd.context->action);
	    if (info->cursor != cursor)
	      {
		XChangeActivePointerGrab (GDK_WINDOW_XDISPLAY (widget->window), 
					  PointerMotionMask | PointerMotionHintMask | ButtonReleaseMask,
					  ((GdkCursorPrivate *)cursor)->xcursor,
					  event->dnd.time);
		info->cursor = cursor;
	      }
	    
	    if (info->last_event)
	      {
		gtk_drag_update (info,
				 info->cur_x, info->cur_y,
				 info->last_event);
		info->last_event = NULL;
	      }
	  }
      }
      break;
      
    case GDK_DROP_FINISHED:
      gtk_drag_drop_finished (info, TRUE, event->dnd.time);
      break;
    default:
      g_assert_not_reached ();
    }
}

/*************************************************************
 * gtk_drag_source_check_selection:
 *     Check if we've set up handlers/claimed the selection
 *     for a given drag. If not, add them.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static void
gtk_drag_source_check_selection (GtkDragSourceInfo *info, 
				 GdkAtom            selection,
				 guint32            time)
{
  GList *tmp_list;

  tmp_list = info->selections;
  while (tmp_list)
    {
      if (GPOINTER_TO_UINT (tmp_list->data) == selection)
	return;
      tmp_list = tmp_list->next;
    }

  gtk_selection_owner_set (info->ipc_widget, selection, time);
  info->selections = g_list_prepend (info->selections,
				     GUINT_TO_POINTER (selection));

  tmp_list = info->target_list->list;
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;

      gtk_selection_add_target (info->ipc_widget,
				selection,
				pair->target,
				pair->info);
      tmp_list = tmp_list->next;
    }
  
  if (info->context->protocol == GDK_DRAG_PROTO_MOTIF)
    {
      gtk_selection_add_target (info->ipc_widget,
				selection,
				gdk_atom_intern ("XmTRANSFER_SUCCESS", FALSE),
				TARGET_MOTIF_SUCCESS);
      gtk_selection_add_target (info->ipc_widget,
				selection,
				gdk_atom_intern ("XmTRANSFER_FAILURE", FALSE),
				TARGET_MOTIF_FAILURE);
    }

  gtk_selection_add_target (info->ipc_widget,
			    selection,
			    gdk_atom_intern ("DELETE", FALSE),
			    TARGET_DELETE);
}

/*************************************************************
 * gtk_drag_drop_finished:
 *     Clean up from the drag, and display snapback, if necessary.
 *   arguments:
 *     info:
 *     success:
 *     time:
 *   results:
 *************************************************************/

static void
gtk_drag_drop_finished (GtkDragSourceInfo *info,
			gboolean           success,
			guint              time)
{
  gtk_drag_source_release_selections (info, time); 

  if (info->proxy_dest)
    {
      /* The time from the event isn't reliable for Xdnd drags */
      gtk_drag_finish (info->proxy_dest->context, success, FALSE, 
		       info->proxy_dest->proxy_drop_time);
      gtk_drag_source_info_destroy (info);
    }
  else
    {
      if (success)
	{
	  gtk_drag_source_info_destroy (info);
	}
      else
	{
	  GtkDragAnim *anim = g_new (GtkDragAnim, 1);
	  anim->info = info;
	  anim->step = 0;
	  
	  anim->n_steps = MAX (info->cur_x - info->start_x,
			       info->cur_y - info->start_y) / ANIM_STEP_LENGTH;
	  anim->n_steps = CLAMP (anim->n_steps, ANIM_MIN_STEPS, ANIM_MAX_STEPS);
	  if (info->icon_window)
	    {
	      gtk_widget_show (info->icon_window);
	      gdk_window_raise (info->icon_window->window);
	    }
	  
	  /* Mark the context as dead, so if the destination decides
	   * to respond really late, we still are OK.
	   */
	  g_dataset_set_data (info->context, "gtk-info", NULL);
	  gtk_timeout_add (ANIM_STEP_TIME, gtk_drag_anim_timeout, anim);
	}
    }
}

static void
gtk_drag_source_release_selections (GtkDragSourceInfo *info,
				    guint32            time)
{
  GList *tmp_list = info->selections;
  while (tmp_list)
    {
      GdkAtom selection = GPOINTER_TO_UINT (tmp_list->data);
      if (gdk_selection_owner_get (selection) == info->ipc_widget->window)
	gtk_selection_owner_set (NULL, selection, time);
      tmp_list = tmp_list->next;
    }

  g_list_free (info->selections);
  info->selections = NULL;
}

/*************************************************************
 * gtk_drag_drop:
 *     Send a drop event.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static void
gtk_drag_drop (GtkDragSourceInfo *info, 
	       guint32            time)
{
  if (info->context->protocol == GDK_DRAG_PROTO_ROOTWIN)
    {
      GtkSelectionData selection_data;
      GList *tmp_list;
      GdkAtom target = gdk_atom_intern ("application/x-rootwin-drop", FALSE);
      
      tmp_list = info->target_list->list;
      while (tmp_list)
	{
	  GtkTargetPair *pair = tmp_list->data;
	  
	  if (pair->target == target)
	    {
	      selection_data.selection = GDK_NONE;
	      selection_data.target = target;
	      selection_data.data = NULL;
	      selection_data.length = -1;
	      
	      gtk_signal_emit_by_name (GTK_OBJECT (info->widget), "drag_data_get",
				       info->context, &selection_data,
				       pair->info, 
				       time);
	      
	      /* FIXME: Should we check for length >= 0 here? */
	      gtk_drag_drop_finished (info, TRUE, time);
	      return;
	    }
	  tmp_list = tmp_list->next;
	}
      gtk_drag_drop_finished (info, FALSE, time);
    }
  else
    {
      if (info->icon_window)
	gtk_widget_hide (info->icon_window);
	
      gdk_drag_drop (info->context, time);
      info->drop_timeout = gtk_timeout_add (DROP_ABORT_TIME,
					    gtk_drag_abort_timeout,
					    info);
    }
}

/*
 * Source side callbacks.
 */

static gint
gtk_drag_source_event_cb (GtkWidget      *widget,
			  GdkEvent       *event,
			  gpointer        data)
{
  GtkDragSourceSite *site;
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
	{
	  site->state &= ~(GDK_BUTTON1_MASK << (event->button.button - 1));
	}
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
	  
	  if (MAX (abs (site->x - event->motion.x),
		   abs (site->y - event->motion.y)) > 3)
	    {
	      GtkDragSourceInfo *info;
	      GdkDragContext *context;
	      
	      site->state = 0;
	      context = gtk_drag_begin (widget, site->target_list,
					site->actions, 
					i, event);

	      
	      info = g_dataset_get_data (context, "gtk-info");

	      if (!info->icon_window)
		{
		  if (site->pixmap)
		    gtk_drag_set_icon_pixmap (context,
					      site->colormap,
					      site->pixmap,
					      site->mask, -2, -2);
		  else
		    gtk_drag_set_icon_default (context);
		}

	      return TRUE;
	    }
	}
      break;
      
    default:			/* hit for 2/3BUTTON_PRESS */
      break;
    }
  return FALSE;
}

static void 
gtk_drag_source_site_destroy (gpointer data)
{
  GtkDragSourceSite *site = data;

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  if (site->pixmap)
    gdk_pixmap_unref (site->pixmap);
  
  if (site->mask)
    gdk_pixmap_unref (site->mask);
  
  g_free (site);
}

static void
gtk_drag_selection_get (GtkWidget        *widget, 
			GtkSelectionData *selection_data,
			guint             sel_info,
			guint32           time,
			gpointer          data)
{
  GtkDragSourceInfo *info = data;
  static GdkAtom null_atom = GDK_NONE;
  guint target_info;

  if (!null_atom)
    null_atom = gdk_atom_intern ("NULL", FALSE);

  switch (sel_info)
    {
    case TARGET_DELETE:
      gtk_signal_emit_by_name (GTK_OBJECT (info->widget), 
			       "drag_data_delete", 
			       info->context);
      gtk_selection_data_set (selection_data, null_atom, 8, NULL, 0);
      break;
    case TARGET_MOTIF_SUCCESS:
      gtk_drag_drop_finished (info, TRUE, time);
      gtk_selection_data_set (selection_data, null_atom, 8, NULL, 0);
      break;
    case TARGET_MOTIF_FAILURE:
      gtk_drag_drop_finished (info, FALSE, time);
      gtk_selection_data_set (selection_data, null_atom, 8, NULL, 0);
      break;
    default:
      if (info->proxy_dest)
	{
	  /* This is sort of dangerous and needs to be thought
	   * through better
	   */
	  info->proxy_dest->proxy_data = selection_data;
	  gtk_drag_get_data (info->widget,
			     info->proxy_dest->context,
			     selection_data->target,
			     time);
	  gtk_main ();
	  info->proxy_dest->proxy_data = NULL;
	}
      else
	{
	  if (gtk_target_list_find (info->target_list, 
				    selection_data->target, 
				    &target_info))
	    {
	      gtk_signal_emit_by_name (GTK_OBJECT (info->widget), "drag_data_get",
				       info->context, 
				       selection_data, 
				       target_info, 
				       time);
	    }
	}
      break;
    }
}

static gint
gtk_drag_anim_timeout (gpointer data)
{
  GtkDragAnim *anim = data;
  gint x, y;
  gboolean retval;

  GDK_THREADS_ENTER ();

  if (anim->step == anim->n_steps)
    {
      gtk_drag_source_info_destroy (anim->info);
      g_free (anim);

      retval = FALSE;
    }
  else
    {
      x = (anim->info->start_x * (anim->step + 1) +
	   anim->info->cur_x * (anim->n_steps - anim->step - 1)) / anim->n_steps;
      y = (anim->info->start_y * (anim->step + 1) +
	   anim->info->cur_y * (anim->n_steps - anim->step - 1)) / anim->n_steps;
      if (anim->info->icon_window)
	gtk_widget_set_uposition (anim->info->icon_window, 
				  x - anim->info->hot_x, 
				  y - anim->info->hot_y);
  
      anim->step++;

      retval = TRUE;
    }

  GDK_THREADS_LEAVE ();

  return retval;
}

static void
gtk_drag_remove_icon (GtkDragSourceInfo *info)
{
  if (info->icon_window)
    {
      gtk_widget_hide (info->icon_window);
      if (info->destroy_icon)
	gtk_widget_destroy (info->icon_window);

      gtk_widget_unref (info->icon_window);
      info->icon_window = NULL;
    }
}

static void
gtk_drag_source_info_destroy (gpointer data)
{
  GtkDragSourceInfo *info = data;

  gtk_drag_remove_icon (data);

  if (!info->proxy_dest)
    gtk_signal_emit_by_name (GTK_OBJECT (info->widget), "drag_end", 
			     info->context);

  if (info->widget)
    gtk_widget_unref (info->widget);

  gtk_signal_disconnect_by_data (GTK_OBJECT (info->ipc_widget), info);
  gtk_selection_remove_all (info->ipc_widget);
  gtk_object_set_data (GTK_OBJECT (info->ipc_widget), "gtk-info", NULL);
  source_widgets = g_slist_remove (source_widgets, info->ipc_widget);
  gtk_drag_release_ipc_widget (info->ipc_widget);

  gtk_target_list_unref (info->target_list);

  g_dataset_set_data (info->context, "gtk-info", NULL);
  gdk_drag_context_unref (info->context);

  if (info->drop_timeout)
    gtk_timeout_remove (info->drop_timeout);

  g_free (info);
}

/*************************************************************
 * gtk_drag_update:
 *     Function to update the status of the drag when the
 *     cursor moves or the modifier changes
 *   arguments:
 *     info: DragSourceInfo for the drag
 *     x_root, y_root: position of darg
 *     event: The event that triggered this call
 *   results:
 *************************************************************/

static void
gtk_drag_update (GtkDragSourceInfo *info,
		 gint               x_root,
		 gint               y_root,
		 GdkEvent          *event)
{
  GdkDragAction action;
  GdkDragAction possible_actions;
  GdkWindow *window = NULL;
  GdkWindow *dest_window;
  GdkDragProtocol protocol;
  GdkAtom selection;
  guint32 time = gtk_drag_get_event_time (event);

  gtk_drag_get_event_actions (event,
			      info->button, 
			      info->possible_actions,
			      &action, &possible_actions);
  info->cur_x = x_root;
  info->cur_y = y_root;

  if (info->icon_window)
    {
      gdk_window_raise (info->icon_window->window);
      gtk_widget_set_uposition (info->icon_window, 
				info->cur_x - info->hot_x, 
				info->cur_y - info->hot_y);
      window = info->icon_window->window;
    }
  
  gdk_drag_find_window (info->context,
			window, x_root, y_root,
			&dest_window, &protocol);

  if (gdk_drag_motion (info->context, dest_window, protocol,
		       x_root, y_root, action, 
		       possible_actions,
		       time))
    {
      if (info->last_event)
	gdk_event_free ((GdkEvent *)info->last_event);
      
      info->last_event = gdk_event_copy ((GdkEvent *)event);
    }

  if (dest_window)
    gdk_window_unref (dest_window);

  selection = gdk_drag_get_selection (info->context);
  if (selection)
    gtk_drag_source_check_selection (info, selection, time);
}

/*************************************************************
 * gtk_drag_end:
 *     Called when the user finishes to drag, either by
 *     releasing the mouse, or by pressing Esc.
 *   arguments:
 *     widget: GtkInvisible widget for this drag
 *     info: 
 *   results:
 *************************************************************/

static void
gtk_drag_end (GtkDragSourceInfo *info, guint32 time)
{
  GdkEvent send_event;
  GtkWidget *source_widget = info->widget;

  gdk_pointer_ungrab (time);
  gdk_keyboard_ungrab (time);

  gtk_grab_remove (info->ipc_widget);

  gtk_signal_disconnect_by_func (GTK_OBJECT (info->ipc_widget), 
				 GTK_SIGNAL_FUNC (gtk_drag_button_release_cb),
				 info);
  gtk_signal_disconnect_by_func (GTK_OBJECT (info->ipc_widget), 
				 GTK_SIGNAL_FUNC (gtk_drag_motion_cb),
				 info);
  gtk_signal_disconnect_by_func (GTK_OBJECT (info->ipc_widget), 
				 GTK_SIGNAL_FUNC (gtk_drag_key_cb),
				 info);

  /* Send on a release pair to the the original 
   * widget to convince it to release its grab. We need to
   * call gtk_propagate_event() here, instead of 
   * gtk_widget_event() because widget like GtkList may
   * expect propagation.
   */

  send_event.button.type = GDK_BUTTON_RELEASE;
  send_event.button.window = GDK_ROOT_PARENT ();
  send_event.button.send_event = TRUE;
  send_event.button.time = time;
  send_event.button.x = 0;
  send_event.button.y = 0;
  send_event.button.pressure = 0.;
  send_event.button.xtilt = 0.;
  send_event.button.ytilt = 0.;
  send_event.button.state = 0;
  send_event.button.button = info->button;
  send_event.button.source = GDK_SOURCE_PEN;
  send_event.button.deviceid = GDK_CORE_POINTER;
  send_event.button.x_root = 0;
  send_event.button.y_root = 0;

  gtk_propagate_event (source_widget, &send_event);
}

/*************************************************************
 * gtk_drag_motion_cb:
 *     "motion_notify_event" callback during drag.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static gint
gtk_drag_motion_cb (GtkWidget      *widget, 
		    GdkEventMotion *event, 
		    gpointer        data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;
  gint x_root, y_root;

  if (event->is_hint)
    {
      gdk_window_get_pointer (GDK_ROOT_PARENT(), &x_root, &y_root, NULL);
      event->x_root = x_root;
      event->y_root = y_root;
    }

  gtk_drag_update (info, event->x_root, event->y_root, (GdkEvent *)event);

  return TRUE;
}

/*************************************************************
 * gtk_drag_key_cb:
 *     "key_press/release_event" callback during drag.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static gint 
gtk_drag_key_cb (GtkWidget         *widget, 
		 GdkEventKey       *event, 
		 gpointer           data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;
  GdkModifierType state;
  
  if (event->type == GDK_KEY_PRESS)
    {
      if (event->keyval == GDK_Escape)
	{
	  gtk_drag_end (info, event->time);
	  gdk_drag_abort (info->context, event->time);
	  gtk_drag_drop_finished (info, FALSE, event->time);

	  return TRUE;
	}
    }

  /* Now send a "motion" so that the modifier state is updated */

  /* The state is not yet updated in the event, so we need
   * to query it here. We could use XGetModifierMapping, but
   * that would be overkill.
   */
  gdk_window_get_pointer (GDK_ROOT_PARENT(), NULL, NULL, &state);

  event->state = state;
  gtk_drag_update (info, info->cur_x, info->cur_y, (GdkEvent *)event);

  return TRUE;
}

/*************************************************************
 * gtk_drag_button_release_cb:
 *     "button_release_event" callback during drag.
 *   arguments:
 *     
 *   results:
 *************************************************************/

static gint
gtk_drag_button_release_cb (GtkWidget      *widget, 
			    GdkEventButton *event, 
			    gpointer        data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;

  if (event->button != info->button)
    return FALSE;

  gtk_drag_end (info, event->time);

  if ((info->context->action != 0) && (info->context->dest_window != NULL))
    {
      gtk_drag_drop (info, event->time);
    }
  else
    {
      gdk_drag_abort (info->context, event->time);
      gtk_drag_drop_finished (info, FALSE, event->time);
    }

  return TRUE;
}

static gint
gtk_drag_abort_timeout (gpointer data)
{
  GtkDragSourceInfo *info = data;
  guint32 time = GDK_CURRENT_TIME;

  GDK_THREADS_ENTER ();

  if (info->proxy_dest)
    time = info->proxy_dest->proxy_drop_time;

  info->drop_timeout = 0;
  gtk_drag_drop_finished (info, FALSE, time);
  
  GDK_THREADS_LEAVE ();
  
  return FALSE;
}
