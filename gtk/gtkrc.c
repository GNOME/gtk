/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gtkrc.h"


enum {
  TOKEN_EOF,
  TOKEN_LEFT_CURLY,
  TOKEN_RIGHT_CURLY,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_EQUAL_SIGN,
  TOKEN_COMMA,
  TOKEN_INTEGER,
  TOKEN_FLOAT,
  TOKEN_STRING,
  TOKEN_SYMBOL,
  TOKEN_ACTIVE,
  TOKEN_BASE,
  TOKEN_BG,
  TOKEN_BG_PIXMAP,
  TOKEN_FG,
  TOKEN_FONT,
  TOKEN_FONTSET,
  TOKEN_INSENSITIVE,
  TOKEN_NORMAL,
  TOKEN_PIXMAP_PATH,
  TOKEN_PRELIGHT,
  TOKEN_SELECTED,
  TOKEN_STYLE,
  TOKEN_TEXT,
  TOKEN_WIDGET,
  TOKEN_WIDGET_CLASS
};

enum {
  PARSE_OK,
  PARSE_ERROR,
  PARSE_SYNTAX
};

enum {
  PARSE_START,
  PARSE_COMMENT,
  PARSE_STRING,
  PARSE_SYMBOL,
  PARSE_NUMBER
};


typedef struct _GtkRcStyle  GtkRcStyle;
typedef struct _GtkRcSet    GtkRcSet;

struct _GtkRcStyle
{
  int initialize;
  char *name;
  char *font_name;
  char *fontset_name;
  char *bg_pixmap_name[5];
  GtkStyle *style;
};

struct _GtkRcSet
{
  char *set;
  GtkRcStyle *rc_style;
};


static guint       gtk_rc_style_hash               (const char *name);
static gint        gtk_rc_style_compare            (const char *a,
						    const char *b);
static GtkRcStyle* gtk_rc_style_find               (const char *name);
static GtkRcStyle* gtk_rc_styles_match             (GSList *sets,
						    const char   *path);
static gint        gtk_rc_style_match              (const char *set,
						    const char *path);
static void        gtk_rc_style_init               (GtkRcStyle *rc_style);
static gint        gtk_rc_get_token                (void);
static gint        gtk_rc_simple_token             (char ch);
static gint        gtk_rc_symbol_token             (const char *sym);
static gint        gtk_rc_get_next_token           (void);
static gint        gtk_rc_peek_next_token          (void);
static gint        gtk_rc_parse_statement          (void);
static gint        gtk_rc_parse_style              (void);
static gint        gtk_rc_parse_style_option       (GtkRcStyle   *rc_style);
static gint        gtk_rc_parse_base               (GtkStyle     *style);
static gint        gtk_rc_parse_bg                 (GtkStyle     *style);
static gint        gtk_rc_parse_fg                 (GtkStyle     *style);
static gint        gtk_rc_parse_bg_pixmap          (GtkRcStyle   *rc_style);
static gint        gtk_rc_parse_font               (GtkRcStyle   *rc_style);
static gint	   gtk_rc_parse_fontset 	   (GtkRcStyle   *rc_style);
static gint        gtk_rc_parse_state              (GtkStateType *state);
static gint        gtk_rc_parse_color              (GdkColor     *color);
static gint        gtk_rc_parse_pixmap_path        (void);
static void        gtk_rc_parse_pixmap_path_string (gchar *pix_path);
static char*       gtk_rc_find_pixmap_in_path      (gchar *pixmap_file);
static gint        gtk_rc_parse_widget_style       (void);
static gint        gtk_rc_parse_widget_class_style (void);
static char*       gtk_rc_widget_path              (GtkWidget *widget);
static char*       gtk_rc_widget_class_path        (GtkWidget *widget);


static struct
{
  char *name;
  int token;
} symbols[] =
  {
    { "ACTIVE", TOKEN_ACTIVE },
    { "base", TOKEN_BASE },
    { "bg", TOKEN_BG },
    { "bg_pixmap", TOKEN_BG_PIXMAP },
    { "fg", TOKEN_FG },
    { "font", TOKEN_FONT },
    { "fontset", TOKEN_FONTSET },
    { "INSENSITIVE", TOKEN_INSENSITIVE },
    { "NORMAL", TOKEN_NORMAL },
    { "pixmap_path", TOKEN_PIXMAP_PATH },
    { "PRELIGHT", TOKEN_PRELIGHT },
    { "SELECTED", TOKEN_SELECTED },
    { "style", TOKEN_STYLE },
    { "text", TOKEN_TEXT },
    { "widget", TOKEN_WIDGET },
    { "widget_class", TOKEN_WIDGET_CLASS },
  };

