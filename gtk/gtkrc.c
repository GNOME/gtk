/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#ifdef GDK_WINDOWING_X11
#include <X11/Xlocale.h>	/* so we get the right setlocale */
#else
#include <locale.h>
#endif
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef HAVE_LSTAT
#define lstat stat
#endif

#include <glib.h>
#include "gdkconfig.h"

#include "gtkcompat.h"
#include "gtkrc.h"
#include "gtkbindings.h"
#include "gtkthemes.h"
#include "gtkintl.h"
#include "gtkiconfactory.h"
#include "gtksettings.h"

#ifdef G_OS_WIN32
#include <io.h>
#endif

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

static guint       gtk_rc_style_hash                 (const gchar     *name);
static gboolean    gtk_rc_style_equal                (const gchar     *a,
                                                      const gchar     *b);
static guint       gtk_rc_styles_hash                (const GSList    *rc_styles);
static gboolean    gtk_rc_styles_equal               (const GSList    *a,
                                                      const GSList    *b);
static GtkRcStyle* gtk_rc_style_find                 (const gchar     *name);
static GSList *    gtk_rc_styles_match               (GSList          *rc_styles,
                                                      GSList          *sets,
                                                      guint            path_length,
                                                      const gchar     *path,
                                                      const gchar     *path_reversed);
static GtkStyle *  gtk_rc_style_to_style             (GtkRcStyle      *rc_style);
static GtkStyle*   gtk_rc_init_style                 (GSList          *rc_styles);
static void        gtk_rc_parse_file                 (const gchar     *filename,
                                                      gboolean         reload);
static void        gtk_rc_parse_any                  (const gchar     *input_name,
                                                      gint             input_fd,
                                                      const gchar     *input_string);
static guint       gtk_rc_parse_statement            (GScanner        *scanner);
static guint       gtk_rc_parse_style                (GScanner        *scanner);
static guint       gtk_rc_parse_assignment           (GScanner        *scanner,
						      GtkRcProperty   *prop);
