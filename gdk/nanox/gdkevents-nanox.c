#include "gdk.h"
#include "gdkinternals.h"
#include "gdkprivate-nanox.h"

typedef struct _GdkEventPrivate GdkEventPrivate;

#define DOUBLE_CLICK_TIME      250
#define TRIPLE_CLICK_TIME      500
#define DOUBLE_CLICK_DIST      5
#define TRIPLE_CLICK_DIST      5

#define GR_BUTTON_TO_GDK(b) b&LBUTTON? 1: (b&MBUTTON? 2: (b&RBUTTON? 3: 0))

static guint gr_mod_to_gdk(guint mods, guint buttons) {
	guint res=0;
	if (mods & GR_MODIFIER_SHIFT)
			res |= GDK_SHIFT_MASK;
	if (mods & GR_MODIFIER_CTRL)
			res |= GDK_CONTROL_MASK;
	if (mods & GR_MODIFIER_META)
			res |= GDK_MOD1_MASK;
	if (buttons & LBUTTON)
			res |= GDK_BUTTON1_MASK;
	if (buttons & MBUTTON)
			res |= GDK_BUTTON2_MASK;
	if (buttons & RBUTTON)
			res |= GDK_BUTTON3_MASK;
	return res;
}

static gboolean  gdk_event_prepare      (gpointer   source_data, 
				 	 GTimeVal  *current_time,
					 gint      *timeout,
					 gpointer   user_data);
static gboolean  gdk_event_check        (gpointer   source_data,
				 	 GTimeVal  *current_time,
					 gpointer   user_data);
static gboolean  gdk_event_dispatch     (gpointer   source_data,
					 GTimeVal  *current_time,
					 gpointer   user_data);

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0
} GdkEventFlags;

struct _GdkEventPrivate
{
  GdkEvent event;
  guint    flags;
};


static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  (GDestroyNotify)g_free
};

GPollFD event_poll_fd;
extern int sock;
static guint serial_value = 1;

static int
events_idle () {
    gdk_events_queue();
    return TRUE;
}

void 
gdk_events_init (void)
{
  g_source_add (GDK_PRIORITY_EVENTS, TRUE, &event_funcs, NULL, NULL, NULL);

  event_poll_fd.fd = sock;
  event_poll_fd.events = G_IO_IN;
  
  g_main_add_poll (&event_poll_fd, GDK_PRIORITY_EVENTS);

  g_idle_add(events_idle, NULL);

}

