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
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "gtkrc.h"
#include "gtkbindings.h"


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
  TOKEN_WIDGET_CLASS,
  TOKEN_LAST
};

typedef struct _GtkRcStyle  GtkRcStyle;
typedef struct _GtkRcSet    GtkRcSet;
typedef struct _GtkRcNode   GtkRcNode;
typedef struct _GtkRcFile   GtkRcFile;

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
  GtkPatternSpec pspec;
  GtkRcStyle	*rc_style;
};

struct _GtkRcFile
{
  time_t mtime;
  gchar *name;
  gchar *canonical_name;
  gboolean reload;
};

static guint	   gtk_rc_style_hash		   (const char   *name);
static gint	   gtk_rc_style_compare		   (const char   *a,
						    const char   *b);
static GtkRcStyle* gtk_rc_style_find		   (const char   *name);
static GtkRcStyle* gtk_rc_styles_match		   (GSList       *sets,
						    guint	  path_length,
						    gchar	 *path,
						    gchar	 *path_reversed);
static GtkStyle*   gtk_rc_style_init		   (GtkRcStyle   *rc_style,
						    GdkColormap  *cmap);
static void        gtk_rc_parse_file               (const gchar  *filename,
						    gboolean      reload);

static void	   gtk_rc_parse_any		   (const gchar  *input_name,
						    gint	  input_fd,
						    const gchar  *input_string);
static guint	   gtk_rc_parse_statement	   (GScanner	 *scanner);
static guint	   gtk_rc_parse_style		   (GScanner	 *scanner);
static guint	   gtk_rc_parse_base		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static guint	   gtk_rc_parse_bg		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static guint	   gtk_rc_parse_fg		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static guint	   gtk_rc_parse_text		   (GScanner	 *scanner,
						    GtkStyle	 *style);
static guint	   gtk_rc_parse_bg_pixmap	   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static guint	   gtk_rc_parse_font		   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static guint	   gtk_rc_parse_fontset		   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static guint	   gtk_rc_parse_state		   (GScanner	 *scanner,
						    GtkStateType *state);
static guint	   gtk_rc_parse_color		   (GScanner	 *scanner,
						    GdkColor	 *color);
static guint	   gtk_rc_parse_pixmap_path	   (GScanner	 *scanner);
static void	   gtk_rc_parse_pixmap_path_string (gchar *pix_path);
static char*	   gtk_rc_find_pixmap_in_path	   (GScanner	 *scanner,
						    gchar *pixmap_file);
static guint	   gtk_rc_parse_widget_style	   (GScanner	 *scanner);
static guint	   gtk_rc_parse_widget_class_style (GScanner	 *scanner);
static void        gtk_rc_clear_hash_node          (gpointer   key, 
						    gpointer   data, 
						    gpointer   user_data);
static void        gtk_rc_clear_styles             (void);



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
  guint token;
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
static GSList *gtk_rc_sets_widget = NULL;
static GSList *gtk_rc_sets_widget_class = NULL;

#define GTK_RC_MAX_PIXMAP_PATHS 128
static gchar *pixmap_path[GTK_RC_MAX_PIXMAP_PATHS];

/* The files we have parsed, to reread later if necessary */
GSList *rc_files = NULL;

static GtkImageLoader image_loader = NULL;

void
gtk_rc_init (void)
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

