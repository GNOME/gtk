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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "gtkrc.h"


enum {
  TOKEN_INVALID = G_TOKEN_LAST,
  TOKEN_INCLUDE,
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
  PARSE_SYNTAX,
  PARSE_DONE
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
typedef struct _GtkRcNode   GtkRcNode;

struct _GtkRcNode
{
  GdkColormap *cmap;
  GtkStyle *style;
};

struct _GtkRcStyle
{
  char *name;
  char *font_name;
  char *fontset_name;
  char *bg_pixmap_name[5];
  GtkStyle *proto_style;
  GList *styles;
};

struct _GtkRcSet
{
  char *set;
  GtkRcStyle *rc_style;
};


static guint	   gtk_rc_style_hash		   (const char   *name);
static gint	   gtk_rc_style_compare		   (const char   *a,
						    const char   *b);
static GtkRcStyle* gtk_rc_style_find		   (const char   *name);
static GtkRcStyle* gtk_rc_styles_match		   (GSList       *sets,
						    const char	 *path);
static gint	   gtk_rc_style_match		   (const char   *set,
						    const char   *path);
static GtkStyle*   gtk_rc_style_init		   (GtkRcStyle   *rc_style,
						    GdkColormap  *cmap);
static void	   gtk_rc_parse_any		   (const gchar  *input_name,
						    gint	  input_fd,
						    const gchar  *input_string);
static gint	   gtk_rc_parse_statement	   (GScanner	 *scanner);
static gint	   gtk_rc_parse_style		   (GScanner	 *scanner);
static gint	   gtk_rc_parse_style_option	   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static gint	   gtk_rc_parse_base		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static gint	   gtk_rc_parse_bg		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static gint	   gtk_rc_parse_fg		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static gint	   gtk_rc_parse_text		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static gint	   gtk_rc_parse_bg_pixmap	   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static gint	   gtk_rc_parse_font		   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static gint	   gtk_rc_parse_fontset		   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static gint	   gtk_rc_parse_state		   (GScanner	 *scanner,
						    GtkStateType *state);
static gint	   gtk_rc_parse_color		   (GScanner	 *scanner,
						    GdkColor	 *color);
static gint	   gtk_rc_parse_pixmap_path	   (GScanner	 *scanner);
static void	   gtk_rc_parse_pixmap_path_string (gchar *pix_path);
static char*	   gtk_rc_find_pixmap_in_path	   (GScanner	 *scanner,
						    gchar *pixmap_file);
static gint	   gtk_rc_parse_widget_style	   (GScanner	 *scanner);
static gint	   gtk_rc_parse_widget_class_style (GScanner	 *scanner);
static char*	   gtk_rc_widget_path		   (GtkWidget *widget);
static char*	   gtk_rc_widget_class_path	   (GtkWidget *widget);


static	GScannerConfig	gtk_rc_scanner_config =
{
  (
   " \t\n"
   )			/* cset_skip_characters */,
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   )			/* cset_identifier_first */,
  (
   G_CSET_a_2_z
   "_0123456789"
   G_CSET_A_2_Z
   G_CSET_LATINS
   G_CSET_LATINC
   )			/* cset_identifier_nth */,
  ( "#\n" )		/* cpair_comment_single */,
  
  TRUE			/* case_sensitive */,
  
  TRUE			/* skip_comment_multi */,
  TRUE			/* skip_comment_single */,
  TRUE			/* scan_comment_multi */,
  TRUE			/* scan_identifier */,
  FALSE			/* scan_identifier_1char */,
  FALSE			/* scan_identifier_NULL */,
  TRUE			/* scan_symbols */,
  TRUE			/* scan_binary */,
  TRUE			/* scan_octal */,
  TRUE			/* scan_float */,
  TRUE			/* scan_hex */,
  TRUE			/* scan_hex_dollar */,
  TRUE			/* scan_string_sq */,
  TRUE			/* scan_string_dq */,
  TRUE			/* numbers_2_int */,
  FALSE			/* int_2_float */,
  TRUE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  TRUE			/* symbol_2_token */,
};