static int nsymbols = sizeof (symbols) / sizeof (symbols[0]);

static struct
{
  char ch;
  int token;
} simple_tokens[] =
  {
    { '{', TOKEN_LEFT_CURLY },
    { '}', TOKEN_RIGHT_CURLY },
    { '[', TOKEN_LEFT_BRACE },
    { ']', TOKEN_RIGHT_BRACE },
    { '=', TOKEN_EQUAL_SIGN },
    { ',', TOKEN_COMMA },
  };

static int nsimple_tokens = sizeof (simple_tokens) / sizeof (simple_tokens[0]);

static FILE *input_fp = NULL;
static char *buffer = NULL;
static char *tokenbuf = NULL;
static int position = 0;
static int linenum = 1;
static int buffer_size = 1024;
static int tokenbuf_size = 1024;

static int done;
static int cur_token;
static int next_token;

static char *token_str;
static double token_float;
static int token_int;

static GHashTable *rc_style_ht = NULL;
static GSList *widget_sets = NULL;
static GSList *widget_class_sets = NULL;

#define GTK_RC_MAX_PIXMAP_PATHS 128
static gchar *pixmap_path[GTK_RC_MAX_PIXMAP_PATHS];


void
gtk_rc_init ()
{
  rc_style_ht = g_hash_table_new ((GHashFunc) gtk_rc_style_hash,
				  (GCompareFunc) gtk_rc_style_compare);
}

void
gtk_rc_parse (const char *filename)
{
  input_fp = fopen (filename, "r");
  if (!input_fp)
	  return;

  buffer = g_new (char, buffer_size + tokenbuf_size);
  tokenbuf = buffer + buffer_size;
  position = 0;
  linenum = 1;

  cur_token = -1;
  next_token = -1;
  done = FALSE;

  while (!done)
    {
      if (gtk_rc_parse_statement () != PARSE_OK)
	{
	  g_warning ("rc file parse error: \"%s\" line %d",
		     filename, linenum);
	  done = TRUE;
	}
    }

  fclose (input_fp);

  g_free (buffer);

  buffer = NULL;
  tokenbuf = NULL;
  position = 0;
  linenum = 1;
}

GtkStyle*
gtk_rc_get_style (GtkWidget *widget)
{
  GtkRcStyle *rc_style;
  char *path;

  if (widget_sets)
    {
      path = gtk_rc_widget_path (widget);
      if (path)
	{
	  rc_style = gtk_rc_styles_match (widget_sets, path);
	  g_free (path);

	  if (rc_style)
	    {
	      gtk_rc_style_init (rc_style);
	      return rc_style->style;
	    }
	}
    }

  if (widget_class_sets)
    {
      path = gtk_rc_widget_class_path (widget);
      if (path)
	{
	  rc_style = gtk_rc_styles_match (widget_class_sets, path);
	  g_free (path);

	  if (rc_style)
	    {
	      gtk_rc_style_init (rc_style);
	      return rc_style->style;
	    }
	}
    }

  return widget->style;
}

void
gtk_rc_add_widget_name_style (GtkStyle *style,
			      const char     *pattern)
{
  GtkRcStyle *rc_style;
  GtkRcSet *rc_set;
  int i;

  gtk_style_ref (style);

  rc_style = g_new (GtkRcStyle, 1);
  rc_style->initialize = FALSE;
  rc_style->name = NULL;
  rc_style->font_name = NULL;
  rc_style->fontset_name = NULL;

  for (i = 0; i < 5; i++)
    rc_style->bg_pixmap_name[i] = NULL;

  rc_style->style = style;

  rc_set = g_new (GtkRcSet, 1);
  rc_set->set = g_strdup (pattern);
  rc_set->rc_style = rc_style;

  widget_sets = g_slist_append (widget_sets, rc_set);
}

