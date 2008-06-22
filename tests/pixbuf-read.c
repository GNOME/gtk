/* -*- Mode: C; c-basic-offset: 2; -*- */
/* GdkPixbuf library - test loaders
 *
 * Copyright (C) 2001 Søren Sandmann (sandmann@daimi.au.dk)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gdk-pixbuf/gdk-pixbuf.h"
#include <stdio.h>
#include <stdlib.h>

static gboolean
test_loader (const guchar *bytes, gsize len, GError **err)
{
  GdkPixbufLoader *loader;

  loader = gdk_pixbuf_loader_new ();
  gdk_pixbuf_loader_write (loader, bytes, len, err);
  if (*err)
      return FALSE;
  
  gdk_pixbuf_loader_close (loader, err);
  if (*err)
    return FALSE;

  return TRUE;
}

static void
usage (void)
{
  g_print ("usage: pixbuf-read <files>\n");
  exit (EXIT_FAILURE);
}
  
int
main (int argc, char **argv)
{
  int i;
  
  g_type_init ();
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

  if (argc == 1)
    usage();
  
  for (i = 1; i < argc; ++i)
    {
      gchar *contents;
      gsize size;
      GError *err = NULL;
      
      g_print ("%s\t\t", argv[i]);
      fflush (stdout);
      if (!g_file_get_contents (argv[i], &contents, &size, &err))
	{
	  fprintf (stderr, "%s: error: %s\n", argv[i], err->message);
	}
      else
	{
	  err = NULL;

	  if (test_loader ((guchar *) contents, size, &err))
	    g_print ("success\n");
	  else
	    g_print ("error: %s\n", err->message);

	  g_free (contents);
	}
    }
  
  return 0;
}