static void
gtk_rc_parse_file (const gchar *filename, gboolean reload)
{
  GtkRcFile *rc_file = NULL;
  struct stat statbuf;
  GSList *tmp_list;

  g_return_if_fail (filename != NULL);

  tmp_list = rc_files;
  while (tmp_list)
    {
      rc_file = tmp_list->data;
      if (!strcmp (rc_file->name, filename))
	break;
      
      tmp_list = tmp_list->next;
    }

  if (!tmp_list)
    {
      rc_file = g_new (GtkRcFile, 1);
      rc_file->name = g_strdup (filename);
      rc_file->canonical_name = NULL;
      rc_file->mtime = 0;
      rc_file->reload = reload;

      rc_files = g_slist_append (rc_files, rc_file);
    }

  if (!rc_file->canonical_name)
    {
      /* Get the absolute pathname */

      if (rc_file->name[0] == '/')
	rc_file->canonical_name = rc_file->name;
      else
	{
	  GString *str;
	  gchar buffer[MAXPATHLEN];
	  
#if defined(sun) && !defined(__SVR4)
	  if(!getwd(buffer))
#else
	  if(!getcwd(buffer, MAXPATHLEN))
#endif    
	      return;

	  str = g_string_new (buffer);
	  g_string_append_c (str, '/');
	  g_string_append (str, rc_file->name);
	  
	  rc_file->canonical_name = str->str;
	  g_string_free (str, FALSE);
	}
    }

  if (!lstat (rc_file->canonical_name, &statbuf))
    {
      gint fd;

      rc_file->mtime = statbuf.st_mtime;

      fd = open (rc_file->canonical_name, O_RDONLY);
      if (fd < 0)
	return;

      gtk_rc_parse_any (filename, fd, NULL);

      close (fd);
    }
}

void
gtk_rc_parse (const gchar *filename)
{
  g_return_if_fail (filename != NULL);

  gtk_rc_parse_file (filename, TRUE);
}

static void
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

static void
gtk_rc_clear_styles (void)
{
  GSList *tmp_list;

  /* Clear out all old rc_styles */

  g_hash_table_foreach (rc_style_ht, gtk_rc_clear_hash_node, NULL);
  g_hash_table_destroy (rc_style_ht);
  rc_style_ht = NULL;

  tmp_list = gtk_rc_sets_widget;
  while (tmp_list)
    {
      GtkRcSet *rc_set;

      rc_set = tmp_list->data;
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      
      tmp_list = tmp_list->next;
    }
  g_slist_free (gtk_rc_sets_widget);
  gtk_rc_sets_widget = NULL;

  tmp_list = gtk_rc_sets_widget_class;
  while (tmp_list)
    {
      GtkRcSet *rc_set;

      rc_set = tmp_list->data;
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      
      tmp_list = tmp_list->next;
    }
  g_slist_free (gtk_rc_sets_widget_class);
  gtk_rc_sets_widget_class = NULL;

  gtk_rc_init ();
}

gboolean
gtk_rc_reparse_all (void)
{
  GSList *tmp_list;
  gboolean mtime_modified = FALSE;
  GtkRcFile *rc_file;

  struct stat statbuf;

  /* Check through and see if any of the RC's have had their
   * mtime modified. If so, reparse everything.
   */
  tmp_list = rc_files;
  while (tmp_list)
    {
      rc_file = tmp_list->data;
      
      if (!lstat (rc_file->name, &statbuf) && 
	  (statbuf.st_mtime > rc_file->mtime))
	{
	  mtime_modified = TRUE;
	  break;
	}
      
      tmp_list = tmp_list->next;
    }

  if (mtime_modified)
    {
      gtk_rc_clear_styles();

      tmp_list = rc_files;
      while (tmp_list)
	{
	  rc_file = tmp_list->data;
	  if (rc_file->reload)
	    gtk_rc_parse_file (rc_file->name, FALSE);
	  
	  tmp_list = tmp_list->next;
	}
    }

  return mtime_modified;
}

