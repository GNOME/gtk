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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>
#include <gmodule.h>
#include "gtkimmoduleprivate.h"
#include "gtkimcontextsimple.h"
#include "gtksettings.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#undef GDK_DEPRECATED
#undef GDK_DEPRECATED_FOR
#define GDK_DEPRECATED
#define GDK_DEPRECATED_FOR(f)

#include "deprecated/gtkrc.h"

#define SIMPLE_ID "gtk-im-context-simple"
#define NONE_ID   "gtk-im-context-none"

/**
 * GtkIMContextInfo:
 * @context_id: The unique identification string of the input method.
 * @context_name: The human-readable name of the input method.
 * @domain: Translation domain to be used with dgettext()
 * @domain_dirname: Name of locale directory for use with bindtextdomain()
 * @default_locales: A colon-separated list of locales where this input method
 *   should be the default. The asterisk “*” sets the default for all locales.
 *
 * Bookkeeping information about a loadable input method.
 */

typedef struct _GtkIMModule      GtkIMModule;
typedef struct _GtkIMModuleClass GtkIMModuleClass;

#define GTK_TYPE_IM_MODULE          (gtk_im_module_get_type ())
#define GTK_IM_MODULE(im_module)    (G_TYPE_CHECK_INSTANCE_CAST ((im_module), GTK_TYPE_IM_MODULE, GtkIMModule))
#define GTK_IS_IM_MODULE(im_module) (G_TYPE_CHECK_INSTANCE_TYPE ((im_module), GTK_TYPE_IM_MODULE))

struct _GtkIMModule
{
  GTypeModule parent_instance;
  
  gboolean builtin;

  GModule *library;

  void          (*list)   (const GtkIMContextInfo ***contexts,
 		           guint                    *n_contexts);
  void          (*init)   (GTypeModule              *module);
  void          (*exit)   (void);
  GtkIMContext *(*create) (const gchar              *context_id);

  GtkIMContextInfo **contexts;
  guint n_contexts;

  gchar *path;
};

struct _GtkIMModuleClass 
{
  GTypeModuleClass parent_class;
};

static GType gtk_im_module_get_type (void);

static gint n_loaded_contexts = 0;
static GHashTable *contexts_hash = NULL;
static GSList *modules_list = NULL;

static GObjectClass *parent_class = NULL;

static gboolean
gtk_im_module_load (GTypeModule *module)
{
  GtkIMModule *im_module = GTK_IM_MODULE (module);
  
  if (!im_module->builtin)
    {
      im_module->library = g_module_open (im_module->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
      if (!im_module->library)
	{
	  g_warning ("%s", g_module_error());
	  return FALSE;
	}
  
      /* extract symbols from the lib */
      if (!g_module_symbol (im_module->library, "im_module_init",
			    (gpointer *)&im_module->init) ||
	  !g_module_symbol (im_module->library, "im_module_exit", 
			    (gpointer *)&im_module->exit) ||
	  !g_module_symbol (im_module->library, "im_module_list", 
			    (gpointer *)&im_module->list) ||
	  !g_module_symbol (im_module->library, "im_module_create", 
			    (gpointer *)&im_module->create))
	{
	  g_warning ("%s", g_module_error());
	  g_module_close (im_module->library);
	  
	  return FALSE;
	}
    }
	    
  /* call the module's init function to let it */
  /* setup anything it needs to set up. */
  im_module->init (module);

  return TRUE;
}

static void
gtk_im_module_unload (GTypeModule *module)
{
  GtkIMModule *im_module = GTK_IM_MODULE (module);
  
  im_module->exit();

  if (!im_module->builtin)
    {
      g_module_close (im_module->library);
      im_module->library = NULL;

      im_module->init = NULL;
      im_module->exit = NULL;
      im_module->list = NULL;
      im_module->create = NULL;
    }
}

/* This only will ever be called if an error occurs during
 * initialization
 */
static void
gtk_im_module_finalize (GObject *object)
{
  GtkIMModule *module = GTK_IM_MODULE (object);

  g_free (module->path);

  parent_class->finalize (object);
}

G_DEFINE_TYPE (GtkIMModule, gtk_im_module, G_TYPE_TYPE_MODULE)

static void
gtk_im_module_class_init (GtkIMModuleClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (class));
  
  module_class->load = gtk_im_module_load;
  module_class->unload = gtk_im_module_unload;

  gobject_class->finalize = gtk_im_module_finalize;
}

