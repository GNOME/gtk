/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Themes added by The Rasterman <raster@redhat.com>
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gmodule.h>
#include <pango/pango-utils.h>
#include "gtkimmodule.h"
#include "gtkimcontextsimple.h"
#include "gtkprivate.h"
#include "gtkrc.h"
#include "config.h"
#include "gtkintl.h"

#define SIMPLE_ID "gtk-im-context-simple"

typedef struct _GtkIMModule      GtkIMModule;
typedef struct _GtkIMModuleClass GtkIMModuleClass;

#define GTK_TYPE_IM_MODULE          (gtk_im_module_get_type ())
#define GTK_IM_MODULE(im_module)    (G_TYPE_CHECK_INSTANCE_CAST ((im_module), GTK_TYPE_IM_MODULE, GtkIMModule))
#define GTK_IS_IM_MODULE(im_module) (G_TYPE_CHECK_INSTANCE_TYPE ((im_module), GTK_TYPE_IM_MODULE))

struct _GtkIMModule
{
  GTypeModule parent_instance;
  
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

GType gtk_im_module_get_type (void);

gint n_loaded_contexts = 0;
static GHashTable *contexts_hash = NULL;
static GSList *modules_list = NULL;

static GObjectClass *parent_class = NULL;

static gboolean
gtk_im_module_load (GTypeModule *module)
{
  GtkIMModule *im_module = GTK_IM_MODULE (module);
  
  im_module->library = g_module_open (im_module->path, 0);
  if (!im_module->library)
    {
      g_warning (g_module_error());
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
      g_warning (g_module_error());
      g_module_close (im_module->library);
      
      return FALSE;
    }
	    
  /* call the theme's init (theme_init) function to let it */
  /* setup anything it needs to set up. */
  im_module->init (module);

  return TRUE;
}

static void
gtk_im_module_unload (GTypeModule *module)
{
  GtkIMModule *im_module = GTK_IM_MODULE (module);
  
  im_module->exit();

  g_module_close (im_module->library);
  im_module->library = NULL;

  im_module->init = NULL;
  im_module->exit = NULL;
  im_module->list = NULL;
  im_module->create = NULL;
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

GType
gtk_im_module_get_type (void)
{
  static GType im_module_type = 0;

  if (!im_module_type)
    {
      static const GTypeInfo im_module_info = {
        sizeof (GtkIMModuleClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gtk_im_module_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkIMModule),
        0,              /* n_preallocs */
        NULL,           /* instance_init */
      };

      im_module_type = g_type_register_static (G_TYPE_TYPE_MODULE, "GtkIMModule", &im_module_info, 0);
    }
  
  return im_module_type;
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
      *path = g_strconcat (GTK_LIBDIR, tem + strlen (GTK_LIBDIR), NULL);
      g_free (tem);
    }
}
#endif


static void
gtk_im_module_init ()
{
  GString *line_buf = g_string_new (NULL);
  GString *tmp_buf = g_string_new (NULL);
  gchar *filename = gtk_rc_get_im_module_file();
  FILE *file;
  gboolean have_error = FALSE;

  GtkIMModule *module = NULL;
  GSList *infos = NULL;

  contexts_hash = g_hash_table_new (g_str_hash, g_str_equal);

  file = fopen (filename, "r");
  if (!file)
    {
      g_warning ("Can not open Input Method module file '%s': %s",
		 filename, g_strerror (errno));
      /* We are leaking all kinds of memory here. */
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
	  correct_libdir_prefix (&info->domain_dirname);
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

      g_object_unref (G_OBJECT (module));
    }
  else if (module)
    add_module (module, infos);

  fclose (file);
  g_string_free (line_buf, TRUE);
  g_string_free (tmp_buf, TRUE);
  g_free (filename);
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
 **/
void
_gtk_im_module_list (const GtkIMContextInfo ***contexts,
		     guint                    *n_contexts)
{
  int n = 0;

  static const GtkIMContextInfo simple_context_info = {
    SIMPLE_ID,
    "Default",
    "gtk+",
    NULL,
    ""
  };

  if (!contexts_hash)
    gtk_im_module_init ();

  if (n_contexts)
    *n_contexts = (n_loaded_contexts + 1);

  if (contexts)
    {
      GSList *tmp_list;
      int i;
      
      *contexts = g_new (const GtkIMContextInfo *, n_loaded_contexts + 1);

      (*contexts)[n++] = &simple_context_info;

      tmp_list = modules_list;
      while (tmp_list)
	{
	  GtkIMModule *module = tmp_list->data;

	  for (i=0; i<module->n_contexts; i++)
	    (*contexts)[n++] = module->contexts[i];
	  
	  tmp_list = tmp_list->next;
	}
    }
}

/**
 * _gtk_im_module_create:
 * @context_id: the context ID for the context type to create
 * 
 * Create an IM context of a type specified by the string
 * ID @context_id.
 * 
 * Return value: a newly created input context of or @context_id, or
 * if that could not be created, a newly created GtkIMContextSimple.
 **/
GtkIMContext *
_gtk_im_module_create (const gchar *context_id)
{
  GtkIMModule *im_module;
  GtkIMContext *context = NULL;
  
  if (!contexts_hash)
    gtk_im_module_init ();

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
 * 'en_US' against 'en_US'       => 4
 * 'en_US' against 'en'          => 3
 * 'en', 'en_UK' against 'en_US' => 2
 *  all locales, against '*' 	 => 1
 */
static gint
match_locale (const gchar *locale,
	      const gchar *against,
	      gint         against_len)
{
  if (strcmp (against, "*") == 0)
    return 1;

  if (strcmp (locale, against) == 0)
    return 4;

  if (strncmp (locale, against, 2) == 0)
    return (against_len == 2) ? 3 : 2;

  return 0;
}

/**
 * _gtk_im_module_get_default_context_id:
 * @locale: a locale id in the form 'en_US'
 * 
 * Return the context_id of the best IM context type
 * for the given locale ID.
 * 
 * Return value: the context ID (will never be %NULL)
 *    the value is newly allocated and must be freed
 *    with g_free().
 **/
const gchar *
_gtk_im_module_get_default_context_id (const gchar *locale)
{
  GSList *tmp_list;
  const gchar *context_id = NULL;
  gint best_goodness = 0;
  gint i;
  gchar *tmp_locale, *tmp;
  const gchar *envvar;
      
  if (!contexts_hash)
    gtk_im_module_init ();

  envvar = g_getenv ("GTK_IM_MODULE");
  if (envvar && g_hash_table_lookup (contexts_hash, envvar))
    return g_strdup (envvar);

  /* Strip the locale code down to the essentials
   */
  tmp_locale = g_strdup (locale);
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
     
      for (i=0; i<module->n_contexts; i++)
	{
	  const gchar *p = module->contexts[i]->default_locales;
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
  
  return g_strdup (context_id ? context_id : SIMPLE_ID);
}