void
gtk_rc_add_widget_class_style (GtkStyle *style,
			       const char     *pattern)
{
  GtkRcStyle *rc_style;
  GtkRcSet *rc_set;
  int i;

  gtk_style_ref (style);

  rc_style = g_new (GtkRcStyle, 1);
  rc_style->initialize = FALSE;
  rc_style->name = NULL;
  rc_style->font_name = NULL;
  rc_style->fontset_name = NULL;

  for (i = 0; i < 5; i++)
    rc_style->bg_pixmap_name[i] = NULL;

  rc_style->style = style;

  rc_set = g_new (GtkRcSet, 1);
  rc_set->set = g_strdup (pattern);
  rc_set->rc_style = rc_style;

  widget_class_sets = g_slist_append (widget_class_sets, rc_set);
}


static guint
gtk_rc_style_hash (const char *name)
{
  guint result;

  result = 0;
  while (*name)
    result += (result << 3) + *name++;

  return result;
}

static gint
gtk_rc_style_compare (const char *a,
		      const char *b)
{
  return (strcmp (a, b) == 0);
}

static GtkRcStyle*
gtk_rc_style_find (const char *name)
{
  GtkRcStyle *rc_style;

  rc_style = g_hash_table_lookup (rc_style_ht, (gpointer) name);

  return rc_style;
}

static GtkRcStyle*
gtk_rc_styles_match (GSList       *sets,
		     const char   *path)
{
  GtkRcSet *rc_set;

  while (sets)
    {
      rc_set = sets->data;
      sets = sets->next;

      if (gtk_rc_style_match (rc_set->set, path))
	return rc_set->rc_style;
    }

  return NULL;
}

static gint
gtk_rc_style_match (const char *set,
		    const char *path)
{
  char ch;

  while (1)
    {
      ch = *set++;
      if (ch == '\0')
	return (*path == '\0');

      switch (ch)
	{
	case '*':
	  while (*set == '*')
	    set++;

	  ch = *set++;
	  if (ch == '\0')
	    return TRUE;

	  while (*path)
	    {
	      while (*path && (ch != *path))
		path++;

	      if (!(*path))
		return FALSE;

	      path++;
	      if (gtk_rc_style_match (set, path))
		return TRUE;
	    }
	  break;

	case '?':
	  break;

	default:
	  if (ch == *path)
	    path++;
	  else
	    return FALSE;
	  break;
	}
    }

  return TRUE;
}

static void
gtk_rc_style_init (GtkRcStyle *rc_style)
{
  GdkFont *old_font;
  gint i;

  if (rc_style->initialize)
    {
      rc_style->initialize = FALSE;

      if (rc_style->fontset_name)
	{
	  old_font = rc_style->style->font;
	  rc_style->style->font = gdk_fontset_load (rc_style->fontset_name);
	  if (rc_style->style->font)
	    gdk_font_unref (old_font);
	  else
	    rc_style->style->font = old_font;
	}
      else if (rc_style->font_name)
	{
	  old_font = rc_style->style->font;
	  rc_style->style->font = gdk_font_load (rc_style->font_name);
	  if (rc_style->style->font)
	    gdk_font_unref (old_font);
	  else
	    rc_style->style->font = old_font;
	}

      for (i = 0; i < 5; i++)
	if (rc_style->bg_pixmap_name[i])
	  {
	    if (strcmp (rc_style->bg_pixmap_name[i], "<parent>") == 0)
	      rc_style->style->bg_pixmap[i] = (GdkPixmap*) GDK_PARENT_RELATIVE;
	    else
	      rc_style->style->bg_pixmap[i] = gdk_pixmap_create_from_xpm (NULL, NULL,
									  &rc_style->style->bg[i],
									  rc_style->bg_pixmap_name[i]);
	  }
    }
}