static void 
gtk_im_module_init (GtkIMModule* object)
{
}

static void
free_info (GtkIMContextInfo *info)
{
  g_free ((char *)info->context_id);
  g_free ((char *)info->context_name);
  g_free ((char *)info->domain);
  g_free ((char *)info->domain_dirname);
  g_free ((char *)info->default_locales);
  g_free (info);
}

static void
add_module (GtkIMModule *module, GSList *infos)
{
  GSList *tmp_list = infos;
  gint i = 0;
  gint n = g_slist_length (infos);
  module->contexts = g_new (GtkIMContextInfo *, n);

  while (tmp_list)
    {
      GtkIMContextInfo *info = tmp_list->data;
  
      if (g_hash_table_lookup (contexts_hash, info->context_id))
	{
	  free_info (info);	/* Duplicate */
	}
      else
	{
	  g_hash_table_insert (contexts_hash, (char *)info->context_id, module);
	  module->contexts[i++] = tmp_list->data;
	  n_loaded_contexts++;
	}
      
      tmp_list = tmp_list->next;
    }
  g_slist_free (infos);
  module->n_contexts = i;

  modules_list = g_slist_prepend (modules_list, module);
}

#ifdef G_OS_WIN32

static void
correct_libdir_prefix (gchar **path)
{
  /* GTK_LIBDIR is the build-time libdir */
  if (strncmp (*path, GTK_LIBDIR, strlen (GTK_LIBDIR)) == 0)
    {
      /* This is an entry put there by make install on the
       * packager's system. On Windows a prebuilt GTK+
       * package can be installed in a random
       * location. The gtk.immodules file distributed in
       * such a package contains paths from the package
       * builder's machine. Replace the path with the real
       * one on this machine.
       */
      gchar *tem = *path;
      *path = g_strconcat (_gtk_get_libdir (), tem + strlen (GTK_LIBDIR), NULL);
      g_free (tem);
    }
}

static void
correct_localedir_prefix (gchar **path)
{
  /* See above */
  if (strncmp (*path, GTK_LOCALEDIR, strlen (GTK_LOCALEDIR)) == 0)
    {
      gchar *tem = *path;
      *path = g_strconcat (_gtk_get_localedir (), tem + strlen (GTK_LOCALEDIR), NULL);
      g_free (tem);
    }
}
#endif


G_GNUC_UNUSED static GtkIMModule *
add_builtin_module (const gchar             *module_name,
		    const GtkIMContextInfo **contexts,
		    int                      n_contexts)
{
  GtkIMModule *module = g_object_new (GTK_TYPE_IM_MODULE, NULL);
  GSList *infos = NULL;
  int i;

  for (i = 0; i < n_contexts; i++)
    {
      GtkIMContextInfo *info = g_new (GtkIMContextInfo, 1);
      info->context_id = g_strdup (contexts[i]->context_id);
      info->context_name = g_strdup (contexts[i]->context_name);
      info->domain = g_strdup (contexts[i]->domain);
      info->domain_dirname = g_strdup (contexts[i]->domain_dirname);
#ifdef G_OS_WIN32
      correct_localedir_prefix ((char **) &info->domain_dirname);
#endif
      info->default_locales = g_strdup (contexts[i]->default_locales);
      infos = g_slist_prepend (infos, info);
    }

  module->builtin = TRUE;
  g_type_module_set_name (G_TYPE_MODULE (module), module_name);
  add_module (module, infos);

  return module;
}

