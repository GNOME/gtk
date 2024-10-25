#include "config.h"

#include "gdkwin32messagesourceprivate.h"

#include "gdkevents.h"

#include <windows.h>

typedef struct _GdkWin32MessageSource GdkWin32MessageSource;

struct _GdkWin32MessageSource
{
  GSource source;

  GPollFD poll_fd;
} ;

static gboolean
gdk_win32_message_source_prepare (GSource *source,
		                          int     *timeout)
{
  GdkWin32MessageSource *self = (GdkWin32MessageSource *) source;
  gboolean retval;

  *timeout = -1;

  return GetQueueStatus (QS_ALLINPUT) != 0;
}

static gboolean
gdk_win32_message_source_check (GSource *source)
{
  GdkWin32MessageSource *self = (GdkWin32MessageSource *) source;

  self->poll_fd.revents = 0;

  return GetQueueStatus (QS_ALLINPUT) != 0;
}

static gboolean
gdk_win32_message_source_dispatch (GSource     *source,
                                   GSourceFunc  callback,
                                   gpointer     user_data)
{
  MSG msg;

  while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
  
  return G_SOURCE_CONTINUE;
}

static void
gdk_win32_message_source_finalize (GSource *source)
{
#ifdef G_WITH_CYGWIN
  close (self->poll_fd.fd);
#endif
}

static GSourceFuncs gdk_win32_message_source_funcs = {
  gdk_win32_message_source_prepare,
  gdk_win32_message_source_check,
  gdk_win32_message_source_dispatch,
  gdk_win32_message_source_finalize,
};

/*
 * gdk_win32_message_source_new:
 *
 * Creates a new source for processing events of the Windows main loop.
 * 
 * Returns: The new source
 */
GSource *
gdk_win32_message_source_new (void)
{
  GdkWin32MessageSource *self;
  GSource *source;

  source = g_source_new (&gdk_win32_message_source_funcs, sizeof (GdkWin32MessageSource));
  g_source_set_static_name (source, "GDK Win32 message source");
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  
  self = (GdkWin32MessageSource *) source;
#ifdef G_WITH_CYGWIN
  self->poll_fd.fd = open ("/dev/windows", O_RDONLY);
  if (self->poll_fd.fd == -1)
    g_error ("can't open \"/dev/windows\": %s", g_strerror (errno));
#else
  self->poll_fd.fd = G_WIN32_MSG_HANDLE;
#endif
  self->poll_fd.events = G_IO_IN;

  g_source_add_poll (source, &self->poll_fd);
  g_source_set_can_recurse (source, TRUE);

  return source;
}

/*
 * gdk_win32_message_source_add:
 * @context: (nullable): The context to add the source to
 *
 * Adds a message source to the default main context.
 * 
 * Returns: the ID (greater than 0) of the event source.
 */
guint
gdk_win32_message_source_add (GMainContext *context)
{
  GSource *source;
  guint id;

  source = gdk_win32_message_source_new ();
  id = g_source_attach (source, context);
  g_source_unref (source);

  return id;
}