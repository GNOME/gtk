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
#include "gtkalias.h"

static void gtk_print_backend_base_init (gpointer g_class);

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

G_DEFINE_TYPE (GtkPrintBackendModule, _gtk_print_backend_module, G_TYPE_TYPE_MODULE);
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

/* Like g_module_path, but use .la as the suffix
 */
static gchar*
module_build_la_path (const gchar *directory,
		      const gchar *module_name)
{
  gchar *filename;
  gchar *result;
	
  if (strncmp (module_name, "lib", 3) == 0)
    filename = (gchar *)module_name;
  else
    filename =  g_strconcat ("libgtkprintbackend", module_name, ".la", NULL);

  if (directory && *directory)
    result = g_build_filename (directory, filename, NULL);
  else
    result = g_strdup (filename);

  if (filename != module_name)
    g_free (filename);

  return result;
}

GtkPrintBackend *
_gtk_print_backend_create (const char *backend_name)
{
  GSList *l;
  char *module_path;
  char *full_name;
  GtkPrintBackendModule *pb_module;
  GtkPrintBackend *pb;

  /* TODO: make module loading code work */
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
	}
      
      g_free (module_path);
    }

  return pb;

  return NULL;
}

static GList * 
property_parse_list (const gchar *string)
{
  GScanner *scanner;
  gboolean success = FALSE;
  gboolean need_closing_brace = FALSE;
  GList *results = NULL;

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, string, strlen (string));

  g_scanner_get_next_token (scanner);

  if (scanner->token == G_TOKEN_LEFT_CURLY)
    {
      need_closing_brace = TRUE;
      g_scanner_get_next_token (scanner);
    }

  while (scanner->token != G_TOKEN_EOF && scanner->token != G_TOKEN_RIGHT_CURLY)
    {
      if (scanner->token == G_TOKEN_STRING)
        {
          results = g_list_append (results, g_strdup (scanner->value.v_string));
        }
      else if (scanner->token == G_TOKEN_IDENTIFIER)
        {
          results = g_list_append (results, g_strdup (scanner->value.v_identifier));
        }
      else if (scanner->token == G_TOKEN_COMMA)
        {
          /* noop */
        }
      else
        goto err;
         
      g_scanner_get_next_token (scanner);
    }

  if (scanner->token == G_TOKEN_RIGHT_CURLY && need_closing_brace)
    success = TRUE;

  if (scanner->token == G_TOKEN_RIGHT_CURLY && !need_closing_brace)
    success = TRUE;

 err:
  if (!success)
    if (results)
      {
        g_list_free (results);
        results = NULL;
      }

  g_scanner_destroy (scanner);

  return results;
}

static void
gtk_print_backend_initialize (void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      gtk_settings_install_property (g_param_spec_string ("gtk-print-backends",
							  P_("Default print backend"),
							  P_("List of the GtkPrintBackend backends to use by default"),
							  "{\"pdf\", \"cups\"}",
							  GTK_PARAM_READWRITE));

      initialized = TRUE;
    }
}



GList *
gtk_print_backend_load_modules ()
{
  GList *result;
  GtkPrintBackend *backend;
  gchar * s_backend_list;
  GList *backend_list, *node;
  GtkSettings *settings;

  result = NULL;

  gtk_print_backend_initialize ();
  
  settings = gtk_settings_get_default ();

  g_object_get (settings, "gtk-print-backends", &s_backend_list, NULL);

  backend_list = property_parse_list (s_backend_list);

  node = backend_list;
  while (node)
    {
      g_message ("node: %s", (char *)node->data);

      backend = _gtk_print_backend_create ((char *)node->data);
      
      if (backend)
        result = g_list_append (result, backend);

      node = node->next;
    }

  g_free (s_backend_list);

  if (backend_list)
    g_list_free (backend_list);

  return result;
}

/*****************************************
 *             GtkPrintBackend           *
 *****************************************/
GType
gtk_print_backend_get_type (void)
{
  static GType print_backend_type = 0;

  if (!print_backend_type)
    {
      static const GTypeInfo print_backend_info =
      {
	sizeof (GtkPrintBackendIface),  /* class_size */
	gtk_print_backend_base_init,    /* base_init */
	NULL,                         /* base_finalize */
      };

      print_backend_type = g_type_register_static (G_TYPE_INTERFACE,
						 "GtkPrintBackend",
						 &print_backend_info, 0);

      g_type_interface_add_prerequisite (print_backend_type, G_TYPE_OBJECT);
    }

  return print_backend_type;
}

static void
gtk_print_backend_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_class);

      g_signal_new ("printer-list-changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintBackendIface, printer_list_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      g_signal_new ("printer-added",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintBackendIface, printer_added),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__OBJECT,
		    G_TYPE_NONE, 1, G_TYPE_OBJECT);
      g_signal_new ("printer-removed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintBackendIface, printer_removed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__OBJECT,
		    G_TYPE_NONE, 1, G_TYPE_OBJECT);
      g_signal_new ("printer-status-changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintBackendIface, printer_status_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__OBJECT,
		    G_TYPE_NONE, 1, G_TYPE_OBJECT);

      initialized = TRUE;
    }
}

GList *
gtk_print_backend_get_printer_list (GtkPrintBackend *print_backend)
{
  g_return_val_if_fail (GTK_IS_PRINT_BACKEND (print_backend), NULL);

  return GTK_PRINT_BACKEND_GET_IFACE (print_backend)->get_printer_list (print_backend);

}

GtkPrinter *
gtk_print_backend_find_printer (GtkPrintBackend *print_backend,
                                const gchar *printer_name)
{
  g_return_val_if_fail (GTK_IS_PRINT_BACKEND (print_backend), NULL);

  return GTK_PRINT_BACKEND_GET_IFACE (print_backend)->find_printer (print_backend, printer_name);

}

void
gtk_print_backend_print_stream (GtkPrintBackend *print_backend,
                                GtkPrintJob *job,
                                const gchar *title,
                                gint data_fd,
                                GtkPrintJobCompleteFunc callback,
                                gpointer user_data)
{
  g_return_if_fail (GTK_IS_PRINT_BACKEND (print_backend));

  return GTK_PRINT_BACKEND_GET_IFACE (print_backend)->print_stream (print_backend,
                                                                    job,
                                                                    title,
                                                                    data_fd,
                                                                    callback,
                                                                    user_data);
}

#define __GTK_PRINT_BACKEND_C__
#include "gtkaliasdef.c"