static void
gtk_im_module_initialize (void)
{
  GString *line_buf = g_string_new (NULL);
  GString *tmp_buf = g_string_new (NULL);
  gchar *filename = gtk_rc_get_im_module_file();
  FILE *file;
  gboolean have_error = FALSE;

  GtkIMModule *module = NULL;
  GSList *infos = NULL;

  contexts_hash = g_hash_table_new (g_str_hash, g_str_equal);

#define do_builtin(m)							\
  {									\
    const GtkIMContextInfo **contexts;					\
    int n_contexts;							\
    extern void _gtk_immodule_ ## m ## _list (const GtkIMContextInfo ***contexts, \
					      guint                    *n_contexts); \
    extern void _gtk_immodule_ ## m ## _init (GTypeModule *module);	\
    extern void _gtk_immodule_ ## m ## _exit (void);			\
    extern GtkIMContext *_gtk_immodule_ ## m ## _create (const gchar *context_id); \
									\
    _gtk_immodule_ ## m ## _list (&contexts, &n_contexts);		\
    module = add_builtin_module (#m, contexts, n_contexts);		\
    module->init = _gtk_immodule_ ## m ## _init;			\
    module->exit = _gtk_immodule_ ## m ## _exit;			\
    module->create = _gtk_immodule_ ## m ## _create;			\
    module = NULL;							\
  }

#ifdef INCLUDE_IM_am_et
  do_builtin (am_et);
#endif
#ifdef INCLUDE_IM_cedilla
  do_builtin (cedilla);
#endif
#ifdef INCLUDE_IM_cyrillic_translit
  do_builtin (cyrillic_translit);
#endif
#ifdef INCLUDE_IM_ime
  do_builtin (ime);
#endif
#ifdef INCLUDE_IM_inuktitut
  do_builtin (inuktitut);
#endif
#ifdef INCLUDE_IM_ipa
  do_builtin (ipa);
#endif
#ifdef INCLUDE_IM_multipress
  do_builtin (multipress);
#endif
#ifdef INCLUDE_IM_thai
  do_builtin (thai);
#endif
#ifdef INCLUDE_IM_ti_er
  do_builtin (ti_er);
#endif
#ifdef INCLUDE_IM_ti_et
  do_builtin (ti_et);
#endif
#ifdef INCLUDE_IM_viqr
  do_builtin (viqr);
#endif
#ifdef INCLUDE_IM_xim
  do_builtin (xim);
#endif
#ifdef INCLUDE_IM_broadway
  do_builtin (broadway);
#endif

#undef do_builtin

  file = g_fopen (filename, "r");
  if (!file)
    {
      /* In case someone wants only the default input method,
       * we allow no file at all.
       */
      g_string_free (line_buf, TRUE);
      g_string_free (tmp_buf, TRUE);
      g_free (filename);
      return;
    }

  while (!have_error && pango_read_line (file, line_buf))
    {
      const char *p;
      
      p = line_buf->str;

      if (!pango_skip_space (&p))
	{
	  /* Blank line marking the end of a module
	   */
	  if (module && *p != '#')
	    {
	      add_module (module, infos);
	      module = NULL;
	      infos = NULL;
	    }
	  
	  continue;
	}

      if (!module)
	{
	  /* Read a module location
	   */
	  module = g_object_new (GTK_TYPE_IM_MODULE, NULL);

	  if (!pango_scan_string (&p, tmp_buf) ||
	      pango_skip_space (&p))
	    {
	      g_warning ("Error parsing context info in '%s'\n  %s", 
			 filename, line_buf->str);
	      have_error = TRUE;
	    }

	  module->path = g_strdup (tmp_buf->str);
#ifdef G_OS_WIN32
	  correct_libdir_prefix (&module->path);
#endif
	  g_type_module_set_name (G_TYPE_MODULE (module), module->path);
	}
      else
	{
	  GtkIMContextInfo *info = g_new0 (GtkIMContextInfo, 1);
	  
	  /* Read information about a context type
	   */
	  if (!pango_scan_string (&p, tmp_buf))
	    goto context_error;
	  info->context_id = g_strdup (tmp_buf->str);

	  if (!pango_scan_string (&p, tmp_buf))
	    goto context_error;
	  info->context_name = g_strdup (tmp_buf->str);

	  if (!pango_scan_string (&p, tmp_buf))
	    goto context_error;
	  info->domain = g_strdup (tmp_buf->str);

	  if (!pango_scan_string (&p, tmp_buf))
	    goto context_error;
	  info->domain_dirname = g_strdup (tmp_buf->str);
#ifdef G_OS_WIN32
	  correct_localedir_prefix ((char **) &info->domain_dirname);
#endif

	  if (!pango_scan_string (&p, tmp_buf))
	    goto context_error;
	  info->default_locales = g_strdup (tmp_buf->str);

	  if (pango_skip_space (&p))
	    goto context_error;

	  infos = g_slist_prepend (infos, info);
	  continue;

	context_error:
	  g_warning ("Error parsing context info in '%s'\n  %s", 
		     filename, line_buf->str);
	  have_error = TRUE;
	}
    }

  if (have_error)
    {
      GSList *tmp_list = infos;
      while (tmp_list)
	{
	  free_info (tmp_list->data);
	  tmp_list = tmp_list->next;
	}
      g_slist_free (infos);

      g_object_unref (module);
    }
  else if (module)
    add_module (module, infos);

  fclose (file);
  g_string_free (line_buf, TRUE);
  g_string_free (tmp_buf, TRUE);
  g_free (filename);
}

