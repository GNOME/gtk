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
#include "gtkthemes.h"

typedef struct _GtkRcSet    GtkRcSet;
typedef struct _GtkRcNode   GtkRcNode;
typedef struct _GtkRcFile   GtkRcFile;

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
static guint	   gtk_rc_styles_hash		   (const GSList *rc_styles);
static gint	   gtk_rc_styles_compare	   (const GSList *a,
						    const GSList *b);
static GtkRcStyle* gtk_rc_style_find		   (const char   *name);
static GSList *    gtk_rc_styles_match             (GSList       *rc_styles,
						    GSList       *sets,
						    guint         path_length,
						    gchar        *path,
						    gchar        *path_reversed);
static GtkStyle *  gtk_rc_style_to_style           (GtkRcStyle   *rc_style);
static GtkStyle*   gtk_rc_style_init		   (GSList       *rc_styles);
static void        gtk_rc_parse_file               (const gchar  *filename,
						    gboolean      reload);

static void	   gtk_rc_parse_any		   (const gchar  *input_name,
						    gint	  input_fd,
						    const gchar  *input_string);
static guint	   gtk_rc_parse_statement	   (GScanner	 *scanner);
static guint	   gtk_rc_parse_style		   (GScanner	 *scanner);
static guint	   gtk_rc_parse_base		   (GScanner	 *scanner,
						    GtkRcStyle	 *style);
static guint	   gtk_rc_parse_bg		   (GScanner	 *scanner,
						    GtkRcStyle	 *style);
static guint	   gtk_rc_parse_fg		   (GScanner	 *scanner,
						    GtkRcStyle	 *style);
static guint	   gtk_rc_parse_text		   (GScanner	 *scanner,
						    GtkRcStyle	 *style);
static guint	   gtk_rc_parse_bg_pixmap	   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static guint	   gtk_rc_parse_font		   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static guint	   gtk_rc_parse_fontset		   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static guint	   gtk_rc_parse_engine		   (GScanner	 *scanner,
						    GtkRcStyle	 *rc_style);
static guint	   gtk_rc_parse_pixmap_path	   (GScanner	 *scanner);
static void	   gtk_rc_parse_pixmap_path_string (gchar *pix_path);
static guint	   gtk_rc_parse_module_path	   (GScanner	 *scanner);
static void	   gtk_rc_parse_module_path_string (gchar *mod_path);
static guint	   gtk_rc_parse_path_pattern	   (GScanner     *scanner);
static void        gtk_rc_clear_hash_node          (gpointer   key, 
						    gpointer   data, 
						    gpointer   user_data);
static void        gtk_rc_clear_styles               (void);
static void        gtk_rc_append_default_pixmap_path (void);
static void        gtk_rc_append_default_module_path (void);
static void        gtk_rc_append_pixmap_path         (gchar *dir);


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
   "_-0123456789"
   G_CSET_A_2_Z
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
  FALSE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  TRUE			/* symbol_2_token */,
  FALSE			/* scope_0_fallback */,
};

static struct
{
  gchar *name;
  guint token;
} symbols[] = {
  { "include", GTK_RC_TOKEN_INCLUDE },
  { "NORMAL", GTK_RC_TOKEN_NORMAL },
  { "ACTIVE", GTK_RC_TOKEN_ACTIVE },
  { "PRELIGHT", GTK_RC_TOKEN_PRELIGHT },
  { "SELECTED", GTK_RC_TOKEN_SELECTED },
  { "INSENSITIVE", GTK_RC_TOKEN_INSENSITIVE },
  { "fg", GTK_RC_TOKEN_FG },
  { "bg", GTK_RC_TOKEN_BG },
  { "base", GTK_RC_TOKEN_BASE },
  { "text", GTK_RC_TOKEN_TEXT },
  { "font", GTK_RC_TOKEN_FONT },
  { "fontset", GTK_RC_TOKEN_FONTSET },
  { "bg_pixmap", GTK_RC_TOKEN_BG_PIXMAP },
  { "pixmap_path", GTK_RC_TOKEN_PIXMAP_PATH },
  { "style", GTK_RC_TOKEN_STYLE },
  { "binding", GTK_RC_TOKEN_BINDING },
  { "bind", GTK_RC_TOKEN_BIND },
  { "widget", GTK_RC_TOKEN_WIDGET },
  { "widget_class", GTK_RC_TOKEN_WIDGET_CLASS },
  { "class", GTK_RC_TOKEN_CLASS },
  { "lowest", GTK_RC_TOKEN_LOWEST },
  { "gtk", GTK_RC_TOKEN_GTK },
  { "application", GTK_RC_TOKEN_APPLICATION },
  { "rc", GTK_RC_TOKEN_RC },
  { "highest", GTK_RC_TOKEN_HIGHEST },
  { "engine", GTK_RC_TOKEN_ENGINE },
  { "module_path", GTK_RC_TOKEN_MODULE_PATH },
};

