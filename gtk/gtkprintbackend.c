/* GTK - The GIMP Toolkit
 * gtkprintbackend.h: Abstract printer backend interfaces
 * Copyright (C) 2003, Red Hat, Inc.
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

#include "config.h"
#include <string.h>

#include <gmodule.h>

#include "gtkintl.h"
#include "gtkmodules.h"
#include "gtkprivate.h"
#include "gtkprintbackend.h"
#include "gtkprinter-private.h"
#include "gtkalias.h"

#define GTK_PRINT_BACKEND_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_PRINT_BACKEND, GtkPrintBackendPrivate))

static void gtk_print_backend_dispose (GObject *object);

struct _GtkPrintBackendPrivate
{
  GHashTable *printers;
  guint printer_list_requested : 1;
  guint printer_list_done : 1;
};

enum {
  PRINTER_LIST_CHANGED,
  PRINTER_LIST_DONE,
  PRINTER_ADDED,
  PRINTER_REMOVED,
  PRINTER_STATUS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *backend_parent_class;

GQuark
gtk_print_backend_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("gtk-print-backend-error-quark");
  return quark;
}

/*****************************************
 *     GtkPrintBackendModule modules     *
 *****************************************/

typedef struct _GtkPrintBackendModule GtkPrintBackendModule;
typedef struct _GtkPrintBackendModuleClass GtkPrintBackendModuleClass;

struct _GtkPrintBackendModule
{
  GTypeModule parent_instance;
  
  GModule *library;

  void             (*init)     (GTypeModule    *module);
  void             (*exit)     (void);
  GtkPrintBackend* (*create)   (void);

  gchar *path;
};

struct _GtkPrintBackendModuleClass
{
  GTypeModuleClass parent_class;
};

G_DEFINE_TYPE (GtkPrintBackendModule, _gtk_print_backend_module, G_TYPE_TYPE_MODULE)
#define GTK_TYPE_PRINT_BACKEND_MODULE      (_gtk_print_backend_module_get_type ())
#define GTK_PRINT_BACKEND_MODULE(module)   (G_TYPE_CHECK_INSTANCE_CAST ((module), GTK_TYPE_PRINT_BACKEND_MODULE, GtkPrintBackendModule))

static GSList *loaded_backends;

