#include "config.h"

#include <glib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "gdkprivate-quartz.h"

static GPollFD event_poll_fd;
static GQueue *current_events;

static GPollFunc old_poll_func;

static gboolean select_fd_waiting = FALSE, ready_for_poll = FALSE;
static pthread_t select_thread = 0;
static int wakeup_pipe[2];
static pthread_mutex_t pollfd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;
static GPollFD *pollfds;
static guint n_pollfds;
static CFRunLoopSourceRef select_main_thread_source;
static CFRunLoopRef main_thread_run_loop;
static NSAutoreleasePool *autorelease_pool;

gboolean
_gdk_quartz_event_loop_check_pending (void)
{
  return current_events && current_events->head;
}

NSEvent*
_gdk_quartz_event_loop_get_pending (void)
{
  NSEvent *event = NULL;

  if (current_events)
    event = g_queue_pop_tail (current_events);

  return event;
}

void
_gdk_quartz_event_loop_release_event (NSEvent *event)
{
  [event release];
}

static gboolean
gdk_event_prepare (GSource *source,
		   gint    *timeout)
{
  NSEvent *event;
  gboolean retval;

  GDK_THREADS_ENTER ();
  
  *timeout = -1;

  event = [NSApp nextEventMatchingMask: NSAnyEventMask
	                     untilDate: [NSDate distantPast]
	                        inMode: NSDefaultRunLoopMode
	                       dequeue: NO];

  retval = (_gdk_event_queue_find_first (_gdk_display) != NULL ||
	    (current_events && current_events->head) ||
            event != NULL);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
gdk_event_check (GSource *source)
{
  gboolean retval;

  GDK_THREADS_ENTER ();

  if (autorelease_pool)
    [autorelease_pool release];
  autorelease_pool = [[NSAutoreleasePool alloc] init];

  if (_gdk_event_queue_find_first (_gdk_display) != NULL ||
      _gdk_quartz_event_loop_check_pending ())
    retval = TRUE;
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
gdk_event_dispatch (GSource     *source,
		    GSourceFunc  callback,
		    gpointer     user_data)
{
  GdkEvent *event;

  GDK_THREADS_ENTER ();

  _gdk_events_queue (_gdk_display);

  event = _gdk_event_unqueue (_gdk_display);

  if (event)
    {
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);

      gdk_event_free (event);
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

static void 
got_fd_activity (void *info)
{
  NSEvent *event;

  /* Post a message so we'll break out of the message loop */
  event = [NSEvent otherEventWithType: NSApplicationDefined
	                     location: NSZeroPoint
	                modifierFlags: 0
	                    timestamp: 0
	                 windowNumber: 0
	                      context: nil
                              subtype: GDK_QUARTZ_EVENT_SUBTYPE_EVENTLOOP
	                        data1: 0 
	                        data2: 0];

  [NSApp postEvent:event atStart:YES];
}

static void *
select_thread_func (void *arg)
{
  int n_active_fds;

  pthread_mutex_lock (&pollfd_mutex);

  while (1)
    {
      char c;
      int n;

      ready_for_poll = TRUE;
      pthread_cond_signal (&ready_cond);
      pthread_cond_wait (&ready_cond, &pollfd_mutex);
      ready_for_poll = FALSE;

      select_fd_waiting = TRUE;
      pthread_cond_signal (&ready_cond);
      pthread_mutex_unlock (&pollfd_mutex);
      n_active_fds = old_poll_func (pollfds, n_pollfds, -1);
      pthread_mutex_lock (&pollfd_mutex);
      select_fd_waiting = FALSE;
      n = read (wakeup_pipe[0], &c, 1);
      if (n == 1)
        {
	  g_assert (c == 'A');

	  n_active_fds --;
	}
      pthread_mutex_unlock (&pollfd_mutex);

      if (n_active_fds)
	{
	  /* We have active fds, signal the main thread */
	  CFRunLoopSourceSignal (select_main_thread_source);
	  if (CFRunLoopIsWaiting (main_thread_run_loop))
	    CFRunLoopWakeUp (main_thread_run_loop);
	}

      pthread_mutex_lock (&pollfd_mutex);
    }
}

static gint
poll_func (GPollFD *ufds, guint nfds, gint timeout_)
{
  NSEvent *event;
  NSDate *limit_date;
  gboolean poll_event_fd = FALSE;
  int n_active = 0;
  int i;

  if (nfds > 1 || ufds[0].fd != -1)
    {
      if (!select_thread) {
        /* Create source used for signalling the main thread */
        main_thread_run_loop = CFRunLoopGetCurrent ();
        CFRunLoopSourceContext source_context = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, got_fd_activity };
        select_main_thread_source = CFRunLoopSourceCreate (NULL, 0, &source_context);
        CFRunLoopAddSource (main_thread_run_loop, select_main_thread_source, kCFRunLoopDefaultMode);

        pipe (wakeup_pipe);
        fcntl(wakeup_pipe[0], F_SETFL, O_NONBLOCK);

        pthread_mutex_lock (&pollfd_mutex);
        pthread_create (&select_thread, NULL, select_thread_func, NULL);
      } else
        pthread_mutex_lock (&pollfd_mutex);

      while (!ready_for_poll)
        pthread_cond_wait (&ready_cond, &pollfd_mutex);

      /* We cheat and use the fake fd (if it's polled) for our pipe */

      for (i = 0; i < nfds; i++)
        if (ufds[i].fd == -1)
          {
            poll_event_fd = TRUE;
            break;
          }

      g_free (pollfds);

      if (i == nfds)
        {
          n_pollfds = nfds + 1;
          pollfds = g_new (GPollFD, nfds + 1);
          memcpy (pollfds, ufds, nfds * sizeof (GPollFD));
        }
      else
        {
          pollfds = g_memdup (ufds, nfds * sizeof (GPollFD));
          n_pollfds = nfds;
        }

      pollfds[i].fd = wakeup_pipe[0];
      pollfds[i].events = G_IO_IN;

      /* Start our thread */
      pthread_cond_signal (&ready_cond);
      pthread_cond_wait (&ready_cond, &pollfd_mutex);
      pthread_mutex_unlock (&pollfd_mutex);
    }

  if (timeout_ == -1)
    limit_date = [NSDate distantFuture];
  else if (timeout_ == 0)
    limit_date = [NSDate distantPast];
  else
    limit_date = [NSDate dateWithTimeIntervalSinceNow:timeout_/1000.0];

  event = [NSApp nextEventMatchingMask: NSAnyEventMask
	                     untilDate: limit_date
	                        inMode: NSDefaultRunLoopMode
                               dequeue: YES];
  
  if (event)
    {
      if ([event type] == NSApplicationDefined &&
          [event subtype] == GDK_QUARTZ_EVENT_SUBTYPE_EVENTLOOP)
        {
          pthread_mutex_lock (&pollfd_mutex);

          for (i = 0; i < nfds; i++)
            {
              if (ufds[i].fd == -1)
                continue;

              g_assert (ufds[i].fd == pollfds[i].fd);
              g_assert (ufds[i].events == pollfds[i].events);

              if (pollfds[i].revents)
                {
                  ufds[i].revents = pollfds[i].revents;
                  n_active ++;
                }
            }

          pthread_mutex_unlock (&pollfd_mutex);

          /* Try to get a Cocoa event too, if requested */
          if (poll_event_fd)
            event = [NSApp nextEventMatchingMask: NSAnyEventMask
                                       untilDate: [NSDate distantPast]
                                          inMode: NSDefaultRunLoopMode
                                         dequeue: YES];
          else
            event = NULL;
        }
    }

  /* There were no active fds, break out of the other thread's poll() */
  pthread_mutex_lock (&pollfd_mutex);
  if (select_fd_waiting && wakeup_pipe[1])
    {
      char c = 'A';

      write (wakeup_pipe[1], &c, 1);
    }
  pthread_mutex_unlock (&pollfd_mutex);

  if (event) 
    {
      for (i = 0; i < nfds; i++)
        {
          if (ufds[i].fd == -1)
            {
              ufds[i].revents = G_IO_IN;
              break;
            }
        }

      if (!current_events)
        current_events = g_queue_new ();
      g_queue_push_head (current_events, [event retain]);

      n_active ++;
    }

  return n_active;
}

void
_gdk_quartz_event_loop_init (void)
{
  GSource *source;

  event_poll_fd.events = G_IO_IN;
  event_poll_fd.fd = -1;

  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_add_poll (source, &event_poll_fd);
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  old_poll_func = g_main_context_get_poll_func (NULL);
  g_main_context_set_poll_func (NULL, poll_func);  

  autorelease_pool = [[NSAutoreleasePool alloc] init];
}