static guint       gtk_rc_parse_bg                   (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_fg                   (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_text                 (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_base                 (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_xthickness           (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_ythickness           (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_bg_pixmap            (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_font                 (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_fontset              (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_font_name            (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_engine               (GScanner        *scanner,
                                                      GtkRcStyle     **rc_style);
static guint       gtk_rc_parse_pixmap_path          (GScanner        *scanner);
static void        gtk_rc_parse_pixmap_path_string   (gchar           *pix_path);
static guint       gtk_rc_parse_module_path          (GScanner        *scanner);
static void        gtk_rc_parse_module_path_string   (gchar           *mod_path);
static guint       gtk_rc_parse_im_module_path       (GScanner        *scanner);
static guint       gtk_rc_parse_im_module_file       (GScanner        *scanner);
static guint       gtk_rc_parse_path_pattern         (GScanner        *scanner);
static guint       gtk_rc_parse_stock                (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style,
                                                      GtkIconFactory  *factory);
static void        gtk_rc_clear_hash_node            (gpointer         key,
                                                      gpointer         data,
                                                      gpointer         user_data);
static void        gtk_rc_clear_styles               (void);
static void        gtk_rc_append_default_module_path (void);
static void        gtk_rc_add_initial_default_files  (void);

static void        gtk_rc_style_init                 (GtkRcStyle      *style);
static void        gtk_rc_style_class_init           (GtkRcStyleClass *klass);
static void        gtk_rc_style_finalize             (GObject         *object);
static void        gtk_rc_style_real_merge           (GtkRcStyle      *dest,
                                                      GtkRcStyle      *src);
static GtkRcStyle* gtk_rc_style_real_create_rc_style (GtkRcStyle      *rc_style);
static GtkStyle*   gtk_rc_style_real_create_style    (GtkRcStyle      *rc_style);
static gint	   gtk_rc_properties_cmp	     (gconstpointer    bsearch_node1,
						      gconstpointer    bsearch_node2);

static gpointer parent_class = NULL;

static const GScannerConfig gtk_rc_scanner_config =
{
  (
   " \t\r\n"
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

static const struct
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
  { "text", GTK_RC_TOKEN_TEXT },
  { "base", GTK_RC_TOKEN_BASE },
  { "xthickness", GTK_RC_TOKEN_XTHICKNESS },
  { "ythickness", GTK_RC_TOKEN_YTHICKNESS },
  { "font", GTK_RC_TOKEN_FONT },
  { "fontset", GTK_RC_TOKEN_FONTSET },
  { "font_name", GTK_RC_TOKEN_FONT_NAME },
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
  { "stock", GTK_RC_TOKEN_STOCK },
  { "im_module_path", GTK_RC_TOKEN_IM_MODULE_PATH },
  { "im_module_file", GTK_RC_TOKEN_IM_MODULE_FILE },
  { "LTR", GTK_RC_TOKEN_LTR },
  { "RTL", GTK_RC_TOKEN_RTL }
};

static const guint n_symbols = sizeof (symbols) / sizeof (symbols[0]);

static gchar *im_module_path = NULL;
static gchar *im_module_file = NULL;

static GHashTable *rc_style_ht = NULL;
static GHashTable *realized_style_ht = NULL;
static GSList *gtk_rc_sets_widget = NULL;
static GSList *gtk_rc_sets_widget_class = NULL;
static GSList *gtk_rc_sets_class = NULL;

#define GTK_RC_MAX_DEFAULT_FILES 128
static gchar *gtk_rc_default_files[GTK_RC_MAX_DEFAULT_FILES];
static gboolean gtk_rc_auto_parse = TRUE;

#define GTK_RC_MAX_PIXMAP_PATHS 128
static gchar *pixmap_path[GTK_RC_MAX_PIXMAP_PATHS];
#define GTK_RC_MAX_MODULE_PATHS 128
static gchar *module_path[GTK_RC_MAX_MODULE_PATHS];

/* A stack of directories for RC files we are parsing currently.
 * these are implicitely added to the end of PIXMAP_PATHS
 */
GSList *rc_dir_stack = NULL;

/* The files we have parsed, to reread later if necessary */
GSList *rc_files = NULL;

static GtkImageLoader image_loader = NULL;

/* RC file handling */


#ifdef G_OS_WIN32
static gchar *
get_gtk_dll_name (void)
{
  static gchar *gtk_dll = NULL;

  if (!gtk_dll)
    gtk_dll = g_strdup_printf ("gtk-%d.%d.dll", GTK_MAJOR_VERSION, GTK_MINOR_VERSION);

  return gtk_dll;
}

static gchar *
get_themes_directory (void)
{
  return g_win32_get_package_installation_subdirectory (GETTEXT_PACKAGE,
							get_gtk_dll_name (),
							"themes");
}
#endif /* G_OS_WIN32 */
 
static gchar *
gtk_rc_make_default_dir (const gchar *type)
{
  gchar *var, *path;

#ifndef G_OS_WIN32
  var = getenv("GTK_EXE_PREFIX");
  if (var)
    path = g_strconcat (var, "/lib/gtk-2.0/" GTK_VERSION "/", type, NULL);
  else
    path = g_strconcat (GTK_LIBDIR "/gtk-2.0/" GTK_VERSION "/", type, NULL);
#else
  path = g_strconcat ("%s\\%s", get_themes_directory (), type);
#endif

  return path;
}

gchar *
gtk_rc_get_im_module_path (void)
{
  const gchar *result = g_getenv ("GTK_IM_MODULE_PATH");

  if (!result)
    {
      if (im_module_path)
	result = im_module_path;
      else
	return gtk_rc_make_default_dir ("immodules");
    }

  return g_strdup (result);
}

gchar *
gtk_rc_get_im_module_file (void)
{
  gchar *result = g_strdup (g_getenv ("GTK_IM_MODULE_FILE"));

  if (!result)
    {
      if (im_module_file)
	result = g_strdup (im_module_file);
      else
#ifndef G_OS_WIN32
	result = g_strdup (GTK_SYSCONFDIR G_DIR_SEPARATOR_S "gtk-2.0" G_DIR_SEPARATOR_S "gtk.immodules");
#else
	result = g_strdup_printf ("%s\\gtk.immodules", g_win32_get_package_installation_directory (GETTEXT_PACKAGE, get_gtk_dll_name ()));
#endif
    }

  return result;
}

gchar *
gtk_rc_get_theme_dir(void)
{
  gchar *var, *path;

#ifndef G_OS_WIN32
  var = getenv("GTK_DATA_PREFIX");
  if (var)
    path = g_strconcat (var, "/share/themes", NULL);
  else
    path = g_strconcat (GTK_DATA_PREFIX, "/share/themes", NULL);
#else
  path = g_strdup (get_themes_directory ());
#endif

  return path;
}

gchar *
gtk_rc_get_module_dir(void)
{
  return gtk_rc_make_default_dir ("engines");
}

static void
gtk_rc_append_default_module_path(void)
{
  const gchar *var;
  gchar *path;
  gint n;

  for (n = 0; module_path[n]; n++) ;
  if (n >= GTK_RC_MAX_MODULE_PATHS - 1)
    return;
  
#ifndef G_OS_WIN32
  var = getenv("GTK_EXE_PREFIX");
  if (var)
    path = g_strconcat(var, "/lib/gtk-2.0/" GTK_VERSION "/engines", NULL);
  else
    path = g_strdup (GTK_LIBDIR "/gtk-2.0/" GTK_VERSION "/engines");
#else
  path = g_strconcat (get_themes_directory (), "\\engines", NULL);
#endif
  module_path[n++] = path;

  var = g_get_home_dir ();
  if (var)
    {
      gchar *sep;
      /* Don't duplicate the directory separator, causes trouble at
       * least on Windows.
       */
      if (var[strlen (var) -1] != G_DIR_SEPARATOR)
	sep = G_DIR_SEPARATOR_S;
      else
	sep = "";
      /* This produces something like ~/.gtk-2.0/2.0/engines */
      path = g_strconcat (var, sep,
			  ".gtk-2.0" G_DIR_SEPARATOR_S
			  GTK_VERSION G_DIR_SEPARATOR_S
			  "engines", NULL);
      module_path[n++] = path;
    }
  module_path[n] = NULL;
}

static void
gtk_rc_add_initial_default_files (void)
{
  static gint init = FALSE;
  const gchar *var;
  gchar *str;
  gchar **files;
  gint i;

  if (init)
    return;
  
  gtk_rc_default_files[0] = NULL;
  init = TRUE;

  var = g_getenv("GTK_RC_FILES");
  if (var)
    {
      files = g_strsplit (var, G_SEARCHPATH_SEPARATOR_S, 128);
      i=0;
      while (files[i])
	{
	  gtk_rc_add_default_file (files[i]);
	  i++;
	}
      g_strfreev (files);
    }
  else
    {
#ifndef G_OS_WIN32
      str = g_strdup (GTK_SYSCONFDIR G_DIR_SEPARATOR_S "gtk-2.0" G_DIR_SEPARATOR_S "gtkrc");
#else
      str = g_strdup_printf ("%s\\gtkrc", g_win32_get_package_installation_directory (GETTEXT_PACKAGE, get_gtk_dll_name ()));
#endif

      gtk_rc_add_default_file (str);
      g_free (str);

      var = g_get_home_dir ();
      if (var)
	{
	  gchar *sep;
	  if (var[strlen (var) -1] != G_DIR_SEPARATOR)
	    sep = G_DIR_SEPARATOR_S;
	  else
	    sep = "";
	  str = g_strdup_printf ("%s%s.gtkrc-2.0", var, sep);
	  gtk_rc_add_default_file (str);
	  g_free (str);
	}
    }
}

void
gtk_rc_add_default_file (const gchar *file)
{
  guint n;
  
  gtk_rc_add_initial_default_files ();

  for (n = 0; gtk_rc_default_files[n]; n++) ;
  if (n >= GTK_RC_MAX_DEFAULT_FILES - 1)
    return;
  
  gtk_rc_default_files[n++] = g_strdup (file);
  gtk_rc_default_files[n] = NULL;
}

void
gtk_rc_set_default_files (gchar **files)
{
  gint i;

  gtk_rc_add_initial_default_files ();

  i = 0;
  while (gtk_rc_default_files[i])
    {
      g_free (gtk_rc_default_files[i]);
      i++;
    }
    
  gtk_rc_default_files[0] = NULL;
  gtk_rc_auto_parse = FALSE;

  i = 0;
  while (files[i] != NULL)
    {
      gtk_rc_add_default_file (files[i]);
      i++;
    }
}

gchar **
gtk_rc_get_default_files (void)
{
  gtk_rc_add_initial_default_files ();

  return gtk_rc_default_files;
}

 /* The following routine is based on _nl_normalize_codeset from
  * the GNU C library. Contributed by
  *
  * Contributed by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.
  * Copyright (C) 1995, 1996, 1997, 1998, 1999 Free Software Foundation, Inc.
  * 
  * Normalize codeset name.  There is no standard for the codeset
  * names.  Normalization allows the user to use any of the common
  * names.
  */
 static gchar *
 _gtk_normalize_codeset (const gchar *codeset, gint name_len)
 {
   gint len = 0;
   gint only_digit = 1;
   gchar *retval;
   gchar *wp;
   gint cnt;
 
   for (cnt = 0; cnt < name_len; ++cnt)
     if (isalnum (codeset[cnt]))
       {
 	++len;
 
 	if (isalpha (codeset[cnt]))
 	  only_digit = 0;
       }
 
   retval = g_malloc ((only_digit ? 3 : 0) + len + 1);
 
   if (only_digit)
     {
       memcpy (retval, "iso", 4);
       wp = retval + 3;
     }
   else
     wp = retval;
   
   for (cnt = 0; cnt < name_len; ++cnt)
     if (isalpha (codeset[cnt]))
       *wp++ = isupper(codeset[cnt]) ? tolower (codeset[cnt]) : codeset[cnt];
     else if (isdigit (codeset[cnt]))
       *wp++ = codeset[cnt];
   
   *wp = '\0';
 
   return retval;
 }
 
void
gtk_rc_init (void)
{
  static gboolean initialized = FALSE;
  static gchar *locale_suffixes[3];
  static gint n_locale_suffixes = 0;
  gint i, j;


  if (!initialized)
    {
      gint length;
      gchar *locale;
      gchar *p;

#ifdef G_OS_WIN32      
      locale = g_win32_getlocale ();
#else      
      locale = setlocale (LC_CTYPE, NULL);
#endif      
      initialized = TRUE;

      pixmap_path[0] = NULL;
      module_path[0] = NULL;
      gtk_rc_append_default_module_path();
      
      gtk_rc_add_initial_default_files ();

      if (strcmp (locale, "C") && strcmp (locale, "POSIX"))
	{
	  /* Determine locale-specific suffixes for RC files
	   *
	   * We normalize the charset into a standard form,
	   * which has all '-' and '_' characters removed,
	   * and is lowercase.
	   */
	  gchar *normalized_locale;

	  p = strchr (locale, '@');
	  length = p ? (p -locale) : strlen (locale);

	  p = strchr (locale, '.');
	  if (p)
	    {
	      gchar *tmp1 = g_strndup (locale, p - locale + 1);
	      gchar *tmp2 = _gtk_normalize_codeset (p + 1, length - (p - locale + 1));
	      
	      normalized_locale = g_strconcat (tmp1, tmp2, NULL);
	      g_free (tmp1);
	      g_free (tmp2);
						 
	      locale_suffixes[n_locale_suffixes++] = g_strdup (normalized_locale);
	      length = p - locale;
	    }
	  else
	    normalized_locale = g_strndup (locale, length);
	  
	  p = strchr (normalized_locale, '_');
	  if (p)
	    {
	      locale_suffixes[n_locale_suffixes++] = g_strndup (normalized_locale, length);
	      length = p - normalized_locale;
	    }
	  
	  locale_suffixes[n_locale_suffixes++] = g_strndup (normalized_locale, length);

	  g_free (normalized_locale);
	}
    }
  
  g_object_freeze_notify (G_OBJECT (gtk_settings_get_global ()));
  for (i = 0; gtk_rc_default_files[i] != NULL; i++)
    {
      /* Try to find a locale specific RC file corresponding to the
       * current locale to parse before the default file.
       */
      for (j = n_locale_suffixes - 1; j >= 0; j--)
	{
	  gchar *name = g_strconcat (gtk_rc_default_files[i],
				     ".",
				     locale_suffixes[j],
				     NULL);
	  gtk_rc_parse (name);
	  g_free (name);
	}
      gtk_rc_parse (gtk_rc_default_files[i]);
    }
  g_object_thaw_notify (G_OBJECT (gtk_settings_get_global ()));
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

      if (g_path_is_absolute (rc_file->name))
	rc_file->canonical_name = rc_file->name;
      else
	{
	  GString *str;
	  gchar *cwd;

	  cwd = g_get_current_dir ();

	  str = g_string_new (cwd);
	  g_free (cwd);
	  g_string_append_c (str, G_DIR_SEPARATOR);
	  g_string_append (str, rc_file->name);
	  
	  rc_file->canonical_name = str->str;
	  g_string_free (str, FALSE);
	}
    }

  if (!lstat (rc_file->canonical_name, &statbuf))
    {
      gint fd;
      GSList *tmp_list;

      rc_file->mtime = statbuf.st_mtime;

      fd = open (rc_file->canonical_name, O_RDONLY);
      if (fd < 0)
	return;

      /* Temporarily push directory name for this file on
       * a stack of directory names while parsing it
       */
      rc_dir_stack = 
	g_slist_prepend (rc_dir_stack,
			 g_path_get_dirname (rc_file->canonical_name));
      gtk_rc_parse_any (filename, fd, NULL);
 
      tmp_list = rc_dir_stack;
      rc_dir_stack = rc_dir_stack->next;
 
      g_free (tmp_list->data);
      g_slist_free_1 (tmp_list);

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

GType
gtk_rc_style_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GtkRcStyleClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_rc_style_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkRcStyle),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_rc_style_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GtkRcStyle",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gtk_rc_style_init (GtkRcStyle *style)
{
  guint i;

  style->name = NULL;
  style->font_desc = NULL;

  for (i = 0; i < 5; i++)
    {
      static const GdkColor init_color = { 0, 0, 0, 0, };

      style->bg_pixmap_name[i] = NULL;
      style->color_flags[i] = 0;
      style->fg[i] = init_color;
      style->bg[i] = init_color;
      style->text[i] = init_color;
      style->base[i] = init_color;
    }
  style->xthickness = -1;
  style->ythickness = -1;
  style->rc_properties = NULL;

  style->rc_style_lists = NULL;
  style->icon_factories = NULL;
}

static void
gtk_rc_style_class_init (GtkRcStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gtk_rc_style_finalize;

  klass->parse = NULL;
  klass->create_rc_style = gtk_rc_style_real_create_rc_style;
  klass->merge = gtk_rc_style_real_merge;
  klass->create_style = gtk_rc_style_real_create_style;
}

static void
gtk_rc_style_finalize (GObject *object)
{
  GSList *tmp_list1, *tmp_list2;
  GtkRcStyle *rc_style;
  gint i;

  rc_style = GTK_RC_STYLE (object);
  
  if (rc_style->name)
    g_free (rc_style->name);
  if (rc_style->font_desc)
    pango_font_description_free (rc_style->font_desc);
      
  for (i = 0; i < 5; i++)
    if (rc_style->bg_pixmap_name[i])
      g_free (rc_style->bg_pixmap_name[i]);
  
  /* Now remove all references to this rc_style from
   * realized_style_ht
   */
  tmp_list1 = rc_style->rc_style_lists;
  while (tmp_list1)
    {
      GSList *rc_styles = tmp_list1->data;
      GtkStyle *style = g_hash_table_lookup (realized_style_ht, rc_styles);
      gtk_style_unref (style);

      /* Remove the list of styles from the other rc_styles
       * in the list
       */
      tmp_list2 = rc_styles;
      while (tmp_list2)
        {
          GtkRcStyle *other_style = tmp_list2->data;

          if (other_style != rc_style)
            other_style->rc_style_lists = g_slist_remove_all (other_style->rc_style_lists,
							      rc_styles);
          tmp_list2 = tmp_list2->next;
        }

      /* And from the hash table itself
       */
      g_hash_table_remove (realized_style_ht, rc_styles);
      g_slist_free (rc_styles);

      tmp_list1 = tmp_list1->next;
    }
  g_slist_free (rc_style->rc_style_lists);

  if (rc_style->rc_properties)
    {
      guint i;

      for (i = 0; i < rc_style->rc_properties->n_nodes; i++)
	{
	  GtkRcProperty *node = g_bsearch_array_get_nth (rc_style->rc_properties, i);

	  g_free (node->origin);
	  g_value_unset (&node->value);
	}
      g_bsearch_array_destroy (rc_style->rc_properties);
      rc_style->rc_properties = NULL;
    }

  tmp_list1 = rc_style->icon_factories;
  while (tmp_list1)
    {
      g_object_unref (G_OBJECT (tmp_list1->data));

      tmp_list1 = tmp_list1->next;
    }
  g_slist_free (rc_style->icon_factories);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkRcStyle *
gtk_rc_style_new (void)
{
  GtkRcStyle *style;
  
  style = g_object_new (GTK_TYPE_RC_STYLE, NULL);
  
  return style;
}

/**
 * gtk_rc_style_copy:
 * @orig: the style to copy
 * 
 * Make a copy of the specified #GtkRcStyle. This function
 * will correctly copy an rc style that is a member of a class
 * derived from #GtkRcStyle.
 * 
 * Return value: the resulting #GtkRcStyle
 **/
GtkRcStyle *
gtk_rc_style_copy (GtkRcStyle *orig)
{
  GtkRcStyle *style;

  g_return_val_if_fail (GTK_IS_RC_STYLE (orig), NULL);
  
  style = GTK_RC_STYLE_GET_CLASS (orig)->create_rc_style (orig);
  GTK_RC_STYLE_GET_CLASS (style)->merge (style, orig);

  return style;
}

void      
gtk_rc_style_ref (GtkRcStyle  *rc_style)
{
  g_return_if_fail (GTK_IS_RC_STYLE (rc_style));

  g_object_ref (G_OBJECT (rc_style));
}

void      
gtk_rc_style_unref (GtkRcStyle  *rc_style)
{
  g_return_if_fail (GTK_IS_RC_STYLE (rc_style));

  g_object_unref (G_OBJECT (rc_style));
}

static GtkRcStyle *
gtk_rc_style_real_create_rc_style (GtkRcStyle *style)
{
  return GTK_RC_STYLE (g_object_new (G_OBJECT_TYPE (style), NULL));
}

static gint
gtk_rc_properties_cmp (gconstpointer bsearch_node1,
		       gconstpointer bsearch_node2)
{
  const GtkRcProperty *prop1 = bsearch_node1;
  const GtkRcProperty *prop2 = bsearch_node2;
  gint cmp;

  cmp = G_BSEARCH_ARRAY_CMP (prop1->type_name, prop2->type_name);
  if (cmp == 0)
    cmp = G_BSEARCH_ARRAY_CMP (prop1->property_name, prop2->property_name);

  return cmp;
}

static void
gtk_rc_style_real_merge (GtkRcStyle *dest,
			 GtkRcStyle *src)
{
  gint i;
  
  for (i = 0; i < 5; i++)
    {
      if (!dest->bg_pixmap_name[i] && src->bg_pixmap_name[i])
	dest->bg_pixmap_name[i] = g_strdup (src->bg_pixmap_name[i]);
      
      if (!(dest->color_flags[i] & GTK_RC_FG) && 
	  src->color_flags[i] & GTK_RC_FG)
	{
	  dest->fg[i] = src->fg[i];
	  dest->color_flags[i] |= GTK_RC_FG;
	}
      if (!(dest->color_flags[i] & GTK_RC_BG) && 
	  src->color_flags[i] & GTK_RC_BG)
	{
	  dest->bg[i] = src->bg[i];
	  dest->color_flags[i] |= GTK_RC_BG;
	}
      if (!(dest->color_flags[i] & GTK_RC_TEXT) && 
	  src->color_flags[i] & GTK_RC_TEXT)
	{
	  dest->text[i] = src->text[i];
	  dest->color_flags[i] |= GTK_RC_TEXT;
	}
      if (!(dest->color_flags[i] & GTK_RC_BASE) && 
	  src->color_flags[i] & GTK_RC_BASE)
	{
	  dest->base[i] = src->base[i];
	  dest->color_flags[i] |= GTK_RC_BASE;
	}
    }

  if (dest->xthickness < 0 && src->xthickness >= 0)
    dest->xthickness = src->xthickness;
  if (dest->ythickness < 0 && src->ythickness >= 0)
    dest->ythickness = src->ythickness;

  if (!dest->font_desc && src->font_desc)
    dest->font_desc = pango_font_description_copy (src->font_desc);

  if (src->rc_properties)
    {
      guint i;

      if (!dest->rc_properties)
	dest->rc_properties = g_bsearch_array_new (sizeof (GtkRcProperty),
						   gtk_rc_properties_cmp,
						   0);
      for (i = 0; i < src->rc_properties->n_nodes; i++)
	{
	  GtkRcProperty *node = g_bsearch_array_get_nth (src->rc_properties, i);
	  GtkRcProperty *prop, key = { 0, 0, NULL, { 0, }, };

	  key.type_name = node->type_name;
	  key.property_name = node->property_name;
	  prop = g_bsearch_array_insert (dest->rc_properties, &key, FALSE);
	  if (!prop->origin)
	    {
	      prop->origin = g_strdup (node->origin);
	      g_value_init (&prop->value, G_VALUE_TYPE (&node->value));
	      g_value_copy (&node->value, &prop->value);
	    }
	}
    }
}

static GtkStyle *
gtk_rc_style_real_create_style (GtkRcStyle *rc_style)
{
  return gtk_style_new ();
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

  gtk_rc_free_rc_sets (gtk_rc_sets_widget);
  g_slist_free (gtk_rc_sets_widget);
  gtk_rc_sets_widget = NULL;

  gtk_rc_free_rc_sets (gtk_rc_sets_widget_class);
  g_slist_free (gtk_rc_sets_widget_class);
  gtk_rc_sets_widget_class = NULL;

  gtk_rc_free_rc_sets (gtk_rc_sets_class);
  g_slist_free (gtk_rc_sets_class);
  gtk_rc_sets_class = NULL;
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
		     const gchar  *path,
		     const gchar  *path_reversed)
		     
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

GtkStyle *
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
    rc_styles = g_slist_prepend (rc_styles, widget_rc_style);
  
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
	  const gchar *path;
          gchar *path_reversed;
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
    return gtk_rc_init_style (rc_styles);

  return NULL;
}

static GSList *
gtk_rc_add_rc_sets (GSList      *slist,
		    GtkRcStyle  *rc_style,
		    const gchar *pattern)
{
  GtkRcStyle *new_style;
  GtkRcSet *rc_set;
  guint i;
  
  new_style = gtk_rc_style_new ();
  *new_style = *rc_style;
  new_style->name = g_strdup (rc_style->name);
  if (rc_style->font_desc)
    new_style->font_desc = pango_font_description_copy (rc_style->font_desc);
  
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

GScanner*
gtk_rc_scanner_new (void)
{
  return g_scanner_new (&gtk_rc_scanner_config);
}

static void
gtk_rc_parse_any (const gchar  *input_name,
		  gint		input_fd,
		  const gchar  *input_string)
{
  GScanner *scanner;
  guint	   i;
  gboolean done;

  scanner = gtk_rc_scanner_new ();
  
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

  for (i = 0; i < n_symbols; i++)
    g_scanner_add_symbol (scanner, symbols[i].name, GINT_TO_POINTER (symbols[i].token));
  
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

static gboolean
gtk_rc_styles_equal (const GSList *a,
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
gtk_rc_style_hash (const gchar *name)
{
  guint result;
  
  result = 0;
  while (*name)
    result += (result << 3) + *name++;
  
  return result;
}

static gboolean
gtk_rc_style_equal (const gchar *a,
		    const gchar *b)
{
  return (strcmp (a, b) == 0);
}

static GtkRcStyle*
gtk_rc_style_find (const gchar *name)
{
  if (rc_style_ht)
    return g_hash_table_lookup (rc_style_ht, (gpointer) name);
  else
    return NULL;
}

static GtkStyle *
gtk_rc_style_to_style (GtkRcStyle *rc_style)
{
  GtkStyle *style;

  style = GTK_RC_STYLE_GET_CLASS (rc_style)->create_style (rc_style);

  style->rc_style = rc_style;

  gtk_rc_style_ref (rc_style);
  
  GTK_STYLE_GET_CLASS (style)->init_from_rc (style, rc_style);  

  return style;
}

/* Reuses or frees rc_styles */
static GtkStyle *
gtk_rc_init_style (GSList *rc_styles)
{
  GtkStyle *style = NULL;
  gint i;

  g_return_val_if_fail (rc_styles != NULL, NULL);
  
  if (!realized_style_ht)
    realized_style_ht = g_hash_table_new ((GHashFunc) gtk_rc_styles_hash,
					  (GEqualFunc) gtk_rc_styles_equal);

  style = g_hash_table_lookup (realized_style_ht, rc_styles);

  if (!style)
    {
      GtkRcStyle *base_style = NULL;
      GtkRcStyle *proto_style;
      GtkRcStyleClass *proto_style_class;
      GSList *tmp_styles;
      GType rc_style_type = GTK_TYPE_RC_STYLE;

      /* Find the first derived style in the list, and use that to
       * create the merged style. If we only have raw GtkRcStyles, use
       * the first style to create the merged style.
       */
      base_style = rc_styles->data;
      tmp_styles = rc_styles;
      while (tmp_styles)
	{
	  GtkRcStyle *rc_style = tmp_styles->data;
          
	  if (G_OBJECT_TYPE (rc_style) != rc_style_type)
	    {
	      base_style = rc_style;
	      break;
	    }
          
	  tmp_styles = tmp_styles->next;
	}
      
      proto_style_class = GTK_RC_STYLE_GET_CLASS (base_style);
      proto_style = proto_style_class->create_rc_style (base_style);
      
      tmp_styles = rc_styles;
      while (tmp_styles)
	{
	  GtkRcStyle *rc_style = tmp_styles->data;
          GSList *factories;
          
	  proto_style_class->merge (proto_style, rc_style);	  
          
	  /* Point from each rc_style to the list of styles */
	  if (!g_slist_find (rc_style->rc_style_lists, rc_styles))
	    rc_style->rc_style_lists = g_slist_prepend (rc_style->rc_style_lists, rc_styles);

          factories = g_slist_copy (rc_style->icon_factories);
          if (factories)
            {
              GSList *iter;
              
              iter = factories;
              while (iter != NULL)
                {
                  g_object_ref (G_OBJECT (iter->data));
                  iter = g_slist_next (iter);
                }

              proto_style->icon_factories = g_slist_concat (proto_style->icon_factories,
                                                            factories);

            }
          
	  tmp_styles = tmp_styles->next;
	}

      for (i = 0; i < 5; i++)
	if (proto_style->bg_pixmap_name[i] &&
	    (strcmp (proto_style->bg_pixmap_name[i], "<none>") == 0))
	  {
	    g_free (proto_style->bg_pixmap_name[i]);
	    proto_style->bg_pixmap_name[i] = NULL;
	  }

      style = gtk_rc_style_to_style (proto_style);
      gtk_rc_style_unref (proto_style);

      g_hash_table_insert (realized_style_ht, rc_styles, style);
    }
  else
    g_slist_free (rc_styles);

  return style;
}

/*********************
 * Parsing functions *
 *********************/

static guint
rc_parse_token_or_compound (GScanner  *scanner,
			    GString   *gstring,
			    GTokenType delimiter)
{
  guint token = g_scanner_get_next_token (scanner);

  /* we either scan a single token (skipping comments)
   * or a compund statement.
   * compunds are enclosed in (), [] or {} braces, we read
   * them in via deep recursion.
   */

  switch (token)
    {
      gchar *string;
    case G_TOKEN_INT:
      g_string_printfa (gstring, " 0x%lx", scanner->value.v_int);
      break;
    case G_TOKEN_FLOAT:
      g_string_printfa (gstring, " %f", scanner->value.v_float);
      break;
    case G_TOKEN_STRING:
      string = g_strescape (scanner->value.v_string, NULL);
      g_string_append (gstring, " \"");
      g_string_append (gstring, string);
      g_string_append_c (gstring, '"');
      g_free (string);
      break;
    case G_TOKEN_IDENTIFIER:
      g_string_append_c (gstring, ' ');
      g_string_append (gstring, scanner->value.v_identifier);
      break;
    case G_TOKEN_COMMENT_SINGLE:
    case G_TOKEN_COMMENT_MULTI:
      return rc_parse_token_or_compound (scanner, gstring, delimiter);
    case G_TOKEN_LEFT_PAREN:
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      token = rc_parse_token_or_compound (scanner, gstring, G_TOKEN_RIGHT_PAREN);
      if (token != G_TOKEN_NONE)
	return token;
      break;
    case G_TOKEN_LEFT_CURLY:
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      token = rc_parse_token_or_compound (scanner, gstring, G_TOKEN_RIGHT_CURLY);
      if (token != G_TOKEN_NONE)
	return token;
      break;
    case G_TOKEN_LEFT_BRACE:
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      token = rc_parse_token_or_compound (scanner, gstring, G_TOKEN_RIGHT_BRACE);
      if (token != G_TOKEN_NONE)
	return token;
      break;
    default:
      if (token >= 256 || token < 1)
	return delimiter ? delimiter : G_TOKEN_STRING;
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      if (token == delimiter)
	return G_TOKEN_NONE;
      break;
    }
  if (!delimiter)
    return G_TOKEN_NONE;
  else
    return rc_parse_token_or_compound (scanner, gstring, delimiter);
}

static guint
gtk_rc_parse_assignment (GScanner      *scanner,
			 GtkRcProperty *prop)
{
  gboolean scan_identifier = scanner->config->scan_identifier;
  gboolean scan_symbols = scanner->config->scan_symbols;
  gboolean identifier_2_string = scanner->config->identifier_2_string;
  gboolean char_2_token = scanner->config->char_2_token;
  gboolean scan_identifier_NULL = scanner->config->scan_identifier_NULL;
  gboolean numbers_2_int = scanner->config->numbers_2_int;
  gboolean negate = FALSE;
  guint token;

  /* check that this is an assignment */
  if (g_scanner_get_next_token (scanner) != '=')
    return '=';

  /* adjust scanner mode */
  scanner->config->scan_identifier = TRUE;
  scanner->config->scan_symbols = FALSE;
  scanner->config->identifier_2_string = FALSE;
  scanner->config->char_2_token = TRUE;
  scanner->config->scan_identifier_NULL = FALSE;
  scanner->config->numbers_2_int = TRUE;

  /* record location */
  prop->origin = g_strdup_printf ("%s:%u", scanner->input_name, scanner->line);

  /* parse optional sign */
  if (g_scanner_peek_next_token (scanner) == '-')
    {
      g_scanner_get_next_token (scanner); /* eat sign */
      negate = TRUE;
    }

  /* parse one of LONG, DOUBLE and STRING or, if that fails, create an unparsed compund */
  token = g_scanner_peek_next_token (scanner);
  switch (token)
    {
    case G_TOKEN_INT:
      g_scanner_get_next_token (scanner);
      g_value_init (&prop->value, G_TYPE_LONG);
      g_value_set_long (&prop->value, negate ? -scanner->value.v_int : scanner->value.v_int);
      token = G_TOKEN_NONE;
      break;
    case G_TOKEN_FLOAT:
      g_scanner_get_next_token (scanner);
      g_value_init (&prop->value, G_TYPE_DOUBLE);
      g_value_set_double (&prop->value, negate ? -scanner->value.v_float : scanner->value.v_float);
      token = G_TOKEN_NONE;
      break;
    case G_TOKEN_STRING:
      g_scanner_get_next_token (scanner);
      if (negate)
	token = G_TOKEN_INT;
      else
	{
	  g_value_init (&prop->value, G_TYPE_STRING);
	  g_value_set_string (&prop->value, scanner->value.v_string);
	  token = G_TOKEN_NONE;
	}
      break;
    case G_TOKEN_IDENTIFIER:
    case G_TOKEN_LEFT_PAREN:
    case G_TOKEN_LEFT_CURLY:
    case G_TOKEN_LEFT_BRACE:
      if (!negate)
	{
	  GString *gstring = g_string_new ("");

	  token = rc_parse_token_or_compound (scanner, gstring, 0);
	  if (token == G_TOKEN_NONE)
	    {
	      g_string_append_c (gstring, ' ');
	      g_value_init (&prop->value, G_TYPE_GSTRING);
	      g_value_set_static_boxed (&prop->value, gstring);
	    }
	  else
	    g_string_free (gstring, TRUE);
	  break;
	}
      /* fall through */
    default:
      g_scanner_get_next_token (scanner);
      token = G_TOKEN_INT;
      break;
    }

  /* restore scanner mode */
  scanner->config->scan_identifier = scan_identifier;
  scanner->config->scan_symbols = scan_symbols;
  scanner->config->identifier_2_string = identifier_2_string;
  scanner->config->char_2_token = char_2_token;
  scanner->config->scan_identifier_NULL = scan_identifier_NULL;
  scanner->config->numbers_2_int = numbers_2_int;

  return token;
}

static gboolean
is_c_identifier (const gchar *string)
{
  const gchar *p;
  gboolean is_varname;

  is_varname = strchr (G_CSET_a_2_z G_CSET_A_2_Z "_", string[0]) != NULL;
  for (p = string + 1; *p && is_varname; p++)
    is_varname &= strchr (G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "_-", *p) != NULL;

  return is_varname;
}

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
      
    case GTK_RC_TOKEN_IM_MODULE_PATH:
      return gtk_rc_parse_im_module_path (scanner);
      
    case GTK_RC_TOKEN_IM_MODULE_FILE:
      return gtk_rc_parse_im_module_file (scanner);

    case G_TOKEN_IDENTIFIER:
      if (is_c_identifier (scanner->next_value.v_identifier))
	{
	  GtkRcProperty prop = { 0, 0, NULL, { 0, }, };
	  gchar *name;
	  
	  g_scanner_get_next_token (scanner); /* eat identifier */
	  name = g_strdup (scanner->value.v_identifier);
	  
	  token = gtk_rc_parse_assignment (scanner, &prop);
	  if (token == G_TOKEN_NONE)
	    {
	      GtkSettingsValue svalue;

	      svalue.origin = prop.origin;
	      memcpy (&svalue.value, &prop.value, sizeof (prop.value));
	      g_strcanon (name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');
	      gtk_settings_set_property_value (gtk_settings_get_global (),
					       name,
					       &svalue);
	    }
	  g_free (prop.origin);
	  if (G_VALUE_TYPE (&prop.value))
	    g_value_unset (&prop.value);
	  g_free (name);
	  
	  return token;
	}
      else
	{
	  g_scanner_get_next_token (scanner);
	  return G_TOKEN_IDENTIFIER;
	}
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
  GtkIconFactory *our_factory = NULL;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_STYLE)
    return GTK_RC_TOKEN_STYLE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  insert = FALSE;
  rc_style = gtk_rc_style_find (scanner->value.v_string);

  /* If there's a list, its first member is always the factory belonging
   * to this RcStyle
   */
  if (rc_style && rc_style->icon_factories)
    our_factory = rc_style->icon_factories->data;
  
  if (!rc_style)
    {
      insert = TRUE;
      rc_style = gtk_rc_style_new ();
      rc_style->name = g_strdup (scanner->value.v_string);
      
      for (i = 0; i < 5; i++)
	rc_style->bg_pixmap_name[i] = NULL;

      for (i = 0; i < 5; i++)
	rc_style->color_flags[i] = 0;
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
      
      parent_style = gtk_rc_style_find (scanner->value.v_string);
      if (parent_style)
	{
          GSList *factories;
          
	  for (i = 0; i < 5; i++)
	    {
	      rc_style->color_flags[i] = parent_style->color_flags[i];
	      rc_style->fg[i] = parent_style->fg[i];
	      rc_style->bg[i] = parent_style->bg[i];
	      rc_style->text[i] = parent_style->text[i];
	      rc_style->base[i] = parent_style->base[i];
	    }

	  rc_style->xthickness = parent_style->xthickness;
	  rc_style->ythickness = parent_style->ythickness;
	  
	  if (parent_style->font_desc)
	    {
	      if (rc_style->font_desc)
		pango_font_description_free (rc_style->font_desc);
	      rc_style->font_desc = pango_font_description_copy (parent_style->font_desc);
	    }

	  if (parent_style->rc_properties)
	    {
	      guint i;

	      if (!rc_style->rc_properties)
		rc_style->rc_properties = g_bsearch_array_new (sizeof (GtkRcProperty),
							       gtk_rc_properties_cmp,
							       0);
	      for (i = 0; i < parent_style->rc_properties->n_nodes; i++)
		{
		  GtkRcProperty *node = g_bsearch_array_get_nth (parent_style->rc_properties, i);
		  GtkRcProperty *prop, key = { 0, 0, NULL, { 0, }, };

		  key.type_name = node->type_name;
		  key.property_name = node->property_name;
		  prop = g_bsearch_array_insert (rc_style->rc_properties, &key, FALSE);
		  if (prop->origin)
		    {
		      g_free (prop->origin);
		      g_value_unset (&prop->value);
		    }
		  prop->origin = g_strdup (node->origin);
		  g_value_init (&prop->value, G_VALUE_TYPE (&node->value));
		  g_value_copy (&node->value, &prop->value);
		}
	    }
	  
	  for (i = 0; i < 5; i++)
	    {
	      if (rc_style->bg_pixmap_name[i])
		g_free (rc_style->bg_pixmap_name[i]);
	      rc_style->bg_pixmap_name[i] = g_strdup (parent_style->bg_pixmap_name[i]);
	    }
          
          /* Append parent's factories, adding a ref to them */
          if (parent_style->icon_factories != NULL)
            {
              /* Add a factory for ourselves if we have none,
               * in case we end up defining more stock icons.
               * I see no real way around this; we need to maintain
               * the invariant that the first factory in the list
               * is always our_factory, the one belonging to us,
               * and if we put parent factories in the list we can't
               * do that if the style is reopened.
               */
              if (our_factory == NULL)
                {
                  our_factory = gtk_icon_factory_new ();
                  rc_style->icon_factories = g_slist_prepend (rc_style->icon_factories,
                                                              our_factory);
                }
              
              rc_style->icon_factories = g_slist_concat (rc_style->icon_factories,
                                                         g_slist_copy (parent_style->icon_factories));
              
              factories = parent_style->icon_factories;
              while (factories != NULL)
                {
                  g_object_ref (G_OBJECT (factories->data));
                  factories = factories->next;
                }
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
	case GTK_RC_TOKEN_BG:
	  token = gtk_rc_parse_bg (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FG:
	  token = gtk_rc_parse_fg (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_TEXT:
	  token = gtk_rc_parse_text (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_BASE:
	  token = gtk_rc_parse_base (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_XTHICKNESS:
	  token = gtk_rc_parse_xthickness (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_YTHICKNESS:
	  token = gtk_rc_parse_ythickness (scanner, rc_style);
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
	case GTK_RC_TOKEN_FONT_NAME:
	  token = gtk_rc_parse_font_name (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_ENGINE:
	  token = gtk_rc_parse_engine (scanner, &rc_style);
	  break;
        case GTK_RC_TOKEN_STOCK:
          if (our_factory == NULL)
            {
              our_factory = gtk_icon_factory_new ();
              rc_style->icon_factories = g_slist_prepend (rc_style->icon_factories,
                                                          our_factory);
            }
          token = gtk_rc_parse_stock (scanner, rc_style, our_factory);
          break;
	case G_TOKEN_IDENTIFIER:
	  if (is_c_identifier (scanner->next_value.v_identifier) &&
	      scanner->next_value.v_identifier[0] >= 'A' &&
	      scanner->next_value.v_identifier[0] <= 'Z') /* match namespaced type names */
	    {
	      GtkRcProperty prop = { 0, 0, NULL, { 0, }, };
	      
	      g_scanner_get_next_token (scanner); /* eat type name */
	      prop.type_name = g_quark_from_string (scanner->value.v_identifier);
	      if (g_scanner_get_next_token (scanner) != ':' ||
		  g_scanner_get_next_token (scanner) != ':')
		{
		  token = ':';
		  break;
		}
	      if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER ||
		  !is_c_identifier (scanner->value.v_identifier))
		{
		  token = G_TOKEN_IDENTIFIER;
		  break;
		}

	      /* it's important that we do the same canonification as GParamSpecPool here */
	      g_strcanon (scanner->value.v_identifier, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');
	      prop.property_name = g_quark_from_string (scanner->value.v_identifier);

	      token = gtk_rc_parse_assignment (scanner, &prop);
	      if (token == G_TOKEN_NONE)
		{
		  GtkRcProperty *tmp;

		  g_return_val_if_fail (prop.origin != NULL && G_VALUE_TYPE (&prop.value) != 0, G_TOKEN_ERROR);

		  if (!rc_style->rc_properties)
		    rc_style->rc_properties = g_bsearch_array_new (sizeof (GtkRcProperty),
								   gtk_rc_properties_cmp,
								   0);
		  tmp = g_bsearch_array_insert (rc_style->rc_properties, &prop, FALSE);
		  if (prop.origin != tmp->origin)
		    {
		      g_free (tmp->origin);
		      g_value_unset (&tmp->value);
		      tmp->origin = prop.origin;
		      memcpy (&tmp->value, &prop.value, sizeof (prop.value));
		    }
		}
	      else
		{
		  g_free (prop.origin);
		  if (G_VALUE_TYPE (&prop.value))
		    g_value_unset (&prop.value);
		}
	    }
	  else
	    {
	      g_scanner_get_next_token (scanner);
	      token = G_TOKEN_IDENTIFIER;
	    }
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	{
	  if (insert)
	    gtk_rc_style_unref (rc_style);

	  return token;
	}
      token = g_scanner_peek_next_token (scanner);
    } /* while (token != G_TOKEN_RIGHT_CURLY) */
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    {
      if (insert)
	{
	  if (rc_style->font_desc)
	    pango_font_description_free (rc_style->font_desc);
	  
	  for (i = 0; i < 5; i++)
	    if (rc_style->bg_pixmap_name[i])
	      g_free (rc_style->bg_pixmap_name[i]);
	  
	  g_free (rc_style);
	}
      return G_TOKEN_RIGHT_CURLY;
    }
  
  if (insert)
    {
      if (!rc_style_ht)
	rc_style_ht = g_hash_table_new ((GHashFunc) gtk_rc_style_hash,
					(GEqualFunc) gtk_rc_style_equal);
      
      g_hash_table_insert (rc_style_ht, rc_style->name, rc_style);
    }
  
  return G_TOKEN_NONE;
}

const GtkRcProperty*
_gtk_rc_style_lookup_rc_property (GtkRcStyle *rc_style,
				  GQuark      type_name,
				  GQuark      property_name)
{
  GtkRcProperty *node = NULL;

  g_return_val_if_fail (GTK_IS_RC_STYLE (rc_style), NULL);

  if (rc_style->rc_properties)
    {
      GtkRcProperty key;

      key.type_name = type_name;
      key.property_name = property_name;

      node = g_bsearch_array_lookup (rc_style->rc_properties, &key);
    }

  return node;
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
gtk_rc_parse_xthickness (GScanner   *scanner,
			 GtkRcStyle *style)
{
  if (g_scanner_get_next_token (scanner) != GTK_RC_TOKEN_XTHICKNESS)
    return GTK_RC_TOKEN_XTHICKNESS;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_INT)
    return G_TOKEN_INT;

  style->xthickness = scanner->value.v_int;

  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_ythickness (GScanner   *scanner,
			 GtkRcStyle *style)
{
  if (g_scanner_get_next_token (scanner) != GTK_RC_TOKEN_YTHICKNESS)
    return GTK_RC_TOKEN_YTHICKNESS;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_INT)
    return G_TOKEN_INT;

  style->ythickness = scanner->value.v_int;

  return G_TOKEN_NONE;
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
  
  if ((strcmp (scanner->value.v_string, "<parent>") == 0) ||
      (strcmp (scanner->value.v_string, "<none>") == 0))
    pixmap_file = g_strdup (scanner->value.v_string);
  else
    pixmap_file = gtk_rc_find_pixmap_in_path (scanner, scanner->value.v_string);
  
  if (pixmap_file)
    {
      if (rc_style->bg_pixmap_name[state])
	g_free (rc_style->bg_pixmap_name[state]);
      rc_style->bg_pixmap_name[state] = pixmap_file;
    }
  
  return G_TOKEN_NONE;
}

static gchar*
gtk_rc_check_pixmap_dir (const gchar *dir, const gchar *pixmap_file)
{
  gchar *buf;
  gint fd;

  buf = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s", dir, pixmap_file);
  
  fd = open (buf, O_RDONLY);
  if (fd >= 0)
    {
      close (fd);
      return buf;
    }
   
  g_free (buf);
 
   return NULL;
 }
 
gchar*
gtk_rc_find_pixmap_in_path (GScanner *scanner,
  			    const gchar *pixmap_file)
{
  gint i;
  gchar *filename;
  GSList *tmp_list;
    
  for (i = 0; (i < GTK_RC_MAX_PIXMAP_PATHS) && (pixmap_path[i] != NULL); i++)
    {
      filename = gtk_rc_check_pixmap_dir (pixmap_path[i], pixmap_file);
      if (filename)
 	return filename;
    }
 
  tmp_list = rc_dir_stack;
  while (tmp_list)
    {
      filename = gtk_rc_check_pixmap_dir (tmp_list->data, pixmap_file);
      if (filename)
 	return filename;
       
      tmp_list = tmp_list->next;
    }
  
  if (scanner)
    g_warning (_("Unable to locate image file in pixmap_path: \"%s\" line %d"),
	       pixmap_file, scanner->line);
  else
    g_warning (_("Unable to locate image file in pixmap_path: \"%s\""),
	       pixmap_file);
    
  return NULL;
}

gchar*
gtk_rc_find_module_in_path (const gchar *module_file)
{
  gint i;
  gint fd;
  gchar *buf;
  
  for (i = 0; (i < GTK_RC_MAX_MODULE_PATHS) && (module_path[i] != NULL); i++)
    {
      buf = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s",
			     module_path[i], module_file);
      
      fd = open (buf, O_RDONLY);
      if (fd >= 0)
	{
	  close (fd);
	  return buf;
	}
      
      g_free (buf);
    }
    
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

  /* Ignore, do nothing */
  
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

  /* Do nothing - silently ignore */
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_font_name (GScanner   *scanner,
			GtkRcStyle *rc_style)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_FONT_NAME)
    return GTK_RC_TOKEN_FONT;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  rc_style->font_desc = pango_font_description_from_string (scanner->value.v_string);
  
  return G_TOKEN_NONE;
}

static guint	   
gtk_rc_parse_engine (GScanner	 *scanner,
		     GtkRcStyle	**rc_style)
{
  guint token;
  GtkThemeEngine *engine;
  guint result = G_TOKEN_NONE;
  GtkRcStyle *new_style = NULL;
  gboolean parsed_curlies = FALSE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_ENGINE)
    return GTK_RC_TOKEN_ENGINE;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  engine = gtk_theme_engine_get (scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  if (engine)
    {
      GtkRcStyleClass *new_class;
      
      new_style = gtk_theme_engine_create_rc_style (engine);
      g_type_module_unuse (G_TYPE_MODULE (engine));

      new_class = GTK_RC_STYLE_GET_CLASS (new_style);

      new_class->merge (new_style, *rc_style);
      if ((*rc_style)->name)
	new_style->name = g_strdup ((*rc_style)->name);
      
      if (new_class->parse)
	{
	  parsed_curlies = TRUE;
	  result = new_class->parse (new_style, scanner);

	  if (result != G_TOKEN_NONE)
	    {
	      g_object_unref (G_OBJECT (new_style));
	      new_style = NULL;
	    }
	}
    }

  if (!parsed_curlies)
    {
      /* Skip over remainder, looking for nested {}'s
       */
      guint count = 1;
      
      result = G_TOKEN_RIGHT_CURLY;
      while ((token = g_scanner_get_next_token (scanner)) != G_TOKEN_EOF)
	{
	  if (token == G_TOKEN_LEFT_CURLY)
	    count++;
	  else if (token == G_TOKEN_RIGHT_CURLY)
	    count--;
	  
	  if (count == 0)
	    {
	      result = G_TOKEN_NONE;
	      break;
	    }
	}
    }

  if (new_style)
    {
      g_object_unref (G_OBJECT (*rc_style));
      *rc_style = new_style;
    }

  return result;
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

  /* we don't need to set our own scope here, because
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
      if ((buf[end_offset] == G_SEARCHPATH_SEPARATOR) ||
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

static guint
gtk_rc_parse_im_module_path (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_IM_MODULE_FILE)
    return GTK_RC_TOKEN_IM_MODULE_FILE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  if (im_module_path)
    g_free (im_module_path);
    
  im_module_path = g_strdup (scanner->value.v_string);

  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_im_module_file (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_IM_MODULE_FILE)
    return GTK_RC_TOKEN_IM_MODULE_FILE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  if (im_module_file)
    g_free (im_module_file);
    
  im_module_file = g_strdup (scanner->value.v_string);

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
      if ((buf[end_offset] == G_SEARCHPATH_SEPARATOR) ||
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

static guint
gtk_rc_parse_stock_id (GScanner	 *scanner,
                       gchar    **stock_id)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_BRACE)
    return G_TOKEN_LEFT_BRACE;

  token = g_scanner_get_next_token (scanner);
  
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  *stock_id = g_strdup (scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    {
      g_free (*stock_id);
      return G_TOKEN_RIGHT_BRACE;
    }
  
  return G_TOKEN_NONE;
}

static void
cleanup_source (GtkIconSource *source)
{
  g_free (source->filename);
}

static guint
gtk_rc_parse_icon_source (GScanner	 *scanner,
                          GtkIconSet     *icon_set)
{
  guint token;
  GtkIconSource source = { NULL, NULL,
                           0, 0, 0,
                           TRUE, TRUE, TRUE };
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token (scanner);
  
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  source.filename = g_strdup (scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);

  if (token == G_TOKEN_RIGHT_CURLY)
    {
      gtk_icon_set_add_source (icon_set, &source);
      cleanup_source (&source);
      return G_TOKEN_NONE;
    }  
  else if (token != G_TOKEN_COMMA)
    {
      cleanup_source (&source);
      return G_TOKEN_COMMA;
    }

  /* Get the direction */
  
  token = g_scanner_get_next_token (scanner);

  switch (token)
    {
    case GTK_RC_TOKEN_RTL:
      source.any_direction = FALSE;
      source.direction = GTK_TEXT_DIR_RTL;
      break;

    case GTK_RC_TOKEN_LTR:
      source.any_direction = FALSE;
      source.direction = GTK_TEXT_DIR_LTR;
      break;
      
    case '*':
      break;
      
    default:
      cleanup_source (&source);
      return GTK_RC_TOKEN_RTL;
      break;
    }

  token = g_scanner_get_next_token (scanner);

  if (token == G_TOKEN_RIGHT_CURLY)
    {
      gtk_icon_set_add_source (icon_set, &source);
      cleanup_source (&source);
      return G_TOKEN_NONE;
    }  
  else if (token != G_TOKEN_COMMA)
    {
      cleanup_source (&source);
      return G_TOKEN_COMMA;
    }

  /* Get the state */
  
  token = g_scanner_get_next_token (scanner);
  
  switch (token)
    {
    case GTK_RC_TOKEN_NORMAL:
      source.any_state = FALSE;
      source.state = GTK_STATE_NORMAL;
      break;

    case GTK_RC_TOKEN_PRELIGHT:
      source.any_state = FALSE;
      source.state = GTK_STATE_PRELIGHT;
      break;
      

    case GTK_RC_TOKEN_INSENSITIVE:
      source.any_state = FALSE;
      source.state = GTK_STATE_INSENSITIVE;
      break;

    case GTK_RC_TOKEN_ACTIVE:
      source.any_state = FALSE;
      source.state = GTK_STATE_ACTIVE;
      break;

    case GTK_RC_TOKEN_SELECTED:
      source.any_state = FALSE;
      source.state = GTK_STATE_SELECTED;
      break;

    case '*':
      break;
      
    default:
      cleanup_source (&source);
      return GTK_RC_TOKEN_PRELIGHT;
      break;
    }  

  token = g_scanner_get_next_token (scanner);

  if (token == G_TOKEN_RIGHT_CURLY)
    {
      gtk_icon_set_add_source (icon_set, &source);
      cleanup_source (&source);
      return G_TOKEN_NONE;
    }
  else if (token != G_TOKEN_COMMA)
    {
      cleanup_source (&source);
      return G_TOKEN_COMMA;
    }
  
  /* Get the size */
  
  token = g_scanner_get_next_token (scanner);

  if (token != '*')
    {
      GtkIconSize size;
      
      if (token != G_TOKEN_STRING)
        {
          cleanup_source (&source);
          return G_TOKEN_STRING;
        }

      size = gtk_icon_size_from_name (scanner->value.v_string);

      if (size != GTK_ICON_SIZE_INVALID)
        {
          source.size = size;
          source.any_size = FALSE;
        }
    }

  /* Check the close brace */
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    {
      cleanup_source (&source);
      return G_TOKEN_RIGHT_CURLY;
    }

  gtk_icon_set_add_source (icon_set, &source);

  cleanup_source (&source);
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_stock (GScanner       *scanner,
                    GtkRcStyle     *rc_style,
                    GtkIconFactory *factory)
{
  GtkIconSet *icon_set = NULL;
  gchar *stock_id = NULL;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_STOCK)
    return GTK_RC_TOKEN_STOCK;
  
  token = gtk_rc_parse_stock_id (scanner, &stock_id);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    {
      g_free (stock_id);
      return G_TOKEN_EQUAL_SIGN;
    }

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    {
      g_free (stock_id);
      return G_TOKEN_LEFT_CURLY;
    }

  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      if (icon_set == NULL)
        icon_set = gtk_icon_set_new ();
      
      token = gtk_rc_parse_icon_source (scanner, icon_set);
      if (token != G_TOKEN_NONE)
        {
          g_free (stock_id);
          gtk_icon_set_unref (icon_set);
          return token;
        }

      token = g_scanner_get_next_token (scanner);
      
      if (token != G_TOKEN_COMMA &&
          token != G_TOKEN_RIGHT_CURLY)
        {
          g_free (stock_id);
          gtk_icon_set_unref (icon_set);
          return G_TOKEN_RIGHT_CURLY;
        }
    }

  if (icon_set)
    {
      gtk_icon_factory_add (factory,
                            stock_id,
                            icon_set);
      
      gtk_icon_set_unref (icon_set);
    }
  
  g_free (stock_id);

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
