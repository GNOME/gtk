#include <config.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

extern guint32 create_child_plug (guint32  xid,
				  gboolean local);

int
main (int argc, char *argv[])
{
  guint32 xid;
  guint32 plug_xid;

  gtk_init (&argc, &argv);

  if (argc != 1 && argc != 2)
    {
      fprintf (stderr, "Usage: testsocket_child [WINDOW_ID]\n");
      exit (1);
    }

  if (argc == 2)
    {
      xid = strtol (argv[1], NULL, 0);
      if (xid == 0)
	{
	  fprintf (stderr, "Invalid window id '%s'\n", argv[1]);
	  exit (1);
	}
      
      create_child_plug (xid, FALSE);
    }
  else
    {
      plug_xid = create_child_plug (0, FALSE);
      printf ("%d\n", plug_xid);
      fflush (stdout);
    }

  gtk_main ();

  return 0;
}