static gint
compare_gtkimcontextinfo_name(const GtkIMContextInfo **a,
			      const GtkIMContextInfo **b)
{
  return g_utf8_collate ((*a)->context_name, (*b)->context_name);
}

/**
 * _gtk_im_module_list:
 * @contexts: location to store an array of pointers to #GtkIMContextInfo
 *            this array should be freed with g_free() when you are finished.
 *            The structures it points are statically allocated and should
 *            not be modified or freed.
 * @n_contexts: the length of the array stored in @contexts
 *
 * List all available types of input method context
 */
void
_gtk_im_module_list (const GtkIMContextInfo ***contexts,
		     guint                    *n_contexts)
{
  int n = 0;

  static
#ifndef G_OS_WIN32
	  const
#endif
		GtkIMContextInfo simple_context_info = {
    SIMPLE_ID,
    NC_("input method menu", "Simple"),
    GETTEXT_PACKAGE,
#ifdef GTK_LOCALEDIR
    GTK_LOCALEDIR,
#else
    "",
#endif
    ""
  };

  static
#ifndef G_OS_WIN32
	  const
#endif
		GtkIMContextInfo none_context_info = {
    NONE_ID,
    NC_("input method menu", "None"),
    GETTEXT_PACKAGE,
#ifdef GTK_LOCALEDIR
    GTK_LOCALEDIR,
#else
    "",
#endif
    ""
  };

#ifdef G_OS_WIN32
  static gboolean beenhere = FALSE;
#endif

  if (!contexts_hash)
    gtk_im_module_initialize ();

#ifdef G_OS_WIN32
  if (!beenhere)
    {
      beenhere = TRUE;
      /* correct_localedir_prefix() requires its parameter to be a
       * malloced string
       */
      simple_context_info.domain_dirname = g_strdup (simple_context_info.domain_dirname);
      correct_localedir_prefix ((char **) &simple_context_info.domain_dirname);
      none_context_info.domain_dirname = g_strdup (none_context_info.domain_dirname);
      correct_localedir_prefix ((char **) &none_context_info.domain_dirname);
    }
#endif

  if (n_contexts)
    *n_contexts = n_loaded_contexts + 2;

  if (contexts)
    {
      GSList *tmp_list;
      int i;

      *contexts = g_new (const GtkIMContextInfo *, n_loaded_contexts + 2);

      (*contexts)[n++] = &none_context_info;
      (*contexts)[n++] = &simple_context_info;

      tmp_list = modules_list;
      while (tmp_list)
	{
	  GtkIMModule *module = tmp_list->data;

	  for (i=0; i<module->n_contexts; i++)
	    (*contexts)[n++] = module->contexts[i];
  
	  tmp_list = tmp_list->next;
	}

      /* first elements (Simple and None) should always be at top */
      qsort ((*contexts)+2, n-2, sizeof (GtkIMContextInfo *), (GCompareFunc)compare_gtkimcontextinfo_name);
    }
}