static guint n_symbols = sizeof (symbols) / sizeof (symbols[0]);

static GHashTable *rc_style_ht = NULL;
static GHashTable *realized_style_ht = NULL;
static GSList *gtk_rc_sets_widget = NULL;
static GSList *gtk_rc_sets_widget_class = NULL;
static GSList *gtk_rc_sets_class = NULL;

#define GTK_RC_MAX_PIXMAP_PATHS 128
static gchar *pixmap_path[GTK_RC_MAX_PIXMAP_PATHS];
#define GTK_RC_MAX_MODULE_PATHS 128
static gchar *module_path[GTK_RC_MAX_MODULE_PATHS];

/* The files we have parsed, to reread later if necessary */
GSList *rc_files = NULL;

static GtkImageLoader image_loader = NULL;

/* RC file handling */


gchar *
gtk_rc_get_theme_dir(void)
{
  gchar *var, *path;

  var = getenv("GTK_DATA_PREFIX");
  if (var)
    {
      path = g_malloc(strlen(var) + strlen("/share/themes") +1);
      sprintf(path, "%s%s", var, "/share/themes");
    }
  else
    {
      path = g_malloc(strlen(GTK_DATA_PREFIX) + strlen("/share/themes") +1);
      sprintf(path, "%s%s", GTK_DATA_PREFIX, "/share/themes");
    }
  return path;
}

gchar *
gtk_rc_get_module_dir(void)
{
  gchar *var, *path;

  var = getenv("GTK_EXE_PREFIX");
  if (var)
    {
      path = g_malloc(strlen(var) + strlen("/lib/gtk/themes/engines") +1);
      sprintf(path, "%s%s", var, "/lib/gtk/themes/engines");
    }
  else
    {
      path = g_malloc(strlen(GTK_EXE_PREFIX) + strlen("/lib/gtk/themes/engines") +1);
      sprintf(path, "%s%s", GTK_EXE_PREFIX, "/lib/gtk/themes/engines");
    }
  return path;
}

static void
gtk_rc_append_default_pixmap_path(void)
{
  gchar *var, *path;
  gint n;

  var = getenv("GTK_DATA_PREFIX");
  if (var)
    {
      path = g_malloc(strlen(var) + strlen("/share/gtk/themes") +1);
      sprintf(path, "%s%s", var, "/share/gtk/themes");
    }
  else
    {
      path = g_malloc(strlen(GTK_DATA_PREFIX) + strlen("/share/gtk/themes") +1);
      sprintf(path, "%s%s", GTK_DATA_PREFIX, "/share/gtk/themes");
    }
  
  for (n = 0; pixmap_path[n]; n++) ;
  if (n >= GTK_RC_MAX_PIXMAP_PATHS - 1)
    return;
  pixmap_path[n++] = g_strdup(path);
  pixmap_path[n] = NULL;
  g_free(path);
}

static void
gtk_rc_append_pixmap_path(gchar *dir)
{
  gint n;

  for (n = 0; pixmap_path[n]; n++) ;
  if (n >= GTK_RC_MAX_MODULE_PATHS - 1)
    return;
  pixmap_path[n++] = g_strdup(dir);
  pixmap_path[n] = NULL;
}

static void
gtk_rc_append_default_module_path(void)
{
  gchar *var, *path;
  gint n;

  for (n = 0; module_path[n]; n++) ;
  if (n >= GTK_RC_MAX_MODULE_PATHS - 1)
    return;
  
  var = getenv("GTK_EXE_PREFIX");
  if (var)
    {
      path = g_malloc(strlen(var) + strlen("/lib/gtk/themes/engines") +1);
      sprintf(path, "%s%s", var, "/lib/gtk/themes/engines");
    }
  else
    {
      path = g_malloc(strlen(GTK_EXE_PREFIX) + strlen("/lib/gtk/themes/engines") +1);
      sprintf(path, "%s%s", GTK_EXE_PREFIX, "/lib/gtk/themes/engines");
    }
  module_path[n++] = g_strdup(path);
  g_free(path);
  var = getenv("HOME");
  if (var)
    {
      path = g_malloc(strlen(var) + strlen(".gtk/lib/themes/engines") +1);
      sprintf(path, "%s%s", var, ".gtk/lib/themes/engines");
    }
  module_path[n++] = g_strdup(path);
  module_path[n] = NULL;
  g_free(path);
}

