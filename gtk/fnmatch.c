/* Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Stripped down, converted to UTF-8 and test cases added
 *
 *                    Owen Taylor, 13 December 2002;
 */

#include <string.h>

#include <glib.h>

/* We need to make sure that all constants are defined
 * to properly compile this file
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

static gunichar
get_char (const char **str)
{
  gunichar c = g_utf8_get_char (*str);
  *str = g_utf8_next_char (*str);

#ifdef G_PLATFORM_WIN32
  c = g_unichar_tolower (c);
#endif

  return c;
}

#if defined(G_OS_WIN32) || defined(G_WITH_CYGWIN)
#define DO_ESCAPE 0
#else  
#define DO_ESCAPE 1
#endif  

static gunichar
get_unescaped_char (const char **str,
		    gboolean    *was_escaped)
{
  gunichar c = get_char (str);

  *was_escaped = DO_ESCAPE && c == '\\';
  if (*was_escaped)
    c = get_char (str);
  
  return c;
}

/* Match STRING against the filename pattern PATTERN, returning zero if
   it matches, nonzero if not.  */

static gboolean
gtk_fnmatch_intern (const char *pattern,
		    const char *string,
		    gboolean   component_start)
{
  const char *p = pattern, *n = string;
  
  while (*p)
    {
      const char *last_n = n;
      
      gunichar c = get_char (&p);
      gunichar nc = get_char (&n);
      
      switch (c)
	{
   	case '?':
	  if (nc == '\0')
	    return FALSE;
	  else if (nc == G_DIR_SEPARATOR)
	    return FALSE;
	  else if (nc == '.' && component_start)
	    return FALSE;
	  break;
	case '\\':
	  if (DO_ESCAPE)
	    c = get_char (&p);
	  if (nc != c)
	    return FALSE;
	  break;
	case '*':
	  if (nc == '.' && component_start)
	    return FALSE;

	  {
	    const char *last_p = p;

	    for (last_p = p, c = get_char (&p);
		 c == '?' || c == '*';
		 last_p = p, c = get_char (&p))
	      {
		if (c == '?')
		  {
		    if (nc == '\0')
		      return FALSE;
		    else if (nc == G_DIR_SEPARATOR)
		      return FALSE;
		    else
		      {
			last_n = n; nc = get_char (&n);
		      }
		  }
	      }

	    /* If the pattern ends with wildcards, we have a
	     * guaranteed match unless there is a dir separator
	     * in the remainder of the string.
	     */
	    if (c == '\0')
	      {
		if (strchr (last_n, G_DIR_SEPARATOR) != NULL)
		  return FALSE;
		else
		  return TRUE;
	      }

	    if (DO_ESCAPE && c == '\\')
	      c = get_char (&p);

	    for (p = last_p; nc != '\0';)
	      {
		if ((c == '[' || nc == c) &&
		    gtk_fnmatch_intern (p, last_n, component_start))
		  return TRUE;
		
		component_start = (nc == G_DIR_SEPARATOR);
		last_n = n;
		nc = get_char (&n);
	      }
		  
	    return FALSE;
	  }

	case '[':
	  {
	    /* Nonzero if the sense of the character class is inverted.  */
	    gboolean not;
	    gboolean was_escaped;

	    if (nc == '\0' || nc == G_DIR_SEPARATOR)
	      return FALSE;

	    if (nc == '.' && component_start)
	      return FALSE;

	    not = (*p == '!' || *p == '^');
	    if (not)
	      ++p;

	    c = get_unescaped_char (&p, &was_escaped);
	    for (;;)
	      {
		register gunichar cstart = c, cend = c;
		if (c == '\0')
		  /* [ (unterminated) loses.  */
		  return FALSE;

		c = get_unescaped_char (&p, &was_escaped);
		
		if (!was_escaped && c == '-' && *p != ']')
		  {
		    cend = get_unescaped_char (&p, &was_escaped);
		    if (cend == '\0')
		      return FALSE;

		    c = get_char (&p);
		  }

		if (nc >= cstart && nc <= cend)
		  goto matched;

		if (!was_escaped && c == ']')
		  break;
	      }
	    if (!not)
	      return FALSE;
	    break;

	  matched:;
	    /* Skip the rest of the [...] that already matched.  */
	    /* XXX 1003.2d11 is unclear if was_escaped is right.  */
	    while (was_escaped || c != ']')
	      {
		if (c == '\0')
		  /* [... (unterminated) loses.  */
		  return FALSE;

		c = get_unescaped_char (&p, &was_escaped);
	      }
	    if (not)
	      return FALSE;
	  }
	  break;

	default:
	  if (c != nc)
	    return FALSE;
	}

      component_start = (nc == G_DIR_SEPARATOR);
    }

  if (*n == '\0')
    return TRUE;

  return FALSE;
}

/* Match STRING against the filename pattern PATTERN, returning zero if
 *  it matches, nonzero if not.
 *
 * GTK+ used to use a old version of GNU fnmatch() that was buggy
 * in various ways and didn't handle UTF-8. The following is
 * converted to UTF-8. To simplify the process of making it
 * correct, this is special-cased to the combinations of flags
 * that gtkfilesel.c uses.
 *
 *   FNM_FILE_NAME   - always set
 *   FNM_LEADING_DIR - never set
 *   FNM_PERIOD      - always set
 *   FNM_NOESCAPE    - set only on windows
 *   FNM_CASEFOLD    - set only on windows
 */
