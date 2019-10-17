/* GTK - The GIMP Toolkit
 * gtkprintbackendprivate.h: Abstract printer backend interfaces
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>

#include <gmodule.h>

#include "gtkintl.h"
#include "gtkdebug.h"
#include "gtkmodulesprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkprintbackendprivate.h"


static void gtk_print_backend_dispose      (GObject      *object);
static void gtk_print_backend_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void gtk_print_backend_get_property (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

struct _GtkPrintBackendPrivate
{
  GHashTable *printers;
  guint printer_list_requested : 1;
  guint printer_list_done : 1;
  GtkPrintBackendStatus status;
  char **auth_info_required;
  char **auth_info;
  gboolean store_auth_info;
};

enum {
  PRINTER_LIST_CHANGED,
  PRINTER_LIST_DONE,
  PRINTER_ADDED,
  PRINTER_REMOVED,
  PRINTER_STATUS_CHANGED,
  REQUEST_PASSWORD,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum 
{ 
  PROP_ZERO,
  PROP_STATUS
};

static GObjectClass *backend_parent_class;

GQuark
gtk_print_backend_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("gtk-print-backend-error-quark");
  return quark;
}

void
gtk_print_backends_init (void)
{
  GIOExtensionPoint *ep;
  GIOModuleScope *scope;
  char **paths;
  int i;

  GTK_NOTE (MODULES,
            g_print ("Registering extension point %s\n", GTK_PRINT_BACKEND_EXTENSION_POINT_NAME));

  ep = g_io_extension_point_register (GTK_PRINT_BACKEND_EXTENSION_POINT_NAME);
  g_io_extension_point_set_required_type (ep, GTK_TYPE_PRINT_BACKEND);

  scope = g_io_module_scope_new (G_IO_MODULE_SCOPE_BLOCK_DUPLICATES);

  paths = _gtk_get_module_path ("printbackends");
  for (i = 0; paths[i]; i++)
    {
      GTK_NOTE (MODULES,
                g_print ("Scanning io modules in %s\n", paths[i]));
      g_io_modules_scan_all_in_directory_with_scope (paths[i], scope);
    }
  g_strfreev (paths);

  g_io_module_scope_free (scope);

  if (GTK_DEBUG_CHECK (MODULES))
    {
      GList *list, *l;

      list = g_io_extension_point_get_extensions (ep);
      for (l = list; l; l = l->next)
        {
          GIOExtension *ext = l->data;
          g_print ("extension: %s: type %s\n",
                   g_io_extension_get_name (ext),
                   g_type_name (g_io_extension_get_type (ext)));
        }
    }
}

/**
 * gtk_print_backend_load_modules:
 *
 * Returns: (element-type GtkPrintBackend) (transfer container):
 */
GList *
gtk_print_backend_load_modules (void)
{
  GList *result;
  GtkPrintBackend *backend;
  gchar *setting;
  gchar **backends;
  gint i;
  GtkSettings *settings;
  GIOExtensionPoint *ep;

  result = NULL;

  ep = g_io_extension_point_lookup (GTK_PRINT_BACKEND_EXTENSION_POINT_NAME);

  settings = gtk_settings_get_default ();
  if (settings)
    g_object_get (settings, "gtk-print-backends", &setting, NULL);
  else
    setting = g_strdup (GTK_PRINT_BACKENDS);

  backends = g_strsplit (setting, ",", -1);

  for (i = 0; backends[i]; i++)
    {
      GIOExtension *ext;
      GType type;

      ext = g_io_extension_point_get_extension_by_name (ep, backends[i]);
      if (!ext)
        continue;

      GTK_NOTE (PRINTING,
                g_print ("Found %s print backend\n", backends[i]));

      type = g_io_extension_get_type (ext);
      backend = g_object_new (type, NULL);
      result = g_list_append (result, backend);
    }

  g_strfreev (backends);
  g_free (setting);

  return result;
}

/*****************************************
 *             GtkPrintBackend           *
 *****************************************/