void
gtk_rc_init (void)
{
  rc_style_ht = g_hash_table_new ((GHashFunc) gtk_rc_style_hash,
				  (GCompareFunc) gtk_rc_style_compare);
  pixmap_path[0] = NULL;
  module_path[0] = NULL;
  gtk_rc_append_default_pixmap_path();
  gtk_rc_append_default_module_path();
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
	  gchar *cwd;

	  cwd = g_get_current_dir ();

	  str = g_string_new (cwd);
	  g_free (cwd);
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

	{
	  gint i;
	  gchar *dir;
	  
	  dir = g_strdup(rc_file->canonical_name);
	  for (i = strlen(dir) - 1; (i >= 0) && (dir[i] != '/'); i--)
	    dir[i] = 0;
	  gtk_rc_append_pixmap_path(dir);
	  g_free(dir);
	}
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

/* Handling of RC styles */

GtkRcStyle *
gtk_rc_style_new              (void)
{
  GtkRcStyle *new_style;

  new_style = g_new0 (GtkRcStyle, 1);
  new_style->ref_count = 1;

  return new_style;
}

void      
gtk_rc_style_ref (GtkRcStyle  *rc_style)
{
  g_return_if_fail (rc_style != NULL);

  rc_style->ref_count++;
}

void      
gtk_rc_style_unref (GtkRcStyle  *rc_style)
{
  gint i;

  g_return_if_fail (rc_style != NULL);

  rc_style->ref_count--;

  if (rc_style->ref_count == 0)
    {
      if (rc_style->engine)
	{
	  rc_style->engine->destroy_rc_style (rc_style);
	  gtk_theme_engine_unref (rc_style->engine);
	}

      if (rc_style->name)
	g_free (rc_style->name);
      if (rc_style->fontset_name)
	g_free (rc_style->fontset_name);
      if (rc_style->font_name)
	g_free (rc_style->font_name);
      
      for (i=0 ; i<5 ; i++)
	if (rc_style->bg_pixmap_name[i])
	  g_free (rc_style->bg_pixmap_name[i]);
      
      g_free (rc_style);
    }
}

static void
gtk_rc_clear_realized_node (gpointer key,
			    gpointer data,
			    gpointer user_data)
{
  gtk_style_unref (data);
}

static void
gtk_rc_clear_hash_node (gpointer key, 
			gpointer data, 
			gpointer user_data)
{
  gtk_rc_style_unref (data);
}

static void
gtk_rc_free_rc_sets (GSList *slist)
{
  while (slist)
    {
      GtkRcSet *rc_set;

      rc_set = slist->data;
      gtk_pattern_spec_free_segs (&rc_set->pspec);
      g_free (rc_set);

      slist = slist->next;
    }
}

static void
gtk_rc_clear_styles (void)
{
  /* Clear out all old rc_styles */

  if (rc_style_ht)
    {
      g_hash_table_foreach (rc_style_ht, gtk_rc_clear_hash_node, NULL);
      g_hash_table_destroy (rc_style_ht);
      rc_style_ht = NULL;
    }

  if (realized_style_ht)
    {
      g_hash_table_foreach (realized_style_ht, gtk_rc_clear_realized_node, NULL);
      g_hash_table_destroy (realized_style_ht);
      realized_style_ht = NULL;
    }

  gtk_rc_free_rc_sets (gtk_rc_sets_widget);
  g_slist_free (gtk_rc_sets_widget);
  gtk_rc_sets_widget = NULL;

  gtk_rc_free_rc_sets (gtk_rc_sets_widget_class);
  g_slist_free (gtk_rc_sets_widget_class);
  gtk_rc_sets_widget_class = NULL;

  gtk_rc_free_rc_sets (gtk_rc_sets_class);
  g_slist_free (gtk_rc_sets_class);
  gtk_rc_sets_class = NULL;

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

static GSList *
gtk_rc_styles_match (GSList       *rc_styles,
		     GSList	  *sets,
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
	rc_styles = g_slist_append (rc_styles, rc_set->rc_style);
    }
  
  return rc_styles;
}

GtkStyle*
gtk_rc_get_style (GtkWidget *widget)
{
  GtkRcStyle *widget_rc_style;
  GSList *rc_styles = NULL;

  static guint rc_style_key_id = 0;

  /* We allow the specification of a single rc style to be bound
   * tightly to a widget, for application modifications
   */
  if (!rc_style_key_id)
    rc_style_key_id = g_quark_from_static_string ("gtk-rc-style");

  widget_rc_style = gtk_object_get_data_by_id (GTK_OBJECT (widget),
					       rc_style_key_id);

  if (widget_rc_style)
    rc_styles = g_list_prepend (rc_styles, widget_rc_style);
  
  if (gtk_rc_sets_widget)
    {
      gchar *path, *path_reversed;
      guint path_length;

      gtk_widget_path (widget, &path_length, &path, &path_reversed);
      rc_styles = gtk_rc_styles_match (rc_styles, gtk_rc_sets_widget, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
      
    }
  
  if (gtk_rc_sets_widget_class)
    {
      gchar *path, *path_reversed;
      guint path_length;

      gtk_widget_class_path (widget, &path_length, &path, &path_reversed);
      rc_styles = gtk_rc_styles_match (rc_styles, gtk_rc_sets_widget_class, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
    }

  if (gtk_rc_sets_class)
    {
      GtkType type;

      type = GTK_OBJECT_TYPE (widget);
      while (type)
	{
	  gchar *path, *path_reversed;
	  guint path_length;

	  path = gtk_type_name (type);
	  path_length = strlen (path);
	  path_reversed = g_strdup (path);
	  g_strreverse (path_reversed);
	  
	  rc_styles = gtk_rc_styles_match (rc_styles, gtk_rc_sets_class, path_length, path, path_reversed);
	  g_free (path_reversed);
      
	  type = gtk_type_parent (type);
	}
    }
  
  if (rc_styles)
    return gtk_rc_style_init (rc_styles);

  return NULL;
}

static GSList*
gtk_rc_add_rc_sets (GSList     *slist,
		    GtkRcStyle *rc_style,
		    const char *pattern)
{
  GtkRcStyle *new_style;
  GtkRcSet *rc_set;
  guint i;
  
  new_style = gtk_rc_style_new ();
  *new_style = *rc_style;
  new_style->name = g_strdup (rc_style->name);
  new_style->font_name = g_strdup (rc_style->font_name);
  new_style->fontset_name = g_strdup (rc_style->fontset_name);
  
  for (i = 0; i < 5; i++)
    new_style->bg_pixmap_name[i] = g_strdup (rc_style->bg_pixmap_name[i]);
  
  rc_set = g_new (GtkRcSet, 1);
  gtk_pattern_spec_init (&rc_set->pspec, pattern);
  rc_set->rc_style = rc_style;
  
  return g_slist_prepend (slist, rc_set);
}

void
gtk_rc_add_widget_name_style (GtkRcStyle  *rc_style,
			      const gchar *pattern)
{
  g_return_if_fail (rc_style != NULL);
  g_return_if_fail (pattern != NULL);

  gtk_rc_sets_widget = gtk_rc_add_rc_sets (gtk_rc_sets_widget, rc_style, pattern);
}

void
gtk_rc_add_widget_class_style (GtkRcStyle  *rc_style,
			       const gchar *pattern)
{
  g_return_if_fail (rc_style != NULL);
  g_return_if_fail (pattern != NULL);

  gtk_rc_sets_widget_class = gtk_rc_add_rc_sets (gtk_rc_sets_widget_class, rc_style, pattern);
}

void
gtk_rc_add_class_style (GtkRcStyle  *rc_style,
			const gchar *pattern)
{
  g_return_if_fail (rc_style != NULL);
  g_return_if_fail (pattern != NULL);

  gtk_rc_sets_class = gtk_rc_add_rc_sets (gtk_rc_sets_class, rc_style, pattern);
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
  for (i = 0; i < n_symbols; i++)
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
	      symbol_name = NULL;
	      if (scanner->scope_id == 0)
		{
		  /* if we are in scope 0, we know the symbol names
		   * that are associated with certaintoken values.
		   * so we look them up to make the error messages
		   * more readable.
		   */
		  if (expected_token > GTK_RC_TOKEN_INVALID &&
		      expected_token < GTK_RC_TOKEN_LAST)
		    {
		      for (i = 0; i < n_symbols; i++)
			if (symbols[i].token == expected_token)
			  msg = symbols[i].name;
		      if (msg)
			msg = g_strconcat ("e.g. `", msg, "'", NULL);
		    }
		  if (scanner->token > GTK_RC_TOKEN_INVALID &&
		      scanner->token < GTK_RC_TOKEN_LAST)
		    {
		      symbol_name = "???";
		      for (i = 0; i < n_symbols; i++)
			if (symbols[i].token == scanner->token)
			  symbol_name = symbols[i].name;
		    }
		}
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
gtk_rc_styles_hash (const GSList *rc_styles)
{
  guint result;
  
  result = 0;
  while (rc_styles)
    {
      result += (result << 9) + GPOINTER_TO_UINT (rc_styles->data);
      rc_styles = rc_styles->next;
    }
  
  return result;
}

static gint	   
gtk_rc_styles_compare (const GSList *a,
		       const GSList *b)
{
  while (a && b)
    {
      if (a->data != b->data)
	return FALSE;
      a = a->next;
      b = b->next;
    }
  
  return (a == b);
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

/* Assumes ownership of rc_style */
static GtkStyle *
gtk_rc_style_to_style (GtkRcStyle *rc_style)
{
  GtkStyle *style;
  GdkFont *old_font;
  gint i;

  style = gtk_style_new ();

  style->rc_style = rc_style;
  
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
    {
      if (rc_style->color_flags[i] & GTK_RC_FG)
	style->fg[i] = rc_style->fg[i];
      if (rc_style->color_flags[i] & GTK_RC_BG)
	style->bg[i] = rc_style->bg[i];
      if (rc_style->color_flags[i] & GTK_RC_TEXT)
	style->text[i] = rc_style->text[i];
      if (rc_style->color_flags[i] & GTK_RC_BASE)
	style->base[i] = rc_style->base[i];
    }

  if (rc_style->engine)
    {
      style->engine = rc_style->engine;
      gtk_theme_engine_ref (style->engine);
      rc_style->engine->rc_style_to_style (style, rc_style);
    }

  return style;
}

/* Reuses or frees rc_styles */
static GtkStyle *
gtk_rc_style_init (GSList *rc_styles)
{
  gint i;

  GtkStyle *style = NULL;
  GtkRcStyle *proto_style;

  if (!realized_style_ht)
    realized_style_ht = g_hash_table_new ((GHashFunc)gtk_rc_styles_hash,
					   (GCompareFunc)gtk_rc_styles_compare);

  style = g_hash_table_lookup (realized_style_ht, rc_styles);

  if (!style)
    {
      GSList *tmp_styles = rc_styles;
      
      proto_style = gtk_rc_style_new ();

      while (tmp_styles)
	{
	  GtkRcStyle *rc_style = tmp_styles->data;

	  for (i=0; i<5; i++)
	    {
	      if (!proto_style->bg_pixmap_name[i] && rc_style->bg_pixmap_name[i])
		proto_style->bg_pixmap_name[i] = g_strdup (rc_style->bg_pixmap_name[i]);

	      if (!(proto_style->color_flags[i] & GTK_RC_FG) && 
		    rc_style->color_flags[i] & GTK_RC_FG)
		{
		  proto_style->fg[i] = rc_style->fg[i];
		  proto_style->color_flags[i] |= GTK_RC_FG;
		}
	      if (!(proto_style->color_flags[i] & GTK_RC_BG) && 
		    rc_style->color_flags[i] & GTK_RC_BG)
		{
		  proto_style->bg[i] = rc_style->bg[i];
		  proto_style->color_flags[i] |= GTK_RC_BG;
		}
	      if (!(proto_style->color_flags[i] & GTK_RC_TEXT) && 
		    rc_style->color_flags[i] & GTK_RC_TEXT)
		{
		  proto_style->text[i] = rc_style->text[i];
		  proto_style->color_flags[i] |= GTK_RC_TEXT;
		}
	      if (!(proto_style->color_flags[i] & GTK_RC_BASE) && 
		    rc_style->color_flags[i] & GTK_RC_BASE)
		{
		  proto_style->base[i] = rc_style->base[i];
		  proto_style->color_flags[i] |= GTK_RC_BASE;
		}
	    }

	  if (!proto_style->font_name && rc_style->font_name)
	    proto_style->font_name = g_strdup (rc_style->font_name);
	  if (!proto_style->fontset_name && rc_style->fontset_name)
	    proto_style->fontset_name = g_strdup (rc_style->fontset_name);

	  if (!proto_style->engine && rc_style->engine)
	    {
	      proto_style->engine = rc_style->engine;
	      gtk_theme_engine_ref (proto_style->engine);
	    }
	  
	  if (proto_style->engine &&
	      (proto_style->engine == rc_style->engine))
	    proto_style->engine->merge_rc_style (proto_style, rc_style);

	  tmp_styles = tmp_styles->next;
	}

      style = gtk_rc_style_to_style (proto_style);

      g_hash_table_insert (realized_style_ht, rc_styles, style);
    }

  return style;
}

/*********************
 * Parsing functions *
 *********************/

static guint
gtk_rc_parse_statement (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_peek_next_token (scanner);

  switch (token)
    {
    case GTK_RC_TOKEN_INCLUDE:
      token = g_scanner_get_next_token (scanner);
      if (token != GTK_RC_TOKEN_INCLUDE)
	return GTK_RC_TOKEN_INCLUDE;

      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
	return G_TOKEN_STRING;

      gtk_rc_parse_file (scanner->value.v_string, FALSE);
      return G_TOKEN_NONE;

    case GTK_RC_TOKEN_STYLE:
      return gtk_rc_parse_style (scanner);

    case GTK_RC_TOKEN_BINDING:
      return gtk_binding_parse_binding (scanner);

    case GTK_RC_TOKEN_PIXMAP_PATH:
      return gtk_rc_parse_pixmap_path (scanner);

    case GTK_RC_TOKEN_WIDGET:
      return gtk_rc_parse_path_pattern (scanner);

    case GTK_RC_TOKEN_WIDGET_CLASS:
      return gtk_rc_parse_path_pattern (scanner);

    case GTK_RC_TOKEN_CLASS:
      return gtk_rc_parse_path_pattern (scanner);

    case GTK_RC_TOKEN_MODULE_PATH:
      return gtk_rc_parse_module_path (scanner);
       
    default:
      g_scanner_get_next_token (scanner);
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_STYLE;
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
  if (token != GTK_RC_TOKEN_STYLE)
    return GTK_RC_TOKEN_STYLE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  insert = FALSE;
  rc_style = g_hash_table_lookup (rc_style_ht, scanner->value.v_string);
  
  if (!rc_style)
    {
      insert = TRUE;
      rc_style = gtk_rc_style_new ();
      rc_style->name = g_strdup (scanner->value.v_string);
      
      for (i = 0; i < 5; i++)
	rc_style->bg_pixmap_name[i] = NULL;

      for (i = 0; i < 5; i++)
	rc_style->color_flags[i] = 0;

      rc_style->engine = NULL;
      rc_style->engine_data = NULL;
    }
  
  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EQUAL_SIGN)
    {
      token = g_scanner_get_next_token (scanner);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
	{
	  if (insert)
	    g_free (rc_style);

	  return G_TOKEN_STRING;
	}
      
      parent_style = g_hash_table_lookup (rc_style_ht, scanner->value.v_string);
      if (parent_style)
	{
	  for (i = 0; i < 5; i++)
	    {
	      rc_style->color_flags[i] = parent_style->color_flags[i];
	      rc_style->fg[i] = parent_style->fg[i];
	      rc_style->bg[i] = parent_style->bg[i];
	      rc_style->text[i] = parent_style->text[i];
	      rc_style->base[i] = parent_style->base[i];
	    }
	  
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
	g_free (rc_style);

      return G_TOKEN_LEFT_CURLY;
    }
  
  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case GTK_RC_TOKEN_BASE:
	  token = gtk_rc_parse_base (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_BG:
	  token = gtk_rc_parse_bg (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FG:
	  token = gtk_rc_parse_fg (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_TEXT:
	  token = gtk_rc_parse_text (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_BG_PIXMAP:
	  token = gtk_rc_parse_bg_pixmap (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FONT:
	  token = gtk_rc_parse_font (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FONTSET:
	  token = gtk_rc_parse_fontset (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_ENGINE:
	  token = gtk_rc_parse_engine (scanner, rc_style);
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
	  
	  g_free (rc_style);
	}
      return G_TOKEN_RIGHT_CURLY;
    }
  
  if (insert)
    g_hash_table_insert (rc_style_ht, rc_style->name, rc_style);
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_base (GScanner   *scanner,
		   GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_BASE)
    return GTK_RC_TOKEN_BASE;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  style->color_flags[state] |= GTK_RC_BASE;
  return gtk_rc_parse_color (scanner, &style->base[state]);
}

static guint
gtk_rc_parse_bg (GScanner   *scanner,
		 GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_BG)
    return GTK_RC_TOKEN_BG;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  style->color_flags[state] |= GTK_RC_BG;
  return gtk_rc_parse_color (scanner, &style->bg[state]);
}

static guint
gtk_rc_parse_fg (GScanner   *scanner,
		 GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_FG)
    return GTK_RC_TOKEN_FG;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  style->color_flags[state] |= GTK_RC_FG;
  return gtk_rc_parse_color (scanner, &style->fg[state]);
}

static guint
gtk_rc_parse_text (GScanner   *scanner,
		   GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_TEXT)
    return GTK_RC_TOKEN_TEXT;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  style->color_flags[state] |= GTK_RC_TEXT;
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
  if (token != GTK_RC_TOKEN_BG_PIXMAP)
    return GTK_RC_TOKEN_BG_PIXMAP;
  
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

gchar*
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

  if (scanner)
    g_warning ("Unable to locate image file in pixmap_path: \"%s\" line %d",
	       pixmap_file, scanner->line);
  else
    g_warning ("Unable to locate image file in pixmap_path: \"%s\"",
	       pixmap_file);
    
  return NULL;
}

gchar*
gtk_rc_find_module_in_path (GScanner *scanner,
			    gchar    *module_file)
{
  gint i;
  gint fd;
  gchar *buf;
  
  for (i = 0; (i < GTK_RC_MAX_MODULE_PATHS) && (module_path[i] != NULL); i++)
    {
      buf = g_malloc (strlen (module_path[i]) + strlen (module_file) + 2);
      sprintf (buf, "%s%c%s", module_path[i], '/', module_file);
      
      fd = open (buf, O_RDONLY);
      if (fd >= 0)
	{
	  close (fd);
	  return buf;
	}
      
      g_free (buf);
    }
  
  if (scanner)
    g_warning ("Unable to locate loadable module in module_path: \"%s\" line %d",
	       module_file, scanner->line);
  else
    g_warning ("Unable to locate loadable module in module_path: \"%s\",",
	       module_file);
    
  return NULL;
}

static guint
gtk_rc_parse_font (GScanner   *scanner,
		   GtkRcStyle *rc_style)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_FONT)
    return GTK_RC_TOKEN_FONT;
  
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
  if (token != GTK_RC_TOKEN_FONTSET)
    return GTK_RC_TOKEN_FONTSET;
  
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
gtk_rc_parse_engine (GScanner	 *scanner,
		     GtkRcStyle	 *rc_style)
{
  guint token;

  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_ENGINE)
    return GTK_RC_TOKEN_ENGINE;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  rc_style->engine = gtk_theme_engine_get (scanner->value.v_string);

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  if (rc_style->engine)
    return rc_style->engine->parse_rc_style (scanner, rc_style);
  else
    {
      /* Skip over remainder, looking for nested {}'s */
      guint count = 1;
      
      while ((token = g_scanner_get_next_token (scanner)) != G_TOKEN_EOF)
	{
	  if (token == G_TOKEN_LEFT_CURLY)
	    count++;
	  else if (token == G_TOKEN_RIGHT_CURLY)
	    count--;

	  if (count == 0)
	    return G_TOKEN_NONE;
	}

      return G_TOKEN_RIGHT_CURLY;
    }
}

guint
gtk_rc_parse_state (GScanner	 *scanner,
		    GtkStateType *state)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (state != NULL, G_TOKEN_ERROR);
  
  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_BRACE)
    return G_TOKEN_LEFT_BRACE;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_ACTIVE:
      *state = GTK_STATE_ACTIVE;
      break;
    case GTK_RC_TOKEN_INSENSITIVE:
      *state = GTK_STATE_INSENSITIVE;
      break;
    case GTK_RC_TOKEN_NORMAL:
      *state = GTK_STATE_NORMAL;
      break;
    case GTK_RC_TOKEN_PRELIGHT:
      *state = GTK_STATE_PRELIGHT;
      break;
    case GTK_RC_TOKEN_SELECTED:
      *state = GTK_STATE_SELECTED;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_NORMAL;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    return G_TOKEN_RIGHT_BRACE;
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

guint
gtk_rc_parse_priority (GScanner	           *scanner,
		       GtkPathPriorityType *priority)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (priority != NULL, G_TOKEN_ERROR);

  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != ':')
    return ':';
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_LOWEST:
      *priority = GTK_PATH_PRIO_LOWEST;
      break;
    case GTK_RC_TOKEN_GTK:
      *priority = GTK_PATH_PRIO_GTK;
      break;
    case GTK_RC_TOKEN_APPLICATION:
      *priority = GTK_PATH_PRIO_APPLICATION;
      break;
    case GTK_RC_TOKEN_RC:
      *priority = GTK_PATH_PRIO_RC;
      break;
    case GTK_RC_TOKEN_HIGHEST:
      *priority = GTK_PATH_PRIO_HIGHEST;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_APPLICATION;
    }
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

guint
gtk_rc_parse_color (GScanner *scanner,
		    GdkColor *color)
{
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  /* we don't need to set our own scop here, because
   * we don't need own symbols
   */
  
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
  if (token != GTK_RC_TOKEN_PIXMAP_PATH)
    return GTK_RC_TOKEN_PIXMAP_PATH;
  
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
  gtk_rc_append_default_pixmap_path();
}

static guint
gtk_rc_parse_module_path (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_MODULE_PATH)
    return GTK_RC_TOKEN_MODULE_PATH;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  gtk_rc_parse_module_path_string (scanner->value.v_string);
  
  return G_TOKEN_NONE;
}

static void
gtk_rc_parse_module_path_string (gchar *mod_path)
{
  gchar *buf;
  gint end_offset;
  gint start_offset = 0;
  gint path_len;
  gint path_num;
  
  /* free the old one, or just add to the old one ? */
  for (path_num=0; module_path[path_num]; path_num++)
    {
      g_free (module_path[path_num]);
      module_path[path_num] = NULL;
    }
  
  path_num = 0;
  
  path_len = strlen (mod_path);
  
  buf = g_strdup (mod_path);
  
  for (end_offset = 0; end_offset <= path_len; end_offset++)
    {
      if ((buf[end_offset] == ':') ||
	  (end_offset == path_len))
	{
	  buf[end_offset] = '\0';
	  module_path[path_num] = g_strdup (buf + start_offset);
	  path_num++;
	  module_path[path_num] = NULL;
	  start_offset = end_offset + 1;
	}
    }
  g_free (buf);
  gtk_rc_append_default_module_path();
}

static guint
gtk_rc_parse_path_pattern (GScanner   *scanner)
{
  guint token;
  GtkPathType path_type;
  gchar *pattern;
  gboolean is_binding;
  GtkPathPriorityType priority = GTK_PATH_PRIO_RC;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_WIDGET:
      path_type = GTK_PATH_WIDGET;
      break;
    case GTK_RC_TOKEN_WIDGET_CLASS:
      path_type = GTK_PATH_WIDGET_CLASS;
      break;
    case GTK_RC_TOKEN_CLASS:
      path_type = GTK_PATH_CLASS;
      break;
    default:
      return GTK_RC_TOKEN_WIDGET_CLASS;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  pattern = g_strdup (scanner->value.v_string);

  token = g_scanner_get_next_token (scanner);
  if (token == GTK_RC_TOKEN_STYLE)
    is_binding = FALSE;
  else if (token == GTK_RC_TOKEN_BINDING)
    {
      is_binding = TRUE;
      if (g_scanner_peek_next_token (scanner) == ':')
	{
	  token = gtk_rc_parse_priority (scanner, &priority);
	  if (token != G_TOKEN_NONE)
	    {
	      g_free (pattern);
	      return token;
	    }
	}
    }
  else
    {
      g_free (pattern);
      return GTK_RC_TOKEN_STYLE;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    {
      g_free (pattern);
      return G_TOKEN_STRING;
    }

  if (is_binding)
    {
      GtkBindingSet *binding;

      binding = gtk_binding_set_find (scanner->value.v_string);
      if (!binding)
	{
	  g_free (pattern);
	  return G_TOKEN_STRING;
	}
      gtk_binding_set_add_path (binding, path_type, pattern, priority);
    }
  else
    {
      GtkRcStyle *rc_style;
      GtkRcSet *rc_set;

      rc_style = gtk_rc_style_find (scanner->value.v_string);
      
      if (!rc_style)
	{
	  g_free (pattern);
	  return G_TOKEN_STRING;
	}

      rc_set = g_new (GtkRcSet, 1);
      gtk_pattern_spec_init (&rc_set->pspec, pattern);
      rc_set->rc_style = rc_style;

      if (path_type == GTK_PATH_WIDGET)
	gtk_rc_sets_widget = g_slist_prepend (gtk_rc_sets_widget, rc_set);
      else if (path_type == GTK_PATH_WIDGET_CLASS)
	gtk_rc_sets_widget_class = g_slist_prepend (gtk_rc_sets_widget_class, rc_set);
      else
	gtk_rc_sets_class = g_slist_prepend (gtk_rc_sets_class, rc_set);
    }

  g_free (pattern);
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

GdkPixmap *
gtk_rc_load_image (GdkColormap *colormap,
		   GdkColor    *transparent_color,
		   const gchar *filename)
{
  if (strcmp (filename, "<parent>") == 0)
    return (GdkPixmap*) GDK_PARENT_RELATIVE;
  else
    {
      if(image_loader)
	return image_loader(NULL, colormap, NULL,
			    transparent_color,
			    filename);
      else
	return gdk_pixmap_colormap_create_from_xpm (NULL, colormap, NULL,
						    transparent_color,
						    filename);
    }
}