GtkStyle*
gtk_rc_get_style (GtkWidget *widget)
{
  GtkRcStyle *rc_style;
  
  if (gtk_rc_sets_widget)
    {
      gchar *path, *path_reversed;
      guint path_length;

      gtk_widget_path (widget, &path_length, &path, &path_reversed);
      rc_style = gtk_rc_styles_match (gtk_rc_sets_widget, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
      
      if (rc_style)
	return gtk_rc_style_init (rc_style, gtk_widget_get_colormap (widget));
    }
  
  if (gtk_rc_sets_widget_class)
    {
      gchar *path, *path_reversed;
      guint path_length;

      gtk_widget_class_path (widget, &path_length, &path, &path_reversed);
      rc_style = gtk_rc_styles_match (gtk_rc_sets_widget_class, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
      
      if (rc_style)
	return gtk_rc_style_init (rc_style, gtk_widget_get_colormap (widget));
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
  gtk_pattern_spec_init (&rc_set->pspec, pattern);
  rc_set->rc_style = rc_style;
  
  gtk_rc_sets_widget = g_slist_prepend (gtk_rc_sets_widget, rc_set);
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
  gtk_pattern_spec_init (&rc_set->pspec, pattern);
  rc_set->rc_style = rc_style;
  
  gtk_rc_sets_widget_class = g_slist_prepend (gtk_rc_sets_widget_class, rc_set);
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
  scanner->input_name = input_name;
  
  g_scanner_freeze_symbol_table (scanner);
  for (i = 0; i < nsymbols; i++)
    g_scanner_add_symbol (scanner, symbols[i].name, GINT_TO_POINTER (symbols[i].token));
  g_scanner_thaw_symbol_table (scanner);
  
  done = FALSE;
  while (!done)
    {
      if (g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
	done = TRUE;
      else
	{
	  guint expected_token;
	  
	  expected_token = gtk_rc_parse_statement (scanner);

	  if (expected_token != G_TOKEN_NONE)
	    {
	      gchar *symbol_name;
	      gchar *msg;
	      
	      msg = NULL;
	      if (expected_token > TOKEN_INVALID &&
		  expected_token < TOKEN_LAST)
		{
		  for (i = 0; i < nsymbols; i++)
		    if (symbols[i].token == expected_token)
		      msg = symbols[i].name;
		  if (msg)
		    msg = g_strconcat ("e.g. `", msg, "'", NULL);
		}
	      if (scanner->token > TOKEN_INVALID &&
		  scanner->token < TOKEN_LAST)
		{
		  symbol_name = "???";
		  for (i = 0; i < nsymbols; i++)
		    if (symbols[i].token == scanner->token)
		      symbol_name = symbols[i].name;
		}
	      else
		symbol_name = NULL;
	      g_scanner_unexp_token (scanner,
				     expected_token,
				     NULL,
				     "keyword",
				     symbol_name,
				     msg,
				     TRUE);
	      g_free (msg);
	      done = TRUE;
	    }
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
		     guint         path_length,
		     gchar        *path,
		     gchar        *path_reversed)
{
  GtkRcSet *rc_set;

  while (sets)
    {
      rc_set = sets->data;
      sets = sets->next;

      if (gtk_pattern_match (&rc_set->pspec, path_length, path, path_reversed))
	return rc_set->rc_style;
    }
  
  return NULL;
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
	      {
		if(image_loader)
		  style->bg_pixmap[i] = image_loader(NULL, cmap, NULL,
						     &style->bg[i],
						     rc_style->bg_pixmap_name[i]);
		else
	          style->bg_pixmap[i] = 
		    gdk_pixmap_colormap_create_from_xpm (NULL, cmap,
						         NULL,
						         &style->bg[i],
						         rc_style->bg_pixmap_name[i]);
	      }
	  }

      rc_style->styles = g_list_append (rc_style->styles, node);
    }

  return style;
}

static guint
gtk_rc_parse_statement (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_peek_next_token (scanner);

  switch (token)
    {
    case TOKEN_INCLUDE:
      token = g_scanner_get_next_token (scanner);
      if (token != TOKEN_INCLUDE)
	return TOKEN_INCLUDE;

      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
	return G_TOKEN_STRING;

      gtk_rc_parse_file (scanner->value.v_string, FALSE);
      return G_TOKEN_NONE;

    case TOKEN_STYLE:
      return gtk_rc_parse_style (scanner);

    case TOKEN_PIXMAP_PATH:
      return gtk_rc_parse_pixmap_path (scanner);

    case TOKEN_WIDGET:
      return gtk_rc_parse_widget_style (scanner);

    case TOKEN_WIDGET_CLASS:
      return gtk_rc_parse_widget_class_style (scanner);

    default:
      g_scanner_get_next_token (scanner);
      return /* G_TOKEN_SYMBOL */ TOKEN_STYLE;
    }
}

static guint
gtk_rc_parse_style (GScanner *scanner)
{
  GtkRcStyle *rc_style;
  GtkRcStyle *parent_style;
  guint token;
  gint insert;
  gint i;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_STYLE)
    return TOKEN_STYLE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
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
	  return G_TOKEN_STRING;
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
      return G_TOKEN_LEFT_CURLY;
    }
  
  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_BASE:
	  token = gtk_rc_parse_base (scanner, rc_style->proto_style);
	  break;
	case TOKEN_BG:
	  token = gtk_rc_parse_bg (scanner, rc_style->proto_style);
	  break;
	case TOKEN_FG:
	  token = gtk_rc_parse_fg (scanner, rc_style->proto_style);
	  break;
	case TOKEN_TEXT:
	  token = gtk_rc_parse_text (scanner, rc_style->proto_style);
	  break;
	case TOKEN_BG_PIXMAP:
	  token = gtk_rc_parse_bg_pixmap (scanner, rc_style);
	  break;
	case TOKEN_FONT:
	  token = gtk_rc_parse_font (scanner, rc_style);
	  break;
	case TOKEN_FONTSET:
	  token = gtk_rc_parse_fontset (scanner, rc_style);
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	{
	  if (insert)
	    {
	      if (rc_style->fontset_name)
		g_free (rc_style->fontset_name);
	      if (rc_style->font_name)
		g_free (rc_style->font_name);
	      for (i = 0; i < 5; i++)
		if (rc_style->bg_pixmap_name[i])
		  g_free (rc_style->bg_pixmap_name[i]);
	      gtk_style_unref (rc_style->proto_style);
	      g_free (rc_style);
	    }
	  return token;
	}
      token = g_scanner_peek_next_token (scanner);
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    {
      if (insert)
	{
	  if (rc_style->fontset_name)
	    g_free (rc_style->fontset_name);
	  if (rc_style->font_name)
	    g_free (rc_style->font_name);
	  
	  for (i = 0; i < 5; i++)
	    if (rc_style->bg_pixmap_name[i])
	      g_free (rc_style->bg_pixmap_name[i]);
	  
	  gtk_style_unref (rc_style->proto_style);
	  g_free (rc_style);
	}
      return G_TOKEN_RIGHT_CURLY;
    }
  
  if (insert)
    g_hash_table_insert (rc_style_ht, rc_style->name, rc_style);
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_base (GScanner *scanner,
		   GtkStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_BASE)
    return TOKEN_BASE;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  return gtk_rc_parse_color (scanner, &style->base[state]);
}

static guint
gtk_rc_parse_bg (GScanner *scanner,
		 GtkStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_BG)
    return TOKEN_BG;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  return gtk_rc_parse_color (scanner, &style->bg[state]);
}

static guint
gtk_rc_parse_fg (GScanner *scanner,
		 GtkStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_FG)
    return TOKEN_FG;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  return gtk_rc_parse_color (scanner, &style->fg[state]);
}