gboolean
_gtk_fnmatch (const char *pattern,
	      const char *string)
{
  return gtk_fnmatch_intern (pattern, string, TRUE);
}

#undef FNMATCH_TEST_CASES
#ifdef FNMATCH_TEST_CASES

#define TEST(pat, str, result) \
  g_assert (_gtk_fnmatch ((pat), (str)) == result)

int main (int argc, char **argv)
{
  TEST ("[a-]", "-", TRUE);
  
  TEST ("a", "a", TRUE);
  TEST ("a", "b", FALSE);

  /* Test what ? matches */
  TEST ("?", "a", TRUE);
  TEST ("?", ".", FALSE);
  TEST ("a?", "a.", TRUE);
  TEST ("a/?", "a/b", TRUE);
  TEST ("a/?", "a/.", FALSE);
  TEST ("?", "/", FALSE);

  /* Test what * matches */
  TEST ("*", "a", TRUE);
  TEST ("*", ".", FALSE);
  TEST ("a*", "a.", TRUE);
  TEST ("a/*", "a/b", TRUE);
  TEST ("a/*", "a/.", FALSE);
  TEST ("*", "/", FALSE);

  /* Range tests */
  TEST ("[ab]", "a", TRUE);
  TEST ("[ab]", "c", FALSE);
  TEST ("[^ab]", "a", FALSE);
  TEST ("[!ab]", "a", FALSE);
  TEST ("[^ab]", "c", TRUE);
  TEST ("[!ab]", "c", TRUE);
  TEST ("[a-c]", "b", TRUE);
  TEST ("[a-c]", "d", FALSE);
  TEST ("[a-]", "-", TRUE);
  TEST ("[]]", "]", TRUE);
  TEST ("[^]]", "a", TRUE);
  TEST ("[!]]", "a", TRUE);

  /* Various unclosed ranges */
  TEST ("[ab", "a", FALSE);
  TEST ("[a-", "a", FALSE);
  TEST ("[ab", "c", FALSE);
  TEST ("[a-", "c", FALSE);
  TEST ("[^]", "a", FALSE);

  /* Ranges and special no-wildcard matches */
  TEST ("[.]", ".", FALSE);
  TEST ("a[.]", "a.", TRUE);
  TEST ("a/[.]", "a/.", FALSE);
  TEST ("[/]", "/", FALSE);
  TEST ("[^/]", "a", TRUE);
  
  /* Basic tests of * (and combinations of * and ?) */
  TEST ("a*b", "ab", TRUE);
  TEST ("a*b", "axb", TRUE);
  TEST ("a*b", "axxb", TRUE);
  TEST ("a**b", "ab", TRUE);
  TEST ("a**b", "axb", TRUE);
  TEST ("a**b", "axxb", TRUE);
  TEST ("a*?*b", "ab", FALSE);
  TEST ("a*?*b", "axb", TRUE);
  TEST ("a*?*b", "axxb", TRUE);

  /* Test of  *[range] */
  TEST ("a*[cd]", "ac", TRUE);
  TEST ("a*[cd]", "axc", TRUE);
  TEST ("a*[cd]", "axx", FALSE);

  TEST ("a/[.]", "a/.", FALSE);
  TEST ("a*[.]", "a/.", FALSE);

  /* Test of UTF-8 */

  TEST ("ä", "ä", TRUE);      /* TEST ("ä", "ä", TRUE); */
  TEST ("?", "ä", TRUE);       /* TEST ("?", "ä", TRUE); */
  TEST ("*ö", "äö", TRUE);   /* TEST ("*ö", "äö", TRUE); */
  TEST ("*ö", "ääö", TRUE); /* TEST ("*ö", "ääö", TRUE); */
  TEST ("[ä]", "ä", TRUE);    /* TEST ("[ä]", "ä", TRUE); */
  TEST ("[ä-ö]", "é", TRUE); /* TEST ("[ä-ö]", "é", TRUE); */
  TEST ("[ä-ö]", "a", FALSE); /* TEST ("[ä-ö]", "a", FALSE); */

#ifdef DO_ESCAPE
  /* Tests of escaping */
  TEST ("\\\\", "\\", TRUE);
  TEST ("\\?", "?", TRUE);
  TEST ("\\?", "a", FALSE);
  TEST ("\\*", "*", TRUE);
  TEST ("\\*", "a", FALSE);
  TEST ("\\[a-b]", "[a-b]", TRUE);
  TEST ("[\\\\]", "\\", TRUE);
  TEST ("[\\^a]", "a", TRUE);
  TEST ("[a\\-c]", "b", FALSE);
  TEST ("[a\\-c]", "-", TRUE);
  TEST ("[a\\]", "a", FALSE);
#endif /* DO_ESCAPE */
  
  return 0;
}

#endif /* FNMATCH_TEST_CASES */