static struct
{
  gchar *name;
  gint token;
} symbols[] = {
  { "include", TOKEN_INCLUDE },
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
static guint nsymbols = sizeof (symbols) / sizeof (symbols[0]);

static GHashTable *rc_style_ht = NULL;
static GSList *widget_sets = NULL;
static GSList *widget_class_sets = NULL;

#define GTK_RC_MAX_PIXMAP_PATHS 128
static gchar *pixmap_path[GTK_RC_MAX_PIXMAP_PATHS];

/* The files we have parsed, to reread later if necessary */
GSList *rc_files;

void
gtk_rc_init ()
{
  rc_style_ht = g_hash_table_new ((GHashFunc) gtk_rc_style_hash,
				  (GCompareFunc) gtk_rc_style_compare);
}

void
gtk_rc_parse_string (const gchar *rc_string)
{
  g_return_if_fail (rc_string != NULL);

  gtk_rc_parse_any ("-", -1, rc_string);
}

void
gtk_rc_parse (const gchar *filename)
{
  gint	   fd;

  g_return_if_fail (filename != NULL);

  rc_files = g_slist_append (rc_files, g_strdup (filename));

  fd = open (filename, O_RDONLY);
  if (fd < 0)
    return;

  gtk_rc_parse_any (filename, fd, NULL);

  close (fd);
}

void
gtk_rc_clear_hash_node (gpointer key, 
			gpointer data, 
			gpointer user_data)
{
  int i;
  GtkRcStyle *rc_style = data;
  GList *tmp_list;

  g_free (rc_style->name);
  g_free (rc_style->font_name);
  g_free (rc_style->fontset_name);

  for (i=0 ; i<5 ; i++)
    g_free (rc_style->bg_pixmap_name[i]);

  gtk_style_unref (rc_style->proto_style);

  tmp_list = rc_style->styles;
  while (tmp_list)
    {
      GtkRcNode *node = tmp_list->data;

      gdk_colormap_unref (node->cmap);
      gtk_style_unref (node->style);

      g_free (node);
      tmp_list = tmp_list->next;
    }

  g_free (rc_style);
}

void
gtk_rc_reparse_all (void)
{
  GSList *tmp_list, *tmp_files;
  GtkRcSet *rc_set;

  /* Clear out all old rc_styles */

  g_hash_table_foreach (rc_style_ht, gtk_rc_clear_hash_node, NULL);
  g_hash_table_destroy (rc_style_ht);
  rc_style_ht = NULL;

  tmp_list = widget_sets;
  while (tmp_list)
    {
      rc_set = (GtkRcSet *)tmp_list->data;
      g_free (rc_set->set);
      g_free (rc_set);
      
      tmp_list = tmp_list->next;
    }
  g_slist_free (widget_sets);
  widget_sets = NULL;

  tmp_list = widget_class_sets;
  while (tmp_list)
    {
      rc_set = (GtkRcSet *)tmp_list->data;
      g_free (rc_set->set);
      g_free (rc_set);
      
      tmp_list = tmp_list->next;
    }
  g_slist_free (widget_class_sets);
  widget_class_sets = NULL;

  /* Now read the RC's again */
  
  gtk_rc_init ();

  tmp_files = rc_files;
  rc_files = NULL;

  tmp_list = tmp_files;
  while (tmp_list)
    {
      gtk_rc_parse ((gchar *)tmp_list->data);
      g_free (tmp_list->data);
      
      tmp_list = tmp_list->next;
    }
  g_slist_free (tmp_files);
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
	      return gtk_rc_style_init (rc_style,
					gtk_widget_get_colormap (widget));
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
	      return gtk_rc_style_init (rc_style,
					gtk_widget_get_colormap (widget));
	    }
	}
    }
  
  return NULL;
}