static guint
gtk_rc_parse_text (GScanner *scanner,
		   GtkStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_TEXT)
    return TOKEN_TEXT;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  return gtk_rc_parse_color (scanner, &style->text[state]);
}

static guint
gtk_rc_parse_bg_pixmap (GScanner   *scanner,
			GtkRcStyle *rc_style)
{
  GtkStateType state;
  guint token;
  gchar *pixmap_file;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_BG_PIXMAP)
    return TOKEN_BG_PIXMAP;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
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
  
  return G_TOKEN_NONE;
}

static gchar*
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

static guint
gtk_rc_parse_font (GScanner   *scanner,
		   GtkRcStyle *rc_style)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_FONT)
    return TOKEN_FONT;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  if (rc_style->font_name)
    g_free (rc_style->font_name);
  rc_style->font_name = g_strdup (scanner->value.v_string);
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_fontset (GScanner	 *scanner,
		      GtkRcStyle *rc_style)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_FONTSET)
    return TOKEN_FONTSET;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  if (rc_style->fontset_name)
    g_free (rc_style->fontset_name);
  rc_style->fontset_name = g_strdup (scanner->value.v_string);
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_state (GScanner	 *scanner,
		    GtkStateType *state)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_BRACE)
    return G_TOKEN_LEFT_BRACE;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case TOKEN_ACTIVE:
      *state = GTK_STATE_ACTIVE;
      break;
    case TOKEN_INSENSITIVE:
      *state = GTK_STATE_INSENSITIVE;
      break;
    case TOKEN_NORMAL:
      *state = GTK_STATE_NORMAL;
      break;
    case TOKEN_PRELIGHT:
      *state = GTK_STATE_PRELIGHT;
      break;
    case TOKEN_SELECTED:
      *state = GTK_STATE_SELECTED;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ TOKEN_NORMAL;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    return G_TOKEN_RIGHT_BRACE;
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_color (GScanner *scanner,
		    GdkColor *color)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
      gint token_int;
      gint length;
      gint temp;
      gchar buf[9];
      gint i, j;
      
    case G_TOKEN_LEFT_CURLY:
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->red = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->green = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->blue = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_RIGHT_CURLY)
	return G_TOKEN_RIGHT_CURLY;
      return G_TOKEN_NONE;
      
    case G_TOKEN_STRING:
      if (scanner->value.v_string[0] != '#')
	return G_TOKEN_STRING;
      
      length = strlen (scanner->value.v_string) - 1;
      if (((length % 3) != 0) || (length > 12))
	return G_TOKEN_STRING;
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
      return G_TOKEN_NONE;
      
    default:
      return G_TOKEN_STRING;
    }
}

