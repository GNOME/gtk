#include "gdk.h"
#include "gdkprivate-nanox.h"


GdkDragContext *
gdk_drag_context_new        (void)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

void            
gdk_drag_context_ref (GdkDragContext *context)
{
}

void            
gdk_drag_context_unref (GdkDragContext *context)
{
}

void
gdk_dnd_init (void)
{
}

GdkDragContext * 
gdk_drag_begin (GdkWindow     *window,
		GList         *targets)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

guint32
gdk_drag_get_protocol (guint32          xid,
		       GdkDragProtocol *protocol)
{
  *protocol = GDK_DRAG_PROTO_NONE;
  return 0;
}

void
gdk_drag_find_window (GdkDragContext  *context,
		      GdkWindow       *drag_window,
		      gint             x_root,
		      gint             y_root,
		      GdkWindow      **dest_window,
		      GdkDragProtocol *protocol)
{
		g_message("unimplemented %s", __FUNCTION__);
}


gboolean        
gdk_drag_motion (GdkDragContext *context,
		 GdkWindow      *dest_window,
		 GdkDragProtocol protocol,
		 gint            x_root, 
		 gint            y_root,
		 GdkDragAction   suggested_action,
		 GdkDragAction   possible_actions,
		 guint32         time)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void             
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void 
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void             
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void            
gdk_window_register_dnd (GdkWindow      *window)
{
		g_message("unimplemented %s", __FUNCTION__);
}

GdkAtom       
gdk_drag_get_selection (GdkDragContext *context)
{
  return GDK_NONE;
}