void
gtk_rc_add_widget_name_style (GtkStyle	 *style,
			      const char *pattern)
{
  GtkRcStyle *rc_style;
  GtkRcSet *rc_set;
  int i;
  
  gtk_style_ref (style);
  
  rc_style = g_new (GtkRcStyle, 1);
  rc_style->name = NULL;
  rc_style->font_name = NULL;
  rc_style->fontset_name = NULL;
  
  for (i = 0; i < 5; i++)
    rc_style->bg_pixmap_name[i] = NULL;
  
  rc_style->styles = g_list_append (NULL, style);
  
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
  rc_style->name = NULL;
  rc_style->font_name = NULL;
  rc_style->fontset_name = NULL;
  
  for (i = 0; i < 5; i++)
    rc_style->bg_pixmap_name[i] = NULL;
  
  rc_style->styles = g_list_append (NULL, style);
  
  rc_set = g_new (GtkRcSet, 1);
  rc_set->set = g_strdup (pattern);
  rc_set->rc_style = rc_style;
  
  widget_class_sets = g_slist_append (widget_class_sets, rc_set);
}

static void
gtk_rc_parse_any (const gchar  *input_name,
		  gint		input_fd,
		  const gchar  *input_string)
{
  GScanner *scanner;
  guint	   i;
  gboolean done;
  
  scanner = g_scanner_new (&gtk_rc_scanner_config);
  
  if (input_fd >= 0)
    {
      g_assert (input_string == NULL);
      
      g_scanner_input_file (scanner, input_fd);
    }
  else
    {
      g_assert (input_string != NULL);
      
      g_scanner_input_text (scanner, input_string, strlen (input_string));
    }
  
  for (i = 0; i < nsymbols; i++)
    g_scanner_add_symbol (scanner, symbols[i].name, (gpointer) symbols[i].token);
  
  done = FALSE;
  while (!done)
    {
      gint return_val;

      return_val = gtk_rc_parse_statement (scanner);

      switch (return_val)
	{
	case PARSE_OK:
	  break;
	default:
	  if (scanner->next_token != G_TOKEN_NONE)
	    g_scanner_get_next_token (scanner);
	  
	  if (input_string)
	    g_warning ("rc string parse error: line %d",
		       scanner->line);
	  else
	    g_warning ("rc file parse error: \"%s\" line %d",
		       input_name,
		       scanner->line);
	  /* fall through */
	case PARSE_DONE:
	  done = TRUE;
	  break;
	}
    }
  g_scanner_destroy (scanner);
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
gtk_rc_styles_match (GSList	  *sets,
		     const char	  *path)
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

static GtkStyle *
gtk_rc_style_init (GtkRcStyle *rc_style, GdkColormap *cmap)
{
  GdkFont *old_font;
  gboolean match_cmap = FALSE;
  gint i;

  GList *tmp_list;
  GtkStyle *style = NULL;
  GtkRcNode *node;

  tmp_list = rc_style->styles;

  for (i=0; i<5; i++)
    if (rc_style->bg_pixmap_name[i])
      match_cmap = TRUE;
      
  while (tmp_list)
    {
      node = (GtkRcNode *)tmp_list->data;

      if (!match_cmap || (node->cmap == cmap))
	{
	  style = node->style;
	  break;
	}

      tmp_list = tmp_list->next;
    }
  
  if (!style)
    {
      node = g_new (GtkRcNode, 1);
      style = gtk_style_copy (rc_style->proto_style);

     /* FIXME, this leaks colormaps, but if we don't do this, then we'll
       * be screwed, because we identify colormaps by address equality
       */
      gdk_colormap_ref (cmap);
 
      node->style = style;
      node->cmap = cmap;
      
      if (rc_style->fontset_name)
	{
	  old_font = style->font;
	  style->font = gdk_fontset_load (rc_style->fontset_name);
	  if (style->font)
	    gdk_font_unref (old_font);
	  else
	    style->font = old_font;
	}
      else if (rc_style->font_name)
	{
	  old_font = style->font;
	  style->font = gdk_font_load (rc_style->font_name);
	  if (style->font)
	    gdk_font_unref (old_font);
	  else
	    style->font = old_font;
	}
      
      for (i = 0; i < 5; i++)
	if (rc_style->bg_pixmap_name[i])
	  {
	    if (strcmp (rc_style->bg_pixmap_name[i], "<parent>") == 0)
	      style->bg_pixmap[i] = (GdkPixmap*) GDK_PARENT_RELATIVE;
	    else
	      style->bg_pixmap[i] = 
		gdk_pixmap_colormap_create_from_xpm (NULL, cmap,
						     NULL,
						     &style->bg[i],
						     rc_style->bg_pixmap_name[i]);
	  }

      rc_style->styles = g_list_append (rc_style->styles, node);
    }

  return style;
}

static gint
gtk_rc_parse_statement (GScanner *scanner)
{
  gint token;
  gint error;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF)
    return PARSE_DONE;

  if (token == TOKEN_INCLUDE)
    {
      g_scanner_get_next_token (scanner);
      token = g_scanner_get_next_token (scanner);

      if (token != G_TOKEN_STRING)
	return PARSE_ERROR;

      gtk_rc_parse (scanner->value.v_string);

      return PARSE_OK;
    }
  
  error = gtk_rc_parse_style (scanner);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_pixmap_path (scanner);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_widget_style (scanner);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_widget_class_style (scanner);
  
  return error;
}