static guint
gtk_rc_parse_pixmap_path (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_PIXMAP_PATH)
    return TOKEN_PIXMAP_PATH;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  gtk_rc_parse_pixmap_path_string (scanner->value.v_string);
  
  return G_TOKEN_NONE;
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

static guint
gtk_rc_parse_widget_style (GScanner *scanner)
{
  GtkRcSet *rc_set;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_WIDGET)
    return TOKEN_WIDGET;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  rc_set = g_new (GtkRcSet, 1);
  gtk_pattern_spec_init (&rc_set->pspec, scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_STYLE)
    {
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      return TOKEN_STYLE;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    {
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      return G_TOKEN_STRING;
    }
  
  rc_set->rc_style = gtk_rc_style_find (scanner->value.v_string);
  if (!rc_set->rc_style)
    {
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      return G_TOKEN_STRING;
    }
  
  gtk_rc_sets_widget = g_slist_prepend (gtk_rc_sets_widget, rc_set);
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_widget_class_style (GScanner *scanner)
{
  GtkRcSet *rc_set;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_WIDGET_CLASS)
    return TOKEN_WIDGET_CLASS;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  rc_set = g_new (GtkRcSet, 1);
  gtk_pattern_spec_init (&rc_set->pspec, scanner->value.v_string);

  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_STYLE)
    {
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      return G_TOKEN_STRING;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    {
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      return G_TOKEN_STRING;
    }
  
  rc_set->rc_style = gtk_rc_style_find (scanner->value.v_string);
  if (!rc_set->rc_style)
    {
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);
      return G_TOKEN_STRING;
    }
  
  gtk_rc_sets_widget_class = g_slist_prepend (gtk_rc_sets_widget_class, rc_set);
  
  return G_TOKEN_NONE;
}

/*
typedef  GdkPixmap * (*GtkImageLoader) (GdkWindow   *window,
                                        GdkColormap *colormap,
                                        GdkBitmap  **mask,
                                        GdkColor    *transparent_color,
                                        const gchar *filename);
*/

void
gtk_rc_set_image_loader(GtkImageLoader loader)
{
	image_loader = loader;
}