/**
 * _gtk_im_module_create:
 * @context_id: the context ID for the context type to create
 * 
 * Create an IM context of a type specified by the string
 * ID @context_id.
 * 
 * Returns: a newly created input context of or @context_id, or
 *     if that could not be created, a newly created GtkIMContextSimple.
 */
GtkIMContext *
_gtk_im_module_create (const gchar *context_id)
{
  GtkIMModule *im_module;
  GtkIMContext *context = NULL;

  if (strcmp (context_id, NONE_ID) == 0)
    return NULL;

  if (!contexts_hash)
    gtk_im_module_initialize ();

  if (strcmp (context_id, SIMPLE_ID) != 0)
    {
      im_module = g_hash_table_lookup (contexts_hash, context_id);
      if (!im_module)
	{
	  g_warning ("Attempt to load unknown IM context type '%s'", context_id);
	}
      else
	{
	  if (g_type_module_use (G_TYPE_MODULE (im_module)))
	    {
	      context = im_module->create (context_id);
	      g_type_module_unuse (G_TYPE_MODULE (im_module));
	    }

	  if (!context)
	    g_warning ("Loading IM context type '%s' failed", context_id);
	}
    }

  if (!context)
     return gtk_im_context_simple_new ();
  else
    return context;
}

/* Match @locale against @against.
 * 
 * 'en_US' against “en_US”       => 4
 * 'en_US' against “en”          => 3
 * 'en', “en_UK” against “en_US” => 2
 *  all locales, against “*” 	 => 1
 */
static gint
match_locale (const gchar *locale,
	      const gchar *against,
	      gint         against_len)
{
  if (strcmp (against, "*") == 0)
    return 1;

  if (g_ascii_strcasecmp (locale, against) == 0)
    return 4;

  if (g_ascii_strncasecmp (locale, against, 2) == 0)
    return (against_len == 2) ? 3 : 2;

  return 0;
}

static gboolean
match_backend (GtkIMContextInfo *context)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (g_strcmp0 (context->context_id, "wayland") == 0)
    return GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ());
#endif

#ifdef GDK_WINDOWING_BROADWAY
  if (g_strcmp0 (context->context_id, "broadway") == 0)
    return GDK_IS_BROADWAY_DISPLAY (gdk_display_get_default ());
#endif

#ifdef GDK_WINDOWING_X11
  if (g_strcmp0 (context->context_id, "xim") == 0)
    return GDK_IS_X11_DISPLAY (gdk_display_get_default ());
#endif

#ifdef GDK_WINDOWING_WIN32
  if (g_strcmp0 (context->context_id, "ime") == 0)
    return GDK_IS_WIN32_DISPLAY (gdk_display_get_default ());
#endif

  return TRUE;
}

static const gchar *
lookup_immodule (gchar **immodules_list)
{
  while (immodules_list && *immodules_list)
    {
      if (g_strcmp0 (*immodules_list, SIMPLE_ID) == 0)
        return SIMPLE_ID;
      else if (g_strcmp0 (*immodules_list, NONE_ID) == 0)
        return NONE_ID;
      else
	{
	  gboolean found;
	  gchar *context_id;
	  found = g_hash_table_lookup_extended (contexts_hash, *immodules_list,
						(gpointer *) &context_id, NULL);
	  if (found)
	    return context_id;
	}
      immodules_list++;
    }

  return NULL;
}

#ifdef G_OS_WIN32

/* max size for LOCALE_SISO639LANGNAME and LOCALE_SISO3166CTRYNAME is 9 */
#define MAX_NAME_SIZE 9