static gint
gtk_rc_get_token ()
{
  int tokenpos;
  int state;
  int count;
  int token;
  int hex_number = FALSE;
  int float_number = FALSE;
  char ch;

  tokenpos = 0;
  state = PARSE_START;

  while (1)
    {
      if (position >= (buffer_size - 1))
	position = 0;
      if (!position || (buffer[position] == '\0'))
	{
	  count = fread (buffer, sizeof (char), buffer_size - 1, input_fp);
	  if ((count == 0) && feof (input_fp))
	    return TOKEN_EOF;
	  buffer[count] = '\0';
	}

      ch = buffer[position++];
      if (ch == '\n')
	linenum += 1;

      switch (state)
	{
	case PARSE_START:
	  token = gtk_rc_simple_token (ch);

	  if (token)
	    return token;
	  else if (ch == '#')
	    state = PARSE_COMMENT;
	  else if (ch == '"')
	    state = PARSE_STRING;
	  else if ((ch == ' ') || (ch == '\t') || (ch == '\n'))
	    break;
	  else if (ch == '.')
	    {
	      hex_number = FALSE;
	      float_number = TRUE;
	      tokenbuf[tokenpos++] = ch;
	      state = PARSE_NUMBER;
	    }
	  else if ((ch == '$') || (ch == '#'))
	    {
	      hex_number = TRUE;
	      float_number = FALSE;
	      state = PARSE_NUMBER;
	    }
	  else if (isdigit (ch))
	    {
	      hex_number = FALSE;
	      float_number = FALSE;
	      tokenbuf[tokenpos++] = ch;
	      state = PARSE_NUMBER;
	    }
	  else
	    {
	      tokenbuf[tokenpos++] = ch;
	      state = PARSE_SYMBOL;
	    }
	  break;

	case PARSE_COMMENT:
	  if (ch == '\n')
	    state = PARSE_START;
	  break;

	case PARSE_STRING:
	  if (ch != '"')
	    {
	      tokenbuf[tokenpos++] = ch;
	    }
	  else
	    {
	      tokenbuf[tokenpos] = '\0';
	      token_str = tokenbuf;
	      return TOKEN_STRING;
	    }
	  break;

	case PARSE_SYMBOL:
	  if ((ch != ' ') && (ch != '\t') && (ch != '\n') &&
	      (gtk_rc_simple_token (ch) == 0))
	    {
	      tokenbuf[tokenpos++] = ch;
	    }
	  else
	    {
	      position -= 1;
	      tokenbuf[tokenpos] = '\0';
	      token_str = tokenbuf;
	      return gtk_rc_symbol_token (tokenbuf);
	    }
	  break;

	case PARSE_NUMBER:
	  if (isdigit (ch) || (hex_number && isxdigit (ch)))
	    {
	      tokenbuf[tokenpos++] = ch;
	    }
	  else if (!hex_number && !float_number && (ch == '.'))
	    {
	      float_number = TRUE;
	      tokenbuf[tokenpos++] = ch;
	    }
	  else if (!float_number &&
		   (ch == 'x') && (tokenpos == 1) &&
		   (tokenbuf[0] == '0'))
	    {
	      hex_number = TRUE;
	      tokenpos = 0;
	    }
	  else
	    {
	      position -= 1;
	      tokenbuf[tokenpos] = '\0';
	      if (float_number)
		{
		  sscanf (tokenbuf, "%lf", &token_float);
		  return TOKEN_FLOAT;
		}
	      else
		{
		  sscanf (tokenbuf, (hex_number ? "%x" : "%d"), &token_int);
		  return TOKEN_INTEGER;
		}
	    }
	  break;
	}
    }
}

static gint
gtk_rc_simple_token (char ch)
{
  gint i;

  for (i = 0; i < nsimple_tokens; i++)
    if (simple_tokens[i].ch == ch)
      return simple_tokens[i].token;

  return 0;
}

static gint
gtk_rc_symbol_token (const char *sym)
{
  gint i;

  for (i = 0; i < nsymbols; i++)
    if (strcmp (symbols[i].name, sym) == 0)
      return symbols[i].token;

  return TOKEN_STRING;
}

static gint
gtk_rc_get_next_token ()
{
  if (next_token != -1)
    {
      cur_token = next_token;
      next_token = -1;
    }
  else
    {
      cur_token = gtk_rc_get_token ();
    }

  return cur_token;
}

static gint
gtk_rc_peek_next_token ()
{
  if (next_token == -1)
    next_token = gtk_rc_get_token ();

  return next_token;
}

