/* -*- mode: C; c-file-style: "gnu" -*- */
/* test-mime.c: Test program for mime type identification
 *
 * More info can be found at http://www.freedesktop.org/standards/
 * 
 * Copyright (C) 2003  Red Hat, Inc.
 * Copyright (C) 2003  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * Licensed under the Academic Free License version 2.0
 * Or under the following terms:
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "xdgmime.h"
#include "xdgmimeglob.h"
#include <string.h>
#include <stdio.h>


static void
test_individual_glob (const char  *glob,
		      XdgGlobType  expected_type)
{
  XdgGlobType test_type;

  test_type = _xdg_glob_determine_type (glob);
  if (test_type != expected_type)
    {
      printf ("Test Failed: %s is of type %s, but %s is expected\n",
	      glob,
	      ((test_type == XDG_GLOB_LITERAL)?"XDG_GLOB_LITERAL":
	       ((test_type == XDG_GLOB_SIMPLE)?"XDG_GLOB_SIMPLE":"XDG_GLOB_FULL")),
	      ((expected_type == XDG_GLOB_LITERAL)?"XDG_GLOB_LITERAL":
	       ((expected_type == XDG_GLOB_SIMPLE)?"XDG_GLOB_SIMPLE":"XDG_GLOB_COMPLEX")));
    }
}

static void
test_glob_type (void)
{
  test_individual_glob ("*.gif", XDG_GLOB_SIMPLE);
  test_individual_glob ("Foo*.gif", XDG_GLOB_FULL);
  test_individual_glob ("*[4].gif", XDG_GLOB_FULL);
  test_individual_glob ("Makefile", XDG_GLOB_LITERAL);
  test_individual_glob ("sldkfjvlsdf\\\\slkdjf", XDG_GLOB_FULL);
  test_individual_glob ("tree.[ch]", XDG_GLOB_FULL);
}

int
main (int argc, char *argv[])
{
  const char *result;
  const char *file_name;
  int i;

  test_glob_type ();
  for (i = 1; i < argc; i++)
    {
      file_name = argv[i];
      result = xdg_mime_get_mime_type_for_file (file_name);
      printf ("File \"%s\" has a mime-type of %s\n", file_name, result);
    }

  return 0;
}
     