G_DEFINE_TYPE_WITH_PRIVATE (GtkPrintBackend, gtk_print_backend, G_TYPE_OBJECT)

static void                 fallback_printer_request_details       (GtkPrinter          *printer);
static gboolean             fallback_printer_mark_conflicts        (GtkPrinter          *printer,
								    GtkPrinterOptionSet *options);
static gboolean             fallback_printer_get_hard_margins      (GtkPrinter          *printer,
                                                                    gdouble             *top,
                                                                    gdouble             *bottom,
                                                                    gdouble             *left,
                                                                    gdouble             *right);
static gboolean             fallback_printer_get_hard_margins_for_paper_size (GtkPrinter          *printer,
									      GtkPaperSize        *paper_size,
									      gdouble             *top,
									      gdouble             *bottom,
									      gdouble             *left,
									      gdouble             *right);
static GList *              fallback_printer_list_papers           (GtkPrinter          *printer);
static GtkPageSetup *       fallback_printer_get_default_page_size (GtkPrinter          *printer);
static GtkPrintCapabilities fallback_printer_get_capabilities      (GtkPrinter          *printer);
static void                 request_password                       (GtkPrintBackend     *backend,
                                                                    gpointer             auth_info_required,
                                                                    gpointer             auth_info_default,
                                                                    gpointer             auth_info_display,
                                                                    gpointer             auth_info_visible,
                                                                    const gchar         *prompt,
                                                                    gboolean             can_store_auth_info);

