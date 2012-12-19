#include <glib.h>

#include "gdkbroadway-server.h"

int
main (int argc, char *argv[])
{
  GdkBroadwayServer *server;
  GError *error;
  GMainLoop *loop;

  error = NULL;
  server = _gdk_broadway_server_new (8080, &error);
  if (server == NULL)
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  
  return 0;
}


/* TODO: */

void
_gdk_broadway_events_got_input (BroadwayInputMsg *message)
{
}