static gint
gtk_rc_parse_style (GScanner *scanner)
{
  GtkRcStyle *rc_style;
  GtkRcStyle *parent_style;
  gint token;
  gint error;
  gint insert;
  gint i;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_STYLE)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return PARSE_ERROR;
  
  insert = FALSE;
  rc_style = g_hash_table_lookup (rc_style_ht, scanner->value.v_string);
  
  if (!rc_style)
    {
      insert = TRUE;
      rc_style = g_new (GtkRcStyle, 1);
      rc_style->name = g_strdup (scanner->value.v_string);
      rc_style->font_name = NULL;
      rc_style->fontset_name = NULL;
      
      for (i = 0; i < 5; i++)
	rc_style->bg_pixmap_name[i] = NULL;

      rc_style->proto_style = gtk_style_new();
      rc_style->styles = NULL;
    }
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EQUAL_SIGN)
    {
      token = g_scanner_get_next_token (scanner);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
	{
	  if (insert)
	    {
	      gtk_style_unref (rc_style->proto_style);
	      g_free (rc_style);
	    }
	  return PARSE_ERROR;
	}
      
      parent_style = g_hash_table_lookup (rc_style_ht, scanner->value.v_string);
      if (parent_style)
	{
	  for (i = 0; i < 5; i++)
	    {
	      rc_style->proto_style->fg[i] = parent_style->proto_style->fg[i];
	      rc_style->proto_style->bg[i] = parent_style->proto_style->bg[i];
	      rc_style->proto_style->light[i] = parent_style->proto_style->light[i];
	      rc_style->proto_style->dark[i] = parent_style->proto_style->dark[i];
	      rc_style->proto_style->mid[i] = parent_style->proto_style->mid[i];
	      rc_style->proto_style->text[i] = parent_style->proto_style->text[i];
	      rc_style->proto_style->base[i] = parent_style->proto_style->base[i];
	    }
	  
	  rc_style->proto_style->black = parent_style->proto_style->black;
	  rc_style->proto_style->white = parent_style->proto_style->white;
	  
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
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    {
      if (insert)
	{
	  gtk_style_unref (rc_style->proto_style);
	  g_free (rc_style);
	}
      return PARSE_ERROR;
    }
  
  while (1)
    {
      error = gtk_rc_parse_style_option (scanner, rc_style);
      if (error == PARSE_SYNTAX)
	break;
      if (error == PARSE_ERROR)
	{
	  if (insert)
	    {
	      gtk_style_unref (rc_style->proto_style);
	      g_free (rc_style);
	    }
	  return error;
	}
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
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
	  
	  gtk_style_unref (rc_style->proto_style);
	  g_free (rc_style);
	}
      return PARSE_ERROR;
    }
  
  if (insert)
    g_hash_table_insert (rc_style_ht, rc_style->name, rc_style);
  
  return PARSE_OK;
}