static void
gtk_print_backend_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkPrintBackend *backend = GTK_PRINT_BACKEND (object);
  GtkPrintBackendPrivate *priv = backend->priv;

  switch (prop_id)
    {
    case PROP_STATUS:
      priv->status = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_backend_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkPrintBackend *backend = GTK_PRINT_BACKEND (object);
  GtkPrintBackendPrivate *priv = backend->priv;

  switch (prop_id)
    {
    case PROP_STATUS:
      g_value_set_int (value, priv->status);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_backend_class_init (GtkPrintBackendClass *class)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) class;

  backend_parent_class = g_type_class_peek_parent (class);
  
  object_class->dispose = gtk_print_backend_dispose;
  object_class->set_property = gtk_print_backend_set_property;
  object_class->get_property = gtk_print_backend_get_property;

  class->printer_request_details = fallback_printer_request_details;
  class->printer_mark_conflicts = fallback_printer_mark_conflicts;
  class->printer_get_hard_margins = fallback_printer_get_hard_margins;
  class->printer_get_hard_margins_for_paper_size = fallback_printer_get_hard_margins_for_paper_size;
  class->printer_list_papers = fallback_printer_list_papers;
  class->printer_get_default_page_size = fallback_printer_get_default_page_size;
  class->printer_get_capabilities = fallback_printer_get_capabilities;
  class->request_password = request_password;
  
  g_object_class_install_property (object_class, 
                                   PROP_STATUS,
                                   g_param_spec_int ("status",
                                                     "Status",
                                                     "The status of the print backend",
                                                     GTK_PRINT_BACKEND_STATUS_UNKNOWN,
                                                     GTK_PRINT_BACKEND_STATUS_UNAVAILABLE,
                                                     GTK_PRINT_BACKEND_STATUS_UNKNOWN,
                                                     GTK_PARAM_READWRITE)); 
  
  signals[PRINTER_LIST_CHANGED] =
    g_signal_new (I_("printer-list-changed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_list_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);
  signals[PRINTER_LIST_DONE] =
    g_signal_new (I_("printer-list-done"),
		    G_TYPE_FROM_CLASS (class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintBackendClass, printer_list_done),
		    NULL, NULL,
		    NULL,
		    G_TYPE_NONE, 0);
  signals[PRINTER_ADDED] =
    g_signal_new (I_("printer-added"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_added),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINTER);
  signals[PRINTER_REMOVED] =
    g_signal_new (I_("printer-removed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_removed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINTER);
  signals[PRINTER_STATUS_CHANGED] =
    g_signal_new (I_("printer-status-changed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, printer_status_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINTER);
  signals[REQUEST_PASSWORD] =
    g_signal_new (I_("request-password"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintBackendClass, request_password),
		  NULL, NULL, NULL,
		  G_TYPE_NONE, 6, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER,
		  G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_BOOLEAN);
}

static void
gtk_print_backend_init (GtkPrintBackend *backend)
{
  GtkPrintBackendPrivate *priv;

  priv = backend->priv = gtk_print_backend_get_instance_private (backend);

  priv->printers = g_hash_table_new_full (g_str_hash, g_str_equal, 
					  (GDestroyNotify) g_free,
					  (GDestroyNotify) g_object_unref);
  priv->auth_info_required = NULL;
  priv->auth_info = NULL;
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

static gboolean
fallback_printer_get_hard_margins (GtkPrinter *printer,
				   gdouble    *top,
				   gdouble    *bottom,
				   gdouble    *left,
				   gdouble    *right)
{
  return FALSE;
}

static gboolean
fallback_printer_get_hard_margins_for_paper_size (GtkPrinter   *printer,
						  GtkPaperSize *paper_size,
						  gdouble      *top,
						  gdouble      *bottom,
						  gdouble      *left,
						  gdouble      *right)
{
  return FALSE;
}

static GList *
fallback_printer_list_papers (GtkPrinter *printer)
{
  return NULL;
}

static GtkPageSetup *
fallback_printer_get_default_page_size (GtkPrinter *printer)
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


/**
 * gtk_print_backend_get_printer_list:
 *
 * Returns the current list of printers.
 *
 * Returns: (element-type GtkPrinter) (transfer container):
 *   A list of #GtkPrinter objects. The list should be freed
 *   with g_list_free().
 */
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
gtk_print_backend_set_password (GtkPrintBackend  *backend,
                                gchar           **auth_info_required,
                                gchar           **auth_info,
                                gboolean          store_auth_info)
{
  g_return_if_fail (GTK_IS_PRINT_BACKEND (backend));

  if (GTK_PRINT_BACKEND_GET_CLASS (backend)->set_password)
    GTK_PRINT_BACKEND_GET_CLASS (backend)->set_password (backend,
                                                         auth_info_required,
                                                         auth_info,
                                                         store_auth_info);
}

static void
store_auth_info_toggled (GtkCheckButton *chkbtn,
                         gpointer        user_data)
{
  gboolean *data = (gboolean *) user_data;
  *data = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chkbtn));
}

static void
store_entry (GtkEntry  *entry,
             gpointer   user_data)
{
  gchar **data = (gchar **) user_data;

  if (*data != NULL)
    {
      memset (*data, 0, strlen (*data));
      g_free (*data);
    }

  *data = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
}

static void
password_dialog_response (GtkWidget       *dialog,
                          gint             response_id,
                          GtkPrintBackend *backend)
{
  GtkPrintBackendPrivate *priv = backend->priv;
  gint i, auth_info_len;

  if (response_id == GTK_RESPONSE_OK)
    gtk_print_backend_set_password (backend, priv->auth_info_required, priv->auth_info, priv->store_auth_info);
  else
    gtk_print_backend_set_password (backend, priv->auth_info_required, NULL, FALSE);

  /* We want to clear the data before freeing it */
  auth_info_len = g_strv_length (priv->auth_info_required);
  for (i = 0; i < auth_info_len; i++)
    {
      if (priv->auth_info[i] != NULL)
        {
          memset (priv->auth_info[i], 0, strlen (priv->auth_info[i]));
          g_free (priv->auth_info[i]);
          priv->auth_info[i] = NULL;
        }
    }

  g_clear_pointer (&priv->auth_info, g_free);
  g_clear_pointer (&priv->auth_info_required, g_strfreev);

  gtk_widget_destroy (dialog);

  g_object_unref (backend);
}

static void
request_password (GtkPrintBackend  *backend,
                  gpointer          auth_info_required,
                  gpointer          auth_info_default,
                  gpointer          auth_info_display,
                  gpointer          auth_info_visible,
                  const gchar      *prompt,
                  gboolean          can_store_auth_info)
{
  GtkPrintBackendPrivate *priv = backend->priv;
  GtkWidget *dialog, *box, *main_box, *label, *icon, *vbox, *entry, *chkbtn;
  GtkWidget *focus = NULL;
  GtkWidget *content_area;
  gchar     *markup;
  gint       length;
  gint       i;
  gchar    **ai_required = (gchar **) auth_info_required;
  gchar    **ai_default = (gchar **) auth_info_default;
  gchar    **ai_display = (gchar **) auth_info_display;
  gboolean  *ai_visible = (gboolean *) auth_info_visible;

  priv->auth_info_required = g_strdupv (ai_required);
  length = g_strv_length (ai_required);
  priv->auth_info = g_new0 (gchar *, length + 1);
  priv->store_auth_info = FALSE;

  dialog = gtk_dialog_new_with_buttons ( _("Authentication"), NULL, GTK_DIALOG_MODAL, 
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_OK"), GTK_RESPONSE_OK,
                                         NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  main_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  /* Left */
  icon = gtk_image_new_from_icon_name ("dialog-password-symbolic");
  gtk_image_set_icon_size (GTK_IMAGE (icon), GTK_ICON_SIZE_LARGE);
  gtk_widget_set_halign (icon, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (icon, GTK_ALIGN_START);
  g_object_set (icon, "margin", 12, NULL);

  /* Right */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (GTK_WIDGET (vbox), 320, -1);

  /* Right - 1. */
  label = gtk_label_new (NULL);
  markup = g_markup_printf_escaped ("<span weight=\"bold\" size=\"large\">%s</span>", prompt);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_size_request (GTK_WIDGET (label), 320, -1);
  g_free (markup);


  /* Packing */
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_add (GTK_CONTAINER (content_area), main_box);

  gtk_container_add (GTK_CONTAINER (main_box), icon);
  gtk_container_add (GTK_CONTAINER (main_box), vbox);

  gtk_container_add (GTK_CONTAINER (vbox), label);

  /* Right - 2. */
  for (i = 0; i < length; i++)
    {
      priv->auth_info[i] = g_strdup (ai_default[i]);
      if (ai_display[i] != NULL)
        {
          box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
          gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
          gtk_widget_set_margin_top (box, 6);
          gtk_widget_set_margin_bottom (box, 6);

          label = gtk_label_new (ai_display[i]);
          gtk_widget_set_halign (label, GTK_ALIGN_START);
          gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

          entry = gtk_entry_new ();
          focus = entry;

          if (ai_default[i] != NULL)
            gtk_editable_set_text (GTK_EDITABLE (entry), ai_default[i]);

          gtk_entry_set_visibility (GTK_ENTRY (entry), ai_visible[i]);
          gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

          gtk_container_add (GTK_CONTAINER (vbox), box);

          gtk_container_add (GTK_CONTAINER (box), label);
          gtk_container_add (GTK_CONTAINER (box), entry);

          g_signal_connect (entry, "changed",
                            G_CALLBACK (store_entry), &(priv->auth_info[i]));
        }
    }

  if (can_store_auth_info)
    {
      chkbtn = gtk_check_button_new_with_mnemonic (_("_Remember password"));
      gtk_widget_set_margin_top (chkbtn, 6);
      gtk_widget_set_margin_bottom (chkbtn, 6);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chkbtn), FALSE);
      gtk_container_add (GTK_CONTAINER (vbox), chkbtn);
      g_signal_connect (chkbtn, "toggled",
                        G_CALLBACK (store_auth_info_toggled),
                        &(priv->store_auth_info));
    }

  if (focus != NULL)
    {
      gtk_widget_grab_focus (focus);
      focus = NULL;
    }

  g_object_ref (backend);
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (password_dialog_response), backend);

  gtk_widget_show (dialog);
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