static gint
gtk_rc_parse_statement ()
{
  gint token;
  gint error;

  token = gtk_rc_peek_next_token ();
  if (!token)
    {
      done = TRUE;
      return PARSE_OK;
    }

  error = gtk_rc_parse_style ();
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_pixmap_path ();
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_widget_style ();
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_widget_class_style ();

  return error;
}

static gint
gtk_rc_parse_style ()
{
  GtkRcStyle *rc_style;
  GtkRcStyle *parent_style;
  gint token;
  gint error;
  gint insert;
  gint i;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_STYLE)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    return PARSE_ERROR;

  insert = FALSE;
  rc_style = g_hash_table_lookup (rc_style_ht, token_str);

  if (!rc_style)
    {
      insert = TRUE;
      rc_style = g_new (GtkRcStyle, 1);
      rc_style->initialize = TRUE;
      rc_style->name = g_strdup (token_str);
      rc_style->font_name = NULL;
      rc_style->fontset_name = NULL;

      for (i = 0; i < 5; i++)
	rc_style->bg_pixmap_name[i] = NULL;

      rc_style->style = gtk_style_new ();
      gtk_style_ref (rc_style->style);
    }

  token = gtk_rc_peek_next_token ();
  if (token == TOKEN_EQUAL_SIGN)
    {
      token = gtk_rc_get_next_token ();

      token = gtk_rc_get_next_token ();
      if (!token || (token != TOKEN_STRING))
	{
	  if (insert)
	    {
	      gtk_style_unref (rc_style->style);
	      g_free (rc_style);
	    }
	  return PARSE_ERROR;
	}

      parent_style = g_hash_table_lookup (rc_style_ht, token_str);
      if (parent_style)
	{
	  for (i = 0; i < 5; i++)
	    {
	      rc_style->style->fg[i] = parent_style->style->fg[i];
	      rc_style->style->bg[i] = parent_style->style->bg[i];
	      rc_style->style->light[i] = parent_style->style->light[i];
	      rc_style->style->dark[i] = parent_style->style->dark[i];
	      rc_style->style->mid[i] = parent_style->style->mid[i];
	      rc_style->style->text[i] = parent_style->style->text[i];
	      rc_style->style->base[i] = parent_style->style->base[i];
	    }

	  rc_style->style->black = parent_style->style->black;
	  rc_style->style->white = parent_style->style->white;

	  if (rc_style->fontset_name)
	    {
	      g_free (rc_style->fontset_name);
	      rc_style->fontset_name = g_strdup (parent_style->fontset_name);
	    }
	  else if (rc_style->font_name)
	    {
	      g_free (rc_style->font_name);
	      rc_style->font_name = g_strdup (parent_style->font_name);
	    }

	  for (i = 0; i < 5; i++)
	    {
	      if (rc_style->bg_pixmap_name[i])
		g_free (rc_style->bg_pixmap_name[i]);
	      rc_style->bg_pixmap_name[i] = g_strdup (parent_style->bg_pixmap_name[i]);
	    }
	}
    }

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_LEFT_CURLY))
    {
      if (insert)
	{
	  gtk_style_unref (rc_style->style);
	  g_free (rc_style);
	}
      return PARSE_ERROR;
    }

  while (1)
    {
      error = gtk_rc_parse_style_option (rc_style);
      if (error == PARSE_SYNTAX)
	break;
      if (error == PARSE_ERROR)
	{
	  if (insert)
	    {
	      gtk_style_unref (rc_style->style);
	      g_free (rc_style);
	    }
	  return error;
	}
    }

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_RIGHT_CURLY))
    {
      if (insert)
	{
	  if (rc_style->fontset_name)
	    g_free (rc_style->fontset_name);
	  else if (rc_style->font_name)
	    g_free (rc_style->font_name);

	  for (i = 0; i < 5; i++)
	    if (rc_style->bg_pixmap_name[i])
	      g_free (rc_style->bg_pixmap_name[i]);

	  gtk_style_unref (rc_style->style);
	  g_free (rc_style);
	}
      return PARSE_ERROR;
    }

  if (insert)
    g_hash_table_insert (rc_style_ht, rc_style->name, rc_style);

  return PARSE_OK;
}