static gint
gtk_rc_parse_style_option (GScanner   *scanner,
			   GtkRcStyle *rc_style)
{
  gint token;
  gint error;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  
  error = gtk_rc_parse_base (scanner, rc_style->proto_style);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_bg (scanner, rc_style->proto_style);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_fg (scanner, rc_style->proto_style);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_text (scanner, rc_style->proto_style);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_bg_pixmap (scanner, rc_style);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_font (scanner, rc_style);
  if (error != PARSE_SYNTAX)
    return error;
  
  error = gtk_rc_parse_fontset (scanner, rc_style);
  
  return error;
}

static gint
gtk_rc_parse_base (GScanner *scanner,
		   GtkStyle *style)
{
  GtkStateType state;
  gint token;
  gint error;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_BASE)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  error = gtk_rc_parse_state (scanner, &state);
  if (error != PARSE_OK)
    return error;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return PARSE_ERROR;
  
  error = gtk_rc_parse_color (scanner, &style->base[state]);
  
  return error;
}

static gint
gtk_rc_parse_bg (GScanner *scanner,
		 GtkStyle *style)
{
  GtkStateType state;
  gint token;
  gint error;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_BG)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  error = gtk_rc_parse_state (scanner, &state);
  if (error != PARSE_OK)
    return error;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return PARSE_ERROR;
  
  error = gtk_rc_parse_color (scanner, &style->bg[state]);
  
  return error;
}

static gint
gtk_rc_parse_fg (GScanner *scanner,
		 GtkStyle *style)
{
  GtkStateType state;
  gint token;
  gint error;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_FG)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  error = gtk_rc_parse_state (scanner, &state);
  if (error != PARSE_OK)
    return error;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return PARSE_ERROR;
  
  error = gtk_rc_parse_color (scanner, &style->fg[state]);
  
  return error;
}

static gint
gtk_rc_parse_text (GScanner *scanner,
		   GtkStyle *style)
{
  GtkStateType state;
  gint token;
  gint error;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_TEXT)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  error = gtk_rc_parse_state (scanner, &state);
  if (error != PARSE_OK)
    return error;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return PARSE_ERROR;
  
  error = gtk_rc_parse_color (scanner, &style->text[state]);
  
  return error;
}

static gint
gtk_rc_parse_bg_pixmap (GScanner   *scanner,
			GtkRcStyle *rc_style)
{
  GtkStateType state;
  gint token;
  gint error;
  gchar *pixmap_file;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_BG_PIXMAP)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  error = gtk_rc_parse_state (scanner, &state);
  if (error != PARSE_OK)
    return error;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return PARSE_ERROR;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return PARSE_ERROR;
  
  if (strcmp (scanner->value.v_string, "<parent>"))
    pixmap_file = gtk_rc_find_pixmap_in_path (scanner, scanner->value.v_string);
  else
    pixmap_file = g_strdup (scanner->value.v_string);
  
  if (pixmap_file)
    {
      if (rc_style->bg_pixmap_name[state])
	g_free (rc_style->bg_pixmap_name[state]);
      rc_style->bg_pixmap_name[state] = pixmap_file;
    }
  
  return PARSE_OK;
}

static char*
gtk_rc_find_pixmap_in_path (GScanner *scanner,
			    gchar    *pixmap_file)
{
  gint i;
  gint fd;
  gchar *buf;
  
  for (i = 0; (i < GTK_RC_MAX_PIXMAP_PATHS) && (pixmap_path[i] != NULL); i++)
    {
      buf = g_malloc (strlen (pixmap_path[i]) + strlen (pixmap_file) + 2);
      sprintf (buf, "%s%c%s", pixmap_path[i], '/', pixmap_file);
      
      fd = open (buf, O_RDONLY);
      if (fd >= 0)
	{
	  close (fd);
	  return buf;
	}
      
      g_free (buf);
    }
  
  g_warning ("Unable to locate image file in pixmap_path: \"%s\" line %d",
	     pixmap_file, scanner->line);
  
  return NULL;
}