static gboolean  
gdk_event_prepare (gpointer  source_data, 
		   GTimeVal *current_time,
		   gint     *timeout,
		   gpointer  user_data)
{
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  *timeout = -1;

  retval = (gdk_event_queue_find_first () != NULL);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_check (gpointer  source_data,
		 GTimeVal *current_time,
		 gpointer  user_data)
{
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  if (event_poll_fd.revents & G_IO_IN)
    //retval = (gdk_event_queue_find_first () != NULL);
    retval = 1;
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_dispatch (gpointer  source_data,
		    GTimeVal *current_time,
		    gpointer  user_data)
{
  GdkEvent *event;
 
  GDK_THREADS_ENTER ();

  gdk_events_queue();
  event = gdk_event_unqueue();

  if (event)
    {
      if (gdk_event_func)
	(*gdk_event_func) (event, gdk_event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}


gboolean
gdk_events_pending (void)
{
  return gdk_event_queue_find_first();
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  return NULL;
}

static gint gdk_event_translate (GdkEvent *event, GR_EVENT *xevent) {
  GdkWindow *window=NULL;
  GdkWindowPrivate *window_private=NULL;
  gint return_val = FALSE;
  static int lastx=0, lasty=0, lastrootx=0, lastrooty=0;

  if (xevent->type == GR_EVENT_TYPE_FDINPUT)
	return 0;
  window = gdk_window_lookup (xevent->general.wid);
  /* FIXME: window might be a GdkPixmap!!! */
  
  window_private = (GdkWindowPrivate *) window;
  
  if (window != NULL)
    gdk_window_ref (window);
  
  event->any.window = window;
  event->any.send_event = FALSE;
 
  if (window_private && GDK_DRAWABLE_DESTROYED (window))
    {
	return FALSE;
    }
  else
    {
      /* Check for filters for this window
       */
      GdkFilterReturn result = GDK_FILTER_CONTINUE;
      /*result = gdk_event_apply_filters (xevent, event,
					window_private
					?window_private->filters
					:gdk_default_filters);
      */
      if (result != GDK_FILTER_CONTINUE)
	{
	  return (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	}
    }

  return_val = TRUE;

      //g_message("got event (%p) %d", window, xevent->type);
  switch (xevent->type)
    {
    case GR_EVENT_TYPE_KEY_DOWN:
      event->key.keyval = xevent->keystroke.ch;
      event->key.type = GDK_KEY_PRESS;
      event->key.window = window;
      event->key.time = serial_value++;
      event->key.state = gr_mod_to_gdk(xevent->keystroke.modifiers, xevent->keystroke.buttons);
      event->key.string = g_strdup_printf ("%c", xevent->keystroke.ch);
      event->key.length = 1;
      
      break;
    case GR_EVENT_TYPE_KEY_UP:
      event->key.keyval = xevent->keystroke.ch;
      event->key.type = GDK_KEY_RELEASE;
      event->key.window = window;
      event->key.time = serial_value++;
      event->key.state = gr_mod_to_gdk(xevent->keystroke.modifiers, xevent->keystroke.buttons)|GDK_RELEASE_MASK;
      event->key.string = NULL;
      event->key.length = 0;
      
      break;
    case GR_EVENT_TYPE_BUTTON_DOWN:
	  event->button.type = GDK_BUTTON_PRESS;
	  event->button.window = window;
	  event->button.time = serial_value++;
	  event->button.x = xevent->button.x;
	  event->button.y = xevent->button.y;
	  event->button.x_root = (gfloat)xevent->button.rootx;
	  event->button.y_root = (gfloat)xevent->button.rooty;
	  event->button.pressure = 0.5;
	  event->button.xtilt = 0;
	  event->button.ytilt = 0;
      event->button.state = gr_mod_to_gdk(xevent->button.modifiers, xevent->button.buttons);
	  event->button.button = GR_BUTTON_TO_GDK(xevent->button.changebuttons);
	  event->button.source = GDK_SOURCE_MOUSE;
	  event->button.deviceid = GDK_CORE_POINTER;
	  g_message("button down: %d", event->button.button);
	  gdk_event_button_generate (event);
      break;
    case GR_EVENT_TYPE_BUTTON_UP:
	  event->button.type = GDK_BUTTON_RELEASE;
	  event->button.window = window;
	  event->button.time = serial_value++;
	  event->button.x = xevent->button.x;
	  event->button.y = xevent->button.y;
	  event->button.x_root = (gfloat)xevent->button.rootx;
	  event->button.y_root = (gfloat)xevent->button.rooty;
	  event->button.pressure = 0.5;
	  event->button.xtilt = 0;
	  event->button.ytilt = 0;
      event->button.state = gr_mod_to_gdk(xevent->button.modifiers, xevent->button.buttons)|GDK_RELEASE_MASK;
	  event->button.button = GR_BUTTON_TO_GDK(xevent->button.changebuttons);
	  event->button.source = GDK_SOURCE_MOUSE;
	  event->button.deviceid = GDK_CORE_POINTER;
	  g_message("button up: %d", event->button.button);
	  gdk_event_button_generate (event);
      break;
    case GR_EVENT_TYPE_MOUSE_MOTION:
      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = serial_value++;
	  event->motion.x = xevent->mouse.x;
	  event->motion.y = xevent->mouse.y;
	  event->motion.x_root = (gfloat)xevent->mouse.rootx;
	  event->motion.y_root = (gfloat)xevent->mouse.rooty;
      event->motion.pressure = 0.5;
      event->motion.xtilt = 0;
      event->motion.ytilt = 0;
      event->motion.state = gr_mod_to_gdk(xevent->mouse.modifiers, xevent->mouse.buttons);
      event->motion.is_hint = 0;
      event->motion.source = GDK_SOURCE_MOUSE;
      event->motion.deviceid = GDK_CORE_POINTER;
      
      break;
    case GR_EVENT_TYPE_MOUSE_POSITION:
      return_val = FALSE;
      break;
    case GR_EVENT_TYPE_MOUSE_ENTER:
      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      event->crossing.subwindow = NULL;
      event->crossing.time = serial_value++;
      event->crossing.detail = GDK_NOTIFY_UNKNOWN;
      //g_message("subwindow 1: %p", event->crossing.subwindow);
      /* other stuff here , x, y, x_root, y_root */
      break;
    case GR_EVENT_TYPE_MOUSE_EXIT:
      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      event->crossing.subwindow = NULL;
      event->crossing.time = serial_value++;
      event->crossing.mode = GDK_CROSSING_NORMAL;
      event->crossing.detail = GDK_NOTIFY_UNKNOWN;
      //g_message("subwindow 2: %p", event->crossing.subwindow);
      /* other stuff here , x, y, x_root, y_root */
      break;
    case GR_EVENT_TYPE_FOCUS_IN:
    case GR_EVENT_TYPE_FOCUS_OUT:
	  event->focus_change.type = GDK_FOCUS_CHANGE;
	  event->focus_change.window = window;
	  event->focus_change.in = (xevent->general.type == GR_EVENT_TYPE_FOCUS_IN);

      break;
    case GR_EVENT_TYPE_UPDATE:
    case GR_EVENT_TYPE_CHLD_UPDATE:
      if (xevent->update.utype == GR_UPDATE_MAP) {
        event->any.type = GDK_MAP;
        event->any.window = window;
      
      } else if (xevent->update.utype == GR_UPDATE_UNMAP) {
        event->any.type = GDK_UNMAP;
        event->any.window = window;
      
        if (gdk_xgrab_window == window_private)
	  gdk_xgrab_window = NULL;
      } else {
      if (!window || GDK_DRAWABLE_TYPE (window) == GDK_WINDOW_CHILD)
	return_val = FALSE;
      else
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.width = xevent->update.width;
	  event->configure.height = xevent->update.height;
	  event->configure.x = xevent->update.x;
	  event->configure.y = xevent->update.y;
	  window_private->x = event->configure.x;
	  window_private->y = event->configure.y;
	  window_private->drawable.width = event->configure.width;
	  window_private->drawable.height = event->configure.height;
	  if (window_private->resize_count > 1)
	    window_private->resize_count -= 1;
	}
      }
      break;
    case GR_EVENT_TYPE_EXPOSURE:

      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->exposure.x;
      event->expose.area.y = xevent->exposure.y;
      event->expose.area.width = xevent->exposure.width;
      event->expose.area.height = xevent->exposure.height;
      event->expose.count = 0;
      
      break;
    case GR_EVENT_TYPE_FDINPUT:
    case GR_EVENT_TYPE_NONE:
      return_val = FALSE;
      break;
    default:
      return_val = FALSE;
      g_message("event %d not handled\n", xevent->type);
  }
  if (return_val)
    {
      if (event->any.window)
	gdk_window_ref (event->any.window);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.subwindow != NULL))
	gdk_window_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }
  
  if (window)
    gdk_window_unref (window);
  
  return return_val;
}

void
gdk_events_queue (void)
{
  GList *node;
  GdkEvent *event;
  GR_EVENT xevent;

  while (!gdk_event_queue_find_first())
    {
      GrCheckNextEvent (&xevent);
      if (!xevent.type)
	      return;
      
      event = gdk_event_new ();
      
      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      gdk_event_queue_append (event);
      node = gdk_queued_tail;

      if (gdk_event_translate (event, &xevent))
	{
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
	  //g_message("got event: %d", event->type);
	}
      else
	{
	  gdk_event_queue_remove_link (node);
	  g_list_free_1 (node);
	  gdk_event_free (event);
	}
    }
}

gboolean
gdk_event_send_client_message (GdkEvent *event, guint32 xid)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}

void
gdk_event_send_clientmessage_toall (GdkEvent *event)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_flush (void)
{
  GrFlush();
}