static gint
gtk_rc_parse_style_option (GtkRcStyle *rc_style)
{
  gint token;
  gint error;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;

  error = gtk_rc_parse_base (rc_style->style);
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_bg (rc_style->style);
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_fg (rc_style->style);
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_bg_pixmap (rc_style);
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_font (rc_style);
  if (error != PARSE_SYNTAX)
    return error;

  error = gtk_rc_parse_fontset (rc_style);

  return error;
}

static gint
gtk_rc_parse_base (GtkStyle *style)
{
  GtkStateType state;
  gint token;
  gint error;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_BASE)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  error = gtk_rc_parse_state (&state);
  if (error != PARSE_OK)
    return error;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_EQUAL_SIGN))
    return PARSE_ERROR;

  error = gtk_rc_parse_color (&style->base[state]);

  return error;
}

static gint
gtk_rc_parse_bg (GtkStyle *style)
{
  GtkStateType state;
  gint token;
  gint error;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_BG)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  error = gtk_rc_parse_state (&state);
  if (error != PARSE_OK)
    return error;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_EQUAL_SIGN))
    return PARSE_ERROR;

  error = gtk_rc_parse_color (&style->bg[state]);

  return error;
}

static gint
gtk_rc_parse_fg (GtkStyle *style)
{
  GtkStateType state;
  gint token;
  gint error;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_FG)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  error = gtk_rc_parse_state (&state);
  if (error != PARSE_OK)
    return error;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_EQUAL_SIGN))
    return PARSE_ERROR;

  error = gtk_rc_parse_color (&style->fg[state]);

  return error;
}

static gint
gtk_rc_parse_bg_pixmap (GtkRcStyle *rc_style)
{
  GtkStateType state;
  gint token;
  gint error;
  gchar *pixmap_file;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_BG_PIXMAP)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  error = gtk_rc_parse_state (&state);
  if (error != PARSE_OK)
    return error;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_EQUAL_SIGN))
    return PARSE_ERROR;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    return PARSE_ERROR;

  if (strcmp (token_str, "<parent>"))
    pixmap_file = gtk_rc_find_pixmap_in_path (token_str);
  else
    pixmap_file = g_strdup(token_str);

  if (pixmap_file)
    {
      if (rc_style->bg_pixmap_name[state])
	g_free (rc_style->bg_pixmap_name[state]);
      rc_style->bg_pixmap_name[state] = pixmap_file;
    }

  return PARSE_OK;
}

static char*
gtk_rc_find_pixmap_in_path (gchar *pixmap_file)
{
  gint i;
  FILE *fp;
  gchar *buf;

  for (i = 0; (i < GTK_RC_MAX_PIXMAP_PATHS) && (pixmap_path[i] != NULL); i++)
    {
      buf = g_malloc (strlen (pixmap_path[i]) + strlen (pixmap_file) + 2);
      sprintf (buf, "%s%c%s", pixmap_path[i], '/', pixmap_file);

      fp = fopen (buf, "r");
      if (fp)
	{
	  fclose (fp);
	  return buf;
	}

      g_free (buf);
    }

  g_warning ("Unable to locate image file in pixmap_path: \"%s\" line %d",
	     pixmap_file, linenum);

  return NULL;
}

static gint
gtk_rc_parse_font (GtkRcStyle *rc_style)
{
  gint token;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_FONT)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_EQUAL_SIGN))
    return PARSE_ERROR;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    return PARSE_ERROR;

  if (rc_style->font_name)
    g_free (rc_style->font_name);
  rc_style->font_name = g_strdup (token_str);

  return PARSE_OK;
}

static gint
gtk_rc_parse_fontset (GtkRcStyle *rc_style)
{
  gint token;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_FONTSET)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_EQUAL_SIGN))
    return PARSE_ERROR;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    return PARSE_ERROR;

  if (rc_style->fontset_name)
    g_free (rc_style->fontset_name);
  rc_style->fontset_name = g_strdup (token_str);

  return PARSE_OK;
}