static gchar *
get_current_input_language (void)
{
  LCID lcid;
  LANGID langid;
  HKL kblayout;
  int name_size;
  wchar_t name[MAX_NAME_SIZE];
  gchar *language;
  gchar *country;
  gchar *full;

  /* Current thread's keyboard layout */
  kblayout = GetKeyboardLayout(0);
  /* lowest word in the HKL is the LANGID */
  langid = ((guint32)kblayout) & 0xFFFF;
  /* LCID is the LANGID without order */
  lcid = langid;

  /* Get Language ID */
  name_size = GetLocaleInfoW (lcid, LOCALE_SISO639LANGNAME, NULL, 0);
  if (name_size <= 1)
    return NULL;

  g_assert (name_size <= MAX_NAME_SIZE);
  GetLocaleInfoW (lcid, LOCALE_SISO639LANGNAME, name, name_size);

  language = g_utf16_to_utf8 (name, name_size, NULL, NULL, NULL);
  if (!language)
    return NULL;

  if (SUBLANGID (langid) == SUBLANG_NEUTRAL)
    return language;

  /* Get Country ID */
  name_size = GetLocaleInfoW (lcid, LOCALE_SISO3166CTRYNAME, NULL, 0);
  if (name_size <= 1)
    return language;

  g_assert (name_size <= MAX_NAME_SIZE);
  GetLocaleInfoW (lcid, LOCALE_SISO3166CTRYNAME, name, name_size);

  country = g_utf16_to_utf8 (name, name_size, NULL, NULL, NULL);
  if (!country)
    return language;

  full = g_strdup_printf ("%s_%s", language, country);

  g_free (language);
  g_free (country);

  return full;
}

#endif

/**
 * _gtk_im_module_get_default_context_id:
 * @client_window: a window
 * 
 * Return the context_id of the best IM context type 
 * for the given window.
 * 
 * Returns: the context ID (will never be %NULL)
 */
const gchar *
_gtk_im_module_get_default_context_id (GdkWindow *client_window)
{
  GSList *tmp_list;
  const gchar *context_id = NULL;
  gint best_goodness = 0;
  gint i;
  gchar *tmp_locale, *tmp, **immodules;
  const gchar *envvar;
  GdkScreen *screen;
  GtkSettings *settings;

  if (!contexts_hash)
    gtk_im_module_initialize ();

  envvar = g_getenv ("GTK_IM_MODULE");
  if (envvar)
    {
        immodules = g_strsplit (envvar, ":", 0);
        context_id = lookup_immodule (immodules);
        g_strfreev (immodules);

        if (context_id)
          return context_id;
    }

  /* Check if the certain immodule is set in XSETTINGS.
   */
  screen = gdk_screen_get_default ();
  settings = gtk_settings_get_for_screen (screen);
  g_object_get (G_OBJECT (settings), "gtk-im-module", &tmp, NULL);
  if (tmp)
    {
      immodules = g_strsplit (tmp, ":", 0);
      context_id = lookup_immodule (immodules);
      g_strfreev (immodules);
      g_free (tmp);

      if (context_id)
        return context_id;
    }

#ifdef G_OS_WIN32
  /* Read current input locale from the current keyboard info */
  tmp_locale = get_current_input_language ();
  if (!tmp_locale)
    /* Default to system locale when input language is unknown */
    tmp_locale = _gtk_get_lc_ctype ();
#else
  tmp_locale = _gtk_get_lc_ctype ();
#endif

  /* Strip the locale code down to the essentials
   */
  tmp = strchr (tmp_locale, '.');
  if (tmp)
    *tmp = '\0';
  tmp = strchr (tmp_locale, '@');
  if (tmp)
    *tmp = '\0';

  tmp_list = modules_list;
  while (tmp_list)
    {
      GtkIMModule *module = tmp_list->data;

      for (i = 0; i < module->n_contexts; i++)
	{
	  const gchar *p;

          if (!match_backend (module->contexts[i]))
            continue;

          p = module->contexts[i]->default_locales;
	  while (p)
	    {
	      const gchar *q = strchr (p, ':');
	      gint goodness = match_locale (tmp_locale, p, q ? q - p : strlen (p));

	      if (goodness > best_goodness)
		{
		  context_id = module->contexts[i]->context_id;
		  best_goodness = goodness;
		}

	      p = q ? q + 1 : NULL;
	    }
	}

      tmp_list = tmp_list->next;
    }

  g_free (tmp_locale);

  return context_id ? context_id : SIMPLE_ID;
}
