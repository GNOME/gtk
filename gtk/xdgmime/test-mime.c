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
     