static gint
gtk_rc_parse_state (GtkStateType *state)
{
  gint token;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_LEFT_BRACE)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  token = gtk_rc_get_next_token ();
  if (token == TOKEN_ACTIVE)
    *state = GTK_STATE_ACTIVE;
  else if (token == TOKEN_INSENSITIVE)
    *state = GTK_STATE_INSENSITIVE;
  else if (token == TOKEN_NORMAL)
    *state = GTK_STATE_NORMAL;
  else if (token == TOKEN_PRELIGHT)
    *state = GTK_STATE_PRELIGHT;
  else if (token == TOKEN_SELECTED)
    *state = GTK_STATE_SELECTED;
  else
    return PARSE_ERROR;

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_RIGHT_BRACE))
    return PARSE_ERROR;

  return PARSE_OK;
}

static gint
gtk_rc_parse_color (GdkColor *color)
{
  gint token;
  gint length;
  gint temp;
  gchar buf[9];
  gint i, j;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;

  switch (token)
    {
    case TOKEN_LEFT_CURLY:
      token = gtk_rc_get_next_token ();

      token = gtk_rc_get_next_token ();
      if (!token || ((token != TOKEN_INTEGER) && (token != TOKEN_FLOAT)))
	return PARSE_ERROR;

      if (token == TOKEN_FLOAT)
	token_int = token_float * 65535.0;
      if (token_int < 0)
	token_int = 0;
      if (token_int > 65535)
	token_int = 65535;

      color->red = token_int;

      token = gtk_rc_get_next_token ();
      if (!token || (token != TOKEN_COMMA))
	return PARSE_ERROR;

      token = gtk_rc_get_next_token ();
      if (!token || ((token != TOKEN_INTEGER) && (token != TOKEN_FLOAT)))
	return PARSE_ERROR;

      if (token == TOKEN_FLOAT)
	token_int = token_float * 65535.0;
      if (token_int < 0)
	token_int = 0;
      if (token_int > 65535)
	token_int = 65535;

      color->green = token_int;

      token = gtk_rc_get_next_token ();
      if (!token || (token != TOKEN_COMMA))
	return PARSE_ERROR;

      token = gtk_rc_get_next_token ();
      if (!token || ((token != TOKEN_INTEGER) && (token != TOKEN_FLOAT)))
	return PARSE_ERROR;

      if (token == TOKEN_FLOAT)
	token_int = token_float * 65535.0;
      if (token_int < 0)
	token_int = 0;
      if (token_int > 65535)
	token_int = 65535;

      color->blue = token_int;

      token = gtk_rc_get_next_token ();
      if (!token || (token != TOKEN_RIGHT_CURLY))
	return PARSE_ERROR;
      break;

    case TOKEN_STRING:
      token = gtk_rc_get_next_token ();

      if (token_str[0] != '#')
	return PARSE_ERROR;

      length = strlen (token_str) - 1;
      if (((length % 3) != 0) || (length > 12))
	return PARSE_ERROR;
      length /= 3;

      for (i = 0, j = 1; i < length; i++, j++)
	buf[i] = token_str[j];
      buf[i] = '\0';

      sscanf (buf, "%x", &temp);
      color->red = temp;

      for (i = 0; i < length; i++, j++)
	buf[i] = token_str[j];
      buf[i] = '\0';

      sscanf (buf, "%x", &temp);
      color->green = temp;

      for (i = 0; i < length; i++, j++)
	buf[i] = token_str[j];
      buf[i] = '\0';

      sscanf (buf, "%x", &temp);
      color->blue = temp;

      if (length == 1)
	{
	  color->red *= 4369;
	  color->green *= 4369;
	  color->blue *= 4369;
	}
      else if (length == 2)
	{
	  color->red *= 257;
	  color->green *= 257;
	  color->blue *= 257;
	}
      else if (length == 3)
	{
	  color->red *= 16;
	  color->green *= 16;
	  color->blue *= 16;
	}
      break;

    default:
      return PARSE_SYNTAX;
    }

  return PARSE_OK;
}

static gint
gtk_rc_parse_pixmap_path ()
{
  gint token;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_PIXMAP_PATH)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  token = gtk_rc_get_next_token ();

  if (!token || (token != TOKEN_STRING))
    return PARSE_ERROR;

  gtk_rc_parse_pixmap_path_string(token_str);

  return PARSE_OK;
}