static gboolean
gtk_print_backend_module_load (GTypeModule *module)
{
  GtkPrintBackendModule *pb_module = GTK_PRINT_BACKEND_MODULE (module); 
  gpointer initp, exitp, createp;
 
  pb_module->library = g_module_open (pb_module->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  if (!pb_module->library)
    {
      g_warning (g_module_error());
      return FALSE;
    }
  
  /* extract symbols from the lib */
  if (!g_module_symbol (pb_module->library, "pb_module_init",
			&initp) ||
      !g_module_symbol (pb_module->library, "pb_module_exit", 
			&exitp) ||
      !g_module_symbol (pb_module->library, "pb_module_create", 
			&createp))
    {
      g_warning (g_module_error());
      g_module_close (pb_module->library);
      
      return FALSE;
    }

  pb_module->init = initp;
  pb_module->exit = exitp;
  pb_module->create = createp;

  /* call the filesystems's init function to let it */
  /* setup anything it needs to set up. */
  pb_module->init (module);

  return TRUE;
}

static void
gtk_print_backend_module_unload (GTypeModule *module)
{
  GtkPrintBackendModule *pb_module = GTK_PRINT_BACKEND_MODULE (module);
  
  pb_module->exit();

  g_module_close (pb_module->library);
  pb_module->library = NULL;

  pb_module->init = NULL;
  pb_module->exit = NULL;
  pb_module->create = NULL;
}

/* This only will ever be called if an error occurs during
 * initialization
 */
static void
gtk_print_backend_module_finalize (GObject *object)
{
  GtkPrintBackendModule *module = GTK_PRINT_BACKEND_MODULE (object);

  g_free (module->path);

  G_OBJECT_CLASS (_gtk_print_backend_module_parent_class)->finalize (object);
}

static void
_gtk_print_backend_module_class_init (GtkPrintBackendModuleClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  module_class->load = gtk_print_backend_module_load;
  module_class->unload = gtk_print_backend_module_unload;

  gobject_class->finalize = gtk_print_backend_module_finalize;
}

static void
_gtk_print_backend_module_init (GtkPrintBackendModule *pb_module)
{
}

static GtkPrintBackend *
_gtk_print_backend_module_create (GtkPrintBackendModule *pb_module)
{
  GtkPrintBackend *pb;
  
  if (g_type_module_use (G_TYPE_MODULE (pb_module)))
    {
      pb = pb_module->create ();
      g_type_module_unuse (G_TYPE_MODULE (pb_module));
      return pb;
    }
  return NULL;
}

static GtkPrintBackend *
_gtk_print_backend_create (const gchar *backend_name)
{
  GSList *l;
  gchar *module_path;
  gchar *full_name;
  GtkPrintBackendModule *pb_module;
  GtkPrintBackend *pb;

  for (l = loaded_backends; l != NULL; l = l->next)
    {
      pb_module = l->data;
      
      if (strcmp (G_TYPE_MODULE (pb_module)->name, backend_name) == 0)
	return _gtk_print_backend_module_create (pb_module);
    }

  pb = NULL;
  if (g_module_supported ())
    {
      full_name = g_strconcat ("printbackend-", backend_name, NULL);
      module_path = _gtk_find_module (full_name, "printbackends");
      g_free (full_name);

      if (module_path)
	{
	  pb_module = g_object_new (GTK_TYPE_PRINT_BACKEND_MODULE, NULL);

	  g_type_module_set_name (G_TYPE_MODULE (pb_module), backend_name);
	  pb_module->path = g_strdup (module_path);

	  loaded_backends = g_slist_prepend (loaded_backends,
		   		             pb_module);

	  pb = _gtk_print_backend_module_create (pb_module);

	  /* Increase use-count so that we don't unload print backends.
	   * There is a problem with module unloading in the cups module,
	   * see cups_dispatch_watch_finalize for details. 
	   */
	  g_type_module_use (G_TYPE_MODULE (pb_module));
	}
      
      g_free (module_path);
    }

  return pb;
}

static void
gtk_print_backend_initialize (void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      /**
       * GtkSettings:gtk-print-backends:
       *
       * A comma-separated list of print backends to use in the print
       * dialog. Available print backends depend on the GTK+ installation,
       * and may include "pdf", "cups" or "lpr".
       *
       * Since: 2.10
       */
      gtk_settings_install_property (g_param_spec_string ("gtk-print-backends",
							  P_("Default print backend"),
							  P_("List of the GtkPrintBackend backends to use by default"),
							  GTK_PRINT_BACKENDS,
							  GTK_PARAM_READWRITE));

      initialized = TRUE;
    }
}



GList *
gtk_print_backend_load_modules (void)
{
  GList *result;
  GtkPrintBackend *backend;
  gchar *setting;
  gchar **backends;
  gint i;
  GtkSettings *settings;

  result = NULL;

  gtk_print_backend_initialize ();
  
  settings = gtk_settings_get_default ();
  if (settings)
    g_object_get (settings, "gtk-print-backends", &setting, NULL);
  else
    setting = g_strdup (GTK_PRINT_BACKENDS);

  backends = g_strsplit (setting, ",", -1);

  for (i = 0; backends[i]; i++)
    {
      g_strchug (backends[i]);
      g_strchomp (backends[i]);
      backend = _gtk_print_backend_create (backends[i]);
      
      if (backend)
        result = g_list_append (result, backend);
    }

  g_strfreev (backends);
  g_free (setting);

  return result;
}

/*****************************************
 *             GtkPrintBackend           *
 *****************************************/