static gint
gtk_rc_parse_font (GScanner   *scanner,
		   GtkRcStyle *rc_style)
{
  gint token;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_FONT)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return PARSE_ERROR;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return PARSE_ERROR;
  
  if (rc_style->font_name)
    g_free (rc_style->font_name);
  rc_style->font_name = g_strdup (scanner->value.v_string);
  
  return PARSE_OK;
}

static gint
gtk_rc_parse_fontset (GScanner	 *scanner,
		      GtkRcStyle *rc_style)
{
  gint token;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_FONTSET)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return PARSE_ERROR;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return PARSE_ERROR;
  
  if (rc_style->fontset_name)
    g_free (rc_style->fontset_name);
  rc_style->fontset_name = g_strdup (scanner->value.v_string);
  
  return PARSE_OK;
}

static gint
gtk_rc_parse_state (GScanner	 *scanner,
		    GtkStateType *state)
{
  gint token;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != G_TOKEN_LEFT_BRACE)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  token = g_scanner_get_next_token (scanner);
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
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    return PARSE_ERROR;
  
  return PARSE_OK;
}

static gint
gtk_rc_parse_color (GScanner *scanner,
		    GdkColor *color)
{
  gint token;
  gint token_int;
  gint length;
  gint temp;
  gchar buf[9];
  gint i, j;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  
  switch (token)
    {
    case G_TOKEN_LEFT_CURLY:
      token = g_scanner_get_next_token (scanner);
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return PARSE_ERROR;
      color->red = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return PARSE_ERROR;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return PARSE_ERROR;
      color->green = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return PARSE_ERROR;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return PARSE_ERROR;
      color->blue = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_RIGHT_CURLY)
	return PARSE_ERROR;
      break;
      
    case G_TOKEN_STRING:
      token = g_scanner_get_next_token (scanner);
      
      if (scanner->value.v_string[0] != '#')
	return PARSE_ERROR;
      
      length = strlen (scanner->value.v_string) - 1;
      if (((length % 3) != 0) || (length > 12))
	return PARSE_ERROR;
      length /= 3;
      
      for (i = 0, j = 1; i < length; i++, j++)
	buf[i] = scanner->value.v_string[j];
      buf[i] = '\0';
      
      sscanf (buf, "%x", &temp);
      color->red = temp;
      
      for (i = 0; i < length; i++, j++)
	buf[i] = scanner->value.v_string[j];
      buf[i] = '\0';
      
      sscanf (buf, "%x", &temp);
      color->green = temp;
      
      for (i = 0; i < length; i++, j++)
	buf[i] = scanner->value.v_string[j];
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
      
    case G_TOKEN_ERROR:
      return PARSE_ERROR;
      
    default:
      return PARSE_SYNTAX;
    }
  
  return PARSE_OK;
}

static gint
gtk_rc_parse_pixmap_path (GScanner *scanner)
{
  gint token;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_PIXMAP_PATH)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  token = g_scanner_get_next_token (scanner);
  
  if (token != G_TOKEN_STRING)
    return PARSE_ERROR;
  
  gtk_rc_parse_pixmap_path_string (scanner->value.v_string);
  
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
gtk_rc_parse_widget_style (GScanner *scanner)
{
  GtkRcSet *rc_set;
  gint token;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_WIDGET)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return PARSE_ERROR;
  
  rc_set = g_new (GtkRcSet, 1);
  rc_set->set = g_strdup (scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_STYLE)
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }
  
  rc_set->rc_style = gtk_rc_style_find (scanner->value.v_string);
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
gtk_rc_parse_widget_class_style (GScanner *scanner)
{
  GtkRcSet *rc_set;
  gint token;
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EOF || token == G_TOKEN_ERROR)
    return PARSE_ERROR;
  if (token != TOKEN_WIDGET_CLASS)
    return PARSE_SYNTAX;
  token = g_scanner_get_next_token (scanner);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return PARSE_ERROR;
  
  rc_set = g_new (GtkRcSet, 1);
  rc_set->set = g_strdup (scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_STYLE)
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    {
      g_free (rc_set->set);
      g_free (rc_set);
      return PARSE_ERROR;
    }
  
  rc_set->rc_style = gtk_rc_style_find (scanner->value.v_string);
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