static void
gtk_rc_parse_pixmap_path_string (gchar *pix_path)
{
  gchar *buf;
  gint end_offset;
  gint start_offset = 0;
  gint path_len;
  gint path_num;
  
  /* free the old one, or just add to the old one ? */
  for (path_num=0; pixmap_path[path_num]; path_num++)
    {
      g_free (pixmap_path[path_num]);
      pixmap_path[path_num] = NULL;
    }
  
  path_num = 0;
  
  path_len = strlen (pix_path);
  
  buf = g_strdup (pix_path);

  for (end_offset = 0; end_offset <= path_len; end_offset++)
    {
      if ((buf[end_offset] == ':') ||
	  (end_offset == path_len))
	{
	  buf[end_offset] = '\0';
	  pixmap_path[path_num] = g_strdup (buf + start_offset);
	  path_num++;
	  pixmap_path[path_num] = NULL;
	  start_offset = end_offset + 1;
	}
    }
  g_free (buf);
}

static gint
gtk_rc_parse_widget_style ()
{
  GtkRcSet *rc_set;
  gint token;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_WIDGET)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    return PARSE_ERROR;

  rc_set = g_new (GtkRcSet, 1);
  rc_set->set = g_strdup (token_str);

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STYLE))
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }

  rc_set->rc_style = gtk_rc_style_find (token_str);
  if (!rc_set->rc_style)
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }

  widget_sets = g_slist_append (widget_sets, rc_set);

  return PARSE_OK;
}

static gint
gtk_rc_parse_widget_class_style ()
{
  GtkRcSet *rc_set;
  gint token;

  token = gtk_rc_peek_next_token ();
  if (!token)
    return PARSE_ERROR;
  if (token != TOKEN_WIDGET_CLASS)
    return PARSE_SYNTAX;
  token = gtk_rc_get_next_token ();

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    return PARSE_ERROR;

  rc_set = g_new (GtkRcSet, 1);
  rc_set->set = g_strdup (token_str);

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STYLE))
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }

  token = gtk_rc_get_next_token ();
  if (!token || (token != TOKEN_STRING))
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }

  rc_set->rc_style = gtk_rc_style_find (token_str);
  if (!rc_set->rc_style)
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }

  widget_class_sets = g_slist_append (widget_class_sets, rc_set);

  return PARSE_OK;
}

static char*
gtk_rc_widget_path (GtkWidget *widget)
{
  GtkWidget *tmp_widget;
  char *path;
  char *name;
  int pathlength;
  int namelength;

  path = NULL;
  pathlength = 0;

  tmp_widget = widget;
  while (tmp_widget)
    {
      name = gtk_widget_get_name (tmp_widget);
      pathlength += strlen (name);

      tmp_widget = tmp_widget->parent;

      if (tmp_widget)
	pathlength += 1;
    }

  path = g_new (char, pathlength + 1);
  path[pathlength] = '\0';

  tmp_widget = widget;
  while (tmp_widget)
    {
      name = gtk_widget_get_name (tmp_widget);
      namelength = strlen (name);

      strncpy (&path[pathlength - namelength], name, namelength);
      pathlength -= namelength;

      tmp_widget = tmp_widget->parent;

      if (tmp_widget)
	{
	  pathlength -= 1;
	  path[pathlength] = '.';
	}
    }

  return path;
}

static char*
gtk_rc_widget_class_path (GtkWidget *widget)
{
  GtkWidget *tmp_widget;
  char *path;
  char *name;
  int pathlength;
  int namelength;

  path = NULL;
  pathlength = 0;

  tmp_widget = widget;
  while (tmp_widget)
    {
      name = gtk_type_name (GTK_WIDGET_TYPE (tmp_widget));
      pathlength += strlen (name);

      tmp_widget = tmp_widget->parent;

      if (tmp_widget)
	pathlength += 1;
    }

  path = g_new (char, pathlength + 1);
  path[pathlength] = '\0';

  tmp_widget = widget;
  while (tmp_widget)
    {
      name = gtk_type_name (GTK_WIDGET_TYPE (tmp_widget));
      namelength = strlen (name);

      strncpy (&path[pathlength - namelength], name, namelength);
      pathlength -= namelength;

      tmp_widget = tmp_widget->parent;

      if (tmp_widget)
	{
	  pathlength -= 1;
	  path[pathlength] = '.';
	}
    }

  return path;
}
