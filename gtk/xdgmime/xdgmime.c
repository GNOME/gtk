/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmime.c: XDG Mime Spec mime resolver.  Based on version 0.11 of the spec.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 * 
 * Copyright (C) 2003  Red Hat, Inc.
 * Copyright (C) 2003  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * Licensed under the Academic Free License version 1.2
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "xdgmime.h"
#include "xdgmimeint.h"
#include "xdgmimeglob.h"
#include "xdgmimemagic.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

XdgGlobHash *global_hash = NULL;
XdgMimeMagic *global_magic = NULL;


static void
_xdg_mime_init_from_directory (const char *directory)
{
  char *file_name;

  file_name = malloc (strlen (directory) + strlen ("/mime/globs") + 1);
  strcpy (file_name, directory);
  strcat (file_name, "/mime/globs");
  _xdg_mime_glob_read_from_file (global_hash, file_name);
  free (file_name);

  file_name = malloc (strlen (directory) + strlen ("/mime/magic") + 1);
  strcpy (file_name, directory);
  strcat (file_name, "/mime/magic");
  _xdg_mime_magic_read_from_file (global_magic, file_name);
  free (file_name);
}

static void
xdg_mime_init (void)
{
  static int initted = 0;

  if (initted == 0)
    {
      const char *xdg_data_home;
      const char *xdg_data_dirs;
      const char *ptr;

      global_hash = _xdg_glob_hash_new ();
      global_magic = _xdg_mime_magic_new ();

      /* We look for globs and magic files based upon the XDG Base Directory
       * Specification
       */
      xdg_data_home = getenv ("XDG_DATA_HOME");
      if (xdg_data_home)
	{
	  _xdg_mime_init_from_directory (xdg_data_home);
	}
      else
	{
	  const char *home;

	  home = getenv ("HOME");
	  if (home != NULL)
	    {
	      char *guessed_xdg_home;

	      guessed_xdg_home = malloc (strlen (home) + strlen ("/.local/share/") + 1);
	      strcpy (guessed_xdg_home, home);
	      strcat (guessed_xdg_home, "/.local/share/");
	      _xdg_mime_init_from_directory (guessed_xdg_home);
	      free (guessed_xdg_home);
	    }
	}

      xdg_data_dirs = getenv ("XDG_DATA_DIRS");
      if (xdg_data_dirs == NULL)
	xdg_data_dirs = "/usr/local/share/:/usr/share/";

      ptr = xdg_data_dirs;

      while (*ptr != '\000')
	{
	  const char *end_ptr;
	  char *dir;
	  int len;

	  end_ptr = ptr;
	  while (*end_ptr != ':' && *end_ptr != '\000')
	    end_ptr ++;

	  if (end_ptr == ptr)
	    {
	      ptr++;
	      continue;
	    }

	  if (*end_ptr == ':')
	    len = end_ptr - ptr;
	  else
	    len = end_ptr - ptr + 1;
	  dir = malloc (len);
	  strncpy (dir, ptr, len);
	  _xdg_mime_init_from_directory (dir);
	  free (dir);

	  ptr = end_ptr;
	}
      initted = 1;
    }
}

const char *
xdg_mime_get_mime_type_for_data (const void *data,
				 size_t      len)
{
  const char *mime_type;

  xdg_mime_init ();

  mime_type = _xdg_mime_magic_lookup_data (global_magic, data, len);

  if (mime_type)
    return mime_type;

  return XDG_MIME_TYPE_UNKNOWN;
}

const char *
xdg_mime_get_mime_type_for_file (const char *file_name)
{
  const char *mime_type;
  FILE *file;
  unsigned char *data;
  int max_extent;
  int bytes_read;
  struct stat statbuf;
  const char *base_name;

  if (file_name == NULL)
    return NULL;
  if (! _xdg_utf8_validate (file_name))
    return NULL;

  xdg_mime_init ();

  base_name = _xdg_get_base_name (file_name);
  mime_type = xdg_mime_get_mime_type_from_file_name (base_name);

  if (mime_type != XDG_MIME_TYPE_UNKNOWN)
    return mime_type;

  if (stat (file_name, &statbuf) != 0)
    return XDG_MIME_TYPE_UNKNOWN;

  if (!S_ISREG (statbuf.st_mode))
    return XDG_MIME_TYPE_UNKNOWN;

  /* FIXME: Need to make sure that max_extent isn't totally broken.  This could
   * be large and need getting from a stream instead of just reading it all
   * in. */
  max_extent = _xdg_mime_magic_get_buffer_extents (global_magic);
  data = malloc (max_extent);
  if (data == NULL)
    return XDG_MIME_TYPE_UNKNOWN;
        
      file = fopen (file_name, "r");
  if (file == NULL)
    {
      free (data);
      return XDG_MIME_TYPE_UNKNOWN;
    }

  bytes_read = fread (data, 1, max_extent, file);
  if (ferror (file))
    {
      free (data);
      fclose (file);
      return XDG_MIME_TYPE_UNKNOWN;
    }

  mime_type = _xdg_mime_magic_lookup_data (global_magic, data, bytes_read);

  free (data);
  fclose (file);

  if (mime_type)
    return mime_type;

  return XDG_MIME_TYPE_UNKNOWN;
}

const char *
xdg_mime_get_mime_type_from_file_name (const char *file_name)
{
  const char *mime_type;

  xdg_mime_init ();

  mime_type = _xdg_glob_hash_lookup_file_name (global_hash, file_name);
  if (mime_type)
    return mime_type;
  else
    return XDG_MIME_TYPE_UNKNOWN;
}

int
xdg_mime_is_valid_mime_type (const char *mime_type)
{
  /* FIXME: We should make this a better test
   */
  return _xdg_utf8_validate (mime_type);
}