G_DEFINE_TYPE (GtkPrintBackend, gtk_print_backend, G_TYPE_OBJECT)

static void                 fallback_printer_request_details  (GtkPrinter          *printer);
static gboolean             fallback_printer_mark_conflicts   (GtkPrinter          *printer,
							       GtkPrinterOptionSet *options);
static void                 fallback_printer_get_hard_margins (GtkPrinter          *printer,
							       gdouble             *top,
							       gdouble             *bottom,
							       gdouble             *left,
							       gdouble             *right);
static GList *              fallback_printer_list_papers      (GtkPrinter          *printer);
static GtkPrintCapabilities fallback_printer_get_capabilities (GtkPrinter          *printer);
  
static void
gtk_print_backend_class_init (GtkPrintBackendClass *class)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) class;

  backend_parent_class = g_type_class_peek_parent (class);
  
  object_class->dispose = gtk_print_backend_dispose;

  class->printer_request_details = fallback_printer_request_details;
  class->printer_mark_conflicts = fallback_printer_mark_conflicts;
  class->printer_get_hard_margins = fallback_printer_get_hard_margins;
  class->printer_list_papers = fallback_printer_list_papers;
  class->printer_get_capabilities = fallback_printer_get_capabilities;
  
  g_type_class_add_private (class, sizeof (GtkPrintBackendPrivate));
  
  signals[PRINTER_LIST_CHANGED] =
    g_signal_new (I_("printer-list-changed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_list_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  signals[PRINTER_LIST_DONE] =
    g_signal_new (I_("printer-list-done"),
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintBackendClass, printer_list_done),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
  signals[PRINTER_ADDED] =
    g_signal_new (I_("printer-added"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINTER);
  signals[PRINTER_REMOVED] =
    g_signal_new (I_("printer-removed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINTER);
  signals[PRINTER_STATUS_CHANGED] =
    g_signal_new (I_("printer-status-changed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_status_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINTER);
}

static void
gtk_print_backend_init (GtkPrintBackend *backend)
{
  GtkPrintBackendPrivate *priv;

  priv = backend->priv = GTK_PRINT_BACKEND_GET_PRIVATE (backend); 

  priv->printers = g_hash_table_new_full (g_str_hash, g_str_equal, 
					  (GDestroyNotify) g_free,
					  (GDestroyNotify) g_object_unref);
}

static void
gtk_print_backend_dispose (GObject *object)
{
  GtkPrintBackend *backend;
  GtkPrintBackendPrivate *priv;

  backend = GTK_PRINT_BACKEND (object);
  priv = backend->priv;

  /* We unref the printers in dispose, not in finalize so that
   * we can break refcount cycles with gtk_print_backend_destroy 
   */
  if (priv->printers)
    {
      g_hash_table_destroy (priv->printers);
      priv->printers = NULL;
    }

  backend_parent_class->dispose (object);
}


static void
fallback_printer_request_details (GtkPrinter *printer)
{
}

static gboolean
fallback_printer_mark_conflicts (GtkPrinter          *printer,
				 GtkPrinterOptionSet *options)
{
  return FALSE;
}

static void
fallback_printer_get_hard_margins (GtkPrinter *printer,
				   gdouble    *top,
				   gdouble    *bottom,
				   gdouble    *left,
				   gdouble    *right)
{
  *top = 0;
  *bottom = 0;
  *left = 0;
  *right = 0;
}

static GList *
fallback_printer_list_papers (GtkPrinter *printer)
{
  return NULL;
}

static GtkPrintCapabilities
fallback_printer_get_capabilities (GtkPrinter *printer)
{
  return 0;
}


static void
printer_hash_to_sorted_active_list (const gchar  *key,
                                    gpointer      value,
                                    GList       **out_list)
{
  GtkPrinter *printer;

  printer = GTK_PRINTER (value);

  if (gtk_printer_get_name (printer) == NULL)
    return;

  if (!gtk_printer_is_active (printer))
    return;

  *out_list = g_list_insert_sorted (*out_list, value, (GCompareFunc) gtk_printer_compare);
}


void
gtk_print_backend_add_printer (GtkPrintBackend *backend,
			       GtkPrinter      *printer)
{
  GtkPrintBackendPrivate *priv;
  
  g_return_if_fail (GTK_IS_PRINT_BACKEND (backend));

  priv = backend->priv;

  if (!priv->printers)
    return;
  
  g_hash_table_insert (priv->printers,
		       g_strdup (gtk_printer_get_name (printer)), 
		       g_object_ref (printer));
}

void
gtk_print_backend_remove_printer (GtkPrintBackend *backend,
				  GtkPrinter      *printer)
{
  GtkPrintBackendPrivate *priv;
  
  g_return_if_fail (GTK_IS_PRINT_BACKEND (backend));
  priv = backend->priv;

  if (!priv->printers)
    return;
  
  g_hash_table_remove (priv->printers,
		       gtk_printer_get_name (printer));
}

void
gtk_print_backend_set_list_done (GtkPrintBackend *backend)
{
  if (!backend->priv->printer_list_done)
    {
      backend->priv->printer_list_done = TRUE;
      g_signal_emit (backend, signals[PRINTER_LIST_DONE], 0);
    }
}


GList *
gtk_print_backend_get_printer_list (GtkPrintBackend *backend)
{
  GtkPrintBackendPrivate *priv;
  GList *result;
  
  g_return_val_if_fail (GTK_IS_PRINT_BACKEND (backend), NULL);

  priv = backend->priv;

  result = NULL;
  if (priv->printers != NULL)
    g_hash_table_foreach (priv->printers,
                          (GHFunc) printer_hash_to_sorted_active_list,
                          &result);

  if (!priv->printer_list_requested && priv->printers != NULL)
    {
      if (GTK_PRINT_BACKEND_GET_CLASS (backend)->request_printer_list)
	GTK_PRINT_BACKEND_GET_CLASS (backend)->request_printer_list (backend);
      priv->printer_list_requested = TRUE;
    }
  
  return result;
}

gboolean
gtk_print_backend_printer_list_is_done (GtkPrintBackend *print_backend)
{
  g_return_val_if_fail (GTK_IS_PRINT_BACKEND (print_backend), TRUE);

  return print_backend->priv->printer_list_done;
}

GtkPrinter *
gtk_print_backend_find_printer (GtkPrintBackend *backend,
                                const gchar     *printer_name)
{
  GtkPrintBackendPrivate *priv;
  GtkPrinter *printer;
  
  g_return_val_if_fail (GTK_IS_PRINT_BACKEND (backend), NULL);

  priv = backend->priv;

  if (priv->printers)
    printer = g_hash_table_lookup (priv->printers, printer_name);
  else
    printer = NULL;

  return printer;  
}

void
gtk_print_backend_print_stream (GtkPrintBackend        *backend,
                                GtkPrintJob            *job,
                                GIOChannel             *data_io,
                                GtkPrintJobCompleteFunc callback,
                                gpointer                user_data,
				GDestroyNotify          dnotify)
{
  g_return_if_fail (GTK_IS_PRINT_BACKEND (backend));

  GTK_PRINT_BACKEND_GET_CLASS (backend)->print_stream (backend,
						       job,
						       data_io,
						       callback,
						       user_data,
						       dnotify);
}

void
gtk_print_backend_destroy (GtkPrintBackend *print_backend)
{
  /* The lifecycle of print backends and printers are tied, such that
   * the backend owns the printers, but the printers also ref the backend.
   * This is so that if the app has a reference to a printer its backend
   * will be around. However, this results in a cycle, which we break
   * with this call, which causes the print backend to release its printers.
   */
  g_object_run_dispose (G_OBJECT (print_backend));
}

#define __GTK_PRINT_BACKEND_C__
#include "gtkaliasdef.c"
